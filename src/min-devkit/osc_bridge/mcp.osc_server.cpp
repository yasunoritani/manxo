#include "mcp.osc_types.hpp"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"
#include "ip/UdpSocket.h"

#include <thread>
#include <atomic>
#include <mutex>
#include <chrono>

namespace mcp {
namespace osc {

/**
 * OSCパケットリスナークラス
 * 受信したOSCメッセージを処理する
 */
class packet_listener : public ::osc::OscPacketListener {
public:
    /**
     * コンストラクタ
     * @param registry ハンドラーレジストリ
     * @param error_handler エラーハンドラ
     */
    packet_listener(handler_registry& registry, error_handler error_handler)
        : registry_(registry), error_handler_(error_handler) {
    }
    
protected:
    /**
     * OSCメッセージ受信時の処理
     * oscpackの内部コールバック
     */
    virtual void ProcessMessage(const ::osc::ReceivedMessage& msg, 
                                const IpEndpointName& remote_endpoint) override {
        try {
            // アドレスパターンを取得
            std::string address = msg.AddressPattern();
            
            // 引数の抽出
            atoms args;
            ::osc::ReceivedMessage::const_iterator arg = msg.ArgumentsBegin();
            
            while (arg != msg.ArgumentsEnd()) {
                if (arg->IsInt32()) {
                    args.push_back(arg->AsInt32());
                }
                else if (arg->IsFloat()) {
                    args.push_back(arg->AsFloat());
                }
                else if (arg->IsString()) {
                    args.push_back(std::string(arg->AsString()));
                }
                else if (arg->IsBool()) {
                    args.push_back(arg->AsBool());
                }
                else if (arg->IsNil()) {
                    // nilはMax/Minではシンボル「nil」として扱う
                    args.push_back(symbol("nil"));
                }
                else if (arg->IsInfinitum()) {
                    // 無限大はMax/Minではシンボル「infinitum」として扱う
                    args.push_back(symbol("infinitum"));
                }
                else if (arg->IsBlob()) {
                    // BLOBデータは現状サポート外だが、将来の拡張性のために長さを通知
                    const void* blob_data = arg->AsBlob();
                    const osc::osc_bundle_element_size_t blob_size = arg->AsBlobSize();
                    
                    if (blob_size <= MAX_BLOB_SIZE) {
                        // シンプルなblobサイズの通知
                        args.push_back(symbol("blob"));
                        args.push_back(static_cast<int>(blob_size));
                    } else {
                        // 大きすぎるBLOBはセキュリティリスクとして警告
                        std::cerr << "Oversized blob received (" << blob_size << " bytes)" << std::endl;
                        args.push_back(symbol("blob_oversized"));
                    }
                }
                // 将来の型拡張のために予約：TimeTag, MIDI, etc.
                
                arg++;
            }
            
            // レジストリからアドレスに対応するハンドラを取得
            auto handler = registry_.get_handler(address);
            
            // ハンドラが登録されていれば実行
            if (handler) {
                handler(address, args);
            }
        }
        catch (::osc::Exception& e) {
            // エラー情報を設定
            error_info error{
                osc_error_code::receive_failed,
                std::string("Error parsing OSC message: ") + e.what()
            };
            
            // エラーハンドラを呼び出し
            if (error_handler_) {
                error_handler_(error);
            }
        }
    }
    
private:
    /**
     * 受信メッセージのセキュリティ検証
     * 不正なメッセージや大きすぎるメッセージをフィルタリング
     * 
     * @param address OSCアドレス
     * @param sender 送信元ホスト
     * @return 有効なメッセージならtrue
     */
    bool validate_message(const std::string& address, const std::string& sender) {
        // アドレスパターンの基本的な検証
        if (address.empty() || address[0] != '/') {
            std::cerr << "Invalid OSC address pattern: " << address << std::endl;
            return false;
        }
        
        // アドレスの最大長を制限
        if (address.length() > 255) {
            std::cerr << "OSC address pattern too long: " << address.length() << " chars" << std::endl;
            return false;
        }
        
        // NOTE: ここに追加のセキュリティチェックを実装可能
        // 例えば、特定のIPアドレスからのみメッセージを受信するなど
        
        return true;
    }

