/**
 * setup.js
 * Max 9 - Claude Desktop MCP連携プロジェクトの開発環境セットアップと依存関係管理
 * Issue 12の実装
 */

const fs = require('fs');
const path = require('path');
const { execSync } = require('child_process');
const os = require('os');

// カラー出力のための定数
const colors = {
  reset: '\x1b[0m',
  bright: '\x1b[1m',
  red: '\x1b[31m',
  green: '\x1b[32m',
  yellow: '\x1b[33m',
  blue: '\x1b[34m',
  magenta: '\x1b[35m',
  cyan: '\x1b[36m'
};

// バナー表示
console.log(`${colors.bright}${colors.blue}
╔══════════════════════════════════════════════════════════════╗
║         Max 9 - Claude Desktop MCP 開発環境セットアップ         ║
╚══════════════════════════════════════════════════════════════╝${colors.reset}
`);

// プロジェクトのルートディレクトリ
const projectRoot = path.resolve(__dirname);

/**
 * 必要なディレクトリ構造をセットアップ
 */
function setupDirectoryStructure () {
  console.log(`${colors.cyan}[セットアップ] ディレクトリ構造を確認しています...${colors.reset}`);

  const directories = [
    'src/maxpat_generator',
    'src/maxpat_generator/templates',
    'src/maxpat_generator/schemas',
    'src/maxpat_generator/lib',
    'src/mcp_connector',
    'src/max-bridge',
    'src/mcp-server',
    'src/protocol',
    'src/sql_knowledge_base',
    'src/document_knowledge_base',
    'src/graph_knowledge_base',
    'src/vector_knowledge_base',
    'data/vector_db',
    'data/graph_db',
    'data/document_db',
    'data/extracted',
    'data/transformed',
    'data/backups',
    'config',
    'scripts',
    'tests/unit',
    'tests/llm_connector',
    'externals',
    'assets'
  ];

  directories.forEach(dir => {
    const dirPath = path.join(projectRoot, dir);
    if (!fs.existsSync(dirPath)) {
      console.log(`  - ディレクトリを作成: ${dir}`);
      fs.mkdirSync(dirPath, { recursive: true });
    }
  });

  console.log(`${colors.green}[セットアップ] ディレクトリ構造の確認が完了しました${colors.reset}`);
}

/**
 * Node.js依存関係をセットアップ
 */
function setupNodeDependencies () {
  console.log(`${colors.cyan}[セットアップ] Node.js依存関係をセットアップしています...${colors.reset}`);

  // package.jsonが存在するか確認
  const packageJsonPath = path.join(projectRoot, 'package.json');
  if (!fs.existsSync(packageJsonPath)) {
    console.log(`  - package.jsonが見つかりません。新規作成します...`);

    const packageJson = {
      "name": "max9-claude-mcp-integration",
      "version": "0.1.0",
      "description": "Max 9とClaude Desktop MCP連携プロジェクト",
      "main": "src/mcp-server/index.js",
      "scripts": {
        "start": "node src/mcp-server/index.js",
        "test": "jest tests",
        "test:unit": "jest tests/unit",
        "test:integration": "jest tests/integration",
        "test:e2e": "jest tests/e2e",
        "lint": "eslint src",
        "setup": "node setup.js"
      },
      "engines": {
        "node": ">=14.0.0"
      },
      "dependencies": {
        "fastmcp": "^0.4.1",
        "node-osc": "^8.0.0",
        "dotenv": "^16.0.0",
        "express": "^4.17.1",
        "winston": "^3.3.3"
      },
      "devDependencies": {
        "eslint": "^8.10.0",
        "jest": "^29.0.0",
        "nodemon": "^2.0.15"
      },
      "keywords": [
        "max9",
        "claude",
        "mcp",
        "osc"
      ],
      "author": "",
      "license": "MIT"
    };

    fs.writeFileSync(packageJsonPath, JSON.stringify(packageJson, null, 2));
    console.log(`  - package.jsonを作成しました`);
  } else {
    console.log(`  - 既存のpackage.jsonを使用します`);
  }

  // 必要な依存関係をインストール
  try {
    console.log(`  - 依存関係をインストールしています...`);
    execSync('npm install', { stdio: 'inherit', cwd: projectRoot });
    console.log(`${colors.green}  - 依存関係のインストールが完了しました${colors.reset}`);
  } catch (error) {
    console.error(`${colors.red}[エラー] 依存関係のインストールに失敗しました:${colors.reset}`, error.message);
  }
}

/**
 * サンプル環境変数ファイルをセットアップ
 */
