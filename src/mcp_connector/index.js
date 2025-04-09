/**
 * MCP連携モジュール
 * 分散型データベース（ベクトルDB、グラフDB、ドキュメントDB）とMCPサーバーを連携するモジュール
 */

const vectorClient = require('./vector-client');
const graphClient = require('./graph-client');
const documentClient = require('./document-client');

/**
 * MCPサーバーに分散型DBツールを登録
 * @param {Object} server - MCPサーバーインスタンス
 */
function registerDistributedDBTools (server) {
    // ベクトル検索ツール
    server.registerTool({
        name: 'semanticSearch',
        description: 'Perform semantic search based on natural language query',
        inputJsonSchema: {
            type: 'object',
            required: ['query'],
            properties: {
                query: {
                    type: 'string',
                    description: 'Natural language query for semantic search'
                },
                limit: {
                    type: 'integer',
                    description: 'Maximum number of results to return',
                    default: 5
                }
            }
        },
        execute: async ({ query, limit = 5 }) => {
            try {
                const results = await vectorClient.semanticSearch(query, limit);

                if (!results || results.length === 0) {
                    return {
                        content: `No results found for semantic search: ${query}`
                    };
                }

                return {
                    content: JSON.stringify(results, null, 2)
                };
            } catch (error) {
                console.error('Error in semanticSearch:', error);
                return { content: `Error performing semantic search: ${error.message}` };
            }
        }
    });

    // 関連オブジェクト検索ツール
    server.registerTool({
        name: 'findRelatedObjects',
        description: 'Find related objects and patterns using graph database',
        inputJsonSchema: {
            type: 'object',
            required: ['objectName'],
            properties: {
                objectName: {
                    type: 'string',
                    description: 'Name of the object to find related items for'
                },
                relationshipType: {
                    type: 'string',
                    description: 'Type of relationship to find (e.g., CONNECTS_TO, BELONGS_TO)',
                    default: 'CONNECTS_TO'
                },
                limit: {
                    type: 'integer',
                    description: 'Maximum number of results to return',
                    default: 5
                }
            }
        },
        execute: async ({ objectName, relationshipType = 'CONNECTS_TO', limit = 5 }) => {
            try {
                const results = await graphClient.findRelatedObjects(
                    objectName,
                    relationshipType,
                    limit
                );

                if (!results || results.length === 0) {
                    return {
                        content: `No related objects found for: ${objectName}`
                    };
                }

                return {
                    content: JSON.stringify(results, null, 2)
                };
            } catch (error) {
                console.error('Error in findRelatedObjects:', error);
                return { content: `Error finding related objects: ${error.message}` };
            }
        }
    });

    // 統合クエリツール
    server.registerTool({
        name: 'integratedSearch',
        description: 'Perform integrated search across all distributed databases',
        inputJsonSchema: {
            type: 'object',
            required: ['query'],
            properties: {
                query: {
                    type: 'string',
                    description: 'Search query to run across all databases'
                },
                limit: {
                    type: 'integer',
                    description: 'Maximum number of results to return per database',
                    default: 3
                }
            }
        },
        execute: async ({ query, limit = 3 }) => {
            try {
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

                // 結果を統合
                const combinedResults = {
                    semantic_matches: vectorResults || [],
                    related_objects: graphResults || [],
                    documentation: documentResults || []
                };

                // 結果がすべて空かどうかチェック
                const isEmpty = Object.values(combinedResults).every(
                    arr => !arr || arr.length === 0
                );

                if (isEmpty) {
                    return {
                        content: `No results found for: ${query}`
                    };
                }

                return {
                    content: JSON.stringify(combinedResults, null, 2)
                };
            } catch (error) {
                console.error('Error in integratedSearch:', error);
                return { content: `Error performing integrated search: ${error.message}` };
            }
        }
    });
}

module.exports = {
    registerDistributedDBTools
};
