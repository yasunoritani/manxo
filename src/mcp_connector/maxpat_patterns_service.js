/**
 * Max/MSPパッチパターン提供サービス
 * 分析されたmaxpatファイルのパターンをMCPサーバーを通じてClaudeに提供
 */

const fs = require('fs');
const path = require('path');

// 設定
const PATTERNS_DIR = path.join(__dirname, '../../data/transformed/maxpat_analysis');
const MERGED_PATTERNS_PATH = path.join(PATTERNS_DIR, 'merged_patterns.json');
const GRAPH_DB_DIR = path.join(__dirname, '../../data/graph_db/maxpat_patterns');
const VECTOR_DB_DIR = path.join(__dirname, '../../data/vector_db/maxpat_patterns');
const DOCUMENT_DB_DIR = path.join(__dirname, '../../data/document_db/maxpat_patterns');

// パターンデータをキャッシュ
let mergedPatternsCache = null;
let categorizedPatternsCache = null;
let patchAnalysisCache = {};

/**
 * マージされたパターンデータを読み込む
 * @returns {Object} マージされたパターンデータ
 */
function loadMergedPatterns() {
  if (mergedPatternsCache) {
    return mergedPatternsCache;
  }

  try {
    // ファイルが存在しない場合は空のオブジェクトを返す
    if (!fs.existsSync(MERGED_PATTERNS_PATH)) {
      console.warn(`マージされたパターンファイルが見つかりません: ${MERGED_PATTERNS_PATH}`);
      return {
        object_pairs: [],
        feedback_patterns: [],
        object_usage: [],
        common_clusters: []
      };
    }

    const data = fs.readFileSync(MERGED_PATTERNS_PATH, 'utf8');
    mergedPatternsCache = JSON.parse(data);
    return mergedPatternsCache;
  } catch (error) {
    console.error(`パターンデータの読み込みエラー: ${error.message}`);
    return {
      object_pairs: [],
      feedback_patterns: [],
      object_usage: [],
      common_clusters: []
    };
  }
}

/**
 * パターンをカテゴリ別に整理する
 * @returns {Object} カテゴリ別パターン
 */
function categorizePatterns() {
  if (categorizedPatternsCache) {
    return categorizedPatternsCache;
  }

  const patterns = loadMergedPatterns();
  
  // カテゴリ別に整理
  const categorized = {
    audio: {
      object_pairs: [],
      feedback_patterns: [],
      common_objects: []
    },
    visual: {
      object_pairs: [],
      feedback_patterns: [],
      common_objects: []
    },
    control: {
      object_pairs: [],
      feedback_patterns: [],
      common_objects: []
    },
    processing: {
      object_pairs: [],
      feedback_patterns: [],
      common_objects: []
    }
  };

  // オブジェクトペアの分類
  for (const pair of patterns.object_pairs) {
    // オーディオ関連ペア (~で終わるオブジェクト)
    if (pair.source.endsWith('~') || pair.destination.endsWith('~')) {
      categorized.audio.object_pairs.push(pair);
    }
    // ビジュアル関連ペア (jit.で始まるオブジェクト)
    else if (pair.source.startsWith('jit.') || pair.destination.startsWith('jit.')) {
      categorized.visual.object_pairs.push(pair);
    }
    // 制御系ペア (max.や数値オブジェクト)
    else if (['flonum', 'number', 'dial', 'slider', 'button', 'toggle'].includes(pair.source) || 
             ['flonum', 'number', 'dial', 'slider', 'button', 'toggle'].includes(pair.destination)) {
      categorized.control.object_pairs.push(pair);
    }
    // その他処理系
    else {
      categorized.processing.object_pairs.push(pair);
    }
  }

  // フィードバックパターンの分類
  for (const pattern of patterns.feedback_patterns) {
    const objects = pattern.objects || [];
    
    // MSPオブジェクトを含むフィードバック
    if (objects.some(obj => obj.endsWith('~'))) {
      categorized.audio.feedback_patterns.push(pattern);
    }
    // Jitterオブジェクトを含むフィードバック
    else if (objects.some(obj => obj.startsWith('jit.'))) {
      categorized.visual.feedback_patterns.push(pattern);
    }
    // その他
    else {
      categorized.processing.feedback_patterns.push(pattern);
    }
  }

  // よく使用されるオブジェクトの分類
  for (const obj of patterns.object_usage) {
    if (obj.name.endsWith('~')) {
      categorized.audio.common_objects.push(obj);
    } else if (obj.name.startsWith('jit.')) {
      categorized.visual.common_objects.push(obj);
    } else if (['flonum', 'number', 'dial', 'slider', 'button', 'toggle'].includes(obj.name)) {
      categorized.control.common_objects.push(obj);
    } else {
      categorized.processing.common_objects.push(obj);
    }
  }

  categorizedPatternsCache = categorized;
  return categorized;
}

