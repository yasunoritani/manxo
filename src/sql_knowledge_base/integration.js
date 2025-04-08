/**
 * Max/MSP SQL知識ベース検証エンジン - Claude AI統合モジュール
 * Issue #010d対応 - 検証プロセスをClaude AIワークフローに統合する機能
 */

const { McpServer } = require('@modelcontextprotocol/sdk/server/mcp.js');
const { StdioServerTransport } = require('@modelcontextprotocol/sdk/server/stdio.js');
const { getValidationEngine } = require('./validation_engine');
const fs = require('fs').promises;
const path = require('path');
const dotenv = require('dotenv');

// 環境変数の読み込み
dotenv.config();

// Claude API設定
const CLAUDE_API_KEY = process.env.CLAUDE_API_KEY;
const CLAUDE_API_URL = 'https://api.anthropic.com/v1/messages';
const MODEL = process.env.CLAUDE_MODEL || 'claude-3-opus-20240229';

// プロンプトテンプレートの読み込みパス
const TEMPLATES_DIR = path.join(__dirname, '../../templates');

/**
 * Claude AI統合クラス
 * MCPプロトコルを使用してClaude Desktopと連携します
 */
class ClaudeAIIntegration {
    constructor (options = {}) {
        this.debug = options.debug || false;
        this.validationEngine = getValidationEngine();
        this.mcpServer = null;
        this.templates = {};
        this.mcpServerStarted = false;

        // 初期化
        this.initialize();
    }

    /**
     * 初期化処理
     */
    async initialize () {
        // テンプレートの読み込み
        await this.loadTemplates();

        // MCPサーバーの初期化
        this.initMcpServer();

        if (this.debug) {
            console.log('ClaudeAI統合を初期化しました');
        }
    }

    /**
     * MCPサーバーの初期化
     */
    initMcpServer () {
        this.mcpServer = new McpServer({
            id: 'max_validation_engine',
            name: 'Max/MSP Validation Engine',
            description: 'Validates Max/MSP code, patches, and provides fix suggestions',
            owner: {
                name: 'Max-Claude Team'
            },
            version: '1.0.0',
            serverInfo: {
                transport: 'stdio',
                endpoint: 'max_validation'
            }
        });

        // ツール登録
        this.registerTools();

        // サーバー起動
        this.startMcpServer();
    }

