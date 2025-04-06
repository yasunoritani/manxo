/**
 * Max/MSP知識ベースAPIサーバー
 * SQLiteデータベースへのアクセスを提供し、知識検索APIを公開します
 */

const express = require('express');
const Database = require('better-sqlite3');
const cors = require('cors');
const path = require('path');

// 設定
const PORT = process.env.PORT || 3000;
const DB_PATH = path.join(__dirname, 'max_knowledge.sqlite');

// データベース接続
let db;
try {
    db = new Database(DB_PATH, { verbose: process.env.NODE_ENV === 'development' });

    // パフォーマンス最適化
    db.pragma('journal_mode = WAL');
    db.pragma('synchronous = NORMAL');
    db.pragma('cache_size = 1000');
    db.pragma('temp_store = MEMORY');

    console.log('Max知識ベースデータベースに接続しました');

    // データベース初期化時にインデックスを作成
    db.exec(`
        CREATE INDEX IF NOT EXISTS idx_max_objects_name ON max_objects(name);
        CREATE INDEX IF NOT EXISTS idx_max_objects_category ON max_objects(category);
        CREATE INDEX IF NOT EXISTS idx_min_devkit_function_name ON min_devkit_api(function_name);
        CREATE INDEX IF NOT EXISTS idx_connection_patterns_objects ON connection_patterns(source_object, destination_object);
        CREATE INDEX IF NOT EXISTS idx_validation_rules_type ON validation_rules(rule_type);
        CREATE INDEX IF NOT EXISTS idx_api_mapping_intent ON api_mapping(natural_language_intent);
    `);

} catch (err) {
    console.error('データベース接続エラー:', err.message);
    process.exit(1);
}

// キャッシュメカニズム
const cache = {
    categories: null,
    categoryExpiry: 0,
    maxObjects: {},
    maxObjectsExpiry: {},
    search: {},
    searchExpiry: {},
};

// キャッシュの有効期間（ミリ秒）
const CACHE_TTL = 60 * 60 * 1000; // 1時間

// キャッシュ取得関数
function getCache (key, subKey = null) {
    const now = Date.now();
    if (subKey !== null) {
        if (cache[key] && cache[key][subKey] && cache[`${key}Expiry`][subKey] > now) {
            return cache[key][subKey];
        }
    } else {
        if (cache[key] && cache[`${key}Expiry`] > now) {
            return cache[key];
        }
    }
    return null;
}

// キャッシュ設定関数
function setCache (key, value, subKey = null) {
    const expiry = Date.now() + CACHE_TTL;
    if (subKey !== null) {
        if (!cache[key]) {
            cache[key] = {};
            cache[`${key}Expiry`] = {};
        }
        cache[key][subKey] = value;
        cache[`${key}Expiry`][subKey] = expiry;
    } else {
        cache[key] = value;
        cache[`${key}Expiry`] = expiry;
    }
}

// プリペアドステートメントのキャッシュ
const statements = {
    // よく使われるクエリをプリペアしておく
    getMaxObjectByName: db.prepare('SELECT * FROM max_objects WHERE name = ?'),
    getAllCategories: db.prepare('SELECT DISTINCT category FROM max_objects ORDER BY category'),
    getMaxObjectsByCategory: db.prepare('SELECT * FROM max_objects WHERE category = ? LIMIT ?'),
    searchMaxObjects: db.prepare('SELECT * FROM max_objects WHERE name LIKE ? OR description LIKE ? LIMIT ?'),
    searchMinDevkitApi: db.prepare('SELECT * FROM min_devkit_api WHERE function_name LIKE ? OR description LIKE ? LIMIT ?'),
    searchConnectionPatterns: db.prepare('SELECT * FROM connection_patterns WHERE source_object LIKE ? OR destination_object LIKE ? OR description LIKE ? LIMIT ?'),
    searchValidationRules: db.prepare('SELECT * FROM validation_rules WHERE rule_type LIKE ? OR description LIKE ? OR pattern LIKE ? LIMIT ?'),
    searchApiMapping: db.prepare('SELECT am.*, mda.function_name FROM api_mapping am LEFT JOIN min_devkit_api mda ON am.min_devkit_function_id = mda.id WHERE am.natural_language_intent LIKE ? OR am.transformation_template LIKE ? LIMIT ?'),
};

