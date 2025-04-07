/**
 * Max/MSP知識ベースAPIサーバー
 * SQLiteデータベースへのアクセスを提供し、知識検索APIを公開します
 * Issue #010c 対応 - クエリエンジンとAPIの実装
 */

const express = require('express');
const sqlite3 = require('sqlite3').verbose();
const cors = require('cors');
const path = require('path');

// 設定
const PORT = process.env.PORT || 3000;
const DB_PATH = path.join(__dirname, 'max_claude_kb.db');

// プリペアドステートメントのキャッシュ
const statements = {};

// データベース接続
let db;
try {
    db = new sqlite3.Database(DB_PATH, (err) => {
        if (err) {
            console.error('データベース接続エラー:', err.message);
            process.exit(1);
        }
        console.log('Max知識ベースデータベースに接続しました');

        // データベース設定
        db.serialize(() => {
            db.run('PRAGMA journal_mode = WAL');
            db.run('PRAGMA synchronous = NORMAL');
            db.run('PRAGMA cache_size = 1000');
            db.run('PRAGMA temp_store = MEMORY');

            // インデックスの作成
            db.run('CREATE INDEX IF NOT EXISTS idx_max_objects_name ON max_objects(name)');
            db.run('CREATE INDEX IF NOT EXISTS idx_max_objects_category ON max_objects(category)');
            db.run('CREATE INDEX IF NOT EXISTS idx_min_devkit_function_name ON min_devkit_api(function_name)');
            db.run('CREATE INDEX IF NOT EXISTS idx_connection_patterns_objects ON connection_patterns(source_object, destination_object)');
            db.run('CREATE INDEX IF NOT EXISTS idx_validation_rules_type ON validation_rules(rule_type)');
            db.run('CREATE INDEX IF NOT EXISTS idx_api_mapping_intent ON api_mapping(natural_language_intent)');
        });
    });
} catch (err) {
    console.error('データベース初期化エラー:', err.message);
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
    validationRules: {},
    validationRulesExpiry: {},
    connectionPatterns: {},
    connectionPatternsExpiry: {},
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

// ステートメント準備関数
function prepareStatement (sql) {
    return {
        all: (params, callback) => {
            return new Promise((resolve, reject) => {
                if (callback) {
                    db.all(sql, params, callback);
                } else {
                    db.all(sql, params, (err, rows) => {
                        if (err) reject(err);
                        else resolve(rows);
                    });
                }
            });
        },
        get: (params, callback) => {
            return new Promise((resolve, reject) => {
                if (callback) {
                    db.get(sql, params, callback);
                } else {
                    db.get(sql, params, (err, row) => {
                        if (err) reject(err);
                        else resolve(row);
                    });
                }
            });
        },
        run: (params, callback) => {
            return new Promise((resolve, reject) => {
                if (callback) {
                    db.run(sql, params, callback);
                } else {
                    db.run(sql, params, function (err) {
                        if (err) reject(err);
                        else resolve(this);
                    });
                }
            });
        }
    };
}

// ステートメントを初期化
function initStatements () {
    statements.getMaxObjectByName = prepareStatement('SELECT * FROM max_objects WHERE name = ?');
    statements.getAllCategories = prepareStatement('SELECT DISTINCT category FROM max_objects ORDER BY category');
    statements.getMaxObjectsByCategory = prepareStatement('SELECT * FROM max_objects WHERE category = ? LIMIT ?');
    statements.searchMaxObjects = prepareStatement('SELECT * FROM max_objects WHERE name LIKE ? OR description LIKE ? LIMIT ?');
    statements.searchMinDevkitApi = prepareStatement('SELECT * FROM min_devkit_api WHERE function_name LIKE ? OR description LIKE ? LIMIT ?');
    statements.searchConnectionPatterns = prepareStatement('SELECT * FROM connection_patterns WHERE source_object LIKE ? OR destination_object LIKE ? OR description LIKE ? LIMIT ?');
    statements.searchValidationRules = prepareStatement('SELECT * FROM validation_rules WHERE rule_type LIKE ? OR description LIKE ? OR pattern LIKE ? LIMIT ?');
    statements.searchApiMapping = prepareStatement('SELECT am.*, mda.function_name FROM api_mapping am LEFT JOIN min_devkit_api mda ON am.min_devkit_function_id = mda.id WHERE am.natural_language_intent LIKE ? OR am.transformation_template LIKE ? LIMIT ?');

    // 追加クエリ
    statements.getConnectionPattern = prepareStatement('SELECT * FROM connection_patterns WHERE source_object = ? AND destination_object = ? AND source_outlet = ? AND destination_inlet = ?');
    statements.getFunctionsByReturnType = prepareStatement('SELECT * FROM min_devkit_api WHERE return_type = ? LIMIT ?');
    statements.getValidationRulesByType = prepareStatement('SELECT * FROM validation_rules WHERE rule_type = ? LIMIT ?');
    statements.getApiMappingByIntent = prepareStatement('SELECT am.*, mda.function_name, mda.signature, mda.description AS function_description FROM api_mapping am LEFT JOIN min_devkit_api mda ON am.min_devkit_function_id = mda.id WHERE am.natural_language_intent LIKE ? LIMIT ?');
    statements.getRecommendedConnections = prepareStatement('SELECT * FROM connection_patterns WHERE is_recommended = 1 LIMIT ?');
    statements.getAudioConnections = prepareStatement('SELECT * FROM connection_patterns WHERE audio_signal_flow = 1 LIMIT ?');
}

// ステートメントの初期化（データベース接続後に実行）
setTimeout(initStatements, 1000);

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

// APIエラーハンドリング関数
function handleApiError (res, err, statusCode = 500) {
    console.error('API Error:', err);
    res.status(statusCode).json({
        error: err.message,
        status: 'error',
        timestamp: new Date().toISOString()
    });
}

// APIヘルスチェック
app.get('/api/health', (req, res) => {
    try {
        // クイックデータベースクエリで接続を確認
        const result = db.prepare('SELECT COUNT(*) as count FROM max_objects').get();
        const dbStatus = result && result.count > 0 ? 'connected' : 'error';

        res.json({
            status: 'ok',
            message: 'Max知識ベースAPIサーバー稼働中',
            database: dbStatus,
            timestamp: new Date().toISOString(),
            version: '1.0.0'
        });
    } catch (err) {
        handleApiError(res, err);
    }
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
        handleApiError(res, err);
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
        handleApiError(res, err);
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
        handleApiError(res, err);
    }
});

