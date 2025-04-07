/**
 * Max/MSP SQL知識ベース MCP サーバー
 * Model Context Protocol を使用して Claude Desktop からの知識ベースアクセスを提供します
 */

const { McpServer } = require('@modelcontextprotocol/sdk/server/mcp.js');
const { StdioServerTransport } = require('@modelcontextprotocol/sdk/server/stdio.js');
const sqlite3 = require('better-sqlite3');
const path = require('path');

// データベースへの接続
const DB_PATH = path.join(__dirname, 'max_claude_kb.db');
let db;

try {
    db = new sqlite3(DB_PATH, { readonly: true });
    console.error('データベース接続に成功しました');
} catch (err) {
    console.error('データベース接続エラー:', err.message);
    process.exit(1);
}

// プリペアドステートメントのキャッシュ
const statements = {
    getMaxObjectByName: db.prepare('SELECT * FROM max_objects WHERE name = ?'),
    searchMaxObjects: db.prepare('SELECT * FROM max_objects WHERE name LIKE ? OR description LIKE ? LIMIT ?'),
    getMinDevkitFunction: db.prepare('SELECT * FROM min_devkit_api WHERE function_name = ?'),
    searchMinDevkitFunctions: db.prepare('SELECT * FROM min_devkit_api WHERE function_name LIKE ? OR description LIKE ? LIMIT ?'),
    checkConnection: db.prepare('SELECT * FROM connection_patterns WHERE source_object = ? AND destination_object = ? AND source_outlet = ? AND destination_inlet = ?'),
    searchConnectionPatterns: db.prepare('SELECT * FROM connection_patterns WHERE source_object LIKE ? OR destination_object LIKE ? LIMIT ?'),
    getValidationRules: db.prepare('SELECT * FROM validation_rules WHERE rule_type = ? LIMIT ?'),
    findApiByIntent: db.prepare('SELECT am.*, mda.function_name, mda.signature, mda.description as function_description FROM api_mapping am LEFT JOIN min_devkit_api mda ON am.min_devkit_function_id = mda.id WHERE am.natural_language_intent LIKE ? LIMIT ?')
};

// MCPサーバーの作成
const server = new McpServer({
    id: 'mcp_max_knowledge_base',
    name: 'Max/MSP Knowledge Base',
    description: 'Provides access to Max/MSP and Min-DevKit knowledge base',
    owner: {
        name: 'Max-Claude Team'
    },
    version: '1.0.0',
    serverInfo: {
        transport: 'stdio',
        endpoint: 'max_knowledge_base'
    }
});

/**
 * getMaxObject ツール: Max/MSPオブジェクトの情報を取得する
 */
server.registerTool({
    name: 'getMaxObject',
    description: 'Get information about a Max/MSP object',
    inputJsonSchema: {
        type: 'object',
        required: ['objectName'],
        properties: {
            objectName: {
                type: 'string',
                description: 'Name of the Max/MSP object to lookup'
            }
        }
    },
    execute: async ({ objectName }) => {
        try {
            const result = statements.getMaxObjectByName.get(objectName);

            if (!result) {
                return {
                    content: `Could not find information for Max object: ${objectName}`
                };
            }

            // 結果を整形して返す
            return {
                content: JSON.stringify({
                    name: result.name,
                    description: result.description,
                    category: result.category,
                    inlets: result.inlets,
                    outlets: result.outlets,
                    is_ui_object: Boolean(result.is_ui_object),
                    is_deprecated: Boolean(result.is_deprecated),
                    alternative: result.alternative
                }, null, 2)
            };
        } catch (error) {
            console.error('Error in getMaxObject:', error);
            return { content: `Error retrieving information for: ${objectName}` };
        }
    }
});

/**
 * searchMaxObjects ツール: Max/MSPオブジェクトを検索する
 */
