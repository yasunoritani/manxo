/**
 * ベクトルDBクライアント（MCP用）
 * MCPを使用したベクトル検索実装
 */

const fs = require('fs').promises;
const fsSync = require('fs');
const path = require('path');
const crypto = require('crypto');

// ベースディレクトリ設定
const vectorDbDir = path.join(__dirname, '../../data/vector_db');

// キャッシュ設定
const CACHE_ENABLED = true;
const CACHE_TTL = 60 * 1000; // 60秒
const MAX_CACHE_SIZE = 100;  // 最大キャッシュエントリ数

// キャッシュ用オブジェクト
const cache = {
  databaseInfo: null,
  dbInfoTimestamp: 0,
  embeddings: new Map(),
  results: new Map(),
  indexes: new Map()
};

/**
 * パスの安全性を検証
 * @param {string} basePath - ベースパス
 * @param {string} targetPath - 検証対象パス
 * @returns {boolean} - 安全性
 */
function isPathSafe(basePath, targetPath) {
  // 絶対パスに変換
  const absoluteBasePath = path.resolve(basePath);
  const absoluteTargetPath = path.resolve(targetPath);
  
  // ターゲットパスがベースパスから始まっていることを確認
  return absoluteTargetPath.startsWith(absoluteBasePath);
}

/**
 * データベース情報を読み込む
 * @returns {Promise<Object>} - データベース情報
 */
async function loadDatabaseInfo() {
  try {
    // キャッシュが有効で有効期限内の場合はキャッシュから返す
    if (CACHE_ENABLED && 
        cache.databaseInfo && 
        (Date.now() - cache.dbInfoTimestamp < CACHE_TTL)) {
      return cache.databaseInfo;
    }
    
    // データベース情報ファイルパス
    const dbInfoPath = path.join(vectorDbDir, 'database_info.json');
    
    // パスの安全性を検証
    if (!isPathSafe(vectorDbDir, dbInfoPath)) {
      throw new Error('不正なパス操作が検出されました');
    }
    
    // ファイルの存在チェック
    if (!fsSync.existsSync(dbInfoPath)) {
      throw new Error('vector_db/database_info.jsonが見つかりません');
    }
    
    // パフォーマンス計測開始
    const startTime = Date.now();
    
    // ファイル読み込み
    const data = await fs.readFile(dbInfoPath, 'utf8');
    const dbInfo = JSON.parse(data);
    
    // 検証
    if (!dbInfo || !dbInfo.indexes || !Array.isArray(dbInfo.indexes)) {
      throw new Error('database_info.jsonの形式が無効です');
    }
    
    // キャッシュ更新
    if (CACHE_ENABLED) {
      cache.databaseInfo = dbInfo;
      cache.dbInfoTimestamp = Date.now();
    }
    
    const endTime = Date.now();
    console.log(`DB情報読み込みパフォーマンス: ${endTime - startTime}ms`);
    
    return dbInfo;
  } catch (error) {
    console.error('データベース情報読み込みエラー:', error);
    throw new Error(`データベース情報の読み込み中にエラーが発生しました: ${error.message}`);
  }
}

/**
 * インデックスの埋め込みベクトルを読み込む
 * @param {string} indexName - インデックス名
 * @returns {Promise<Object>} - 埋め込みデータ
 */