/**
 * オブジェクトがキーワードにマッチするかチェック
 * @param {Object} obj - チェック対象のオブジェクト
 * @param {Array} keywords - キーワード配列
 * @returns {boolean} マッチしたかどうか
 */
function matchesKeywords(obj, keywords) {
  // オブジェクトを文字列化
  const objString = JSON.stringify(obj).toLowerCase();
  
  // すべてのキーワードがオブジェクト文字列に含まれているかチェック
  return keywords.every(keyword => objString.includes(keyword));
}

/**
 * ペアの複雑さを計算
 * @param {Object} pair - オブジェクトペア
 * @returns {number} 複雑さスコア（1-5）
 */
function calculatePairComplexity(pair) {
  // 単純化のために基本的な複雑さの評価
  const complexObjects = ['gen~', 'pfft~', 'jit.gl.', 'jit.gen', 'expr', 'poly~', 'vst~', 'fluid.'];
  const source = pair.source || '';
  const dest = pair.destination || '';
  
  // 複雑なオブジェクトが含まれているか
  const hasComplexObj = complexObjects.some(co => {
    return source.includes(co) || dest.includes(co);
  });
  
  // MSPオブジェクトの組み合わせは中程度の複雑さ
  const hasMSP = source.endsWith('~') || dest.endsWith('~');
  
  if (hasComplexObj) {
    return 5; // 最も複雑
  } else if (hasMSP) {
    return 3; // 中程度
  } else {
    return 1; // 単純
  }
}

/**
 * Maxパターンアクセス - 統合されたパターン操作関数
 * @param {Object} params - パラメータオブジェクト
 * @param {string} params.action - 実行するアクション（'search', 'recommend', 'findRelated'）
 * @param {string} params.query - 検索クエリまたはオブジェクト名
 * @param {string} [params.category] - カテゴリ（'audio', 'visual', 'control', 'processing', 'all'）
 * @param {string} [params.purpose] - パッチの目的（'synthesis', 'effect', 'visual', 'control', 'generative'）
 * @param {number} [params.complexity] - 複雑さレベル（1-5）
 * @param {number} [params.limit] - 結果の最大数
 * @returns {Object} アクションに応じた結果
 */