server.registerTool({
    name: 'searchMaxObjects',
    description: 'Search Max/MSP objects by name or description',
    inputJsonSchema: {
        type: 'object',
        required: ['query'],
        properties: {
            query: {
                type: 'string',
                description: 'Search term to find relevant Max objects'
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
            const results = statements.searchMaxObjects.all(`%${query}%`, `%${query}%`, limit);

            if (results.length === 0) {
                return { content: `No Max objects found matching: ${query}` };
            }

            const formattedResults = results.map(obj => ({
                name: obj.name,
                description: obj.description,
                category: obj.category
            }));

            return {
                content: JSON.stringify(formattedResults, null, 2)
            };
        } catch (error) {
            console.error('Error in searchMaxObjects:', error);
            return { content: `Error searching for Max objects: ${query}` };
        }
    }
});

/**
 * getMinDevkitFunction ツール: MinDevKit API関数情報を取得する
 */
server.registerTool({
    name: 'getMinDevkitFunction',
    description: 'Get information about a MinDevKit API function',
    inputJsonSchema: {
        type: 'object',
        required: ['functionName'],
        properties: {
            functionName: {
                type: 'string',
                description: 'Name of the MinDevKit function to lookup'
            }
        }
    },
    execute: async ({ functionName }) => {
        try {
            const result = statements.getMinDevkitFunction.get(functionName);

            if (!result) {
                return {
                    content: `Could not find information for MinDevKit function: ${functionName}`
                };
            }

            return {
                content: JSON.stringify({
                    function_name: result.function_name,
                    signature: result.signature,
                    return_type: result.return_type,
                    description: result.description,
                    parameters: result.parameters,
                    example_usage: result.example_usage
                }, null, 2)
            };
        } catch (error) {
            console.error('Error in getMinDevkitFunction:', error);
            return { content: `Error retrieving information for: ${functionName}` };
        }
    }
});

/**
 * checkConnectionPattern ツール: オブジェクト間の接続パターンをチェックする
 */
server.registerTool({
    name: 'checkConnectionPattern',
    description: 'Validate a connection pattern between Max/MSP objects',
    inputJsonSchema: {
        type: 'object',
        required: ['sourceObject', 'destinationObject'],
        properties: {
            sourceObject: {
                type: 'string',
                description: 'Name of the source object'
            },
            sourceOutlet: {
                type: 'integer',
                description: 'Outlet number of the source object',
                default: 0
            },
            destinationObject: {
                type: 'string',
                description: 'Name of the destination object'
            },
            destinationInlet: {
                type: 'integer',
                description: 'Inlet number of the destination object',
                default: 0
            }
        }
    },
    execute: async ({ sourceObject, sourceOutlet = 0, destinationObject, destinationInlet = 0 }) => {
        try {
            // 正確な接続パターンの検索
            const pattern = statements.checkConnection.get(sourceObject, destinationObject, sourceOutlet, destinationInlet);

            if (pattern) {
                return {
                    content: JSON.stringify({
                        valid: true,
                        is_recommended: Boolean(pattern.is_recommended),
                        description: pattern.description,
                        audio_signal_flow: Boolean(pattern.audio_signal_flow),
                        performance_impact: pattern.performance_impact
                    }, null, 2)
                };
            }

            // 類似パターンの検索
            const similarPatterns = statements.searchConnectionPatterns.all(`%${sourceObject}%`, `%${destinationObject}%`, 5);

            if (similarPatterns.length > 0) {
                return {
                    content: JSON.stringify({
                        valid: false,
                        message: '正確なパターンは見つかりませんでしたが、類似パターンがあります',
                        similar_patterns: similarPatterns.map(p => ({
                            source: p.source_object,
                            destination: p.destination_object,
                            description: p.description
                        }))
                    }, null, 2)
                };
            }

            return {
                content: JSON.stringify({
                    valid: false,
                    message: `${sourceObject}から${destinationObject}への接続パターンは見つかりません`
                }, null, 2)
            };
        } catch (error) {
            console.error('Error in checkConnectionPattern:', error);
            return { content: `Error validating connection pattern` };
        }
    }
});

/**
 * findApiByIntent ツール: 自然言語の意図からMinDevKit API関数を検索する
 */
server.registerTool({
    name: 'findApiByIntent',
    description: 'Find MinDevKit API functions based on natural language intent',
    inputJsonSchema: {
        type: 'object',
        required: ['intent'],
        properties: {
            intent: {
                type: 'string',
                description: 'Natural language description of what you want to do'
            },
            limit: {
                type: 'integer',
                description: 'Maximum number of results to return',
                default: 3
            }
        }
    },
    execute: async ({ intent, limit = 3 }) => {
        try {
            const mappings = statements.findApiByIntent.all(`%${intent}%`, limit);

            if (mappings.length === 0) {
                return {
                    content: `Could not find any API functions matching intent: ${intent}`
                };
            }

            const formattedMappings = mappings.map(m => ({
                intent: m.natural_language_intent,
                function_name: m.function_name,
                signature: m.signature,
                description: m.function_description,
                transformation_template: m.transformation_template
            }));

            return {
                content: JSON.stringify(formattedMappings, null, 2)
            };
        } catch (error) {
            console.error('Error in findApiByIntent:', error);
            return { content: `Error finding API functions for intent: ${intent}` };
        }
    }
});

// サーバーの起動
async function main () {
    try {
        const transport = new StdioServerTransport();
        await server.connect(transport);
        console.error('Max/MSP Knowledge Base MCP Server is running on stdio');
    } catch (error) {
        console.error('Fatal error starting MCP server:', error);
        process.exit(1);
    }
}

// プロセス終了時の処理
process.on('SIGINT', () => {
    if (db) {
        db.close();
        console.error('データベース接続を閉じました');
    }
    process.exit(0);
});

// サーバー起動
main().catch(err => {
    console.error('起動エラー:', err);
    process.exit(1);
});