// Min-DevKit API関連
app.get('/api/min-devkit', (req, res) => {
    const query = req.query.query || '';
    const returnType = req.query.return_type || '';
    const limit = parseInt(req.query.limit) || 50;

    try {
        // 戻り値の型によるフィルタリングが指定されている場合
        if (returnType && !query) {
            const rows = statements.getFunctionsByReturnType.all(returnType, limit);
            return res.json(rows);
        }

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
        handleApiError(res, err);
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
        handleApiError(res, err);
    }
});

// 接続パターン関連API
app.get('/api/connection-patterns', (req, res) => {
    const sourceObject = req.query.source || '';
    const destObject = req.query.destination || '';
    const isRecommended = req.query.recommended === 'true';
    const isAudio = req.query.audio === 'true';
    const limit = parseInt(req.query.limit) || 50;

    try {
        // 推奨接続パターンのみを取得
        if (isRecommended && !sourceObject && !destObject) {
            const rows = statements.getRecommendedConnections.all(limit);
            return res.json(rows);
        }

        // オーディオシグナルフローのみを取得
        if (isAudio && !sourceObject && !destObject) {
            const rows = statements.getAudioConnections.all(limit);
            return res.json(rows);
        }

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

        if (isRecommended) {
            sql += ' AND is_recommended = 1';
        }

        if (isAudio) {
            sql += ' AND audio_signal_flow = 1';
        }

        sql += ' LIMIT ?';
        params.push(limit);

        const rows = db.prepare(sql).all(...params);
        res.json(rows);
    } catch (err) {
        handleApiError(res, err);
    }
});

// 接続検証API
app.get('/api/validate-connection', (req, res) => {
    const sourceObject = req.query.source;
    const destinationObject = req.query.destination;
    const sourceOutlet = parseInt(req.query.source_outlet || '0');
    const destinationInlet = parseInt(req.query.destination_inlet || '0');

    if (!sourceObject || !destinationObject) {
        return res.status(400).json({
            error: 'ソースオブジェクトと送信先オブジェクトの両方が必要です'
        });
    }

    try {
        const cacheKey = `${sourceObject}_${sourceOutlet}_${destinationObject}_${destinationInlet}`;
        const cachedResult = getCache('connectionPatterns', cacheKey);

        if (cachedResult) {
            return res.json(cachedResult);
        }

        // 完全一致する接続パターンを探す
        const pattern = statements.getConnectionPattern.get(
            sourceObject,
            destinationObject,
            sourceOutlet,
            destinationInlet
        );

        if (pattern) {
            const result = {
                valid: true,
                pattern: pattern,
                is_recommended: Boolean(pattern.is_recommended),
                audio_signal_flow: Boolean(pattern.audio_signal_flow)
            };

            // キャッシュに保存
            setCache('connectionPatterns', result, cacheKey);

            return res.json(result);
        }

        // 完全一致するパターンが見つからない場合、類似パターンを探す
        const similarPatterns = statements.searchConnectionPatterns.all(
            `%${sourceObject}%`,
            `%${destinationObject}%`,
            '',
            10
        );

        const result = {
            valid: false,
            message: '特定の接続パターンが見つかりませんでした',
            similar_patterns: similarPatterns
        };

        // キャッシュに保存
        setCache('connectionPatterns', result, cacheKey);

        res.json(result);
    } catch (err) {
        handleApiError(res, err);
    }
});