function setupEnvironmentVariables () {
  console.log(`${colors.cyan}[セットアップ] 環境変数ファイルをセットアップしています...${colors.reset}`);

  const envExamplePath = path.join(projectRoot, '.env.example');
  const envPath = path.join(projectRoot, '.env');

  if (!fs.existsSync(envExamplePath)) {
    console.log(`  - .env.exampleが見つかりません。作成します...`);

    const envExample = `# Max 9 - Claude Desktop MCP連携プロジェクト環境変数

# MCP接続設定
MCP_HOST=127.0.0.1
MCP_PORT=7400
MCP_LISTEN_PORT=7500

# Max 9接続設定
MAX_HOST=127.0.0.1
MAX_OSC_PORT=8000
MAX_ACCESS_LEVEL=full  # full, restricted, readonly

# ロギング設定
LOG_LEVEL=info  # debug, info, warn, error
LOG_FILE=mcp-server.log

# 開発設定
NODE_ENV=development  # development, production, test
`;

    fs.writeFileSync(envExamplePath, envExample);
    console.log(`  - .env.exampleを作成しました`);
  } else {
    console.log(`  - 既存の.env.exampleを使用します`);
  }

  // .envファイルがなければ.env.exampleからコピー
  if (!fs.existsSync(envPath)) {
    console.log(`  - .envファイルが見つかりません。.env.exampleからコピーします...`);
    fs.copyFileSync(envExamplePath, envPath);
    console.log(`  - .envファイルを作成しました`);
  } else {
    console.log(`  - 既存の.envファイルを使用します`);
  }

  console.log(`${colors.green}[セットアップ] 環境変数ファイルのセットアップが完了しました${colors.reset}`);
}

/**
 * テスト環境をセットアップ
 */
function setupTestEnvironment () {
  console.log(`${colors.cyan}[セットアップ] テスト環境をセットアップしています...${colors.reset}`);

  // Jestの設定ファイルを作成
  const jestConfigPath = path.join(projectRoot, 'jest.config.js');
  if (!fs.existsSync(jestConfigPath)) {
    console.log(`  - jest.config.jsが見つかりません。作成します...`);

    const jestConfig = `// Jest configuration
module.exports = {
  testEnvironment: 'node',
  testMatch: [
    '**/tests/**/*.test.js'
  ],
  collectCoverage: true,
  coverageDirectory: 'coverage',
  collectCoverageFrom: [
    'src/**/*.js',
    '!src/examples/**',
    '!**/node_modules/**'
  ],
  coverageReporters: ['text', 'lcov', 'clover'],
  verbose: true,
  testTimeout: 10000,
  setupFilesAfterEnv: ['./tests/setup.js']
};
`;

    fs.writeFileSync(jestConfigPath, jestConfig);
    console.log(`  - jest.config.jsを作成しました`);
  } else {
    console.log(`  - 既存のjest.config.jsを使用します`);
  }

  // テストセットアップファイルを作成
  const testSetupPath = path.join(projectRoot, 'tests', 'setup.js');
  if (!fs.existsSync(testSetupPath)) {
    console.log(`  - tests/setup.jsが見つかりません。作成します...`);

    const testSetup = `// テスト環境のグローバルセットアップ
require('dotenv').config();

// テスト終了時のタイムアウトを延長
jest.setTimeout(10000);

// グローバルな事前セットアップ
beforeAll(() => {
  // テスト前の共通セットアップ
  console.log('テスト環境をセットアップしています...');
});

// グローバルな事後クリーンアップ
afterAll(() => {
  // テスト後の共通クリーンアップ
  console.log('テスト環境をクリーンアップしています...');
});
`;

    fs.mkdirSync(path.dirname(testSetupPath), { recursive: true });
    fs.writeFileSync(testSetupPath, testSetup);
    console.log(`  - tests/setup.jsを作成しました`);
  } else {
    console.log(`  - 既存のtests/setup.jsを使用します`);
  }

  // サンプルテストファイルを作成
  const sampleTestPath = path.join(projectRoot, 'tests', 'unit', 'sample.test.js');
  if (!fs.existsSync(sampleTestPath)) {
    console.log(`  - サンプルテストファイルが見つかりません。作成します...`);

    const sampleTest = `// サンプル単体テスト
describe('サンプルテスト', () => {
  test('1 + 1は2である', () => {
    expect(1 + 1).toBe(2);
  });

  test('文字列の結合', () => {
    expect('hello' + ' ' + 'world').toBe('hello world');
  });
});
`;

    fs.mkdirSync(path.dirname(sampleTestPath), { recursive: true });
    fs.writeFileSync(sampleTestPath, sampleTest);
    console.log(`  - サンプルテストファイルを作成しました`);
  } else {
    console.log(`  - 既存のサンプルテストファイルを使用します`);
  }

  console.log(`${colors.green}[セットアップ] テスト環境のセットアップが完了しました${colors.reset}`);
}