    /**
     * MCP ツールの登録
     */
    registerTools () {
        // コード検証ツール
        this.mcpServer.registerTool({
            name: 'validateCode',
            description: 'Validate JavaScript/Min-DevKit code for Max/MSP',
            inputJsonSchema: {
                type: 'object',
                required: ['code'],
                properties: {
                    code: {
                        type: 'string',
                        description: 'Code to validate'
                    },
                    context: {
                        type: 'string',
                        description: 'Validation context (e.g., js, jsui, min_object)'
                    }
                }
            },
            execute: async ({ code, context = '' }) => {
                try {
                    const result = this.validationEngine.validateCode(code, context);
                    return { content: JSON.stringify(result, null, 2) };
                } catch (error) {
                    if (this.debug) {
                        console.error('コード検証エラー:', error);
                    }
                    return { content: JSON.stringify({ error: error.message }, null, 2) };
                }
            }
        });

        // 接続検証ツール
        this.mcpServer.registerTool({
            name: 'validateConnection',
            description: 'Validate connection between Max/MSP objects',
            inputJsonSchema: {
                type: 'object',
                required: ['sourceObject', 'destinationObject'],
                properties: {
                    sourceObject: {
                        type: 'string',
                        description: 'Source object name'
                    },
                    sourceOutlet: {
                        type: 'integer',
                        description: 'Source outlet number',
                        default: 0
                    },
                    destinationObject: {
                        type: 'string',
                        description: 'Destination object name'
                    },
                    destinationInlet: {
                        type: 'integer',
                        description: 'Destination inlet number',
                        default: 0
                    }
                }
            },
            execute: async ({ sourceObject, sourceOutlet = 0, destinationObject, destinationInlet = 0 }) => {
                try {
                    const result = await this.validationEngine.validateConnection(
                        sourceObject, sourceOutlet, destinationObject, destinationInlet
                    );
                    return { content: JSON.stringify(result, null, 2) };
                } catch (error) {
                    if (this.debug) {
                        console.error('接続検証エラー:', error);
                    }
                    return { content: JSON.stringify({ error: error.message }, null, 2) };
                }
            }
        });

        // パッチ検証ツール
        this.mcpServer.registerTool({
            name: 'validatePatch',
            description: 'Validate an entire Max/MSP patch',
            inputJsonSchema: {
                type: 'object',
                required: ['patch'],
                properties: {
                    patch: {
                        type: 'object',
                        description: 'Patch data with objects and connections',
                        properties: {
                            objects: {
                                type: 'array',
                                items: {
                                    type: 'object'
                                }
                            },
                            connections: {
                                type: 'array',
                                items: {
                                    type: 'object'
                                }
                            }
                        }
                    }
                }
            },
            execute: async ({ patch }) => {
                try {
                    const result = await this.validationEngine.validatePatch(
                        patch.objects, patch.connections
                    );
                    return { content: JSON.stringify(result, null, 2) };
                } catch (error) {
                    if (this.debug) {
                        console.error('パッチ検証エラー:', error);
                    }
                    return { content: JSON.stringify({ error: error.message }, null, 2) };
                }
            }
        });

        // 修正提案ツール
        this.mcpServer.registerTool({
            name: 'suggestFixes',
            description: 'Suggest fixes for code issues',
            inputJsonSchema: {
                type: 'object',
                required: ['code', 'issues'],
                properties: {
                    code: {
                        type: 'string',
                        description: 'Code with issues'
                    },
                    issues: {
                        type: 'array',
                        description: 'List of issues found during validation',
                        items: {
                            type: 'object'
                        }
                    }
                }
            },
            execute: async ({ code, issues }) => {
                try {
                    const fix = await this.getFixSuggestion(code, issues);
                    return { content: JSON.stringify(fix, null, 2) };
                } catch (error) {
                    if (this.debug) {
                        console.error('修正提案エラー:', error);
                    }
                    return { content: JSON.stringify({ error: error.message }, null, 2) };
                }
            }
        });
    }

    /**
     * MCPサーバーを起動する
     */
    async startMcpServer () {
        if (this.mcpServerStarted) {
            return;
        }

        try {
            const transport = new StdioServerTransport();
            await this.mcpServer.connect(transport);
            this.mcpServerStarted = true;

            if (this.debug) {
                console.log('検証エンジンMCPサーバーが起動しました');
            }
        } catch (error) {
            console.error('MCPサーバー起動エラー:', error);
        }
    }

    /**
     * テンプレートの読み込み
     */
    async loadTemplates () {
        try {
            // テンプレートディレクトリの存在確認
            try {
                await fs.access(TEMPLATES_DIR);
            } catch (err) {
                // ディレクトリが存在しない場合は作成
                await fs.mkdir(TEMPLATES_DIR, { recursive: true });

                // デフォルトテンプレートの作成
                const defaultTemplates = {
                    'code_fix.txt': `あなたはMax/MSPプログラミングの専門家です。以下のJavaScriptコードに問題があります。コードを詳しく分析し、問題を修正した完全なコードを提供してください。

問題のコード:
{{code}}

検出された問題:
{{issues}}

修正したコード全体を提供してください。変更箇所には簡潔なコメントを入れてください。`,

                    'connection_fix.txt': `あなたはMax/MSPプログラミングの専門家です。以下のオブジェクト接続に問題があります。この接続を最適化する方法を提案してください。

接続情報:
送信元オブジェクト: {{sourceObj}}
送信元アウトレット: {{sourceOutlet}}
送信先オブジェクト: {{destObj}}
送信先インレット: {{destInlet}}

検出された問題:
{{issues}}

この接続を改善するための具体的な提案を行ってください。必要に応じて、中間オブジェクトの追加や代替オブジェクトの使用を提案してください。`,

                    'patch_fix.txt': `あなたはMax/MSPプログラミングの専門家です。以下のパッチに問題があります。パッチの改善方法を提案してください。

パッチ情報:
オブジェクト数: {{objectCount}}
接続数: {{connectionCount}}

検出された問題:
{{issues}}

このパッチを改善するための具体的な提案を行ってください。パフォーマンスの向上、信頼性の向上、可読性の向上のための変更点を含めてください。`
                };

                for (const [filename, content] of Object.entries(defaultTemplates)) {
                    await fs.writeFile(path.join(TEMPLATES_DIR, filename), content);
                }
            }

            // テンプレートファイルの読み込み
            const files = await fs.readdir(TEMPLATES_DIR);
            for (const file of files) {
                if (file.endsWith('.txt')) {
                    const templateName = path.basename(file, '.txt');
                    const templateContent = await fs.readFile(path.join(TEMPLATES_DIR, file), 'utf8');
                    this.templates[templateName] = templateContent;
                }
            }

            if (this.debug) {
                console.log(`${Object.keys(this.templates).length}個のテンプレートを読み込みました`);
            }
        } catch (error) {
            console.error('テンプレート読み込みエラー:', error);
        }
    }