async function loadEmbeddings(indexName) {
  try {
    // キャッシュキー
    const cacheKey = `embedding:${indexName}`;
    
    // キャッシュチェック
    if (CACHE_ENABLED && cache.embeddings.has(cacheKey)) {
      const cachedEmbedding = cache.embeddings.get(cacheKey);
      if (Date.now() - cachedEmbedding.timestamp < CACHE_TTL) {
        return cachedEmbedding.data;
      }
    }
    
    // データベース情報からインデックスを検索
    const dbInfo = await loadDatabaseInfo();
    const indexInfo = dbInfo.indexes.find(idx => idx.name === indexName);
    
    if (!indexInfo) {
      throw new Error(`インデックス「${indexName}」が見つかりません`);
    }
    
    // 埋め込みファイルパス
    const embeddingPath = path.join(vectorDbDir, 'embeddings', `${indexName}.json`);
    
    // パスの安全性を検証
    if (!isPathSafe(vectorDbDir, embeddingPath)) {
      throw new Error('不正なパス操作が検出されました');
    }
    
    // ファイルの存在チェック
    if (!fsSync.existsSync(embeddingPath)) {
      throw new Error(`埋め込みファイル「${indexName}.json」が見つかりません`);
    }
    
    // パフォーマンス計測開始
    const startTime = Date.now();
    
    // ファイル読み込み
    const data = await fs.readFile(embeddingPath, 'utf8');
    const embeddings = JSON.parse(data);
    
    // 検証
    if (!embeddings || !embeddings.vectors || !Array.isArray(embeddings.vectors)) {
      throw new Error(`埋め込みファイル「${indexName}.json」の形式が無効です`);
    }
    
    // キャッシュ更新
    if (CACHE_ENABLED) {
      cache.embeddings.set(cacheKey, {
        data: embeddings,
        timestamp: Date.now()
      });
      
      // キャッシュサイズチェック
      if (cache.embeddings.size > MAX_CACHE_SIZE) {
        const oldestKey = cache.embeddings.keys().next().value;
        cache.embeddings.delete(oldestKey);
      }
    }
    
    const endTime = Date.now();
    console.log(`埋め込み読み込みパフォーマンス: ${endTime - startTime}ms, インデックス: ${indexName}`);
    
    return embeddings;
  } catch (error) {
    console.error('埋め込み読み込みエラー:', error);
    throw new Error(`埋め込み「${indexName}」の読み込み中にエラーが発生しました: ${error.message}`);
  }
}

/**
 * メタデータを読み込む
 * @param {string} indexName - インデックス名
 * @returns {Promise<Object>} - メタデータ
 */
async function loadMetadata(indexName) {
  try {
    // キャッシュキー
    const cacheKey = `metadata:${indexName}`;
    
    // キャッシュチェック
    if (CACHE_ENABLED && cache.indexes.has(cacheKey)) {
      const cachedMetadata = cache.indexes.get(cacheKey);
      if (Date.now() - cachedMetadata.timestamp < CACHE_TTL) {
        return cachedMetadata.data;
      }
    }
    
    // メタデータファイルパス
    const metadataPath = path.join(vectorDbDir, 'metadata', `${indexName}.json`);
    
    // パスの安全性を検証
    if (!isPathSafe(vectorDbDir, metadataPath)) {
      throw new Error('不正なパス操作が検出されました');
    }
    
    // ファイルの存在チェック
    if (!fsSync.existsSync(metadataPath)) {
      throw new Error(`メタデータファイル「${indexName}.json」が見つかりません`);
    }
    
    // パフォーマンス計測開始
    const startTime = Date.now();
    
    // ファイル読み込み
    const data = await fs.readFile(metadataPath, 'utf8');
    const metadata = JSON.parse(data);
    
    // 検証
    if (!metadata || !metadata.items || !Array.isArray(metadata.items)) {
      throw new Error(`メタデータファイル「${indexName}.json」の形式が無効です`);
    }
    
    // キャッシュ更新
    if (CACHE_ENABLED) {
      cache.indexes.set(cacheKey, {
        data: metadata,
        timestamp: Date.now()
      });
      
      // キャッシュサイズチェック
      if (cache.indexes.size > MAX_CACHE_SIZE) {
        const oldestKey = cache.indexes.keys().next().value;
        cache.indexes.delete(oldestKey);
      }
    }
    
    const endTime = Date.now();
    console.log(`メタデータ読み込みパフォーマンス: ${endTime - startTime}ms, インデックス: ${indexName}`);
    
    return metadata;
  } catch (error) {
    console.error('メタデータ読み込みエラー:', error);
    throw new Error(`メタデータ「${indexName}」の読み込み中にエラーが発生しました: ${error.message}`);
  }
}

/**
 * ユークリッド距離を計算
 * @param {Array<number>} vec1 - ベクトル1
 * @param {Array<number>} vec2 - ベクトル2
 * @returns {number} - ユークリッド距離
 */