/**
 * Max 8固有の依存関係をセットアップ
 */
function setupMaxDependencies () {
  console.log(`${colors.cyan}[セットアップ] Max 8固有の依存関係をセットアップしています...${colors.reset}`);

  // Maxパッケージ構成ファイルを作成
  const maxPackagePath = path.join(projectRoot, 'max-package.json');
  if (!fs.existsSync(maxPackagePath)) {
    console.log(`  - max-package.jsonが見つかりません。作成します...`);

    const maxPackage = {
      "name": "max8-claude-mcp",
      "version": "0.1.0",
      "author": "",
      "description": "Max 8とClaude Desktop MCP連携パッケージ",
      "homepatcher": "mcp_osc_bridge.maxpat",
      "max_version_min": "8.0",
      "max_version_max": "none",
      "os": {
        "macintosh": {
          "min_version": "10.13.x",
          "max_version": "none"
        },
        "windows": {
          "min_version": "10",
          "max_version": "none"
        }
      },
      "filelist": {
        "patchers": [
          "mcp_osc_bridge.maxpat"
        ],
        "javascript": [
          "mcp_osc_bridge.js",
          "parameter_sync.js",
          "error_state_manager.js"
        ]
      },
      "website": "",
      "tags": [
        "mcp",
        "claude",
        "ai"
      ]
    };

    fs.writeFileSync(maxPackagePath, JSON.stringify(maxPackage, null, 2));
    console.log(`  - max-package.jsonを作成しました`);
  } else {
    console.log(`  - 既存のmax-package.jsonを使用します`);
  }

  // 基本的なMaxパッチを作成
  const maxPatchPath = path.join(projectRoot, 'src', 'max-osc-bridge', 'mcp_osc_bridge.maxpat');
  if (!fs.existsSync(maxPatchPath)) {
    console.log(`  - 基本的なMaxパッチが見つかりません。作成します...`);

    // 非常に基本的なMaxパッチのテンプレート
    const basicMaxPatch = {
      "patcher": {
        "fileversion": 1,
        "appversion": {
          "major": 8,
          "minor": 5,
          "revision": 3,
          "architecture": "x64",
          "modernui": 1
        },
        "classnamespace": "box",
        "rect": [
          100,
          100,
          800,
          600
        ],
        "bglocked": 0,
        "openinpresentation": 0,
        "default_fontsize": 12.0,
        "default_fontface": 0,
        "default_fontname": "Arial",
        "gridonopen": 1,
        "gridsize": [
          15.0,
          15.0
        ],
        "gridsnaponopen": 1,
        "objectsnaponopen": 1,
        "statusbarvisible": 2,
        "toolbarvisible": 1,
        "lefttoolbarpinned": 0,
        "toptoolbarpinned": 0,
        "righttoolbarpinned": 0,
        "bottomtoolbarpinned": 0,
        "toolbars_unpinned_last_save": 0,
        "tallnewobj": 0,
        "boxanimatetime": 200,
        "enablehscroll": 1,
        "enablevscroll": 1,
        "devicewidth": 0.0,
        "description": "",
        "digest": "",
        "tags": "",
        "style": "",
        "subpatcher_template": "",
        "assistshowspatchername": 0,
        "boxes": [
          {
            "box": {
              "maxclass": "v8ui",
              "patching_rect": [
                225,
                180,
                450,
                300
              ],
              "presentation": 1,
              "presentation_rect": [
                225,
                180,
                450,
                300
              ],
              "id": "mcp_bridge",
              "jsarguments": [

              ],
              "embed": 1,
              "filename": "mcp_osc_bridge.js",
              "numinlets": 1,
              "numoutlets": 1,
              "outlettype": [
                ""
              ],
              "saved_object_attributes": {
                "filename": "mcp_osc_bridge.js",
                "parameter_enable": 0
              }
            }
          },
          {
            "box": {
              "maxclass": "comment",
              "text": "MCP-OSC Bridge",
              "patching_rect": [
                225,
                150,
                450,
                20
              ],
              "fontsize": 16,
              "fontface": 1
            }
          }
        ],
        "lines": []
      }
    };

    fs.mkdirSync(path.dirname(maxPatchPath), { recursive: true });
    fs.writeFileSync(maxPatchPath, JSON.stringify(basicMaxPatch, null, 2));
    console.log(`  - 基本的なMaxパッチを作成しました`);
  } else {
    console.log(`  - 既存のMaxパッチを使用します`);
  }

  console.log(`${colors.green}[セットアップ] Max 8固有の依存関係のセットアップが完了しました${colors.reset}`);
}

