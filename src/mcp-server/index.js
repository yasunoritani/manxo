/**
 * MCP Server
 * MaxとClaude Desktopを接続するためのMCPサーバー
 */

require('dotenv').config();
const { mcp } = require('fastmcp');
const { setupTools, cleanup } = require('./lib/tools');

// MCP接続用環境変数
const ACCESS_LEVEL = process.env.MCP_ACCESS_LEVEL || 'debug';
const CONNECTION_TIMEOUT = parseInt(process.env.MCP_CONNECTION_TIMEOUT) || 30000;
const MAX_RETRY_COUNT = parseInt(process.env.MCP_MAX_RETRY_COUNT) || 5;
const HEARTBEAT_INTERVAL = parseInt(process.env.HEARTBEAT_INTERVAL) || 5000;

// 接続状態管理
let connectionActive = false;
let heartbeatInterval = null;

/**
 * MCP接続のハートビートを維持する
 */
function startHeartbeat () {
  if (heartbeatInterval) {
    clearInterval(heartbeatInterval);
  }

  heartbeatInterval = setInterval(() => {
    if (connectionActive) {
      console.log('MCP接続ハートビート: 接続アクティブ');
    }
  }, HEARTBEAT_INTERVAL);
}

/**
 * MCP接続のハートビートを停止する
 */
function stopHeartbeat () {
  if (heartbeatInterval) {
    clearInterval(heartbeatInterval);
    heartbeatInterval = null;
  }
}

/**
 * シャットダウン処理
 */
async function handleShutdown () {
  console.log('シャットダウン処理を開始...');

  // ハートビートを停止
  stopHeartbeat();

  // リソースをクリーンアップ
  await cleanup();

  console.log('正常にシャットダウンしました');
  process.exit(0);
}

/**
 * メイン処理
 */
async function main () {
  console.log('MCPサーバーを開始します...');

  try {
    // FastMCPを初期化
    const server = mcp({
      accessLevel: ACCESS_LEVEL,
      connectionTimeout: CONNECTION_TIMEOUT,
      maxRetryCount: MAX_RETRY_COUNT
    });

    // 接続イベントのハンドリング
    server.on('connected', () => {
      console.log('Claude Desktopと接続しました');
      connectionActive = true;
      startHeartbeat();
    });

    server.on('disconnected', () => {
      console.log('Claude Desktopとの接続が切断されました');
      connectionActive = false;
      stopHeartbeat();
    });

    // ツールを設定
    await setupTools(server);

    // サーバーを起動
    await server.start();
    console.log('MCPサーバーが正常に起動しました');

    // シャットダウンハンドラーを設定
    process.on('SIGINT', handleShutdown);
    process.on('SIGTERM', handleShutdown);

  } catch (error) {
    console.error('MCPサーバーの起動中にエラーが発生しました:', error);
    process.exit(1);
  }
}

// スクリプトが直接実行された場合のみメイン処理を実行
if (require.main === module) {
  main().catch(error => {
    console.error('予期せぬエラーが発生しました:', error);
    process.exit(1);
  });
}
