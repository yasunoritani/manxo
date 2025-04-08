/**
 * Max/MSP SQL知識ベース検証エンジン - Claude AI統合モジュール
 * Issue #010d対応 - MCPプロトコルを使用した検証プロセスをClaude AIワークフローに統合する機能
 */

const { McpServer } = require('@modelcontextprotocol/sdk/server/mcp.js');
const { StdioServerTransport } = require('@modelcontextprotocol/sdk/server/stdio.js');
const fs = require('fs').promises;
const path = require('path');

// プロンプトテンプレートの読み込みパス
const TEMPLATES_DIR = path.join(__dirname, '../../templates');

// デフォルトテンプレート
const DEFAULT_TEMPLATES = {
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

// シングルトンインスタンスを保持
let claudeIntegrationInstance = null;

/**
 * Claude AI統合クラス
 * MCPプロトコルを使用してClaude Desktopと連携します
 */
class ClaudeAIIntegration {
  /**
   * @param {Object} options - 設定オプション
   * @param {boolean} [options.debug=false] - デバッグモード
   * @param {Object} [options.validationEngine=null] - 検証エンジンインスタンス（依存性注入用）
   * @param {Object} [options.templateDir=null] - テンプレートディレクトリパス
   */
  constructor (options = {}) {
    this.debug = options.debug || false;
    this.validationEngine = options.validationEngine || null;
    this.templateDir = options.templateDir || TEMPLATES_DIR;
    this.mcpServer = null;
    this.templates = {};
    this.mcpServerStarted = false;
    this.initialized = false;
    this.initPromise = null;
  }

  /**
   * 初期化処理
   * @returns {Promise<boolean>} 初期化成功/失敗
   */
  async initialize () {
    if (this.initialized) {
      return true;
    }

    // 既に初期化処理中なら、その結果を待つ
    if (this.initPromise) {
      return this.initPromise;
    }

    this.initPromise = this._doInitialize();
    return this.initPromise;
  }

  /**
   * 初期化処理の実装
   * @private
   * @returns {Promise<boolean>} 初期化成功/失敗
   */
  async _doInitialize () {
    try {
      if (!this.validationEngine) {
        const { getValidationEngine } = require('./validation_engine');
        this.validationEngine = getValidationEngine();
      }

      // テンプレートの読み込み
      await this._loadTemplates();

      // MCPサーバーの初期化と起動
      await this._initMcpServer();

      this.initialized = true;
      if (this.debug) {
        console.log('ClaudeAI統合を初期化しました');
      }

      return true;
    } catch (error) {
      console.error('ClaudeAI統合の初期化に失敗しました:', error);
      this.initialized = false;
      throw error;
    }
  }

  /**
   * MCPサーバーの初期化
   * @private
   */
  _initMcpServer () {
    try {
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
      this._registerTools();

      // サーバー起動
      return this._startMcpServer();
    } catch (error) {
      console.error('MCPサーバーの初期化エラー:', error);
      throw new Error(`MCPサーバーの初期化に失敗しました: ${error.message}`);
    }
  }

  /**
   * MCP ツールの登録
   * @private
   */
  _registerTools () {
    try {
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
          await this._ensureInitialized();
          try {
            const result = this.validationEngine.validateCode(code, context);
            return { content: JSON.stringify(result, null, 2) };
          } catch (error) {
            if (this.debug) {
              console.error('コード検証エラー:', error);
            }
            return {
              content: JSON.stringify({
                valid: false,
                error: error.message,
                issues: [{ severity: 'error', message: error.message }]
              }, null, 2)
            };
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
          await this._ensureInitialized();
          try {
            // 入力検証
            if (!sourceObject || !destinationObject) {
              throw new Error('送信元オブジェクトと送信先オブジェクトは必須です');
            }

            // 数値型のチェック
            sourceOutlet = parseInt(sourceOutlet, 10);
            destinationInlet = parseInt(destinationInlet, 10);

            if (isNaN(sourceOutlet) || isNaN(destinationInlet)) {
              throw new Error('アウトレットとインレットは数値である必要があります');
            }

            const result = await this.validationEngine.validateConnection(
              sourceObject, sourceOutlet, destinationObject, destinationInlet
            );
            return { content: JSON.stringify(result, null, 2) };
          } catch (error) {
            if (this.debug) {
              console.error('接続検証エラー:', error);
            }
            return {
              content: JSON.stringify({
                valid: false,
                error: error.message,
                issues: [{ severity: 'error', message: error.message }]
              }, null, 2)
            };
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
          await this._ensureInitialized();
          try {
            // 入力検証
            if (!patch || !Array.isArray(patch.objects) || !Array.isArray(patch.connections)) {
              throw new Error('パッチにはobjectsとconnectionsの配列が必要です');
            }

            const result = await this.validationEngine.validatePatch(
              patch.objects, patch.connections
            );
            return { content: JSON.stringify(result, null, 2) };
          } catch (error) {
            if (this.debug) {
              console.error('パッチ検証エラー:', error);
            }
            return {
              content: JSON.stringify({
                valid: false,
                error: error.message,
                issues: [{ severity: 'error', message: error.message }]
              }, null, 2)
            };
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
          await this._ensureInitialized();
          try {
            if (!code || typeof code !== 'string') {
              throw new Error('コードが提供されていないか、無効な形式です');
            }

            if (!Array.isArray(issues)) {
              throw new Error('問題リストは配列である必要があります');
            }

            const fix = await this.getFixSuggestion(code, issues);
            return { content: JSON.stringify(fix, null, 2) };
          } catch (error) {
            if (this.debug) {
              console.error('修正提案エラー:', error);
            }
            return {
              content: JSON.stringify({
                has_proposals: false,
                error: error.message,
                proposals: []
              }, null, 2)
            };
          }
        }
      });
    } catch (error) {
      console.error('MCPツール登録エラー:', error);
      throw new Error(`MCPツールの登録に失敗しました: ${error.message}`);
    }
  }

  /**
   * MCPサーバーを起動する
   * @private
   * @returns {Promise<void>}
   */
  async _startMcpServer () {
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
      throw new Error(`MCPサーバーの起動に失敗しました: ${error.message}`);
    }
  }

  /**
   * 初期化が完了していることを確認する
   * @private
   * @returns {Promise<void>}
   */
  async _ensureInitialized () {
    if (!this.initialized) {
      await this.initialize();
      if (!this.initialized) {
        throw new Error('ClaudeAI統合が初期化されていません');
      }
    }
  }

  /**
   * テンプレートの読み込み
   * @private
   * @returns {Promise<void>}
   */
  async _loadTemplates () {
    try {
      // テンプレートディレクトリの存在確認
      try {
        await fs.access(this.templateDir);
      } catch (err) {
        // ディレクトリが存在しない場合は作成
        await fs.mkdir(this.templateDir, { recursive: true });

        // デフォルトテンプレートの作成
        for (const [filename, content] of Object.entries(DEFAULT_TEMPLATES)) {
          await fs.writeFile(path.join(this.templateDir, filename), content);
        }
      }

      // テンプレートファイルの読み込み
      const files = await fs.readdir(this.templateDir);
      for (const file of files) {
        if (file.endsWith('.txt')) {
          const templateName = path.basename(file, '.txt');
          const templateContent = await fs.readFile(path.join(this.templateDir, file), 'utf8');
          this.templates[templateName] = templateContent;
        }
      }

      if (this.debug) {
        console.log(`${Object.keys(this.templates).length}個のテンプレートを読み込みました`);
      }
    } catch (error) {
      console.error('テンプレート読み込みエラー:', error);
      throw new Error(`テンプレートの読み込みに失敗しました: ${error.message}`);
    }
  }

  /**
   * テンプレートにデータを埋め込む
   * @param {string} templateName - テンプレート名
   * @param {object} data - 埋め込むデータ
   * @returns {string} 完成したプロンプト
   */
  fillTemplate (templateName, data) {
    const template = this.templates[templateName];
    if (!template) {
      const error = new Error(`テンプレート ${templateName} が見つかりません`);
      if (this.debug) {
        console.warn(error.message);
      }
      return '';
    }

    // {{変数名}} パターンを置換
    return template.replace(/\{\{([^}]+)\}\}/g, (match, key) => {
      return data[key] !== undefined ? data[key] : match;
    });
  }

  /**
   * 修正提案を取得する
   * @param {string} code - 元のコード
   * @param {Array} issues - 検出された問題
   * @returns {Promise<object>} 修正提案
   */
  async getFixSuggestion (code, issues) {
    await this._ensureInitialized();

    // 問題がないか、問題リストが空の場合は空の提案を返す
    if (!issues || issues.length === 0) {
      return { has_proposals: false, proposals: [] };
    }

    try {
      // 基本的な修正提案を生成
      let basicProposals = { has_proposals: false, proposals: [] };

      if (this.validationEngine.generateFixProposals) {
        try {
          basicProposals = this.validationEngine.generateFixProposals(code, issues);
        } catch (error) {
          if (this.debug) {
            console.warn('基本的な修正提案の生成に失敗しました:', error);
          }
        }
      }

      // テンプレートを使った修正提案用のプロンプトを準備
      const promptData = {
        code: code,
        issues: issues.map(issue =>
          `- ${issue.severity || 'issue'}: ${issue.description || issue.message || JSON.stringify(issue)}`
        ).join('\n')
      };

      // プロンプトテンプレートを埋める
      const prompt = this.fillTemplate('code_fix', promptData);

      // MCP経由でAIに渡されるデータを形成
      return {
        has_proposals: basicProposals.has_proposals,
        proposals: basicProposals.proposals || [],
        prompt_template: prompt.substring(0, 200) + '...' // プロンプトの一部を返す
      };
    } catch (error) {
      console.error('修正提案生成エラー:', error);
      throw new Error(`修正提案の生成に失敗しました: ${error.message}`);
    }
  }

  /**
   * 検証結果をAIに適したフォーマットに変換
   * @param {object} result - 検証結果
   * @param {string} contextType - コンテキストタイプ ('code', 'connection', 'patch')
   * @returns {object} AIコンテキスト
   */
  formatValidationForAI (result, contextType) {
    if (!result) {
      throw new Error('検証結果が提供されていません');
    }

    switch (contextType) {
      case 'code':
        return this._formatCodeValidationForAI(result);
      case 'connection':
        return this._formatConnectionValidationForAI(result);
      case 'patch':
        return this._formatPatchValidationForAI(result);
      default:
        throw new Error(`未知のコンテキストタイプ: ${contextType}`);
    }
  }

  /**
   * コード検証結果をAIコンテキストに変換
   * @private
   * @param {object} result - コード検証結果
   * @returns {object} AIコンテキスト
   */
  _formatCodeValidationForAI (result) {
    // エラーと警告を分類
    const issues = Array.isArray(result.issues) ? result.issues : [];
    const errors = issues.filter(issue => issue.severity === 'error');
    const warnings = issues.filter(issue => issue.severity === 'warning');
    const info = issues.filter(issue => issue.severity === 'info');

    // 問題の種類を集計
    const ruleTypes = {};
    issues.forEach(issue => {
      if (issue.rule_type) {
        ruleTypes[issue.rule_type] = (ruleTypes[issue.rule_type] || 0) + 1;
      }
    });

    return {
      validation_summary: {
        is_valid: result.valid === true,
        total_issues: issues.length,
        error_count: errors.length,
        warning_count: warnings.length,
        info_count: info.length,
        code_length: result.code_length || 0,
        rule_types: ruleTypes
      },
      critical_issues: errors.length > 0 ? errors.slice(0, 3) : [],
      important_warnings: warnings.length > 0 ? warnings.slice(0, 3) : [],
      context: result.context || {}
    };
  }

  /**
   * 接続検証結果をAIコンテキストに変換
   * @private
   * @param {object} result - 接続検証結果
   * @returns {object} AIコンテキスト
   */
  _formatConnectionValidationForAI (result) {
    return {
      validation_summary: {
        is_valid: result.valid === true,
        has_performance_impact: result.performance_impact ? true : false,
        has_compatibility_issues: result.compatibility_issues ? true : false,
        is_recommended: result.is_recommended || false,
        audio_signal_flow: result.audio_signal_flow || false
      },
      issues: Array.isArray(result.issues) ? result.issues : [],
      alternative_suggestions: Array.isArray(result.alternative_suggestions)
        ? result.alternative_suggestions : [],
      similar_patterns: Array.isArray(result.similar_patterns)
        ? result.similar_patterns.slice(0, 3)
        : []
    };
  }

  /**
   * パッチ検証結果をAIコンテキストに変換
   * @private
   * @param {object} result - パッチ検証結果
   * @returns {object} AIコンテキスト
   */
  _formatPatchValidationForAI (result) {
    // エラーと警告を分類
    const issues = Array.isArray(result.issues) ? result.issues : [];
    const errors = issues.filter(issue => issue.severity === 'error');
    const warnings = issues.filter(issue => issue.severity === 'warning');

    // 問題のタイプを集計
    const issueTypes = {};
    issues.forEach(issue => {
      const type = issue.type || (issue.connection_id ? 'connection' : issue.object_id ? 'object' : 'general');
      issueTypes[type] = (issueTypes[type] || 0) + 1;
    });

    return {
      validation_summary: {
        is_valid: result.valid === true,
        total_issues: issues.length,
        error_count: errors.length,
        warning_count: warnings.length,
        total_objects: result.total_objects || 0,
        total_connections: result.total_connections || 0,
        issue_types: issueTypes
      },
      critical_issues: errors.length > 0 ? errors.slice(0, 3) : [],
      important_warnings: warnings.length > 0 ? warnings.slice(0, 3) : []
    };
  }

  /**
   * リソースのクリーンアップを行う
   * 他のクラスのcleanupメソッドとの整合性を保つため実装
   */
  cleanup () {
    if (this.mcpServer) {
      try {
        // 必要に応じてMCPサーバーの接続を閉じる処理があれば実装
        this.mcpServerStarted = false;
      } catch (error) {
        console.error('MCPサーバークリーンアップエラー:', error);
      }
    }

    // validation_engineのcleanupが必要な場合
    if (this.validationEngine && typeof this.validationEngine.cleanup === 'function') {
      try {
        this.validationEngine.cleanup();
      } catch (error) {
        console.error('ValidationEngineクリーンアップエラー:', error);
      }
    }

    this.initialized = false;
    console.log('ClaudeAI統合モジュールがクリーンアップされました');
  }
}

