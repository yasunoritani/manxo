#pragma once

#include "c74_min.h"
#include "mcp.osc_types.hpp"
#include <string>
#include <functional>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <set>
#include <libwebsockets.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <nlohmann/json.hpp>

// JSON操作用エイリアス
using json = nlohmann::json;

using namespace c74::min;

namespace mcp {
namespace osc {

// 転送用メッセージ構造体
struct ServerMessage {
    std::string payload;
    size_t len;
    bool binary;
};

// クライアント情報構造体
struct ClientData {
    struct lws* wsi;
    std::string id;
    std::queue<ServerMessage> outgoing_messages;
};

/**
 * WebSocketサーバークラス
 * OSC over WebSocketをサポートし、MCPプロトコルと連携
 * LLM（Claude Desktop）とMaxの間の通信を実現
 */
class websocket_server {
public:
    /**
     * コンストラクタ
     * @param output メッセージ出力用のMin outlet
     * @param error_out エラー出力用のMin outlet
     */
    websocket_server(outlet<>& output, outlet<>& error_out)
        : output_(output), error_out_(error_out), is_running_(false), should_stop_(false),
          context_(nullptr), port_(0), next_client_id_(0) {
        
        // libwebsocketsのログレベル設定
        lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);
    }
    
    /**
     * デストラクタ
     * サーバーを停止し、リソースをクリーンアップ
     */
    ~websocket_server() {
        stop();
        if (context_) {
            lws_context_destroy(context_);
            context_ = nullptr;
        }
    }
    
    /**
     * サーバーを起動
     * @param port ポート番号（デフォルト8080）
     * @param secure SSL/TLS使用の有無（デフォルトfalse）
     * @return 起動成功の場合はtrue
     */
    bool start(int port = 8080, bool secure = false) {
        if (is_running_) {
            error_out_.send("server_already_running");
            return false;
        }
        
        port_ = port;
        use_ssl_ = secure;
        
        // libwebsocketsコンテキスト作成
        struct lws_context_creation_info info;
        memset(&info, 0, sizeof(info));
        
        info.port = port_;
        info.protocols = get_protocols();
        info.gid = -1;
        info.uid = -1;
        info.user = this; // ユーザーデータとして自身のポインタを設定
        
        // SSL/TLS設定
        if (use_ssl_) {
            info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
            // 証明書ファイルなどは実際の環境に合わせて設定する必要あり
            // info.ssl_cert_filepath = "path/to/cert.pem";
            // info.ssl_private_key_filepath = "path/to/key.pem";
        }
        
        // コンテキスト作成
        context_ = lws_create_context(&info);
        if (!context_) {
            error_out_.send("websocket_context_creation_failed");
            return false;
        }
        
        // サービススレッド起動
        should_stop_ = false;
        service_thread_ = std::thread([this]() {
            this->service_thread_func();
        });
        
        is_running_ = true;
        output_.send("server_started", port_);
        
        return true;
        
        // 実際のサーバー起動処理（将来的な実装）
        // 現時点ではダミーの実装
        port_ = port;
        is_running_ = true;
        
        // サーバー起動を通知
        atoms server_info;
        server_info.push_back("websocket_server_started");
        server_info.push_back(port);
        output_.send(server_info);
        
        return true;
    }
    
    /**
     * サーバーを停止
     */
    void stop() {
        if (!is_running_) {
            return;
        }
        
        // 実際のサーバー停止処理（将来的な実装）
        is_running_ = false;
        
        // サーバー停止を通知
        atoms server_info;
        server_info.push_back("websocket_server_stopped");
        server_info.push_back(port_);
        output_.send(server_info);
    }
    
    /**
     * メッセージ送信（接続中のすべてのクライアントへ）
     * @param message 送信するメッセージ
     * @return 送信成功の場合はtrue
     */
    bool broadcast(const std::string& message) {
        if (!is_running_) {
            error_out_.send("server_not_running");
            return false;
        }
        
        // 実際の送信処理（将来的な実装）
        // 現時点ではダミーの実装
        atoms broadcast_info;
        broadcast_info.push_back("websocket_broadcast");
        broadcast_info.push_back(message);
        output_.send(broadcast_info);
        
        return true;
    }
    
    /**
     * OSCメッセージのWebSocket送信（接続中のすべてのクライアントへ）
     * @param address OSCアドレス
     * @param args OSC引数
     * @return 送信成功の場合はtrue
     */
    bool broadcast_osc(const std::string& address, const atoms& args) {
        if (!is_running_) {
            error_out_.send("server_not_running");
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
        
        return broadcast(json_msg);
    }
    
    /**
     * メッセージハンドラの登録
     * @param handler メッセージハンドラ関数
     */
    using message_handler = std::function<void(const std::string& client_id, const std::string& message)>;
    
    void set_message_handler(message_handler handler) {
        message_handler_ = handler;
    }
    
    /**
     * 接続ハンドラの登録
     * @param handler 接続ハンドラ関数
     */
    using connection_handler = std::function<void(const std::string& client_id)>;
    
    void set_connection_handler(connection_handler handler) {
        connection_handler_ = handler;
    }
    
    /**
     * 切断ハンドラの登録
     * @param handler 切断ハンドラ関数
     */
    using disconnection_handler = std::function<void(const std::string& client_id)>;
    
    void set_disconnection_handler(disconnection_handler handler) {
        disconnection_handler_ = handler;
    }
    
    /**
     * サーバー実行状態の取得
     * @return 実行中の場合はtrue
     */
    bool is_running() const {
        return is_running_;
    }
    
    /**
     * 接続中のクライアント数の取得
     * @return クライアント数
     */
    size_t client_count() const {
        // 実際の実装では接続中のクライアント数を返す
        // 現時点ではダミーの実装
        return 0;
    }

private:
    outlet<>& output_;
    outlet<>& error_out_;
    std::atomic<bool> is_running_;
    int port_;
    message_handler message_handler_;
    connection_handler connection_handler_;
    disconnection_handler disconnection_handler_;
};

} // namespace osc
} // namespace mcp
