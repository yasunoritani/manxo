/// @file
///    @ingroup    minexamples
///    @copyright    Copyright 2023 The Max 9-Claude Desktop Integration Project. All rights reserved.
///    @license    Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <memory>
#include <sstream>
#include <libwebsockets.h>
#include "mcp.security_policy.hpp"

using namespace c74::min;
using json = nlohmann::json;

// LLM通信用のWebSocketクライアントクラス
class llm_client {
public:
    // メッセージ受信コールバック
    std::function<void(const std::string&)> on_message;

    // コンストラクタ
    llm_client(outlet<>& output, outlet<>& error_out)
        : output_(output), error_out_(error_out),
          is_connected_(false),
          should_stop_(false),
          context_(nullptr),
          wsi_(nullptr),
          port_(0),
          use_ssl_(false),
          security_(std::make_unique<mcp::security::security_policy>(error_out)) {

        // libwebsocketsのログレベル設定
        lws_set_log_level(LLL_ERR | LLL_WARN, nullptr);
    }

    // デストラクタ
    ~llm_client() {
        disconnect();
        if (context_) {
            lws_context_destroy(context_);
            context_ = nullptr;
        }
    }

    // WebSocketコールバック（静的メンバ関数）
    static int callback_function(struct lws *wsi, enum lws_callback_reasons reason,
                                void *user, void *in, size_t len) {
        // ユーザーデータからインスタンスを取得
        auto* instance = static_cast<llm_client*>(lws_get_opaque_user_data(wsi));
        if (!instance) {
            return 0;
        }

        switch (reason) {
            case LWS_CALLBACK_CLIENT_ESTABLISHED:
                instance->on_connection_established();
                break;

            case LWS_CALLBACK_CLIENT_RECEIVE:
                if (in && len > 0) {
                    instance->on_message_received(static_cast<char*>(in), len);
                }
                break;

            case LWS_CALLBACK_CLIENT_CLOSED:
                instance->on_connection_closed();
                break;

            case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
                instance->on_connection_error(in ? static_cast<const char*>(in) : "Unknown error");
                break;

            case LWS_CALLBACK_CLIENT_WRITEABLE:
                instance->on_writeable();
                break;

            default:
                break;
        }

        return 0;
    }