// Expressアプリケーション
const app = express();
app.use(express.json());
app.use(cors());

// パフォーマンスモニタリングミドルウェア
app.use((req, res, next) => {
    const start = Date.now();
    res.on('finish', () => {
        const duration = Date.now() - start;
        if (duration > 100) { // 100ms以上かかったリクエストをログ
            console.log(`${req.method} ${req.originalUrl} - ${duration}ms`);
        }
    });
    next();
});

// APIヘルスチェック
app.get('/api/health', (req, res) => {
    res.json({ status: 'ok', message: 'Max知識ベースAPIサーバー稼働中' });
});

// Maxオブジェクト関連API
app.get('/api/max-objects/categories', (req, res) => {
    try {
        // キャッシュから取得を試みる
        const cachedCategories = getCache('categories');
        if (cachedCategories) {
            return res.json(cachedCategories);
        }

        // キャッシュがない場合はDBから取得
        const rows = statements.getAllCategories.all();
        const categories = rows.map(row => row.category).filter(cat => cat);

        // キャッシュに保存
        setCache('categories', categories);

        res.json(categories);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/max-objects/:name', (req, res) => {
    const name = req.params.name;

    try {
        // キャッシュから取得を試みる
        const cachedObject = getCache('maxObjects', name);
        if (cachedObject) {
            return res.json(cachedObject);
        }

        // キャッシュがない場合はDBから取得
        const row = statements.getMaxObjectByName.get(name);
        if (!row) {
            return res.status(404).json({ error: 'オブジェクトが見つかりません' });
        }

        // キャッシュに保存
        setCache('maxObjects', row, name);

        res.json(row);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/max-objects', (req, res) => {
    const query = req.query.query || '';
    const category = req.query.category || '';
    const limit = parseInt(req.query.limit) || 50;

    try {
        // カテゴリが指定されている場合は最適化されたクエリとキャッシュを使用
        if (category && !query) {
            const cacheKey = `category_${category}_limit_${limit}`;
            const cachedObjects = getCache('maxObjects', cacheKey);

            if (cachedObjects) {
                return res.json(cachedObjects);
            }

            const rows = statements.getMaxObjectsByCategory.all(category, limit);
            setCache('maxObjects', rows, cacheKey);
            return res.json(rows);
        }

        // 通常のクエリ処理
        let sql = 'SELECT * FROM max_objects WHERE 1=1';
        const params = [];

        if (query) {
            sql += ' AND (name LIKE ? OR description LIKE ?)';
            params.push(`%${query}%`, `%${query}%`);
        }

        if (category) {
            sql += ' AND category = ?';
            params.push(category);
        }

        sql += ' LIMIT ?';
        params.push(limit);

        // クエリキャッシュのキー
        const cacheKey = `${query}_${category}_${limit}`;
        const cachedResults = getCache('maxObjects', cacheKey);

        if (cachedResults) {
            return res.json(cachedResults);
        }

        const stmt = db.prepare(sql);
        const rows = stmt.all(...params);

        // キャッシュに保存
        setCache('maxObjects', rows, cacheKey);

        res.json(rows);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// Min-DevKit API関連
app.get('/api/min-devkit', (req, res) => {
    const query = req.query.query || '';
    const returnType = req.query.return_type || '';
    const limit = parseInt(req.query.limit) || 50;

    try {
        let sql = 'SELECT * FROM min_devkit_api WHERE 1=1';
        const params = [];

        if (query) {
            sql += ' AND (function_name LIKE ? OR description LIKE ?)';
            params.push(`%${query}%`, `%${query}%`);
        }

        if (returnType) {
            sql += ' AND return_type = ?';
            params.push(returnType);
        }

        sql += ' LIMIT ?';
        params.push(limit);

        const rows = db.prepare(sql).all(...params);
        res.json(rows);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

app.get('/api/min-devkit/:functionName', (req, res) => {
    const functionName = req.params.functionName;

    try {
        const row = db.prepare('SELECT * FROM min_devkit_api WHERE function_name = ?').get(functionName);
        if (!row) {
            return res.status(404).json({ error: '関数が見つかりません' });
        }
        res.json(row);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// 接続パターン関連API
app.get('/api/connection-patterns', (req, res) => {
    const sourceObject = req.query.source || '';
    const destObject = req.query.destination || '';
    const limit = parseInt(req.query.limit) || 50;

    try {
        let sql = 'SELECT * FROM connection_patterns WHERE 1=1';
        const params = [];

        if (sourceObject) {
            sql += ' AND source_object LIKE ?';
            params.push(`%${sourceObject}%`);
        }

        if (destObject) {
            sql += ' AND destination_object LIKE ?';
            params.push(`%${destObject}%`);
        }

        sql += ' LIMIT ?';
        params.push(limit);

        const rows = db.prepare(sql).all(...params);
        res.json(rows);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// 検証ルール関連API
app.get('/api/validation-rules', (req, res) => {
    const ruleType = req.query.type || '';
    const severity = req.query.severity || '';
    const limit = parseInt(req.query.limit) || 50;

    try {
        let sql = 'SELECT * FROM validation_rules WHERE 1=1';
        const params = [];

        if (ruleType) {
            sql += ' AND rule_type = ?';
            params.push(ruleType);
        }

        if (severity) {
            sql += ' AND severity = ?';
            params.push(severity);
        }

        sql += ' LIMIT ?';
        params.push(limit);

        const rows = db.prepare(sql).all(...params);
        res.json(rows);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// API意図マッピング関連API
app.get('/api/api-mapping', (req, res) => {
    const query = req.query.query || '';
    const limit = parseInt(req.query.limit) || 50;

    try {
        let sql = 'SELECT am.*, mda.function_name FROM api_mapping am ' +
            'LEFT JOIN min_devkit_api mda ON am.min_devkit_function_id = mda.id ' +
            'WHERE 1=1';
        const params = [];

        if (query) {
            sql += ' AND (am.natural_language_intent LIKE ? OR am.transformation_template LIKE ?)';
            params.push(`%${query}%`, `%${query}%`);
        }

        sql += ' LIMIT ?';
        params.push(limit);

        const rows = db.prepare(sql).all(...params);
        res.json(rows);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// 自然言語検索API
app.post('/api/search', (req, res) => {
    const { query, type = 'all', limit = 10 } = req.body;

    if (!query) {
        return res.status(400).json({ error: '検索クエリが必要です' });
    }

    try {
        // キャッシュキー
        const cacheKey = `${query}_${type}_${limit}`;
        const cachedResults = getCache('search', cacheKey);

        if (cachedResults) {
            return res.json(cachedResults);
        }

        // 検索結果を格納するオブジェクト
        const results = { objects: [], functions: [], patterns: [], rules: [], mappings: [] };
        const queryLike = `%${query}%`;

        // トランザクションを使用して複数クエリを効率的に実行
        db.transaction(() => {
            // Maxオブジェクト検索
            if (type === 'all' || type === 'objects') {
                results.objects = statements.searchMaxObjects.all(queryLike, queryLike, limit);
            }

            // Min-DevKit API検索
            if (type === 'all' || type === 'functions') {
                results.functions = statements.searchMinDevkitApi.all(queryLike, queryLike, limit);
            }

            // 接続パターン検索
            if (type === 'all' || type === 'patterns') {
                results.patterns = statements.searchConnectionPatterns.all(queryLike, queryLike, queryLike, limit);
            }

            // 検証ルール検索
            if (type === 'all' || type === 'rules') {
                results.rules = statements.searchValidationRules.all(queryLike, queryLike, queryLike, limit);
            }

            // API意図マッピング検索
            if (type === 'all' || type === 'mappings') {
                results.mappings = statements.searchApiMapping.all(queryLike, queryLike, limit);
            }
        })();

        // キャッシュに保存
        setCache('search', results, cacheKey);

        res.json(results);
    } catch (err) {
        res.status(500).json({ error: err.message });
    }
});

// スタティックファイルを提供
app.use(express.static(path.join(__dirname, 'public')));

// サーバー起動
app.listen(PORT, () => {
    console.log(`Max知識ベースAPIサーバーが http://localhost:${PORT} で起動しました`);
});

// プロセス終了時の後処理
process.on('SIGINT', () => {
    if (db) {
        db.close();
        console.log('データベース接続を閉じました');
    }
    process.exit(0);
});