function euclideanDistance(vec1, vec2) {
  if (!Array.isArray(vec1) || !Array.isArray(vec2) || vec1.length !== vec2.length) {
    throw new Error('無効なベクトル形式');
  }
  
  let sum = 0;
  for (let i = 0; i < vec1.length; i++) {
    const diff = vec1[i] - vec2[i];
    sum += diff * diff;
  }
  return Math.sqrt(sum);
}

/**
 * コサイン類似度を計算
 * @param {Array<number>} vec1 - ベクトル1
 * @param {Array<number>} vec2 - ベクトル2
 * @returns {number} - コサイン類似度
 */
function cosineSimilarity(vec1, vec2) {
  if (!Array.isArray(vec1) || !Array.isArray(vec2) || vec1.length !== vec2.length) {
    throw new Error('無効なベクトル形式');
  }
  
  let dotProduct = 0;
  let norm1 = 0;
  let norm2 = 0;
  
  for (let i = 0; i < vec1.length; i++) {
    dotProduct += vec1[i] * vec2[i];
    norm1 += vec1[i] * vec1[i];
    norm2 += vec2[i] * vec2[i];
  }
  
  // ゼロベクトルのチェック
  if (norm1 === 0 || norm2 === 0) {
    return 0;
  }
  
  return dotProduct / (Math.sqrt(norm1) * Math.sqrt(norm2));
}

/**
 * vector_db内のテキストクエリを行う
 * @param {string} query - 検索クエリ
 * @param {Object} options - 検索オプション
 * @returns {Promise<Array>} - 検索結果
 */
async function vectorTextQuery(query, options = {}) {
  if (!query || typeof query !== 'string') {
    return Promise.reject(new Error('有効な検索クエリが必要です'));
  }
  
  try {
    // デフォルトオプション
    const defaultOptions = {
      index: 'default',
      limit: 5,
      threshold: 0.7,
      method: 'cosine'
    };
    
    // オプションのマージ
    const mergedOptions = { ...defaultOptions, ...options };
    
    // オプションの検証
    if (typeof mergedOptions.limit !== 'number' || mergedOptions.limit < 1) {
      mergedOptions.limit = 5;
    }
    
    if (typeof mergedOptions.threshold !== 'number' || 
        mergedOptions.threshold < 0 || 
        mergedOptions.threshold > 1) {
      mergedOptions.threshold = 0.7;
    }
    
    if (!['cosine', 'euclidean'].includes(mergedOptions.method)) {
      mergedOptions.method = 'cosine';
    }
    
    // キャッシュキー
    const cacheKey = `query:${query}:${JSON.stringify(mergedOptions)}`;
    
    // キャッシュチェック
    if (CACHE_ENABLED && cache.results.has(cacheKey)) {
      const cachedResult = cache.results.get(cacheKey);
      if (Date.now() - cachedResult.timestamp < CACHE_TTL) {
        return cachedResult.data;
      }
    }
    
    // パフォーマンス計測開始
    const totalStartTime = Date.now();
    
    // データベース情報を取得してインデックスの存在を確認
    const dbInfo = await loadDatabaseInfo();
    const indexInfo = dbInfo.indexes.find(idx => idx.name === mergedOptions.index);
    
    if (!indexInfo) {
      throw new Error(`インデックス「${mergedOptions.index}」が見つかりません`);
    }
    
    // 埋め込みとメタデータを読み込む
    const embeddings = await loadEmbeddings(mergedOptions.index);
    const metadata = await loadMetadata(mergedOptions.index);
    
    // クエリの埋め込みを生成（実際のシステムでは外部APIを使用）
    // ここでは簡易的に文字列長に基づく擬似埋め込みを生成
    const queryEmbedding = generatePseudoEmbedding(query, embeddings.dimensions);
    
    // 類似度計算と結果のランク付け
    const startTime = Date.now();
    
    let results = [];
    for (let i = 0; i < embeddings.vectors.length; i++) {
      const vec = embeddings.vectors[i];
      let similarity;
      
      // 類似度計算方法を選択
      if (mergedOptions.method === 'cosine') {
        similarity = cosineSimilarity(queryEmbedding, vec.values);
      } else {
        // ユークリッド距離はスコアに変換（近いほど高いスコアに）
        const distance = euclideanDistance(queryEmbedding, vec.values);
        similarity = 1 / (1 + distance); // 0-1のスケールに変換
      }
      
      // しきい値を超える結果のみ追加
      if (similarity >= mergedOptions.threshold) {
        // メタデータの追加
        const metaItem = metadata.items.find(item => item.id === vec.id);
        results.push({
          id: vec.id,
          score: similarity,
          metadata: metaItem || {}
        });
      }
    }
    
    // スコアでソート（高い順）
    results.sort((a, b) => b.score - a.score);
    
    // 上位結果を返す
    results = results.slice(0, mergedOptions.limit);
    
    const endTime = Date.now();
    console.log(`検索パフォーマンス: ${endTime - startTime}ms, クエリ: ${query}`);
    
    const totalEndTime = Date.now();
    console.log(`合計検索パフォーマンス: ${totalEndTime - totalStartTime}ms, クエリ: ${query}`);
    
    // キャッシュに保存
    if (CACHE_ENABLED) {
      cache.results.set(cacheKey, {
        data: results,
        timestamp: Date.now()
      });
      
      // キャッシュサイズチェック
      if (cache.results.size > MAX_CACHE_SIZE) {
        const oldestKey = cache.results.keys().next().value;
        cache.results.delete(oldestKey);
      }
    }
    
    return results;
  } catch (error) {
    console.error('ベクトル検索エラー:', error);
    return Promise.reject(new Error(`検索中にエラーが発生しました: ${error.message}`));
  }
}

