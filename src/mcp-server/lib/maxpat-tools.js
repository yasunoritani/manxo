/**
 * Maxパッチジェネレータのためのツール登録
 */

const fs = require('fs').promises;
const path = require('path');
const aiGenerator = require('../../maxpat_generator/ai-generator');

/**
 * Maxパッチジェネレータツールを設定
 * @param {Object} tools - MCPツールレジストラ
 */
async function setupMaxpatTools (tools) {
    // 初期化
    await aiGenerator.initialize();

    // 1. テンプレート一覧を取得するツール
    tools.registerTool("listTemplates", {
        description: "使用可能なMaxパッチテンプレートの一覧を取得",
        parameters: {},
        handler: async () => {
            try {
                const templatesDir = path.join(__dirname, '../../maxpat_generator/templates');
                const files = await fs.readdir(templatesDir);
                const templates = files
                    .filter(file => file.endsWith('.maxpat'))
                    .map(file => file.replace('.maxpat', ''));

                return {
                    templates,
                    count: templates.length
                };
            } catch (error) {
                console.error('テンプレート一覧取得エラー:', error);
                return {
                    templates: [],
                    count: 0,
                    error: error.message
                };
            }
        }
    });

    // 2. テンプレートからパッチを生成するツール
    tools.registerTool("generatePatchFromTemplate", {
        description: "指定したテンプレートを使用してMaxパッチを生成",
        parameters: {
            templateName: { type: "string", description: "使用するテンプレート名" },
            fileName: { type: "string", description: "生成するファイル名" }
        },
        handler: async ({ templateName, fileName }) => {
            try {
                const templatesDir = path.join(__dirname, '../../maxpat_generator/templates');
                const templatePath = path.join(templatesDir, `${templateName}.maxpat`);
                const outputDir = path.join(__dirname, '../../../output');

                // 出力ディレクトリがなければ作成
                try {
                    await fs.mkdir(outputDir, { recursive: true });
                } catch (e) {
                    // ディレクトリが既に存在する場合は無視
                }

                // テンプレートの読み込みとコピー
                const templateContent = await fs.readFile(templatePath, 'utf8');
                const outputPath = path.join(outputDir, `${fileName}.maxpat`);
                await fs.writeFile(outputPath, templateContent, 'utf8');

                return {
                    success: true,
                    filePath: outputPath,
                    message: `${templateName}テンプレートから${fileName}.maxpatを生成しました。`
                };
            } catch (error) {
                console.error('テンプレートからの生成エラー:', error);
                return {
                    success: false,
                    error: error.message
                };
            }
        }
    });

    // 3. 自然言語からパッチを生成するツール
    tools.registerTool("generatePatchFromText", {
        description: "自然言語の説明からMaxパッチを生成",
        parameters: {
            description: { type: "string", description: "パッチの説明（自然言語）" },
            fileName: { type: "string", description: "生成するファイル名" }
        },
        handler: async ({ description, fileName }) => {
            try {
                console.log(`自然言語からパッチを生成: "${description}"`);

                // AI生成器を使用してパッチを生成
                const patch = await aiGenerator.generatePatch(description);

                // 出力ディレクトリの準備
                const outputDir = path.join(__dirname, '../../../output');
                try {
                    await fs.mkdir(outputDir, { recursive: true });
                } catch (e) {
                    // ディレクトリが既に存在する場合は無視
                }

                // 生成したパッチを保存
                const outputPath = path.join(outputDir, `${fileName}.maxpat`);
                await fs.writeFile(outputPath, JSON.stringify(patch, null, 2), 'utf8');

                return {
                    success: true,
                    filePath: outputPath,
                    message: `"${description}"の説明から${fileName}.maxpatを生成しました。`,
                    objectCount: patch.patcher.boxes.length
                };
            } catch (error) {
                console.error('自然言語からの生成エラー:', error);
                return {
                    success: false,
                    error: error.message
                };
            }
        }
    });

    // 4. Maxオブジェクト情報検索ツール
    tools.registerTool("searchMaxObjects", {
        description: "指定したキーワードに関連するMaxオブジェクトを検索",
        parameters: {
            keyword: { type: "string", description: "検索キーワード" },
            limit: { type: "number", description: "検索結果の最大数" }
        },
        handler: async ({ keyword, limit = 10 }) => {
            try {
                // データベース接続が初期化されていることを確認
                await aiGenerator.initialize();

                // SQLiteデータベースで検索を実行
                const db = require('better-sqlite3')(path.join(__dirname, '../../sql_knowledge_base/max_knowledge.db'), { readonly: true });
                const query = `
          SELECT * FROM max_objects
          WHERE name LIKE ? OR description LIKE ?
          LIMIT ?
        `;

                const results = db.prepare(query).all(`%${keyword}%`, `%${keyword}%`, limit);
                db.close();

                return {
                    count: results.length,
                    objects: results.map(obj => ({
                        name: obj.name,
                        description: obj.description,
                        category: obj.category,
                        inlets: obj.inlets,
                        outlets: obj.outlets,
                        is_ui_object: obj.is_ui_object
                    }))
                };
            } catch (error) {
                console.error('オブジェクト検索エラー:', error);
                return {
                    count: 0,
                    objects: [],
                    error: error.message
                };
            }
        }
    });

    // 5. 接続パターン検索ツール
    tools.registerTool("searchConnectionPatterns", {
        description: "指定したオブジェクト間の接続パターンを検索",
        parameters: {
            sourceObject: { type: "string", description: "ソースオブジェクト名" },
            destinationObject: { type: "string", description: "宛先オブジェクト名（オプション）" }
        },
        handler: async ({ sourceObject, destinationObject }) => {
            try {
                // データベース接続
                const db = require('better-sqlite3')(path.join(__dirname, '../../sql_knowledge_base/max_knowledge.db'), { readonly: true });

                let query;
                let params;

                if (destinationObject) {
                    // ソースと宛先の両方が指定された場合
                    query = `
            SELECT * FROM connection_patterns
            WHERE source_object LIKE ? AND destination_object LIKE ?
            LIMIT 20
          `;
                    params = [`%${sourceObject}%`, `%${destinationObject}%`];
                } else {
                    // ソースのみが指定された場合
                    query = `
            SELECT * FROM connection_patterns
            WHERE source_object LIKE ?
            LIMIT 20
          `;
                    params = [`%${sourceObject}%`];
                }

                const results = db.prepare(query).all(...params);
                db.close();

                return {
                    count: results.length,
                    patterns: results
                };
            } catch (error) {
                console.error('接続パターン検索エラー:', error);
                return {
                    count: 0,
                    patterns: [],
                    error: error.message
                };
            }
        }
    });

    // 6. 複雑なパッチ生成ツール
    tools.registerTool("generateComplexPatch", {
        description: "複雑な概念や要求から高度なMaxパッチを生成",
        parameters: {
            concept: { type: "string", description: "実現したい概念や機能の説明" },
            complexity: { type: "number", description: "複雑さのレベル (1-10)" },
            fileName: { type: "string", description: "生成するファイル名" }
        },
        handler: async ({ concept, complexity = 5, fileName }) => {
            try {
                console.log(`複雑なパッチを生成: "${concept}", 複雑さ: ${complexity}`);

                // 複雑さに基づいて、より詳細な自然言語記述を構築
                let enhancedDescription = concept;

                // 複雑さに応じて、より多くのオブジェクトや接続を含むように調整
                if (complexity >= 7) {
                    enhancedDescription += " より多くのモジュレーション源と処理レイヤーを含む";
                }

                if (complexity >= 9) {
                    enhancedDescription += " フィードバックループと複雑な信号処理チェーンを含む";
                }

                // 強化された説明でパッチを生成
                const patch = await aiGenerator.generatePatch(enhancedDescription);

                // 出力ディレクトリの準備
                const outputDir = path.join(__dirname, '../../../output');
                try {
                    await fs.mkdir(outputDir, { recursive: true });
                } catch (e) {
                    // ディレクトリが既に存在する場合は無視
                }

                // 生成したパッチを保存
                const outputPath = path.join(outputDir, `${fileName}.maxpat`);
                await fs.writeFile(outputPath, JSON.stringify(patch, null, 2), 'utf8');

                return {
                    success: true,
                    filePath: outputPath,
                    message: `"${concept}"の概念から複雑さ${complexity}の${fileName}.maxpatを生成しました。`,
                    objectCount: patch.patcher.boxes.length
                };
            } catch (error) {
                console.error('複雑なパッチ生成エラー:', error);
                return {
                    success: false,
                    error: error.message
                };
            }
        }
    });

    console.log('Maxパッチジェネレータツールを登録しました');
}

// クリーンアップ関数
function cleanup () {
    aiGenerator.cleanup();
}

module.exports = {
    setupMaxpatTools,
    cleanup
};
