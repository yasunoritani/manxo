#pragma once

#include "c74_min.h"
#include "mcp.osc_types.hpp"
#include <string>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <queue>
#include <libwebsockets.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <nlohmann/json.hpp>

// JSON操作用エイリアス
using json = nlohmann::json;

using namespace c74::min;

namespace mcp {
namespace osc {

/**
 * WebSocketクライアントクラス
 * OSC over WebSocketをサポートし、MCPプロトコルと連携
 */
// メッセージ格納用の構造体
struct WebSocketMessage {
    std::string payload;
    size_t len;
    bool binary;
};

class websocket_client {
public:
    /**
     * コンストラクタ
     * @param output メッセージ出力用のMin outlet
     * @param error_out エラー出力用のMin outlet
     */
    websocket_client(outlet<>& output, outlet<>& error_out)
        : output_(output), error_out_(error_out), is_connected_(false), should_stop_(false),
          context_(nullptr), wsi_(nullptr), port_(0), use_ssl_(false) {
        
        // libwebsocketsのログレベル設定
        lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);
    }
    
    /**
     * デストラクタ
     * 接続を終了し、スレッドをクリーンアップ
     */
    ~websocket_client() {
        disconnect();
        if (context_) {
            lws_context_destroy(context_);
            context_ = nullptr;
        }
    }
    
    /**
     * WebSocketサーバーに接続
     * @param url 接続先URL (例: "ws://localhost:8080" または "wss://secure.example.com:443")
     * @param protocols 使用するサブプロトコル (例: "osc")
     * @return 接続成功の場合はtrue
     */
    bool connect(const std::string& url, const std::string& protocols = "osc") {
        if (is_connected_) {
            disconnect();
        }
        
        // URLを解析してホスト、ポート、パスを取得
        std::string scheme, host, path;
        int port;
        
        if (!parse_url(url, scheme, host, port, path)) {
            error_out_.send("websocket_invalid_url", symbol(url));
            return false;
        }
        
        // スキームからSSL使用の有無を判断
        use_ssl_ = (scheme == "wss");
        host_ = host;
        port_ = port;
        path_ = path;
        protocols_ = protocols;
        
        // libwebsocektsのコンテキスト作成
        struct lws_context_creation_info info;
        memset(&info, 0, sizeof(info));
        
        info.port = CONTEXT_PORT_NO_LISTEN;
        info.protocols = get_protocols();
        info.gid = -1;
        info.uid = -1;
        info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        
        // コンテキスト作成
        context_ = lws_create_context(&info);
        if (!context_) {
            error_out_.send("websocket_context_creation_failed");
            return false;
        }
        
        // 接続情報の設定
        struct lws_client_connect_info conn_info;
        memset(&conn_info, 0, sizeof(conn_info));
        
        conn_info.context = context_;
        conn_info.address = host_.c_str();
        conn_info.port = port_;
        conn_info.path = path_.c_str();
        conn_info.host = host_.c_str();
        conn_info.origin = host_.c_str();
        conn_info.ssl_connection = use_ssl_ ? LCCSCF_USE_SSL : 0;
        conn_info.protocol = protocols_.c_str();
        conn_info.pwsi = &wsi_;
        conn_info.userdata = this;
        
        // 接続開始
        wsi_ = lws_client_connect_via_info(&conn_info);
        if (!wsi_) {
            error_out_.send("websocket_connection_failed");
            if (context_) {
                lws_context_destroy(context_);
                context_ = nullptr;
            }
            return false;
        }
        
        // サービススレッド起動
        should_stop_ = false;
        service_thread_ = std::thread([this]() {
            this->service_thread_func();
        });
        
        return true;
        
        // 実際の接続処理（将来的な実装）
        // 現時点ではダミーの実装
        is_connected_ = true;
        url_ = url;
        protocols_ = protocols;
        
        // 接続成功を通知
        atoms connection_info;
        connection_info.push_back("websocket_connected");
        connection_info.push_back(url);
        output_.send(connection_info);
        
        // 受信スレッド開始
        should_stop_ = false;
        receive_thread_ = std::thread(&websocket_client::receive_loop, this);
        
        return true;
    }
    
    /**
     * 接続を終了
     */
    void disconnect() {
        if (!is_connected_) {
            return;
        }
        
        // スレッドを停止
        should_stop_ = true;
        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }
        
        // 実際の切断処理（将来的な実装）
        is_connected_ = false;
        
        // 切断を通知
        atoms disconnect_info;
        disconnect_info.push_back("websocket_disconnected");
        disconnect_info.push_back(url_);
        output_.send(disconnect_info);
    }
    
    /**
     * メッセージ送信
     * @param message 送信するメッセージ
     * @return 送信成功の場合はtrue
     */
    bool send_message(const std::string& message) {
        if (!is_connected_) {
            error_out_.send("websocket_not_connected");
            return false;
        }
        
        // 実際の送信処理（将来的な実装）
        // 現時点ではダミーの実装でエコーバック
        std::unique_lock<std::mutex> lock(queue_mutex_);
        message_queue_.push(message);
        lock.unlock();
        condition_.notify_one();
        
        return true;
    }
    
    /**
     * OSCメッセージのWebSocket送信
     * @param address OSCアドレス
     * @param args OSC引数
     * @return 送信成功の場合はtrue
     */
    bool send_osc(const std::string& address, const atoms& args) {
        if (!is_connected_) {
            error_out_.send("websocket_not_connected");
            return false;
        }
        
        // OSCメッセージをJSON形式に変換（MCP互換）
        std::string json_msg = "{\"address\":\"" + address + "\",\"args\":[";
        
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) json_msg += ",";
            
            const atom& arg = args[i];
            if (arg.type() == message_type::float_argument) {
                json_msg += std::to_string(arg);
            } else if (arg.type() == message_type::int_argument) {
                json_msg += std::to_string(static_cast<int>(arg));
            } else {
                json_msg += "\"" + static_cast<std::string>(arg) + "\"";
            }
        }
        
        json_msg += "]}";
        
        return send_message(json_msg);
    }
    
    /**
     * 接続状態の取得
     * @return 接続中の場合はtrue
     */
    bool is_connected() const {
        return is_connected_;
    }

private:
    /**
     * 受信ループ（別スレッドで実行）
     */
    void receive_loop() {
        while (!should_stop_) {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            
            // キューにメッセージがあるか、停止フラグがセットされるまで待機
            condition_.wait(lock, [this] {
                return !message_queue_.empty() || should_stop_;
            });
            
            // 停止フラグがセットされていたら終了
            if (should_stop_) {
                break;
            }
            
            // キューからメッセージを取得して処理
            if (!message_queue_.empty()) {
                std::string message = message_queue_.front();
                message_queue_.pop();
                lock.unlock();
                
                // メッセージを処理（JSON解析など）
                process_received_message(message);
            } else {
                lock.unlock();
            }
        }
    }
    
    /**
     * 受信メッセージの処理
     * @param message 受信したメッセージ
     */
    void process_received_message(const std::string& message) {
        // 実際のメッセージ処理（将来的な実装）
        // 現時点ではダミーの実装
        atoms received;
        received.push_back("websocket_received");
        received.push_back(message);
        output_.send(received);
    }
    
private:
    outlet<>& output_;
    outlet<>& error_out_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> should_stop_;
    std::string url_;
    std::string protocols_;
    std::thread receive_thread_;
    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::queue<std::string> message_queue_;
};

} // namespace osc
} // namespace mcp