    // 定数
    static constexpr size_t MAX_BLOB_SIZE = 1024 * 1024; // 1MB

private:
    handler_registry& registry_;     // ハンドラレジストリ
    error_handler error_handler_;    // エラーハンドラ
    bool debug_mode_ = false;        // デバッグモード
};

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
    server(const connection_config& config = connection_config{})
        : config_(config), 
          socket_(nullptr),
          listener_(nullptr),
          receive_thread_(nullptr),
          running_(false),
          connection_state_(connection_state::disconnected) {
        
        // ハンドラレジストリを初期化
        registry_ = std::make_unique<handler_registry>();
    }
    
    /**
     * デストラクタ
     * 接続を閉じる
     */
    ~server() {
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
            // 以前の接続があれば閉じる
            if (running_) {
                disconnect();
            }
            
            // パケットリスナーを作成
            listener_ = new packet_listener(*registry_, error_handler_);
            
            // UDPソケットを作成
            socket_ = new UdpListeningReceiveSocket(
                IpEndpointName(IpEndpointName::ANY_ADDRESS, config_.port_in),
                listener_
            );
            
            // 受信スレッドを起動
            running_ = true;
            receive_thread_ = new std::thread([this]() {
                try {
                    // 接続状態を更新
                    connection_state_ = connection_state::connected;
                    
                    // ソケットの受信ループを開始
                    while (running_) {
                        socket_->Run();
                        
                        // 短い間隔でループをチェック
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
                catch (const std::exception& e) {
                    // エラー情報を設定
                    last_error_ = error_info{
                        osc_error_code::receive_failed,
                        std::string("Receive thread error: ") + e.what()
                    };
                    
                    // 接続状態を更新
                    connection_state_ = connection_state::error;
                    
                    // エラーハンドラを呼び出し
                    if (error_handler_) {
                        error_handler_(last_error_);
                    }
                }
            });
            
            // 最後のエラーをクリア
            last_error_ = error_info{ osc_error_code::none, "" };
            
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
        
        // 受信スレッドがあれば終了を待機
        if (receive_thread_) {
            receive_thread_->join();
            delete receive_thread_;
            receive_thread_ = nullptr;
        }
        
        // ソケットを解放
        if (socket_) {
            socket_->Break();
            delete socket_;
            socket_ = nullptr;
        }
        
        // リスナーを解放
        if (listener_) {
            delete listener_;
            listener_ = nullptr;
        }
        
        // 接続状態を更新
        connection_state_ = connection_state::disconnected;
    }
    
    /**
     * OSCメッセージハンドラを登録
     * @param pattern OSCアドレスパターン
     * @param handler メッセージハンドラ
     */
    void register_handler(const std::string& pattern, message_handler handler) {
        registry_->register_handler(pattern, handler);
    }
    
    /**
     * OSCメッセージハンドラを削除
     * @param pattern OSCアドレスパターン
     */
    void unregister_handler(const std::string& pattern) {
        registry_->unregister_handler(pattern);
    }
    
    /**
     * すべてのハンドラを削除
     */
    void clear_handlers() {
        registry_->clear();
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
    connection_config config_;         // 接続設定
    UdpListeningReceiveSocket* socket_; // UDPソケット
    packet_listener* listener_;        // OSCパケットリスナー
    std::thread* receive_thread_;      // 受信スレッド
    std::atomic<bool> running_;        // 実行フラグ
    std::atomic<connection_state> connection_state_;  // 接続状態
    error_info last_error_;            // 最後のエラー
    error_handler error_handler_;      // エラーハンドラ
    std::unique_ptr<handler_registry> registry_;  // ハンドラレジストリ
};

} // namespace osc
} // namespace mcp
