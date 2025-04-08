/**
 * MCP Server Tools
 * Max 9への接続を管理するためのツールセット
 */

const maxTools = require('./max-tools');
const maxpatTools = require('./maxpat-tools');

/**
 * ツールのセットアップを行う
 * @param {Object} tools - MCPツールレジストラ
 * @param {Object} oscClient - OSCクライアント
 */
async function setupTools (tools) {
  // システム関連ツールの登録
  maxTools.setupSystemTools(tools);

  // Maxパッチジェネレータツールの登録
  await maxpatTools.setupMaxpatTools(tools);

  console.log('MCPツールの登録が完了しました');
}

/**
 * リソースのクリーンアップを行う
 */
async function cleanup () {
  await maxTools.cleanup();
  await maxpatTools.cleanup();
}

module.exports = {
  setupTools,
  cleanup
};
