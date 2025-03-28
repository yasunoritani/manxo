/**
 * Max 9 - Claude Desktop MCP連携サーバー
 * MCPサーバーのエントリーポイント
 */

const { FastMCP } = require('fastmcp');
const { OSCClient } = require('./lib/osc-client');
const { setupTools } = require('./lib/tools');
const { loadConfig } = require('./lib/config');

// 設定の読み込み
const config = loadConfig();

// MCPサーバーの初期化
const mcp = new FastMCP();
const oscClient = new OSCClient(config.max.host, config.max.port);

// ツールの登録
setupTools(mcp, oscClient);

// サーバー起動
mcp.start().then(() => {
  console.log(`Max 9 MCP Server running in ${config.environment} mode`);
  console.log(`Connected to Max at ${config.max.host}:${config.max.port}`);
}).catch(err => {
  console.error('Failed to start MCP server:', err);
  process.exit(1);
});

// シャットダウン処理
process.on('SIGINT', async () => {
  console.log('Shutting down MCP server...');
  await oscClient.disconnect();
  await mcp.stop();
  process.exit(0);
});