/**
 * 擬似埋め込みを生成（実際のシステムでは外部APIを使用）
 * @param {string} text - テキスト
 * @param {number} dimensions - 次元数
 * @returns {Array<number>} - 埋め込みベクトル
 */
function generatePseudoEmbedding(text, dimensions) {
  // 文字コードに基づく擬似的な埋め込み生成
  const vec = new Array(dimensions).fill(0);
  
  // シード値として文字列長を使用
  const seed = text.length;
  
  for (let i = 0; i < text.length; i++) {
    const charCode = text.charCodeAt(i);
    const position = i % dimensions;
    
    // 疑似乱数生成（文字コードベース）
    vec[position] += (charCode * (i + 1) * seed) % 1;
  }
  
  // 正規化（ベクトル長さを1に）
  let sumSquared = 0;
  for (let i = 0; i < dimensions; i++) {
    sumSquared += vec[i] * vec[i];
  }
  
  const norm = Math.sqrt(sumSquared);
  if (norm > 0) {
    for (let i = 0; i < dimensions; i++) {
      vec[i] /= norm;
    }
  }
  
  return vec;
}

/**
 * 指定したインデックスのドキュメント数を取得
 * @param {string} indexName - インデックス名
 * @returns {Promise<number>} - ドキュメント数
 */
async function getIndexDocumentCount(indexName) {
  try {
    const embeddings = await loadEmbeddings(indexName);
    return embeddings.vectors.length;
  } catch (error) {
    console.error(`インデックス「${indexName}」のドキュメント数取得エラー:`, error);
    return 0;
  }
}

/**
 * 全てのインデックス情報を取得
 * @returns {Promise<Array>} - インデックス情報一覧
 */
async function listIndexes() {
  try {
    const dbInfo = await loadDatabaseInfo();
    
    // 各インデックスのドキュメント数を追加
    const indexesWithCounts = await Promise.all(
      dbInfo.indexes.map(async (index) => {
        const count = await getIndexDocumentCount(index.name);
        return {
          ...index,
          document_count: count
        };
      })
    );
    
    return indexesWithCounts;
  } catch (error) {
    console.error('インデックス一覧取得エラー:', error);
    return [];
  }
}

/**
 * キャッシュをクリア
 */
function clearCache() {
  cache.databaseInfo = null;
  cache.dbInfoTimestamp = 0;
  cache.embeddings.clear();
  cache.results.clear();
  cache.indexes.clear();
  console.log('ベクトルDBキャッシュをクリアしました');
}

module.exports = {
  vectorTextQuery,
  listIndexes,
  clearCache
};