function maxPatternAccess(params) {
  try {
    const { action, query, category, purpose, complexity = 3, limit = 10 } = params;
    const patterns = loadMergedPatterns();

    // アクションに応じて処理を分岐
    switch (action) {
      case 'search': {
        // 検索クエリの処理（小文字化、トリミング）
        const normalizedQuery = query ? query.trim().toLowerCase() : '';
        if (!normalizedQuery) {
          return { error: '検索クエリが空です' };
        }
        
        const keywords = normalizedQuery.split(/\s+/);
        
        // 検索結果の配列
        const results = {
          object_pairs: [],
          feedback_patterns: [],
          common_objects: []
        };

        // カテゴリの選択
        const categories = category === 'all' || !category
          ? ['audio', 'visual', 'control', 'processing'] 
          : [category];

        // 各カテゴリ内で検索
        const categorizedPatterns = categorizePatterns();
        for (const cat of categories) {
          if (!categorizedPatterns[cat]) continue;

          // オブジェクトペアの検索
          for (const pair of categorizedPatterns[cat].object_pairs) {
            if (matchesKeywords(pair, keywords)) {
              results.object_pairs.push({
                ...pair,
                category: cat
              });
            }
          }

          // フィードバックパターンの検索
          for (const pattern of categorizedPatterns[cat].feedback_patterns) {
            if (matchesKeywords(pattern, keywords)) {
              results.feedback_patterns.push({
                ...pattern,
                category: cat
              });
            }
          }

          // 共通オブジェクトの検索
          for (const obj of categorizedPatterns[cat].common_objects) {
            if (matchesKeywords(obj, keywords)) {
              results.common_objects.push({
                ...obj,
                category: cat
              });
            }
          }
        }

        // 検索結果の制限
        return {
          action: 'search',
          query,
          category,
          results: {
            object_pairs: results.object_pairs.slice(0, limit),
            feedback_patterns: results.feedback_patterns.slice(0, limit),
            common_objects: results.common_objects.slice(0, limit)
          }
        };
      }
      
      case 'recommend': {
        if (!purpose) {
          return { error: 'パッチの目的が指定されていません' };
        }
        
        // 目的別のキーワード
        const purposeKeywords = {
          synthesis: ['cycle~', 'phasor~', 'noise~', 'oscillator', 'gen~', 'synthesizer', 'frequency', 'pitch'],
          effect: ['delay~', 'reverb~', 'filter~', 'biquad~', 'distortion', 'effect', 'processing'],
          visual: ['jit.', 'gl.', 'window', 'matrix', 'video', 'shader', 'color', 'image'],
          control: ['midi', 'controller', 'interface', 'live.', 'transport', 'dial', 'slider', 'button'],
          generative: ['random', 'probability', 'noise~', 'chaos', 'algorithmic', 'generative', 'pattern']
        };
        
        // 選択された目的のキーワード
        const keywords = purposeKeywords[purpose] || [];
        if (keywords.length === 0) {
          return { error: `不明な目的: ${purpose}` };
        }
        
        // パターンの選択（スコア付け）
        const scoredPairs = patterns.object_pairs.map(pair => {
          const objString = JSON.stringify(pair).toLowerCase();
          const keywordScore = keywords.reduce((score, kw) => {
            return score + (objString.includes(kw) ? 1 : 0);
          }, 0);
          
          // 複雑さによるフィルタリング
          const pairComplexity = calculatePairComplexity(pair);
          const matchesComplexity = Math.abs(pairComplexity - complexity) <= 1;
          
          return {
            ...pair,
            score: keywordScore * (matchesComplexity ? 1 : 0.5)
          };
        })
        .filter(p => p.score > 0)
        .sort((a, b) => b.score - a.score)
        .slice(0, limit);
        
        // フィードバックパターンのスコア付け
        const scoredFeedback = patterns.feedback_patterns.map(pattern => {
          const objString = JSON.stringify(pattern).toLowerCase();
          const keywordScore = keywords.reduce((score, kw) => {
            return score + (objString.includes(kw) ? 1 : 0);
          }, 0);
          
          // 複雑さによるフィルタリング
          const patternComplexity = (pattern.length || 0) / 2;
          const matchesComplexity = Math.abs(patternComplexity - complexity) <= 1;
          
          return {
            ...pattern,
            score: keywordScore * (matchesComplexity ? 1 : 0.5)
          };
        })
        .filter(p => p.score > 0)
        .sort((a, b) => b.score - a.score)
        .slice(0, limit);
        
        return {
          action: 'recommend',
          purpose,
          complexity,
          results: {
            recommended_pairs: scoredPairs,
            recommended_feedback: scoredFeedback
          }
        };
      }
      
      case 'findRelated': {
        if (!query) {
          return { error: 'オブジェクト名が指定されていません' };
        }
        
        const objectName = query;
        
        // 結果オブジェクト
        const results = {
          as_source: [],
          as_destination: [],
          in_feedback: []
        };
        
        // ソースとしてのパターン
        results.as_source = patterns.object_pairs
          .filter(pair => pair.source === objectName)
          .sort((a, b) => b.count - a.count)
          .slice(0, limit);
        
        // デスティネーションとしてのパターン
        results.as_destination = patterns.object_pairs
          .filter(pair => pair.destination === objectName)
          .sort((a, b) => b.count - a.count)
          .slice(0, limit);
        
        // フィードバックでの使用
        results.in_feedback = patterns.feedback_patterns
          .filter(pattern => (pattern.objects || []).includes(objectName))
          .sort((a, b) => b.count - a.count)
          .slice(0, limit);
        
        // 関連する共通オブジェクト
        const sourceObjects = new Set(results.as_source.map(p => p.destination));
        const destObjects = new Set(results.as_destination.map(p => p.source));
        
        // よく一緒に使われるオブジェクト（直接接続されているもの）
        const relatedObjects = [...sourceObjects, ...destObjects].filter(o => o !== objectName);
        
        return {
          action: 'findRelated',
          object_name: objectName,
          results: {
            patterns: {
              as_source: results.as_source,
              as_destination: results.as_destination,
              in_feedback: results.in_feedback
            },
            related_objects: Array.from(new Set(relatedObjects)).slice(0, limit)
          }
        };
      }
      
      default:
        return { error: `不明なアクション: ${action}` };
    }
  } catch (error) {
    console.error(`Maxパターンアクセスエラー: ${error.message}`);
    return { 
      error: `Maxパターン処理中にエラーが発生しました: ${error.message}`,
      action: params?.action 
    };
  }
}