// 検証ルール関連API
app.get('/api/validation-rules', (req, res) => {
    const ruleType = req.query.type || '';
    const severity = req.query.severity || '';
    const limit = parseInt(req.query.limit) || 50;

    try {
        // タイプによるフィルタリングが指定されている場合はキャッシュを活用
        if (ruleType && !severity) {
            const cacheKey = `type_${ruleType}_limit_${limit}`;
            const cachedRules = getCache('validationRules', cacheKey);

            if (cachedRules) {
                return res.json(cachedRules);
            }

            const rows = statements.getValidationRulesByType.all(ruleType, limit);
            setCache('validationRules', rows, cacheKey);
            return res.json(rows);
        }

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
        handleApiError(res, err);
    }
});

// コード検証API
app.post('/api/validate-code', (req, res) => {
    const { code, context } = req.body;

    if (!code) {
        return res.status(400).json({ error: '検証するコードが必要です' });
    }

    try {
        // まず適切な検証ルールを取得
        let ruleQuery = 'SELECT * FROM validation_rules WHERE 1=1';
        const params = [];

        if (context) {
            ruleQuery += ' AND (context_requirements LIKE ? OR context_requirements IS NULL)';
            params.push(`%${context}%`);
        }

        const rules = db.prepare(ruleQuery).all(...params);
        const issues = [];

        // 各ルールでコードをチェック
        rules.forEach(rule => {
            try {
                const regex = new RegExp(rule.pattern, 'g');
                const matches = code.match(regex);

                if (matches) {
                    issues.push({
                        rule_id: rule.id,
                        rule_type: rule.rule_type,
                        severity: rule.severity,
                        description: rule.description,
                        suggestion: rule.suggestion,
                        example_fix: rule.example_fix,
                        matches: matches.length
                    });
                }
            } catch (error) {
                console.error(`正規表現エラー (${rule.pattern}): ${error.message}`);
            }
        });

        res.json({
            valid: issues.length === 0,
            issues: issues,
            total_issues: issues.length,
            code_length: code.length
        });
    } catch (err) {
        handleApiError(res, err);
    }
});

// API意図マッピング関連API
app.get('/api/api-mapping', (req, res) => {
    const query = req.query.query || '';
    const limit = parseInt(req.query.limit) || 50;

    try {
        if (query) {
            const mappings = statements.getApiMappingByIntent.all(`%${query}%`, limit);
            return res.json(mappings);
        }

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
        handleApiError(res, err);
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
        handleApiError(res, err);
    }
});

// Node for Max統合API
app.post('/api/node-for-max/query', (req, res) => {
    const { action, params } = req.body;

    if (!action) {
        return res.status(400).json({ error: '実行するアクションが必要です' });
    }

    try {
        let result = null;

        switch (action) {
            case 'get_max_object':
                if (!params.name) {
                    return res.status(400).json({ error: 'オブジェクト名が必要です' });
                }
                result = statements.getMaxObjectByName.get(params.name);
                break;

            case 'check_connection':
                if (!params.source || !params.destination) {
                    return res.status(400).json({ error: 'ソースと送信先が必要です' });
                }
                result = statements.getConnectionPattern.get(
                    params.source,
                    params.destination,
                    params.source_outlet || 0,
                    params.destination_inlet || 0
                );
                break;

            case 'search_max_objects':
                result = statements.searchMaxObjects.all(
                    `%${params.query || ''}%`,
                    `%${params.query || ''}%`,
                    params.limit || 10
                );
                break;

            case 'find_api_by_intent':
                result = statements.getApiMappingByIntent.all(
                    `%${params.intent || ''}%`,
                    params.limit || 5
                );
                break;

            case 'validate_rules':
                result = statements.getValidationRulesByType.all(
                    params.rule_type || 'syntax',
                    params.limit || 20
                );
                break;

            default:
                return res.status(400).json({ error: '不明なアクション: ' + action });
        }

        res.json({
            status: 'success',
            action: action,
            result: result
        });
    } catch (err) {
        handleApiError(res, err);
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
