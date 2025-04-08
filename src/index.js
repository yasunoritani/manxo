/**
 * Max9-Claude MCP統合サーバー
 * SQL知識ベースとMaxpatジェネレーターを統合したMCPサーバー
 */

const { McpServer } = require('@modelcontextprotocol/sdk/server/mcp.js');
const { StdioServerTransport } = require('@modelcontextprotocol/sdk/server/stdio.js');
const path = require('path');
const fs = require('fs');
const sqlite3 = require('better-sqlite3');

// Maxpatジェネレーターのツール
const { registerMaxpatGeneratorTools } = require('./maxpat_generator/mcp-tool');

// MCPサーバーの作成
const server = new McpServer({
    id: 'mcp_max_integration',
    name: 'Max Integration Server',
    description: 'Provides Max knowledge base and maxpat generation capabilities',
    owner: {
        name: 'Max-Claude Team'
    },
    version: '1.0.0',
    serverInfo: {
        transport: 'stdio',
        endpoint: 'max_integration'
    }
});

// データベースへの接続
const DB_PATH = path.join(__dirname, 'sql_knowledge_base/max_claude_kb.db');
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
            const result = statements.checkConnection.get(sourceObject, destinationObject, sourceOutlet, destinationInlet);

            if (!result) {
                // 一般的な接続パターンを検索
                const generalPattern = statements.searchConnectionPatterns.all(`%${sourceObject}%`, `%${destinationObject}%`, 1);

                if (generalPattern.length > 0) {
                    return {
                        content: `No exact match found for connection pattern, but similar pattern exists: ${generalPattern[0].source_object} to ${generalPattern[0].destination_object}`
                    };
                }

                return {
                    content: `No known connection pattern found between ${sourceObject} and ${destinationObject}`
                };
            }

            return {
                content: JSON.stringify({
                    source_object: result.source_object,
                    source_outlet: result.source_outlet,
                    destination_object: result.destination_object,
                    destination_inlet: result.destination_inlet,
                    description: result.description,
                    is_recommended: Boolean(result.is_recommended),
                    audio_signal_flow: Boolean(result.audio_signal_flow),
                    performance_impact: result.performance_impact
                }, null, 2)
            };
        } catch (error) {
            console.error('Error in checkConnectionPattern:', error);
            return { content: `Error checking connection pattern: ${error.message}` };
        }
    }
});

/**
 * searchConnectionPatterns ツール: 接続パターンを検索する
 */
server.registerTool({
    name: 'searchConnectionPatterns',
    description: 'Search for connection patterns involving a specific object',
    inputJsonSchema: {
        type: 'object',
        required: ['objectName'],
        properties: {
            objectName: {
                type: 'string',
                description: 'Name of the object to find connection patterns for'
            },
            limit: {
                type: 'integer',
                description: 'Maximum number of results to return',
                default: 5
            }
        }
    },
    execute: async ({ objectName, limit = 5 }) => {
        try {
            const results = statements.searchConnectionPatterns.all(`%${objectName}%`, `%${objectName}%`, limit);

            if (results.length === 0) {
                return { content: `No connection patterns found involving: ${objectName}` };
            }

            const formattedResults = results.map(pattern => ({
                source_object: pattern.source_object,
                destination_object: pattern.destination_object,
                description: pattern.description,
                is_recommended: Boolean(pattern.is_recommended)
            }));

            return {
                content: JSON.stringify(formattedResults, null, 2)
            };
        } catch (error) {
            console.error('Error in searchConnectionPatterns:', error);
            return { content: `Error searching for connection patterns: ${error.message}` };
        }
    }
});

// Maxpatジェネレーターツールの登録
registerMaxpatGeneratorTools(server);

// サーバー開始
(async () => {
    try {
        const transport = new StdioServerTransport();
        await server.start(transport);
        console.log('Max Integration MCPサーバーが開始しました');
    } catch (error) {
        console.error('サーバー起動エラー:', error);
        process.exit(1);
    }
})();
