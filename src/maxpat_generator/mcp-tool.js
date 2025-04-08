/**
 * Maxpatジェネレーター MCP Tool
 * MCPサーバーとmaxpatジェネレーターを統合するモジュール
 */

const maxpatGenerator = require('./index');
const path = require('path');
const fs = require('fs');

/**
 * MCPサーバーにmaxpatジェネレーターツールを登録
 * @param {Object} server - MCPサーバーインスタンス
 */
function registerMaxpatGeneratorTools (server) {
    /**
     * generateMaxPatch ツール: 基本的なMaxパッチを生成する
     */
    server.registerTool({
        name: 'generateMaxPatch',
        description: 'Generate a basic Max/MSP patch file',
        inputJsonSchema: {
            type: 'object',
            required: ['patchName'],
            properties: {
                patchName: {
                    type: 'string',
                    description: 'Name of the patch to generate'
                },
                templateName: {
                    type: 'string',
                    description: 'Template to use (default: basic_template)',
                    default: 'basic_template'
                },
                description: {
                    type: 'string',
                    description: 'Description of the patch'
                },
                outputPath: {
                    type: 'string',
                    description: 'Path where to save the generated patch'
                }
            }
        },
        execute: async ({ patchName, templateName = 'basic_template', description, outputPath }) => {
            try {
                // 出力パスの処理
                const finalOutputPath = outputPath || path.join(process.cwd(), `${patchName.replace(/\s+/g, '_')}.maxpat`);

                // カスタム値の設定
                const customValues = {};
                if (description) {
                    customValues.description = description;
                }

                // パッチの生成
                const result = maxpatGenerator.generateBasicPatch({
                    templateName,
                    patchName,
                    outputPath: finalOutputPath,
                    customValues
                });

                if (!result) {
                    return {
                        content: `Failed to generate patch: ${patchName}`
                    };
                }

                return {
                    content: `Successfully generated Max patch: ${finalOutputPath}`
                };
            } catch (error) {
                console.error('Error in generateMaxPatch:', error);
                return { content: `Error generating Max patch: ${error.message}` };
            }
        }
    });

    /**
     * listTemplates ツール: 利用可能なテンプレート一覧を取得する
     */
    server.registerTool({
        name: 'listTemplates',
        description: 'List available Max/MSP patch templates',
        inputJsonSchema: {
            type: 'object',
            properties: {}
        },
        execute: async () => {
            try {
                const templates = maxpatGenerator.getAvailableTemplates();

                if (templates.length === 0) {
                    return {
                        content: 'No templates available. Please check templates directory.'
                    };
                }

                return {
                    content: JSON.stringify({
                        available_templates: templates
                    }, null, 2)
                };
            } catch (error) {
                console.error('Error in listTemplates:', error);
                return { content: `Error listing templates: ${error.message}` };
            }
        }
    });

    /**
     * createAdvancedPatch ツール: 高度なMaxパッチを生成する
     */
    server.registerTool({
        name: 'createAdvancedPatch',
        description: 'Create an advanced Max/MSP patch with custom objects',
        inputJsonSchema: {
            type: 'object',
            required: ['patchName', 'objects'],
            properties: {
                patchName: {
                    type: 'string',
                    description: 'Name of the patch to generate'
                },
                description: {
                    type: 'string',
                    description: 'Description of the patch'
                },
                objects: {
                    type: 'array',
                    description: 'Array of objects to add to the patch',
                    items: {
                        type: 'object',
                        required: ['type', 'x', 'y'],
                        properties: {
                            type: {
                                type: 'string',
                                description: 'Type of the object (e.g., button, slider, message)'
                            },
                            x: {
                                type: 'number',
                                description: 'X coordinate'
                            },
                            y: {
                                type: 'number',
                                description: 'Y coordinate'
                            },
                            text: {
                                type: 'string',
                                description: 'Text content (for message, comment, etc.)'
                            }
                        }
                    }
                },
                connections: {
                    type: 'array',
                    description: 'Array of connections between objects',
                    items: {
                        type: 'object',
                        required: ['sourceIndex', 'sourceOutlet', 'destIndex', 'destInlet'],
                        properties: {
                            sourceIndex: {
                                type: 'integer',
                                description: 'Index of the source object in the objects array'
                            },
                            sourceOutlet: {
                                type: 'integer',
                                description: 'Outlet number of the source object'
                            },
                            destIndex: {
                                type: 'integer',
                                description: 'Index of the destination object in the objects array'
                            },
                            destInlet: {
                                type: 'integer',
                                description: 'Inlet number of the destination object'
                            }
                        }
                    }
                },
                outputPath: {
                    type: 'string',
                    description: 'Path where to save the generated patch'
                }
            }
        },
        execute: async ({ patchName, description, objects, connections = [], outputPath }) => {
            try {
                // 出力パスの処理
                const finalOutputPath = outputPath || path.join(process.cwd(), `${patchName.replace(/\s+/g, '_')}.maxpat`);

                // カスタム値の設定
                const customValues = {};
                if (description) {
                    customValues.description = description;
                }

                // 基本パッチの生成
                const patchObj = maxpatGenerator.generateBasicPatch({
                    patchName,
                    customValues
                });

                if (!patchObj) {
                    return {
                        content: `Failed to generate base patch: ${patchName}`
                    };
                }

                // オブジェクトの追加
                const objectIds = [];
                for (const obj of objects) {
                    const id = maxpatGenerator.addObject(patchObj, obj);
                    objectIds.push(id);
                }

                // 接続の追加
                for (const conn of connections) {
                    const { sourceIndex, sourceOutlet, destIndex, destInlet } = conn;

                    // インデックスが有効かチェック
                    if (sourceIndex < 0 || sourceIndex >= objectIds.length ||
                        destIndex < 0 || destIndex >= objectIds.length) {
                        console.warn(`Invalid connection indices: ${sourceIndex} -> ${destIndex}`);
                        continue;
                    }

                    maxpatGenerator.connect(
                        patchObj,
                        objectIds[sourceIndex],
                        sourceOutlet,
                        objectIds[destIndex],
                        destInlet
                    );
                }

                // 結果をJSONに変換して保存
                const resultJson = JSON.stringify(patchObj, null, 2);
                fs.writeFileSync(finalOutputPath, resultJson, 'utf8');

                return {
                    content: `Successfully generated advanced Max patch: ${finalOutputPath}`
                };
            } catch (error) {
                console.error('Error in createAdvancedPatch:', error);
                return { content: `Error generating advanced Max patch: ${error.message}` };
            }
        }
    });
}

module.exports = {
    registerMaxpatGeneratorTools
};
