/**
 * Max関連ツールの定義
 * Claude DesktopからMax 9を操作するためのツール群
 */

/**
 * システム関連ツールの設定
 * @param {Object} tools - MCPツールレジストラ
 */
function setupSystemTools (tools) {
    // Max接続状態確認ツール
    tools.registerTool("max_connection_status", {
        description: "Max 9との接続状態を確認",
        parameters: {},
        handler: async () => {
            try {
                // 現在は直接接続はサポートしていないため、常に非接続状態を返す
                return {
                    status: 'disconnected',
                    message: 'Direct OSC connection is not currently supported. Using file-based transfer instead.'
                };
            } catch (error) {
                console.error('接続状態確認エラー:', error);
                return {
                    status: 'error',
                    message: error.message
                };
            }
        }
    });

    console.log('Maxシステムツールを登録しました');
}

/**
 * リソースのクリーンアップ
 */
function cleanup () {
    // 特にクリーンアップするリソースがない場合は空の関数を用意
}

module.exports = {
    setupSystemTools,
    cleanup
};