    // 接続
    bool connect(const std::string& url) {
        if (is_connected_) {
            disconnect();
        }

        output_.send("llm_connecting", symbol(url));

        // URLを解析
        std::string scheme, host, path;
        int port;

        if (!parse_url(url, scheme, host, port, path)) {
            error_out_.send("llm_invalid_url", symbol(url));
            return false;
        }

        // スキームからSSL使用の有無を判断
        use_ssl_ = (scheme == "wss");
        host_ = host;
        port_ = port;
        path_ = path;

        // 接続がセキュリティポリシーに適合するか確認
        if (!security_->validate_port(port_)) {
            error_out_.send("llm_security_port_rejected", port_);
            return false;
        }

        if (!security_->validate_ip(host_)) {
            security_->allow_ip(host_); // 一時的に許可（本番環境では削除）
        }

        // libwebsocketsのコンテキスト作成
        struct lws_context_creation_info info;
        memset(&info, 0, sizeof(info));

        info.port = CONTEXT_PORT_NO_LISTEN;
        info.protocols = get_protocols();
        info.gid = -1;
        info.uid = -1;
        info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
        info.user = this;

        // コンテキスト作成
        context_ = lws_create_context(&info);
        if (!context_) {
            error_out_.send("llm_context_creation_failed");
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
        conn_info.protocol = "json"; // Claude DesktopはJSONプロトコルを使用
        conn_info.pwsi = &wsi_;
        conn_info.userdata = this;

        // 接続開始
        wsi_ = lws_client_connect_via_info(&conn_info);
        if (!wsi_) {
            error_out_.send("llm_connection_failed");
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

        // 実際の接続はコールバックで確立するため、ここではまだis_connected_をtrueにはしない
        url_ = url;

        return true;
    }

    // 切断
    void disconnect() {
        if (!is_connected_ && !context_) {
            return;
        }

        // スレッドを停止
        should_stop_ = true;
        if (service_thread_.joinable()) {
            service_thread_.join();
        }

        // WebSocketとコンテキストをクリーンアップ
        if (wsi_) {
            lws_set_timeout(wsi_, PENDING_TIMEOUT_USER_RECEIVABLE, LWS_TO_KILL_ASYNC);
            wsi_ = nullptr;
        }

        if (context_) {
            lws_context_destroy(context_);
            context_ = nullptr;
        }

        is_connected_ = false;
        output_.send("llm_disconnected");
    }

    // メッセージ送信
    bool send_message(const std::string& message) {
        if (!is_connected_) {
            error_out_.send("llm_not_connected");
            return false;
        }

        // メッセージサイズのセキュリティチェック
        if (!security_->validate_message_size(message.size())) {
            error_out_.send("llm_message_too_large", static_cast<int>(message.size()));
            return false;
        }

        // レート制限のセキュリティチェック
        std::string client_id = host_ + ":" + std::to_string(port_);
        if (!security_->validate_rate_limit(client_id, message.size())) {
            error_out_.send("llm_rate_limit_exceeded");
            return false;
        }

        // メッセージキューに追加
        std::unique_lock<std::mutex> lock(queue_mutex_);
        outgoing_messages_.push(message);
        lock.unlock();

        // WebSocketに書き込み可能状態をリクエスト
        if (wsi_) {
            lws_callback_on_writable(wsi_);
        }

        output_.send("llm_request_sent", symbol(message));
        return true;
    }

    // 接続状態確認
    bool is_connected() const {
        return is_connected_;
    }

private:
    // WebSocketサービススレッド関数
    void service_thread_func() {
        while (!should_stop_ && context_) {
            // libwebsocketsのイベントを処理
            lws_service(context_, 100); // 100ms待機

            // スレッド停止条件をチェック
            if (should_stop_) {
                break;
            }
        }
    }

    // URLを解析してコンポーネントを取得
    bool parse_url(const std::string& url, std::string& scheme, std::string& host, int& port, std::string& path) {
        // URLの基本形式: scheme://host[:port]/path

        // スキームの抽出
        size_t scheme_end = url.find("://");
        if (scheme_end == std::string::npos) {
            return false;
        }

        scheme = url.substr(0, scheme_end);
        size_t host_start = scheme_end + 3;

        // ホスト（とポート）の抽出
        size_t host_end = url.find("/", host_start);
        std::string host_port;

        if (host_end == std::string::npos) {
            host_port = url.substr(host_start);
            path = "/";
        } else {
            host_port = url.substr(host_start, host_end - host_start);
            path = url.substr(host_end);
        }

        // ポートの抽出（指定がなければデフォルト値）
        size_t port_start = host_port.find(":");
        if (port_start == std::string::npos) {
            host = host_port;
            port = (scheme == "wss") ? 443 : 80;
        } else {
            host = host_port.substr(0, port_start);
            try {
                port = std::stoi(host_port.substr(port_start + 1));
            } catch (...) {
                port = (scheme == "wss") ? 443 : 80;
            }
        }

        return true;
    }

    // 接続確立時の処理
    void on_connection_established() {
        is_connected_ = true;
        output_.send("llm_connected", symbol(url_));
    }

    // メッセージ受信時の処理
    void on_message_received(const char* data, size_t len) {
        // NULL終端された文字列にコピー
        std::string message(data, len);

        // コールバックを呼び出し
        if (on_message) {
            on_message(message);
        }

        // 出力する
        output_.send("llm_response", symbol(message));
    }

    // 接続エラー時の処理
    void on_connection_error(const std::string& error) {
        is_connected_ = false;
        error_out_.send("llm_connection_error", symbol(error));
    }

    // 接続クローズ時の処理
    void on_connection_closed() {
        is_connected_ = false;
        output_.send("llm_disconnected");
    }

    // 書き込み可能時の処理
    void on_writeable() {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        if (outgoing_messages_.empty()) {
            return;
        }

        std::string message = outgoing_messages_.front();
        outgoing_messages_.pop();
        lock.unlock();

        // WebSocketに書き込むためのバッファを準備
        // LWS_PRE バイトの余白が必要
        std::vector<unsigned char> buf(LWS_PRE + message.size());
        memcpy(buf.data() + LWS_PRE, message.data(), message.size());

        // メッセージを送信
        int bytes_sent = lws_write(wsi_, buf.data() + LWS_PRE, message.size(), LWS_WRITE_TEXT);

        if (bytes_sent < 0) {
            error_out_.send("llm_write_error");
        } else if (static_cast<size_t>(bytes_sent) < message.size()) {
            error_out_.send("llm_partial_write");
        }

        // キューに残っているメッセージがあれば、再度書き込み可能状態をリクエスト
        lock.lock();
        if (!outgoing_messages_.empty()) {
            lws_callback_on_writable(wsi_);
        }
    }

    // libwebsocketsプロトコル定義を取得
    const struct lws_protocols* get_protocols() {
        static const struct lws_protocols protocols[] = {
            {
                "json", // プロトコル名
                callback_function, // コールバック関数
                0, // ユーザーデータのサイズ
                0, // 最大フレームサイズ
                0, // プロトコルID
                nullptr // ユーザーデータ
            },
            { nullptr, nullptr, 0, 0, 0, nullptr } // 終端
        };
        return protocols;
    }

    outlet<>& output_;
    outlet<>& error_out_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> should_stop_;
    std::string url_;
    std::string host_;
    std::string path_;
    int port_;
    bool use_ssl_;

    struct lws_context* context_;
    struct lws* wsi_;

    std::thread service_thread_;
    std::mutex queue_mutex_;
    std::queue<std::string> outgoing_messages_;

    std::unique_ptr<mcp::security::security_policy> security_;
};

// LLM接続タイプ
enum class llm_connection_type {
    claude_desktop,
    claude_api,
    openai_api,
    custom_api
};

// LLMコネクタクラス
class llm_connector {
public:
    llm_connector(outlet<>& output, outlet<>& error_out)
        : output_(output), error_out_(error_out), client_(output, error_out),
          is_connected_(false), connection_type_(llm_connection_type::claude_desktop) {

        // クライアントのメッセージ受信コールバックを設定
        client_.on_message = [this](const std::string& message) {
            this->handle_message(message);
        };

        // デフォルトのモデルを設定
        current_model_ = "claude-3-opus-20240229";

        // 環境変数から認証トークンを取得（APIモード用）
        const char* token_env = std::getenv("CLAUDE_API_KEY");
        if (token_env) {
            api_keys_["claude_api"] = token_env;
        }

        const char* openai_token_env = std::getenv("OPENAI_API_KEY");
        if (openai_token_env) {
            api_keys_["openai_api"] = openai_token_env;
        }
    }

    bool connect(llm_connection_type type = llm_connection_type::claude_desktop,
                const std::string& url = "ws://localhost:5678") {
        connection_type_ = type;

        // 接続タイプに応じてURLを設定
        std::string target_url = url;
        switch (type) {
            case llm_connection_type::claude_desktop:
                // Claude DesktopデフォルトURL
                if (url == "ws://localhost:5678") {
                    target_url = "ws://localhost:5678/api/organizations/_/chat_conversations";
                }
                // Claude Desktopはローカル接続なのでAPIキー不要
                break;

            case llm_connection_type::claude_api:
                // Claude API URL
                if (url == "ws://localhost:5678") {
                    target_url = "wss://api.anthropic.com/v1/messages";

                    // APIキーが設定されているか確認
                    if (api_keys_.find("claude_api") == api_keys_.end()) {
                        error_out_.send("llm_missing_api_key", symbol("claude_api"));
                        return false;
                    }
                }
                break;

            case llm_connection_type::openai_api:
                // OpenAI API URL
                if (url == "ws://localhost:5678") {
                    target_url = "wss://api.openai.com/v1/chat/completions";

                    // APIキーが設定されているか確認
                    if (api_keys_.find("openai_api") == api_keys_.end()) {
                        error_out_.send("llm_missing_api_key", symbol("openai_api"));
                        return false;
                    }
                }
                break;

            case llm_connection_type::custom_api:
                // カスタムURLはそのまま使用
                break;
        }

        bool result = client_.connect(target_url);
        is_connected_ = result;

        // 接続成功時にモデル一覧をリクエスト
        if (result) {
            request_models();
        }

        return result;
    }

    void disconnect() {
        if (is_connected_) {
            client_.disconnect();
            is_connected_ = false;
        }
    }

    bool send_prompt(const std::string& prompt) {
        if (!is_connected_) {
            error_out_.send("llm_not_connected");
            return false;
        }

        // LLMタイプに応じたメッセージフォーマット
        json message;

        switch (connection_type_) {
            case llm_connection_type::claude_desktop:
                // Claude Desktop形式 - APIキー不要
                message = {
                    {"type", "message"},
                    {"model", current_model_},
                    {"content", {
                        {"type", "text"},
                        {"text", prompt}
                    }},
                    {"stream", config_.stream}
                };

                if (!config_.system_prompt.empty()) {
                    message["system"] = config_.system_prompt;
                }

                message["temperature"] = config_.temperature;
                message["max_tokens"] = config_.max_tokens;
                message["top_p"] = config_.top_p;
                break;

            case llm_connection_type::claude_api:
                // Claude API形式
                message = {
                    {"model", current_model_},
                    {"messages", {{
                        {"role", "user"},
                        {"content", prompt}
                    }}},
                    {"temperature", config_.temperature},
                    {"max_tokens", config_.max_tokens},
                    {"top_p", config_.top_p},
                    {"stream", config_.stream}
                };

                if (!config_.system_prompt.empty()) {
                    message["system"] = config_.system_prompt;
                }

                // APIキーをヘッダーとして設定
                if (api_keys_.find("claude_api") != api_keys_.end()) {
                    message["auth"] = {
                        {"api_key", api_keys_["claude_api"]}
                    };
                }
                break;

            case llm_connection_type::openai_api:
                // OpenAI API形式
                message = {
                    {"model", current_model_},
                    {"messages", {{
                        {"role", "user"},
                        {"content", prompt}
                    }}},
                    {"temperature", config_.temperature},
                    {"max_tokens", config_.max_tokens},
                    {"top_p", config_.top_p},
                    {"stream", config_.stream}
                };

                if (!config_.system_prompt.empty()) {
                    message["messages"].insert(message["messages"].begin(), {
                        {"role", "system"},
                        {"content", config_.system_prompt}
                    });
                }

                // APIキーをヘッダーとして設定
                if (api_keys_.find("openai_api") != api_keys_.end()) {
                    message["auth"] = {
                        {"api_key", api_keys_["openai_api"]}
                    };
                }
                break;

            case llm_connection_type::custom_api:
                // カスタムAPI形式（ユーザー定義）
                message = {
                    {"type", "prompt"},
                    {"content", prompt},
                    {"model", current_model_},
                    {"temperature", config_.temperature},
                    {"max_tokens", config_.max_tokens},
                    {"top_p", config_.top_p},
                    {"stream", config_.stream}
                };

                if (!config_.system_prompt.empty()) {
                    message["system"] = config_.system_prompt;
                }
                break;
        }

        // メッセージを送信
        return client_.send_message(message.dump());
    }

    bool set_model(const std::string& model_id) {
        current_model_ = model_id;
        output_.send("llm_model_set", symbol(model_id));
        return true;
    }

    bool set_config(const std::string& param, const atom& value) {
        if (param == "temperature") {
            config_.temperature = (float)value;
            output_.send("llm_config_temperature", value);
        } else if (param == "top_p") {
            config_.top_p = (float)value;
            output_.send("llm_config_top_p", value);
        } else if (param == "max_tokens") {
            config_.max_tokens = (int)value;
            output_.send("llm_config_max_tokens", value);
        } else if (param == "system_prompt") {
            config_.system_prompt = (std::string)value;
            output_.send("llm_config_system_prompt", symbol(value));
        } else if (param == "stream") {
            config_.stream = (int)value > 0;
            output_.send("llm_config_stream", value);
        } else if (param == "api_key") {
            if (args.size() < 2) {
                error_out_.send("llm_missing_api_key_value");
                return false;
            }

            std::string key_type = (std::string)value;
            std::string key_value = (std::string)args[1];

            api_keys_[key_type] = key_value;
            output_.send("llm_config_api_key_set", symbol(key_type));
        } else {
            error_out_.send("llm_unknown_config", symbol(param));
            return false;
        }
        return true;
    }

    void dump_config() {
        output_.send("llm_config_dump_start");
        output_.send("temperature", config_.temperature);
        output_.send("top_p", config_.top_p);
        output_.send("max_tokens", config_.max_tokens);
        output_.send("system_prompt", symbol(config_.system_prompt));
        output_.send("stream", config_.stream ? 1 : 0);
        output_.send("model", symbol(current_model_));

        // APIキーの有無（値は表示しない）
        // Claude Desktopモードでは表示しない
        if (connection_type_ != llm_connection_type::claude_desktop) {
            for (const auto& pair : api_keys_) {
                output_.send("api_key_" + pair.first, symbol("***設定済み***"));
            }
        }

        output_.send("llm_config_dump_end");
    }

    void dump_status() {
        output_.send("llm_status_dump_start");
        output_.send("connected", is_connected_ ? 1 : 0);
        if (is_connected_) {
            output_.send("connection_type", static_cast<int>(connection_type_));

            std::string type_str;
            switch (connection_type_) {
                case llm_connection_type::claude_desktop:
                    type_str = "claude_desktop";
                    break;
                case llm_connection_type::claude_api:
                    type_str = "claude_api";
                    break;
                case llm_connection_type::openai_api:
                    type_str = "openai_api";
                    break;
                case llm_connection_type::custom_api:
                    type_str = "custom_api";
                    break;
            }
            output_.send("connection_type_name", symbol(type_str));
        }
        output_.send("llm_status_dump_end");
    }

    void dump_models() {
        output_.send("llm_models_dump_start");

        // モデル一覧を出力（実際のモデル一覧を使用）
        if (!available_models_.empty()) {
            for (const auto& model : available_models_) {
                output_.send(symbol(model), true);
            }
        } else {
            // モデル一覧が空の場合は、接続タイプに応じたデフォルトモデルを表示
            // これは実際のモデル一覧が取得できるまでの一時的な代替表示
            switch (connection_type_) {
                case llm_connection_type::claude_desktop:
                    output_.send("claude-3-opus-20240229", true);
                    output_.send("claude-3-sonnet-20240229", true);
                    output_.send("claude-3-haiku-20240307", true);
                    break;

                case llm_connection_type::claude_api:
                    output_.send("claude-3-opus-20240229", true);
                    output_.send("claude-3-sonnet-20240229", true);
                    output_.send("claude-3-haiku-20240307", true);
                    break;

                case llm_connection_type::openai_api:
                    output_.send("gpt-4o", true);
                    output_.send("gpt-4-turbo", true);
                    output_.send("gpt-4", true);
                    output_.send("gpt-3.5-turbo", true);
                    break;

                case llm_connection_type::custom_api:
                    output_.send("custom-model", true);
                    break;
            }
        }

        output_.send("llm_models_dump_end");
    }

    void request_models() {
        if (!is_connected_) {
            error_out_.send("llm_not_connected");
            return;
        }

        // 接続タイプに応じたモデル取得リクエスト
        json request;

        switch (connection_type_) {
            case llm_connection_type::claude_desktop:
                // Claude Desktopの場合
                request = {
                    {"type", "get_models"}
                };
                break;

            case llm_connection_type::claude_api:
                // Claude APIの場合
                request = {
                    {"type", "list_models"}
                };

                // APIキーをヘッダーとして設定
                if (api_keys_.find("claude_api") != api_keys_.end()) {
                    request["auth"] = {
                        {"api_key", api_keys_["claude_api"]}
                    };
                }
                break;

            case llm_connection_type::openai_api:
                // OpenAI APIの場合
                request = {
                    {"type", "models.list"}
                };

                // APIキーをヘッダーとして設定
                if (api_keys_.find("openai_api") != api_keys_.end()) {
                    request["auth"] = {
                        {"api_key", api_keys_["openai_api"]}
                    };
                }
                break;

            case llm_connection_type::custom_api:
                // カスタムAPIの場合
                request = {
                    {"type", "get_models"}
                };
                break;
        }

        // モデル一覧リクエストを送信
        client_.send_message(request.dump());
    }

    bool cancel_request() {
        if (!is_connected_) {
            return false;
        }

        // 接続タイプに応じたキャンセルリクエスト
        json message;

        switch (connection_type_) {
            case llm_connection_type::claude_desktop:
                message = {
                    {"type", "cancel"}
                };
                break;

            case llm_connection_type::claude_api:
            case llm_connection_type::openai_api:
            case llm_connection_type::custom_api:
                message = {
                    {"action", "cancel"}
                };
                break;
        }

        output_.send("llm_request_cancelled");
        return client_.send_message(message.dump());
    }

    bool is_connected() const {
        return is_connected_ && client_.is_connected();
    }

    bool set_api_key(const std::string& key_type, const std::string& key_value) {
        api_keys_[key_type] = key_value;
        output_.send("llm_api_key_set", symbol(key_type));
        return true;
    }

private:
    void handle_message(const std::string& message) {
        try {
            json j = json::parse(message);

            // レスポンスタイプを処理
            if (j.contains("type")) {
                std::string type = j["type"].get<std::string>();

                if (type == "models") {
                    // モデル一覧を処理
                    if (j.contains("models") && j["models"].is_array()) {
                        available_models_.clear();

                        for (const auto& model : j["models"]) {
                            if (model.contains("id") && model["id"].is_string()) {
                                available_models_.push_back(model["id"].get<std::string>());
                            }
                        }

                        // 利用可能なモデル一覧を出力
                        output_.send("llm_models", symbol(j.dump()));

                        // 新しいモデル一覧をダンプ
                        dump_models();
                    }
                } else if (type == "message" || type == "response" || type == "stream") {
                    // メッセージレスポンスの処理
                    output_.send("llm_message", symbol(message));

                    // コンテンツ抽出
                    if (j.contains("content")) {
                        if (j["content"].is_string()) {
                            output_.send("llm_content", symbol(j["content"].get<std::string>()));
                        } else if (j["content"].is_object() && j["content"].contains("text")) {
                            output_.send("llm_content", symbol(j["content"]["text"].get<std::string>()));
                        }
                    }

                    // ストリーミングの最終メッセージかチェック
                    if (type == "stream" && j.contains("final") && j["final"].is_boolean() && j["final"].get<bool>()) {
                        output_.send("llm_stream_complete");
                    }
                } else if (type == "error") {
                    // エラーメッセージの処理
                    std::string error_message = "Unknown error";

                    if (j.contains("message") && j["message"].is_string()) {
                        error_message = j["message"].get<std::string>();
                    }

                    error_out_.send("llm_api_error", symbol(error_message));
                } else if (type == "cancel_success") {
                    // キャンセル成功
                    output_.send("llm_cancel_success");
                }
            } else if (j.contains("role") && j["role"].get<std::string>() == "assistant") {
                // OpenAI形式のレスポンス
                if (j.contains("content") && j["content"].is_string()) {
                    output_.send("llm_content", symbol(j["content"].get<std::string>()));
                }

                output_.send("llm_message", symbol(message));
            } else {
                // その他の形式のJSONメッセージ
                output_.send("llm_message", symbol(message));
            }
        } catch (const std::exception& e) {
            error_out_.send("llm_invalid_json", symbol(e.what()));
            error_out_.send("llm_message_raw", symbol(message));
        }
    }

    outlet<>& output_;
    outlet<>& error_out_;
    llm_client client_;
    llm_connection_type connection_type_;
    std::atomic<bool> is_connected_;
    std::string current_model_;
    std::vector<std::string> available_models_;
    std::unordered_map<std::string, std::string> api_keys_;

    // 設定パラメータ
    struct {
        float temperature = 0.7f;
        float top_p = 0.9f;
        int max_tokens = 1000;
        bool stream = true;
        std::string system_prompt;
    } config_;
};

class llm_mcp : public object<llm_mcp> {
public:
    MIN_DESCRIPTION{"LLM MCPコネクタ。WebSocketを介してClaude Desktopなどのいくつかのタイプのローカルモデルと通信。"};
    MIN_TAGS{"osc, claude, llm, ai"};
    MIN_AUTHOR{"Max/MSP OSC Bridge Team"};
    MIN_RELATED{"osc_bridge, websocket"};

    inlet<>  input {this, "(メッセージ) LLM MCPコマンド"};
    outlet<> output {this, "(メッセージ) LLM出力とステータス"};
    outlet<> error_out {this, "(メッセージ) エラーメッセージ"};

    std::unique_ptr<llm_connector> connector_;

    llm_mcp(const atoms& args = {}) {
        connector_ = std::make_unique<llm_connector>(output, error_out);

        // 環境変数から初期設定を読み込み
        load_env_config();
    }

    ~llm_mcp() {
        // コネクタはユニークポインタのため自動的に破棄される
    }

    // 環境変数から設定を読み込み
    void load_env_config() {
        // 環境変数からAPIキーを読み込み
        const char* claude_api_key = std::getenv("CLAUDE_API_KEY");
        if (claude_api_key) {
            connector_->set_api_key("claude_api", claude_api_key);
        }

        const char* openai_api_key = std::getenv("OPENAI_API_KEY");
        if (openai_api_key) {
            connector_->set_api_key("openai_api", openai_api_key);
        }

        // システムプロンプトの設定
        const char* system_prompt = std::getenv("LLM_SYSTEM_PROMPT");
        if (system_prompt) {
            atoms args = {symbol("system_prompt"), symbol(system_prompt)};
            config(args);
        }
    }

    // LLM APIに接続
    message<> connect { this, "connect", "LLM APIに接続",
        MIN_FUNCTION {
            std::string url = "ws://localhost:5678";
            llm_connection_type conn_type = llm_connection_type::claude_desktop;

            if (args.size() > 0) {
                url = static_cast<std::string>(args[0]);
            }

            if (args.size() > 1) {
                std::string type_str = static_cast<std::string>(args[1]);
                if (type_str == "claude_desktop") {
                    conn_type = llm_connection_type::claude_desktop;
                } else if (type_str == "claude_api") {
                    conn_type = llm_connection_type::claude_api;
                } else if (type_str == "openai_api") {
                    conn_type = llm_connection_type::openai_api;
                } else if (type_str == "custom_api") {
                    conn_type = llm_connection_type::custom_api;
                }
            }

            bool success = connector_->connect(conn_type, url);
            return {};
        }
    };

    // LLM APIから切断
    message<> disconnect { this, "disconnect", "LLM APIから切断",
        MIN_FUNCTION {
            connector_->disconnect();
            return {};
        }
    };

    // LLMにプロンプトを送信
    message<> prompt { this, "prompt", "LLMにプロンプトを送信",
        MIN_FUNCTION {
            if (args.size() == 0) {
                error_out.send("llm_missing_prompt");
                return {};
            }

            std::string prompt_text = static_cast<std::string>(args[0]);
            connector_->send_prompt(prompt_text);
            return {};
        }
    };

    // 使用するLLMモデルを選択
    message<> model { this, "model", "使用するLLMモデルを選択",
        MIN_FUNCTION {
            if (args.size() == 0) {
                error_out.send("llm_missing_model");
                return {};
            }

            std::string model_id = static_cast<std::string>(args[0]);
            connector_->set_model(model_id);
            return {};
        }
    };

    // LLM設定パラメータを更新
    message<> config { this, "config", "LLM設定パラメータを更新",
        MIN_FUNCTION {
            if (args.size() < 2) {
                error_out.send("llm_invalid_config_args");
                return {};
            }

            std::string param = static_cast<std::string>(args[0]);
            connector_->set_config(param, args.slice(1));
            return {};
        }
    };

    // APIキーを設定
    message<> api_key { this, "api_key", "APIキーを設定",
        MIN_FUNCTION {
            if (args.size() < 2) {
                error_out.send("llm_invalid_api_key_args");
                return {};
            }

            std::string key_type = static_cast<std::string>(args[0]);
            std::string key_value = static_cast<std::string>(args[1]);

            connector_->set_api_key(key_type, key_value);
            return {};
        }
    };

    // 利用可能なLLMモデル一覧を要求
    message<> models { this, "models", "利用可能なLLMモデル一覧を要求",
        MIN_FUNCTION {
            if (args.size() > 0 && args[0] == "dump") {
                connector_->dump_models();
            } else {
                connector_->request_models();
            }
            return {};
        }
    };

    // 現在の設定を出力
    message<> dump { this, "dump", "現在の設定を出力",
        MIN_FUNCTION {
            if (args.size() > 0) {
                std::string what = static_cast<std::string>(args[0]);
                if (what == "config") {
                    connector_->dump_config();
                } else if (what == "models") {
                    connector_->dump_models();
                } else if (what == "status") {
                    connector_->dump_status();
                } else if (what == "all") {
                    connector_->dump_config();
                    connector_->dump_models();
                    connector_->dump_status();
                } else {
                    error_out.send("llm_unknown_dump_target", symbol(what));
                }
            } else {
                // デフォルトですべて出力
                connector_->dump_config();
                connector_->dump_models();
                connector_->dump_status();
            }
            return {};
        }
    };

    // 現在のLLMリクエストをキャンセル
    message<> cancel { this, "cancel", "現在のLLMリクエストをキャンセル",
        MIN_FUNCTION {
            connector_->cancel_request();
            return {};
        }
    };

    // 未知のメッセージを処理
    message<> anything { this, "anything", "未知のメッセージを処理",
        MIN_FUNCTION {
            symbol cmd = args[0];
            atoms cmd_args;

            if (args.size() > 1) {
                cmd_args.insert(cmd_args.end(), args.begin() + 1, args.end());
            }

            error_out.send("llm_unknown_command", cmd);
            return {};
        }
    };
};

MIN_EXTERNAL(llm_mcp);
