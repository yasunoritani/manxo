#include "mcp.osc_types.hpp"
#include "osc/OscOutboundPacketStream.h"
#include "ip/UdpSocket.h"

#include <stdexcept>
#include <thread>
#include <atomic>
#include <chrono>

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
    client(const connection_config& config = connection_config{})
        : config_(config), 
          socket_(nullptr),
          running_(false),
          connection_state_(connection_state::disconnected) {
    }
    
    /**
     * デストラクタ
     * 接続を閉じる
     */
    ~client() {
        disconnect();
    }
    
    /**
     * 接続を開始
     * @return 成功時true、失敗時false
     */
    bool connect() {
        if (connection_state_ == connection_state::connected) {
            return true; // 既に接続済み
        }
        
        // 接続中状態に設定
        connection_state_ = connection_state::connecting;
        
        try {
            // 以前のソケットがあれば閉じる
            if (socket_) {
                disconnect();
            }
            
            // 新しいUDPソケットを作成
            socket_ = new UdpTransmitSocket(
                IpEndpointName(config_.host.c_str(), config_.port_out)
            );
            
            // 接続状態を設定
            connection_state_ = connection_state::connected;
            last_error_ = error_info{ osc_error_code::none, "" };
            
            // 実行フラグを設定
            running_ = true;
            
            return true;
        }
        catch (const std::exception& e) {
            // エラー情報を設定
            last_error_ = error_info{
                osc_error_code::connection_failed,
                std::string("Connection failed: ") + e.what()
            };
            
            // 接続状態を更新
            connection_state_ = connection_state::error;
            
            // エラーハンドラを呼び出し
            if (error_handler_) {
                error_handler_(last_error_);
            }
            
            return false;
        }
    }
    
    /**
     * 接続を閉じる
     */
    void disconnect() {
        // スレッドを停止
        running_ = false;
        
        // ソケットを解放
        if (socket_) {
            delete socket_;
            socket_ = nullptr;
        }
        
        // 接続状態を更新
        connection_state_ = connection_state::disconnected;
    }
    
    /**
     * OSCメッセージを送信
     * @param address OSCアドレスパターン
     * @param args 引数リスト
     * @return 成功時true、失敗時false
     */
    bool send(const std::string& address, const atoms& args) {
        return send_message({ address, args });
    }
    
    /**
     * OSCメッセージを送信
     * @param msg 送信するメッセージ
     * @return 成功時true、失敗時false
     */
    bool send_message(const message& msg) {
        // 接続状態を確認
        if (connection_state_ != connection_state::connected) {
            // 自動再接続を試みる
            if (!connect()) {
                return false;
            }
        }
        
        try {
            // バッファを準備
            char buffer[config_.buffer_size];
            osc::OutboundPacketStream packet(buffer, config_.buffer_size);
            
            // メッセージの作成を開始
            packet << osc::BeginMessage(msg.address.c_str());
            
            // 各引数を適切な型でパケットに追加
            for (const auto& arg : msg.args) {
                if (arg.is_int()) {
                    packet << arg.as_int();
                }
                else if (arg.is_float()) {
                    packet << arg.as_float();
                }
                else if (arg.is_string()) {
                    packet << arg.as_string().c_str();
                }
                // TODO: 他の型もサポートする（配列、ブール値など）
            }
            
            // メッセージの終了
            packet << osc::EndMessage;
            
            // メッセージの送信
            socket_->Send(packet.Data(), packet.Size());
            
            // 最後のエラーをクリア
            last_error_ = error_info{ osc_error_code::none, "" };
            
            return true;
        }
        catch (const std::exception& e) {
            // エラー情報を設定
            last_error_ = error_info{
                osc_error_code::send_failed,
                std::string("Send failed: ") + e.what()
            };
            
            // エラーハンドラを呼び出し
            if (error_handler_) {
                error_handler_(last_error_);
            }
            
            return false;
        }
    }
    
    /**
     * エラーハンドラを設定
     * @param handler エラー発生時に呼び出される関数
     */
    void set_error_handler(error_handler handler) {
        error_handler_ = handler;
    }
    
    /**
     * 接続状態を取得
     * @return 現在の接続状態
     */
    connection_state get_connection_state() const {
        return connection_state_;
    }
    
    /**
     * 最後に発生したエラー情報を取得
     * @return エラー情報
     */
    const error_info& get_last_error() const {
        return last_error_;
    }
    
    /**
     * 接続設定を取得
     * @return 現在の接続設定
     */
    const connection_config& get_config() const {
        return config_;
    }
    
    /**
     * 接続設定を更新
     * @param config 新しい接続設定
     * @param reconnect 設定更新後に再接続するかどうか
     * @return 成功時true、失敗時false
     */
    bool update_config(const connection_config& config, bool reconnect = true) {
        // 一旦切断
        disconnect();
        
        // 設定を更新
        config_ = config;
        
        // 再接続が必要な場合
        if (reconnect) {
            return connect();
        }
        
        return true;
    }
    
private:
    connection_config config_;       // 接続設定
    UdpTransmitSocket* socket_;      // UDPソケット
    std::atomic<bool> running_;      // 実行フラグ
    std::atomic<connection_state> connection_state_;  // 接続状態
    error_info last_error_;          // 最後のエラー
    error_handler error_handler_;    // エラーハンドラ
};

} // namespace osc
} // namespace mcp
