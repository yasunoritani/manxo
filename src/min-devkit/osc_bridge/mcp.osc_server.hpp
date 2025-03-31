#pragma once

#include "mcp.osc_types.hpp"
#include <memory>
#include <functional>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>

// Forward declarations
namespace UdpTransport {
    class UdpReceiveSocket;
}

namespace mcp {
namespace osc {

// Forward declaration
class packet_listener;

/**
 * OSCサーバークラス
 * OSCメッセージの受信を担当
 */
class server {
public:
    /**
     * コンストラクタ
     * @param config 接続設定
     */
    server(const connection_config& config = connection_config{});
    
    /**
     * デストラクタ
     */
    ~server();
    
    /**
     * サーバーを開始する
     * @return 開始成功の場合true
     */
    bool start();
    
    /**
     * サーバーを接続する
     * (互換性のためのconnect名)
     * @return 開始成功の場合true
     */
    bool connect() { return start(); }
    
    /**
     * サーバーを停止する
     */
    void stop();
    
    /**
     * サーバーを切断する
     * (互換性のためのdisconnect名)
     */
    void disconnect() { stop(); }
    
    /**
     * 接続設定を更新する
     * @param config 新しい接続設定
     * @param reconnect 設定更新後に再接続するかどうか
     * @return 成功時true、失敗時false
     */
    bool update_config(const connection_config& config, bool reconnect = true);
    
    /**
     * OSCアドレスパターンにハンドラーを登録する
     * @param pattern OSCアドレスパターン
     * @param handler メッセージハンドラー
     * @return 登録成功の場合true
     */
    bool add_handler(const std::string& pattern, message_handler handler);
    
    /**
     * OSCアドレスパターンにハンドラーを登録する
     * (互換性のためのregister_handler名)
     * @param pattern OSCアドレスパターン
     * @param handler メッセージハンドラー
     * @return 登録成功の場合true
     */
    bool register_handler(const std::string& pattern, message_handler handler) {
        return add_handler(pattern, handler);
    }
    
    /**
     * OSCアドレスパターンのハンドラーを解除する
     * @param pattern OSCアドレスパターン
     * @return 解除成功の場合true
     */
    bool remove_handler(const std::string& pattern);
    
    /**
     * OSCアドレスパターンのハンドラーを解除する
     * (互換性のためのunregister_handler名)
     * @param pattern OSCアドレスパターン
     * @return 解除成功の場合true
     */
    bool unregister_handler(const std::string& pattern) {
        return remove_handler(pattern);
    }
    
    /**
     * すべてのハンドラーを解除する
     */
    void clear_handlers();
    
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
    // 受信スレッド関数
    void receive_thread_func();
    
    // ポート競合を解決する（動的ポート割り当て）
    bool resolve_port_conflict(int& port, int max_attempts);
    
    connection_config config_;                 // 接続設定
    std::unique_ptr<UdpTransport::UdpReceiveSocket> socket_; // UDPソケット
    std::unique_ptr<packet_listener> listener_;   // OSCパケットリスナー
    handler_registry handlers_;                 // メッセージハンドラーレジストリ
    std::atomic<bool> running_;                // 実行中フラグ
    std::atomic<connection_state> connection_state_; // 接続状態
    error_info last_error_;                    // 最後のエラー
    error_handler error_handler_;              // エラーハンドラー
    std::mutex mutex_;                        // スレッド同期用ミューテックス
    std::thread receive_thread_;              // 受信スレッド
};

}} // namespace mcp::osc