/**
 * VSCode設定をセットアップ
 */
function setupVSCodeConfig () {
  console.log(`${colors.cyan}[セットアップ] VSCode設定をセットアップしています...${colors.reset}`);

  const vscodeDir = path.join(projectRoot, '.vscode');
  if (!fs.existsSync(vscodeDir)) {
    fs.mkdirSync(vscodeDir, { recursive: true });
  }

  // settings.json
  const settingsPath = path.join(vscodeDir, 'settings.json');
  if (!fs.existsSync(settingsPath)) {
    console.log(`  - VSCode settings.jsonが見つかりません。作成します...`);

    const settings = {
      "editor.formatOnSave": true,
      "editor.codeActionsOnSave": {
        "source.fixAll.eslint": true
      },
      "javascript.format.insertSpaceBeforeFunctionParenthesis": true,
      "javascript.format.insertSpaceAfterConstructor": true,
      "javascript.preferences.quoteStyle": "single",
      "javascript.updateImportsOnFileMove.enabled": "always",
      "files.trimTrailingWhitespace": true,
      "files.eol": "\n",
      "files.insertFinalNewline": true,
      "search.exclude": {
        "**/node_modules": true,
        "**/bower_components": true,
        "**/coverage": true
      }
    };

    fs.writeFileSync(settingsPath, JSON.stringify(settings, null, 2));
    console.log(`  - VSCode settings.jsonを作成しました`);
  } else {
    console.log(`  - 既存のVSCode settings.jsonを使用します`);
  }

  // launch.json
  const launchPath = path.join(vscodeDir, 'launch.json');
  if (!fs.existsSync(launchPath)) {
    console.log(`  - VSCode launch.jsonが見つかりません。作成します...`);

    const launch = {
      "version": "0.2.0",
      "configurations": [
        {
          "type": "node",
          "request": "launch",
          "name": "Launch MCP Server",
          "skipFiles": [
            "<node_internals>/**"
          ],
          "program": "${workspaceFolder}/src/mcp-server/index.js",
          "env": {
            "NODE_ENV": "development"
          }
        },
        {
          "type": "node",
          "request": "launch",
          "name": "Run Tests",
          "program": "${workspaceFolder}/node_modules/jest/bin/jest",
          "args": [
            "--runInBand"
          ],
          "console": "integratedTerminal",
          "internalConsoleOptions": "neverOpen"
        }
      ]
    };

    fs.writeFileSync(launchPath, JSON.stringify(launch, null, 2));
    console.log(`  - VSCode launch.jsonを作成しました`);
  } else {
    console.log(`  - 既存のVSCode launch.jsonを使用します`);
  }

  console.log(`${colors.green}[セットアップ] VSCode設定のセットアップが完了しました${colors.reset}`);
}

/**
 * プラットフォーム固有の設定をセットアップ
 */
function setupPlatformSpecific () {
  const platform = os.platform();
  console.log(`${colors.cyan}[セットアップ] プラットフォーム固有の設定をセットアップしています (${platform})...${colors.reset}`);

  if (platform === 'darwin') {
    // macOS固有の設定
    console.log(`  - macOS固有の設定をセットアップしています...`);

    // Maxパッケージディレクトリへのシンボリックリンク作成のガイド
    console.log(`
  ${colors.yellow}[注意] Max 8パッケージディレクトリへのリンク:${colors.reset}
  以下のコマンドを実行して、プロジェクトをMax 8パッケージディレクトリにリンクできます:

  ${colors.bright}ln -s "${projectRoot}" "$HOME/Documents/Max 8/Packages/max8-claude-mcp"${colors.reset}

  これにより、開発中の変更がMax 8に自動的に反映されます。
    `);

  } else if (platform === 'win32') {
    // Windows固有の設定
    console.log(`  - Windows固有の設定をセットアップしています...`);

    // Maxパッケージディレクトリへのシンボリックリンク作成のガイド
    console.log(`
  ${colors.yellow}[注意] Max 8パッケージディレクトリへのリンク:${colors.reset}
  以下のコマンドを管理者権限で実行して、プロジェクトをMax 8パッケージディレクトリにリンクできます:

  ${colors.bright}mklink /D "%USERPROFILE%\\Documents\\Max 8\\Packages\\max8-claude-mcp" "${projectRoot}"${colors.reset}

  これにより、開発中の変更がMax 8に自動的に反映されます。
    `);
  }

  console.log(`${colors.green}[セットアップ] プラットフォーム固有の設定のセットアップが完了しました${colors.reset}`);
}

