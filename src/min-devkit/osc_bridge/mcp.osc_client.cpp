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
     * @param retry_count 接続試行回数
     * @param retry_interval_ms リトライ間隔（ミリ秒）
     * @return 成功時true、失敗時false
     */
    bool connect(int retry_count = 3, int retry_interval_ms = 100) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (connection_state_ == connection_state::connected) {
            return true; // 既に接続済み
        }
        
        // 接続中状態に設定
        connection_state_ = connection_state::connecting;
        
        // リトライループ
        for (int attempt = 0; attempt <= retry_count; ++attempt) {
            try {
                // 以前のソケットがあれば閉じる
                if (socket_) {
                    disconnect_socket();
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
                
                // 接続成功ログ
                std::cout << "OSC client connected to " << config_.host << ":" << config_.port_out << std::endl;
                return true;
            }
            catch (const std::exception& e) {
                // 最後の試行でなければ待機して再試行
                if (attempt < retry_count) {
                    std::cerr << "Connection attempt " << (attempt + 1) << " failed: " << e.what() 
                              << ". Retrying in " << retry_interval_ms << "ms..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(retry_interval_ms));
                    continue;
                }
                
                // すべての試行が失敗した場合
                // エラー情報を設定
                last_error_ = error_info{
                    osc_error_code::connection_failed,
                    std::string("Connection failed after ") + std::to_string(retry_count + 1) +
                    " attempts: " + e.what()
                };
                
                // 接続状態を更新
                connection_state_ = connection_state::error;
                
                // 詳細なエラー情報をログ出力
                std::cerr << "OSC client connection failed: " << last_error_.message << std::endl;
                
                // エラーハンドラをスレッドセーフに呼び出し
                if (error_handler_) {
                    error_handler_(last_error_);
                }
                
                return false;
            }
            catch (...) {
                // 不明な例外の捕捉
                last_error_ = error_info{
                    osc_error_code::connection_failed,
                    "Unknown connection error occurred"
                };
                connection_state_ = connection_state::error;
                
                std::cerr << "OSC client: Unknown connection error" << std::endl;
                
                if (error_handler_) {
                    error_handler_(last_error_);
                }
                
                return false;
            }
        }
        
        return false; // ここには到達しないはず
    }
    
    /**
     * 接続を閉じる
     */
    void disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 実行フラグをクリア
        running_ = false;
        
        // 内部関数を使用して安全にソケットを閉じる
        disconnect_socket();
        
        // 接続状態を更新
        connection_state_ = connection_state::disconnected;
        
        std::cout << "OSC client disconnected from " << config_.host << ":" << config_.port_out << std::endl;
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
            ::osc::OutboundPacketStream packet(buffer, config_.buffer_size);
            
            // メッセージの作成を開始
            packet << ::osc::BeginMessage(msg.address.c_str());
            
            // 各引数を適切な型でパケットに追加
            for (const auto& arg : msg.args) {
                if (arg.a_type == c74::max::A_LONG) {
                    packet << static_cast<int>(arg);
                }
                else if (arg.a_type == c74::max::A_FLOAT) {
                    packet << static_cast<float>(arg);
                }
                else if (arg.a_type == c74::max::A_SYM) {
                    packet << static_cast<std::string>(arg).c_str();
                }
                else if (arg.a_type == c74::max::A_LONG) { // Boolとして扱う
                    packet << static_cast<bool>(arg);
                }
                else if (arg.a_type == c74::max::A_SYM) { // シンボルとして再処理
                    std::string sym = static_cast<std::string>(arg);
                    
                    if (sym == "nil") {
                        // nil値のOSC送信
                        packet << ::osc::Nil;
                    }
                    else if (sym == "infinitum") {
                        // 無限大のOSC送信
                        packet << ::osc::Infinitum;
                    }
                    else if (sym == "blob" && msg.args.size() > 1) {
                        // 次の引数がblobサイズを示す場合はスキップ
                        continue;
                    }
                    else {
                        // それ以外のシンボルは文字列として送信
                        packet << sym.c_str();
                    }
                }
            }
            
            // メッセージの終了
            packet << ::osc::EndMessage;
            
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
        // 原子変数なのでロックは不要
        return connection_state_;
    }
    
    /**
     * 最後に発生したエラー情報を取得
     * @return エラー情報
     */
    const error_info& get_last_error() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_;
    }
    
    /**
     * 接続設定を取得
     * @return 現在の接続設定
     */
    const connection_config& get_config() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }
    
    /**
     * 接続設定を更新
     * @param config 新しい接続設定
     * @param reconnect 設定更新後に再接続するかどうか
     * @return 成功時true、失敗時false
     */
    bool update_config(const connection_config& config, bool reconnect = true) {
        bool was_connected = false;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // 現在の接続状態を保存
            was_connected = (connection_state_ == connection_state::connected);
            
            // 一旦切断 — ロックは内部で取得される
            if (was_connected) {
                disconnect_socket();
            }
            
            // Max for Live環境の設定を延長する
            connection_config new_config = config;
            
            // 旧設定から特定の設定を引き継ぐ
            if (config_.m4l_compatibility && !new_config.m4l_compatibility) {
                std::cout << "Preserving M4L compatibility mode from previous config" << std::endl;
                new_config.m4l_compatibility = true;
            }
            
            // ポートが変更されている場合はログ出力
            if (config_.port_out != new_config.port_out) {
                std::cout << "Changing output port from " << config_.port_out 
                          << " to " << new_config.port_out << std::endl;
            }
            
            // 設定を更新
            config_ = new_config;
        }
        
        // 再接続が必要な場合
        if (reconnect && was_connected) {
            return connect();
        }
        
        return true;
    }
    
    /**
     * OSCメッセージ送信時のエラーハンドリングを強化
     * @param address 送信先アドレス
     * @param args 引数
     * @param retry_on_error エラー時に再試行するか
     * @return 成功時true
     */
    bool send_message_with_error_handling(const std::string& address, const atoms& args, bool retry_on_error = true) {
        try {
            return send(address, args);
        } catch (const std::exception& e) {
            // 送信失敗時のエラー処理
            error_info err = {osc_error_code::send_failed, std::string("Failed to send OSC message: ") + e.what()};
            std::cerr << "OSC client error: " << err.message << std::endl;
            
            if (error_handler_) {
                error_handler_(err);
            }
            
            // 接続が切れている場合は再接続を試みる
            if (retry_on_error && connection_state_ != connection_state::connected) {
                std::cout << "Connection appears to be broken, attempting to reconnect..." << std::endl;
                if (connect()) {
                    std::cout << "Successfully reconnected, retrying send..." << std::endl;
                    return send_message_with_error_handling(address, args, false); // 再試行は1回のみ
                }
            }
            
            return false;
        } catch (...) {
            // 不明なエラー
            error_info err = {osc_error_code::unknown_error, "Unknown error during OSC message send"};
            std::cerr << "OSC client unknown error during send" << std::endl;
            
            if (error_handler_) {
                error_handler_(err);
            }
            
            return false;
        }
    }

    /**
     * 安全にソケットを解放する内部関数
     * ロックは呼び出し側で取得済みであること
     */
    void disconnect_socket() {
        if (socket_) {
            try {
                delete socket_;
            } catch (const std::exception& e) {
                std::cerr << "Error during socket cleanup: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "Unknown error during socket cleanup" << std::endl;
            }
            socket_ = nullptr;
        }
    }

    // エラーハンドラを設定
    void set_error_handler(error_handler handler) {
        error_handler_ = handler;
    }
    
    // 接続状態を取得
    connection_state get_connection_state() const {
        return connection_state_;
    }
    
    // 最後のエラーを取得
    const error_info& get_last_error() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return last_error_;
    }
    
    // 接続設定を更新
    bool update_config(const connection_config& config, bool reconnect = true) {
        bool was_connected = false;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // 現在の接続状態を保存
            was_connected = (connection_state_ == connection_state::connected);
            
            // 一旦切断
            if (was_connected) {
                disconnect_socket();
            }
            
            // 設定を更新
            config_ = config;
        }
        
        // 再接続が必要な場合
        if (reconnect && was_connected) {
            return connect();
        }
        
        return true;
    }

private:
    connection_config config_;                  // 接続設定
    UdpTransmitSocket* socket_;                // UDPソケット
    std::atomic<bool> running_;                // 実行中フラグ
    std::atomic<connection_state> connection_state_; // 接続状態
    error_info last_error_;                    // 最後のエラー
    error_handler error_handler_;              // エラーハンドラ
    mutable std::mutex mutex_;                 // スレッド同期用ミューテックス
};

} // namespace osc
} // namespace mcp
