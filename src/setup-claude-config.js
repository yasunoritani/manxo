#!/usr/bin/env node

/**
 * Claudeの設定ファイル(claude_desktop_config.json)を自動的に更新するスクリプト
 * MCP用のツール設定を追加します
 */

const fs = require('fs');
const path = require('path');
const os = require('os');

// Claudeの設定ファイルのパスをプラットフォームごとに決定
let claudeConfigPath;
const platform = os.platform();

if (platform === 'darwin') {
    // macOS
    claudeConfigPath = path.join(os.homedir(), 'Library', 'Application Support', 'Claude', 'claude_desktop_config.json');
} else if (platform === 'win32') {
    // Windows
    claudeConfigPath = path.join(os.homedir(), 'AppData', 'Roaming', 'Claude', 'claude_desktop_config.json');
} else {
    // Linux またはその他のプラットフォーム
    claudeConfigPath = path.join(os.homedir(), '.config', 'Claude', 'claude_desktop_config.json');
}

// 追加するMCPツール設定
const mcpTools = [
    {
        name: "generateMaxPatch",
        description: "Max/MSPパッチを自然言語から生成します",
        icon: "🎵",
        address: "http://localhost:3000/mcp",
        authType: "none"
    },
    {
        name: "searchMaxObjects",
        description: "Max/MSPオブジェクトをキーワードで検索します",
        icon: "🔍",
        address: "http://localhost:3000/mcp",
        authType: "none"
    },
    {
        name: "checkConnectionPattern",
        description: "オブジェクト間の接続が有効か検証します",
        icon: "✅",
        address: "http://localhost:3000/mcp",
        authType: "none"
    },
    // 分散型DB用の新しいツール設定
    {
        name: "semanticSearch",
        description: "意味的な質問からベクトル検索を実行します",
        icon: "💡",
        address: "http://localhost:3000/mcp",
        authType: "none"
    },
    {
        name: "findRelatedObjects",
        description: "グラフ構造から関連オブジェクトを探索します",
        icon: "🕸️",
        address: "http://localhost:3000/mcp",
        authType: "none"
    },
    {
        name: "integratedSearch",
        description: "複数のデータベースを横断した統合検索を実行します",
        icon: "🔄",
        address: "http://localhost:3000/mcp",
        authType: "none"
    }
];

// メイン関数
async function updateClaudeConfig () {
    console.log('📝 Claudeの設定ファイルを更新中...');

    try {
        // 設定ファイルの存在確認
        if (!fs.existsSync(claudeConfigPath)) {
            console.log('⚠️ Claudeの設定ファイルが見つかりません。新規ファイルを作成します。');
            fs.writeFileSync(claudeConfigPath, JSON.stringify({ tools: [] }, null, 2));
        }

        // 現在の設定を読み込む
        const configContent = fs.readFileSync(claudeConfigPath, 'utf8');
        let config;

        try {
            config = JSON.parse(configContent);
        } catch (e) {
            console.error('❌ 設定ファイルの解析に失敗しました:', e.message);
            console.log('新しい設定ファイルを作成します。');
            config = { tools: [] };
        }

        // toolsプロパティがない場合は追加
        if (!config.tools) {
            config.tools = [];
        }

        // 既存のManxoツールを削除
        const filteredTools = config.tools.filter(tool =>
            !mcpTools.some(mcpTool => mcpTool.name === tool.name && tool.address.includes('localhost:3000'))
        );

        // 新しいツールを追加
        config.tools = [...filteredTools, ...mcpTools];

        // 設定を書き戻す
        fs.writeFileSync(claudeConfigPath, JSON.stringify(config, null, 2));

        console.log('✅ Claudeの設定ファイルを更新しました!');
        console.log(`📂 設定ファイルの場所: ${claudeConfigPath}`);
        console.log('🚀 MCPサーバーを起動して Claude Desktop を再起動すると変更が反映されます。');

    } catch (error) {
        console.error('❌ エラーが発生しました:', error);
        process.exit(1);
    }
}

// スクリプト実行
updateClaudeConfig();
