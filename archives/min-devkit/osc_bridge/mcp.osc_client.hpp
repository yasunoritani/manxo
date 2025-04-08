#pragma once

#include "mcp.osc_types.hpp"
#include <memory>
#include <functional>
#include <string>
#include <atomic>

// Forward declarations for oscpack
namespace osc {
    class OutboundPacketStream;
}

namespace UdpTransport {
    class UdpTransmitSocket;
}

namespace mcp {
namespace osc {

/**
 * OSCクライアントクラス
 * OSCメッセージの送信を担当
 */
class client {
public:
    /**
     * コンストラクタ
     * @param config 接続設定
     */
    client(const connection_config& config = connection_config{});
    
    /**
     * デストラクタ
     */
    ~client();
    
    /**
     * クライアントを接続する
     * @return 接続成功の場合true
     */
    bool connect();
    
    /**
     * クライアントを切断する
     */
    void disconnect();
    
    /**
     * 接続設定を更新する
     * @param config 新しい接続設定
     * @param reconnect 設定更新後に再接続するかどうか
     * @return 成功時true、失敗時false
     */
    bool update_config(const connection_config& config, bool reconnect = true);
    
    /**
     * OSCメッセージを送信する
     * @param address OSCアドレスパターン
     * @param args メッセージの引数
     * @return 送信成功の場合true
     */
    bool send(const std::string& address, const atoms& args);
    
    /**
     * 接続状態を取得する
     * @return 現在の接続状態
     */
    connection_state get_connection_state() const;
    
    /**
     * 最後のエラーを取得する
     * @return 最後に発生したエラー
     */
    const error_info& get_last_error() const;
    
    /**
     * エラーハンドラを設定
     * @param handler エラー発生時に呼び出される関数
     */
    void set_error_handler(error_handler handler);
    

    
private:
    /**
     * バッファサイズを取得する
     * @return 現在のバッファサイズ
     */
    int get_buffer_size() const;
    
    connection_config config_;                  // 接続設定
    std::unique_ptr<UdpTransport::UdpTransmitSocket> socket_; // UDPソケット
    std::atomic<bool> running_;                // 実行中フラグ
    std::atomic<connection_state> connection_state_; // 接続状態
    error_info last_error_;                   // 最後のエラー
    error_handler error_handler_;             // エラーハンドラー
    std::mutex mutex_;                        // スレッド同期用ミューテックス
};

}} // namespace mcp::osc