/**
 * ClaudeAI統合のシングルトンインスタンスを取得
 * 他のモジュールとの整合性を保つためのファクトリ関数
 * @param {Object} [options={}] - 設定オプション
 * @returns {ClaudeAIIntegration} ClaudeAI統合のインスタンス
 */
function getClaudeAIIntegration (options = {}) {
  if (!claudeIntegrationInstance) {
    claudeIntegrationInstance = new ClaudeAIIntegration(options);
  }
  return claudeIntegrationInstance;
}

/**
 * ClaudeAI統合のクリーンアップ
 * 他のモジュールとの整合性を保つためのクリーンアップ関数
 */
function cleanupClaudeAIIntegration () {
  if (claudeIntegrationInstance) {
    claudeIntegrationInstance.cleanup();
    claudeIntegrationInstance = null;
  }
}

// プロセス終了時の処理
process.on('SIGINT', () => {
  console.log('ClaudeAI統合をシャットダウンしています...');
  cleanupClaudeAIIntegration();
});

process.on('SIGTERM', () => {
  console.log('ClaudeAI統合をシャットダウンしています...');
  cleanupClaudeAIIntegration();
});

process.on('uncaughtException', (err) => {
  console.error('ClaudeAI統合で予期しないエラーが発生しました:', err);
  cleanupClaudeAIIntegration();
  process.exit(1);
});

module.exports = {
  ClaudeAIIntegration,
  getClaudeAIIntegration,
  cleanupClaudeAIIntegration
};
