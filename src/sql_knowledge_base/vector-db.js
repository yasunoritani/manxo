/**
 * SQLiteベクトル検索拡張
 * Max/MSPオブジェクトとパターンのセマンティック検索を可能にする
 */

const path = require('path');
const Database = require('better-sqlite3');
const loadVssExtension = require('sqlite-vss');
const { pipeline } = require('@xenova/transformers');

// データベースパス
const DB_PATH = path.join(__dirname, 'max_knowledge.db');
let db = null;
let embedder = null;

/**
 * ベクトルデータベースを初期化
 */
async function initVectorDB () {
    try {
        // メインデータベース接続
        db = new Database(DB_PATH);

        // VSS拡張をロード
        loadVssExtension(db);

        // 埋め込みモデルを初期化
        embedder = await pipeline('feature-extraction', 'Xenova/all-MiniLM-L6-v2');

        // ベクトル検索用テーブルの存在確認
        const tables = db.prepare(`SELECT name FROM sqlite_master WHERE type='table' AND name='max_objects_vss'`).all();

        if (tables.length === 0) {
            console.log('ベクトル検索テーブルを作成します...');

            // ベクトル検索用テーブル作成
            db.exec(`
        CREATE VIRTUAL TABLE max_objects_vss USING vss0(
          embedding(384),
          id UNINDEXED,
          name UNINDEXED,
          description UNINDEXED,
          category UNINDEXED
        );
      `);

            // 既存オブジェクトの埋め込みを生成して挿入
            await generateEmbeddingsForExistingObjects();
        }

        console.log('ベクトルデータベース初期化完了');
        return true;
    } catch (error) {
        console.error('ベクトルデータベース初期化エラー:', error);
        return false;
    }
}

/**
 * 既存のオブジェクトに対して埋め込みを生成
 */
async function generateEmbeddingsForExistingObjects () {
    try {
        // トランザクション開始
        const insertStmt = db.prepare(`
      INSERT INTO max_objects_vss (id, name, description, category, embedding)
      VALUES (?, ?, ?, ?, ?)
    `);

        // 既存オブジェクトを取得
        const objects = db.prepare(`SELECT * FROM max_objects`).all();

        // バッチ処理
        const batchSize = 50;
        console.log(`${objects.length}個のオブジェクトの埋め込みを生成します...`);

        db.exec('BEGIN TRANSACTION');

        for (let i = 0; i < objects.length; i += batchSize) {
            const batch = objects.slice(i, i + batchSize);

            // 各オブジェクトの埋め込みを並列生成
            await Promise.all(batch.map(async (obj) => {
                // 名前と説明を連結してエンベディング
                const text = `${obj.name} ${obj.description || ''}`;
                const embedding = await generateEmbedding(text);

                // データベースに挿入
                insertStmt.run(obj.id, obj.name, obj.description, obj.category, embedding);
            }));

            console.log(`進捗: ${Math.min(i + batchSize, objects.length)}/${objects.length}`);
        }

        db.exec('COMMIT');
        console.log('埋め込み生成完了');
    } catch (error) {
        db.exec('ROLLBACK');
        console.error('埋め込み生成エラー:', error);
        throw error;
    }
}

/**
 * テキストの埋め込みベクトルを生成
 * @param {string} text - 埋め込みを生成するテキスト
 * @returns {Float32Array} - 埋め込みベクトル
 */
async function generateEmbedding (text) {
    if (!embedder) {
        throw new Error('埋め込みモデルが初期化されていません');
    }

    try {
        const output = await embedder(text, { pooling: 'mean', normalize: true });
        return Array.from(output.data);
    } catch (error) {
        console.error('埋め込み生成エラー:', error);
        throw error;
    }
}

/**
 * 自然言語クエリに基づいてオブジェクトをセマンティック検索
 * @param {string} query - 検索クエリ
 * @param {number} limit - 結果の最大数
 * @returns {Array} - 関連オブジェクトの配列
 */
async function semanticSearch (query, limit = 10) {
    try {
        if (!db) {
            await initVectorDB();
        }

        // クエリの埋め込みを生成
        const embedding = await generateEmbedding(query);

        // 類似度検索を実行
        const results = db.prepare(`
      SELECT id, name, description, category, distance
      FROM max_objects_vss
      WHERE vss_search(embedding, ?)
      LIMIT ?
    `).all(embedding, limit);

        // 元のオブジェクト情報を取得して結果と結合
        const enrichedResults = results.map(result => {
            const obj = db.prepare(`
        SELECT * FROM max_objects WHERE id = ?
      `).get(result.id);

            return {
                ...obj,
                similarity: 1 - result.distance // 距離を類似度に変換
            };
        });

        return enrichedResults;
    } catch (error) {
        console.error('セマンティック検索エラー:', error);
        return [];
    }
}

/**
 * 自然言語クエリに基づいて接続パターンをセマンティック検索
 * @param {string} query - 検索クエリ
 * @param {number} limit - 結果の最大数
 * @returns {Array} - 関連接続パターンの配列
 */
async function semanticSearchConnectionPatterns (query, limit = 10) {
    try {
        if (!db) {
            await initVectorDB();
        }

        // クエリの埋め込みを生成
        const embedding = await generateEmbedding(query);

        // オブジェクト名とカテゴリの類似度検索
        const relevantObjects = db.prepare(`
      SELECT id, name, category, distance
      FROM max_objects_vss
      WHERE vss_search(embedding, ?)
      LIMIT 20
    `).all(embedding);

        // 関連オブジェクト名のリスト
        const objectNames = relevantObjects.map(obj => obj.name);

        // 関連オブジェクトを含む接続パターンを検索
        let patterns = [];

        if (objectNames.length > 0) {
            // プレースホルダー作成
            const placeholders = objectNames.map(() => '?').join(',');

            // 接続パターン検索
            patterns = db.prepare(`
        SELECT * FROM connection_patterns
        WHERE source_object IN (${placeholders})
           OR destination_object IN (${placeholders})
        LIMIT ?
      `).all([...objectNames, ...objectNames, limit]);
        }

        return patterns;
    } catch (error) {
        console.error('接続パターンセマンティック検索エラー:', error);
        return [];
    }
}

/**
 * データベース接続をクローズ
 */
function closeVectorDB () {
    if (db) {
        db.close();
        db = null;
    }
}

module.exports = {
    initVectorDB,
    semanticSearch,
    semanticSearchConnectionPatterns,
    generateEmbedding,
    closeVectorDB
};
