/**
 * MCP連携モジュール
 * 分散型データベース（ベクトルDB、グラフDB、ドキュメントDB）とMCPサーバーを連携するモジュール
 */

const vectorClient = require('./vector-client');
const graphClient = require('./graph-client');
const documentClient = require('./document-client');
const maxpatPatterns = require('./maxpat_patterns_service');

/**
 * 統合データベースアクセス関数
 * @param {Object} params - パラメータオブジェクト
 * @param {string} params.action - 実行するアクション
 * @param {string} params.query - 検索クエリ
 * @param {string} [params.objectName] - オブジェクト名
 * @param {string} [params.relationshipType] - 関係タイプ
 * @param {number} [params.limit] - 結果の最大数
 * @returns {Promise<Object>} アクションに応じた結果
 */
async function databaseAccess(params) {
    const { action, query, objectName, relationshipType = 'CONNECTS_TO', limit = 5 } = params;

    try {
        switch (action) {
            case 'semanticSearch': {
                const results = await vectorClient.semanticSearch(query, limit);
                return {
                    action,
                    query,
                    results: results || []
                };
            }
            
            case 'findRelatedObjects': {
                const results = await graphClient.findRelatedObjects(
                    objectName,
                    relationshipType,
                    limit
                );
                return {
                    action,
                    objectName,
                    relationshipType,
                    results: results || []
                };
            }
            
            case 'documentSearch': {
                const results = await documentClient.textSearch(query, limit);
                return {
                    action,
                    query,
                    results: results || []
                };
            }
            
            case 'integratedSearch': {
                // 並列に各DBでクエリを実行
                const [vectorResults, graphResults, documentResults] = await Promise.all([
                    vectorClient.semanticSearch(query, limit).catch(err => {
                        console.error('Vector search error:', err);
                        return [];
                    }),
                    graphClient.searchByProperty(query, limit).catch(err => {
                        console.error('Graph search error:', err);
                        return [];
                    }),
                    documentClient.textSearch(query, limit).catch(err => {
                        console.error('Document search error:', err);
                        return [];
                    })
                ]);

                return {
                    action,
                    query,
                    results: {
                        semantic_matches: vectorResults || [],
                        related_objects: graphResults || [],
                        documentation: documentResults || []
                    }
                };
            }
            
            default:
                return { error: `不明なアクション: ${action}` };
        }
    } catch (error) {
        console.error(`データベースアクセスエラー (${action}):`, error);
        return { 
            error: `データベースアクセス中にエラーが発生しました: ${error.message}`,
            action 
        };
    }
}

/**
 * MCPサーバーに分散型DBツールを登録
 * @param {Object} server - MCPサーバーインスタンス
 */
function registerDistributedDBTools(server) {
    // 統合データベースアクセスツール
    server.registerTool({
        name: 'databaseAccess',
        description: '分散型データベース（ベクトルDB、グラフDB、ドキュメントDB）にアクセスする統合ツール',
        inputJsonSchema: {
            type: 'object',
            required: ['action'],
            properties: {
                action: {
                    type: 'string',
                    enum: ['semanticSearch', 'findRelatedObjects', 'documentSearch', 'integratedSearch'],
                    description: '実行するアクション'
                },
                query: {
                    type: 'string',
                    description: '検索クエリ'
                },
                objectName: {
                    type: 'string',
                    description: '関連オブジェクトを検索する際のオブジェクト名'
                },
                relationshipType: {
                    type: 'string',
                    description: '関係タイプ (例: CONNECTS_TO, BELONGS_TO)',
                    default: 'CONNECTS_TO'
                },
                limit: {
                    type: 'integer',
                    description: '返す結果の最大数',
                    minimum: 1,
                    maximum: 20,
                    default: 5
                }
            }
        },
        execute: async (params) => {
            try {
                const result = await databaseAccess(params);
                
                // エラーチェック
                if (result.error) {
                    return { content: result.error };
                }
                
                // 結果が空かどうかチェック
                const isEmpty = Array.isArray(result.results) 
                    ? result.results.length === 0
                    : Object.values(result.results).every(arr => !arr || arr.length === 0);
                    
                if (isEmpty) {
                    let message = '結果が見つかりませんでした';
                    if (result.query) message += `: ${result.query}`;
                    if (result.objectName) message += `: ${result.objectName}`;
                    return { content: message };
                }
                
                return {
                    content: JSON.stringify(result, null, 2)
                };
            } catch (error) {
                console.error('データベースアクセスツールエラー:', error);
                return { 
                    content: `データベースアクセス中にエラーが発生しました: ${error.message}` 
                };
            }
        }
    });
    
    // 後方互換性のためのエイリアスツール（徐々に移行するため）
    server.registerTool({
        name: 'semanticSearch',
        description: 'ベクトル検索（レガシーツール）- databaseAccessの使用を推奨',
        inputJsonSchema: {
            type: 'object',
            required: ['query'],
            properties: {
                query: {
                    type: 'string',
                    description: '自然言語検索クエリ'
                },
                limit: {
                    type: 'integer',
                    description: '返す結果の最大数',
                    default: 5
                }
            }
        },
        execute: async ({ query, limit = 5 }) => {
            const result = await databaseAccess({
                action: 'semanticSearch',
                query,
                limit
            });
            
            return {
                content: JSON.stringify(result, null, 2)
            };
        }
    });
    
    // Maxパッチパターンツールを登録
    maxpatPatterns.registerMaxPatternTools(server);
}

module.exports = {
    registerDistributedDBTools,
    databaseAccess // エクスポートして他のモジュールからも使用可能に
};
