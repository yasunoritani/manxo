/**
 * Max 9 - Claude Desktop MCP連携サーバー
 * MCPサーバーのエントリーポイント
 * エラー処理と接続回復機能強化版
 */

const { FastMCP } = require('fastmcp');
const { OSCClient } = require('./lib/osc-client');
const { setupTools } = require('./lib/tools');
const { loadConfig } = require('./lib/config');
const dotenv = require('dotenv');

// 環境変数の読み込み
dotenv.config();

// 環境変数から設定を読み込み
const ACCESS_LEVEL = process.env.ACCESS_LEVEL || 'restricted'; // full/restricted/readonly
const CONNECTION_TIMEOUT = parseInt(process.env.CONNECTION_TIMEOUT || '10000');
const MAX_RETRY_COUNT = parseInt(process.env.MAX_RETRY_COUNT || '5');
const RETRY_INTERVAL = parseInt(process.env.RETRY_INTERVAL || '3000');
const HEARTBEAT_INTERVAL = parseInt(process.env.HEARTBEAT_INTERVAL || '30000');

// コネクション管理用変数
let heartbeatTimer = null;
let reconnectTimer = null;

// 設定の読み込み
const config = loadConfig();

// MCPサーバーの初期化
const mcp = new FastMCP();
const oscClient = new OSCClient(config.max.host, config.max.port);

// Maxセッション状態の管理
const maxSession = {
  isConnected: false,
  patches: {},
  objects: {},
  parameters: {},
  connectionStatus: {
    lastConnectedAt: null,
    lastDisconnectedAt: null,
    reconnectAttempts: 0,
    lastHeartbeatAt: null,
    errors: []
  }
};

// ツールの登録 - セッション情報も渡すように修正
setupTools(mcp, oscClient, maxSession);

/**
 * ハートビートの開始
 */
function startHeartbeat() {
  // 既存のタイマーがあればクリア
  if (heartbeatTimer) {
    clearInterval(heartbeatTimer);
  }
  
  // 定期的にピングを送信
  heartbeatTimer = setInterval(() => {
    if (maxSession.isConnected && oscClient) {
      try {
        console.log('ハートビート送信...');
        oscClient.send('/mcp/ping', Date.now());
      } catch (e) {
        console.error(`ハートビートエラー: ${e.message}`);
        
        // 長時間応答がない場合は切断と判断
        const lastHeartbeat = maxSession.connectionStatus.lastHeartbeatAt;
        if (lastHeartbeat && (Date.now() - lastHeartbeat > HEARTBEAT_INTERVAL * 2)) {
          console.error('ハートビートが長時間受信されていません。接続が切断された可能性があります。');
          maxSession.isConnected = false;
          maxSession.connectionStatus.lastDisconnectedAt = Date.now();
          
          // 再接続をスケジュール
          scheduleReconnect();
        }
      }
    }
  }, HEARTBEAT_INTERVAL);
}

/**
 * 再接続のスケジュール
 */
function scheduleReconnect() {
  // 既存のリトライタイマーがあればクリア
  if (reconnectTimer) {
    clearTimeout(reconnectTimer);
  }
  
  // 再試行回数の上限を確認
  if (maxSession.connectionStatus.reconnectAttempts >= MAX_RETRY_COUNT) {
    console.error(`最大再試行回数(${MAX_RETRY_COUNT})に達しました。手動での再接続が必要です。`);
    return;
  }
  
  // 再接続を試行
  maxSession.connectionStatus.reconnectAttempts++;
  console.log(`Max 9への再接続をスケジュールしています... (${maxSession.connectionStatus.reconnectAttempts}/${MAX_RETRY_COUNT})`);
  
  reconnectTimer = setTimeout(async () => {
    try {
      // 再接続を試みる
      await oscClient.reconnect();
      maxSession.isConnected = true;
      maxSession.connectionStatus.lastConnectedAt = Date.now();
      console.log('Max 9への接続が回復しました');
      
      // ハートビートの再開
      startHeartbeat();
    } catch (error) {
      console.error(`再接続に失敗しました: ${error.message}`);
      maxSession.connectionStatus.errors.push({
        type: 'reconnect_error',
        message: error.message,
        timestamp: Date.now()
      });
      
      // 再度スケジュール
      scheduleReconnect();
    }
  }, RETRY_INTERVAL);
}

/**
 * アプリケーションの初期化と起動
 */
async function main() {
  console.log('Max 9 MCPサーバーを起動しています...');
  
  // サーバー起動時間を記録
  global.serverStartTime = Date.now();
  
  try {
    // MCPサーバーを起動
    await mcp.start();
    
    // OSC接続を開始
    await oscClient.connect();
    maxSession.isConnected = true;
    maxSession.connectionStatus.lastConnectedAt = Date.now();
    
    // ハートビートを開始
    startHeartbeat();
    
    console.log(`Max 9 MCPサーバーが起動しました (ポート: ${mcp.port})`);
    console.log(`OSC設定: ${config.max.host}:${config.max.port}`);
    console.log(`アクセスレベル: ${ACCESS_LEVEL}`);
  } catch (error) {
    console.error(`サーバー起動エラー: ${error.message}`);
    process.exit(1);
  }
}

// シャットダウン処理
process.on('SIGINT', async () => {
  console.log('MCPサーバーをシャットダウンしています...');
  
  // タイマーをクリア
  if (heartbeatTimer) clearInterval(heartbeatTimer);
  if (reconnectTimer) clearTimeout(reconnectTimer);
  
  // 接続を閉じる
  await oscClient.disconnect();
  await mcp.stop();
  
  console.log('MCPサーバーを正常に終了しました');
  process.exit(0);
});

// アプリケーションを起動
main().catch(error => {
  console.error('サーバー起動エラー:', error);
  process.exit(1);
});
