/**
 * MCP Tools定義
 * Claude DesktopからMax 9を操作するためのツール群の定義
 */

/**
 * MCPサーバーにツールを登録する
 * @param {FastMCP} mcp - FastMCPインスタンス
 * @param {OSCClient} oscClient - OSCクライアントインスタンス
 */
function setupTools(mcp, oscClient) {
  // システム関連ツール
  setupSystemTools(mcp, oscClient);
  
  // 現段階では基本的な接続確認ツールのみ実装
  // 将来的には以下のツールを追加予定:
  // - パッチ操作ツール
  // - オブジェクト操作ツール
  // - 接続操作ツール
  // - パラメータ操作ツール
}

/**
 * システム関連ツールの設定
 * @param {FastMCP} mcp - FastMCPインスタンス
 * @param {OSCClient} oscClient - OSCクライアントインスタンス
 */
function setupSystemTools(mcp, oscClient) {
  // 接続確認ツール
  mcp.tool({
    name: 'max_connection_status',
    description: 'Check the connection status with Max 9',
    parameters: {},
    handler: async (ctx) => {
      try {
        await oscClient.connect();
        
        return {
          status: oscClient.connected ? 'connected' : 'disconnected',
          host: oscClient.host,
          port: oscClient.port
        };
      } catch (error) {
        ctx.log.error(`Connection check failed: ${error.message}`);
        return {
          status: 'error',
          message: error.message
        };
      }
    }
  });
  
  // Maxバージョン情報取得ツール
  mcp.tool({
    name: 'max_version_info',
    description: 'Get version information from Max 9',
    parameters: {},
    handler: async (ctx) => {
      try {
        if (!oscClient.connected) {
          await oscClient.connect();
        }
        
        return new Promise((resolve, reject) => {
          oscClient.sendMessage('/mcp/system/version', [], (error, response) => {
            if (error) {
              ctx.log.error(`Failed to get Max version: ${error.message}`);
              reject(error);
              return;
            }
            
            resolve({
              version: response[0] || 'unknown',
              build: response[1] || 'unknown'
            });
          });
        });
      } catch (error) {
        ctx.log.error(`Version check failed: ${error.message}`);
        throw error;
      }
    }
  });
  
  // Ping確認ツール
  mcp.tool({
    name: 'max_ping',
    description: 'Send a ping to Max 9 and measure response time',
    parameters: {},
    handler: async (ctx) => {
      try {
        if (!oscClient.connected) {
          await oscClient.connect();
        }
        
        const startTime = Date.now();
        
        return new Promise((resolve, reject) => {
          oscClient.sendMessage('/mcp/system/ping', [startTime], (error, response) => {
            if (error) {
              ctx.log.error(`Ping failed: ${error.message}`);
              reject(error);
              return;
            }
            
            const endTime = Date.now();
            const roundTripTime = endTime - startTime;
            
            resolve({
              success: true,
              roundTripTime,
              timestamp: new Date().toISOString()
            });
          });
        });
      } catch (error) {
        ctx.log.error(`Ping failed: ${error.message}`);
        throw error;
      }
    }
  });
}

module.exports = { setupTools };