/**
 * パターンサービスのMCPツール登録
 * @param {Object} mcpServer - MCPサーバーオブジェクト
 */
function registerMaxPatternTools(mcpServer) {
  // 単一の統合されたツール登録
  mcpServer.registerTool({
    name: 'maxPatternAccess',
    description: 'Max/MSPパッチパターンにアクセスする統合ツール。検索、推奨、関連パターン検索などの機能を提供します。',
    inputJsonSchema: {
      type: 'object',
      required: ['action'],
      properties: {
        action: {
          type: 'string',
          enum: ['search', 'recommend', 'findRelated'],
          description: '実行するアクション'
        },
        query: {
          type: 'string',
          description: '検索クエリまたはオブジェクト名'
        },
        category: {
          type: 'string',
          enum: ['audio', 'visual', 'control', 'processing', 'all'],
          description: '検索するパターンのカテゴリ',
          default: 'all'
        },
        purpose: {
          type: 'string',
          enum: ['synthesis', 'effect', 'visual', 'control', 'generative'],
          description: 'パッチの目的'
        },
        complexity: {
          type: 'integer',
          description: '複雑さのレベル (1:簡単 - 5:複雑)',
          minimum: 1,
          maximum: 5,
          default: 3
        },
        limit: {
          type: 'integer',
          description: '返す結果の最大数',
          minimum: 1,
          maximum: 20,
          default: 10
        }
      }
    },
    execute: async (params) => {
      try {
        const result = maxPatternAccess(params);
        
        // エラーチェック
        if (result.error) {
          return { content: result.error };
        }
        
        // 結果が空かどうかチェック
        const isEmpty = result.results 
          ? Object.values(result.results).every(arr => 
              !arr || (Array.isArray(arr) ? arr.length === 0 : Object.keys(arr).length === 0)
            )
          : true;
          
        if (isEmpty) {
          let message = '該当するパターンが見つかりませんでした';
          if (result.query) message += `: ${result.query}`;
          if (result.purpose) message += ` (目的: ${result.purpose})`;
          if (result.object_name) message += `: ${result.object_name}`;
          return { content: message };
        }
        
        return {
          content: JSON.stringify(result, null, 2)
        };
      } catch (error) {
        console.error('Maxパターンアクセスツールエラー:', error);
        return { 
          content: `Maxパターン処理中にエラーが発生しました: ${error.message}` 
        };
      }
    }
  });

  console.log('Maxパターンアクセスツールを登録しました');
}

module.exports = {
  registerMaxPatternTools,
  maxPatternAccess // エクスポートして他のモジュールからも使用可能に
}; 