/**
 * READMEファイルをセットアップ
 */
function setupReadme () {
  console.log(`${colors.cyan}[セットアップ] READMEファイルをセットアップしています...${colors.reset}`);

  const readmePath = path.join(projectRoot, 'README.md');
  if (!fs.existsSync(readmePath)) {
    console.log(`  - README.mdが見つかりません。作成します...`);

    const readme = `# Max 9 - Claude Desktop MCP連携プロジェクト

Max 9とClaude Desktop（AI）の連携を実現するためのプロジェクトです。MCPプロトコルを介して、自然言語によるMax 9の制御や創造的なパッチ生成を可能にします。

## 機能

- OSCを介したMax 9とClaude Desktop間の双方向通信
- パラメータ同期メカニズム（param.oscを活用）
- 階層化されたエラーハンドリングと状態管理
- 多言語開発環境（JavaScript, Python, C++）

## 必要条件

- Max 9
- Node.js 14以上
- Claude Desktop

## インストール方法

1. リポジトリをクローン
   \`\`\`
   git clone https://github.com/yourusername/max9-claude-mcp.git
   \`\`\`

2. 依存関係をインストール
   \`\`\`
   cd max9-claude-mcp
   npm install
   \`\`\`

3. 環境変数を設定
   \`\`\`
   cp .env.example .env
   # .envファイルを編集して設定を調整
   \`\`\`

4. MCPサーバーを起動
   \`\`\`
   npm start
   \`\`\`

## 使用方法

1. Max 9を起動し、\`mcp_osc_bridge.maxpat\`パッチを開きます
2. Claude Desktopを設定して、MCPサーバーに接続します
3. 自然言語でMax 9を操作できるようになります

## プロジェクト構造

- \`src/mcp-server\`: MCPサーバー実装（Node.js）
- \`src/max-osc-bridge\`: Max 9 OSCブリッジ（v8ui JavaScript）
- \`src/max-bridge\`: Max 9拡張実装（C++/JS）
- \`docs\`: ドキュメント
- \`tests\`: テストファイル

## 開発ガイド

開発を始める前に、以下のドキュメントを参照してください：

- [開発環境セットアップ](docs/setup_guide.md)
- [APIリファレンス](docs/api_reference.md)
- [コントリビューションガイド](CONTRIBUTING.md)

## ライセンス

MIT
`;

    fs.writeFileSync(readmePath, readme);
    console.log(`  - README.mdを作成しました`);
  } else {
    console.log(`  - 既存のREADME.mdを使用します`);
  }

  console.log(`${colors.green}[セットアップ] READMEファイルのセットアップが完了しました${colors.reset}`);
}

/**
 * 使用方法ガイドを表示
 */
function showUsageGuide () {
  console.log(`
${colors.bright}${colors.green}セットアップが完了しました！${colors.reset}

${colors.cyan}使用方法:${colors.reset}

1. MCPサーバーを起動:
   ${colors.bright}npm start${colors.reset}

2. テストを実行:
   ${colors.bright}npm test${colors.reset}

3. 開発環境の再セットアップ:
   ${colors.bright}npm run setup${colors.reset}

${colors.cyan}プロジェクト構造:${colors.reset}
- src/mcp-server: MCPサーバー実装（Node.js）
- src/max-osc-bridge: Max 9 OSCブリッジ（v8ui JavaScript）
- src/max-bridge: Max 9拡張実装（必要に応じてC++）
- docs: ドキュメント
- tests: テストファイル

${colors.yellow}注意:${colors.reset} Max 9のパッケージディレクトリにシンボリックリンクを作成することで、開発中の変更をMax 9に直接反映できます。
`);
}

/**
 * すべてのセットアップ関数を順番に実行
 */
async function runSetup () {
  try {
    setupDirectoryStructure();
    setupNodeDependencies();
    setupEnvironmentVariables();
    setupTestEnvironment();
    setupMaxDependencies();
    setupVSCodeConfig();
    setupPlatformSpecific();
    setupReadme();

    showUsageGuide();
  } catch (error) {
    console.error(`${colors.red}[エラー] セットアップ中にエラーが発生しました:${colors.reset}`, error);
    process.exit(1);
  }
}

// セットアップの実行
runSetup();