    /**
     * テンプレートにデータを埋め込む
     * @param {string} templateName - テンプレート名
     * @param {object} data - 埋め込むデータ
     * @returns {string} 完成したプロンプト
     */
    fillTemplate (templateName, data) {
        let template = this.templates[templateName];
        if (!template) {
            console.warn(`テンプレート ${templateName} が見つかりません`);
            return '';
        }

        // {{変数名}} パターンを置換
        return template.replace(/\{\{([^}]+)\}\}/g, (match, key) => {
            return data[key] !== undefined ? data[key] : match;
        });
    }

    /**
     * 検証結果をClaudeに送信
     * @param {object} validationResult - 検証結果
     * @param {string} contextType - コンテキストタイプ ('code', 'connection', 'patch')
     * @returns {Promise<object>} Claudeからのレスポンス
     */
    async sendValidationToAI (validationResult, contextType) {
        try {
            if (this.debug) {
                console.log(`ClaudeAI統合: ${contextType}検証結果を送信します`);
            }

            // 検証結果をAIに適したフォーマットに変換
            const aiContext = this.formatValidationForAI(validationResult, contextType);

            // MCP経由で送信
            // ここでは実際の送信コードは実装しません（同じMCPインターフェイスを通じてAIがアクセスするため）
            // 実際のMCP実装ではAIがMCPサーバーにアクセスし、検証結果を取得します

            if (this.debug) {
                console.log('ClaudeAI統合: AIにコンテキストを提供しました');
            }

            return {
                status: 'success',
                context: contextType,
                summary: `検証タイプ: ${contextType}, 問題数: ${validationResult.total_issues || 0}`
            };
        } catch (error) {
            console.error('ClaudeAI統合エラー:', error.message);
            throw new Error(`ClaudeAIへの検証結果送信に失敗しました: ${error.message}`);
        }
    }

    /**
     * 検証結果をAIに適したフォーマットに変換
     * @param {object} result - 検証結果
     * @param {string} contextType - コンテキストタイプ
     * @returns {object} AIコンテキスト
     */
    formatValidationForAI (result, contextType) {
        switch (contextType) {
            case 'code':
                return this.formatCodeValidationForAI(result);
            case 'connection':
                return this.formatConnectionValidationForAI(result);
            case 'patch':
                return this.formatPatchValidationForAI(result);
            default:
                throw new Error(`未知のコンテキストタイプ: ${contextType}`);
        }
    }

    /**
     * コード検証結果をAIコンテキストに変換
     * @param {object} result - コード検証結果
     * @returns {object} AIコンテキスト
     */
    formatCodeValidationForAI (result) {
        // エラーと警告を分類
        const errors = result.issues.filter(issue => issue.severity === 'error');
        const warnings = result.issues.filter(issue => issue.severity === 'warning');
        const info = result.issues.filter(issue => issue.severity === 'info');

        // 問題の種類を集計
        const ruleTypes = {};
        result.issues.forEach(issue => {
            ruleTypes[issue.rule_type] = (ruleTypes[issue.rule_type] || 0) + 1;
        });

        return {
            validation_summary: {
                is_valid: result.valid,
                total_issues: result.total_issues,
                error_count: errors.length,
                warning_count: warnings.length,
                info_count: info.length,
                code_length: result.code_length,
                rule_types: ruleTypes
            },
            critical_issues: errors.length > 0 ? errors.slice(0, 3) : [],
            important_warnings: warnings.length > 0 ? warnings.slice(0, 3) : [],
            context: result.context
        };
    }

    /**
     * 接続検証結果をAIコンテキストに変換
     * @param {object} result - 接続検証結果
     * @returns {object} AIコンテキスト
     */
    formatConnectionValidationForAI (result) {
        return {
            validation_summary: {
                is_valid: result.valid,
                has_performance_impact: result.performance_impact ? true : false,
                has_compatibility_issues: result.compatibility_issues ? true : false,
                is_recommended: result.is_recommended || false,
                audio_signal_flow: result.audio_signal_flow || false
            },
            issues: result.issues || [],
            alternative_suggestions: result.alternative_suggestions || [],
            similar_patterns: result.similar_patterns
                ? result.similar_patterns.slice(0, 3)
                : []
        };
    }

    /**
     * パッチ検証結果をAIコンテキストに変換
     * @param {object} result - パッチ検証結果
     * @returns {object} AIコンテキスト
     */
    formatPatchValidationForAI (result) {
        // エラーと警告を分類
        const errors = result.issues.filter(issue => issue.severity === 'error');
        const warnings = result.issues.filter(issue => issue.severity === 'warning');

        // 問題のタイプを集計
        const issueTypes = {};
        result.issues.forEach(issue => {
            const type = issue.type || (issue.connection_id ? 'connection' : issue.object_id ? 'object' : 'general');
            issueTypes[type] = (issueTypes[type] || 0) + 1;
        });

        return {
            validation_summary: {
                is_valid: result.valid,
                total_issues: result.total_issues,
                error_count: errors.length,
                warning_count: warnings.length,
                total_objects: result.total_objects,
                total_connections: result.total_connections,
                issue_types: issueTypes
            },
            critical_issues: errors.length > 0 ? errors.slice(0, 3) : [],
            important_warnings: warnings.length > 0 ? warnings.slice(0, 3) : []
        };
    }

    /**
     * 修正提案を取得する
     * @param {string} code - 元のコード
     * @param {Array} issues - 検出された問題
     * @returns {Promise<object>} 修正提案
     */
    async getFixSuggestion (code, issues) {
        // 問題がないか、問題リストが空の場合は空の提案を返す
        if (!issues || issues.length === 0) {
            return { has_proposals: false, proposals: [] };
        }

        try {
            // 基本的な修正提案を生成
            const basicProposals = this.validationEngine.generateFixProposals
                ? this.validationEngine.generateFixProposals(code, issues)
                : { has_proposals: false, proposals: [] };

            // テンプレートを使った修正提案用のプロンプトを準備
            // MCP経由でAIに渡されるデータを形成
            const promptData = {
                code: code,
                issues: issues.map(issue =>
                    `- ${issue.severity || 'issue'}: ${issue.description || issue.message || JSON.stringify(issue)}`
                ).join('\n')
            };

            // プロンプトテンプレートを埋める
            const prompt = this.fillTemplate('code_fix', promptData);

            // ここでプロンプトをMCPを通じてClaude AIに送信
            // 実際の送信はMCPフレームワークを通じて行われる

            return {
                has_proposals: basicProposals.has_proposals,
                proposals: basicProposals.proposals,
                prompt_template: prompt.substring(0, 200) + '...' // プロンプトの一部を返す
            };
        } catch (error) {
            console.error('修正提案生成エラー:', error);
            return {
                has_proposals: false,
                error: error.message,
                proposals: []
            };
        }
    }
}

module.exports = {
    ClaudeAIIntegration
};
