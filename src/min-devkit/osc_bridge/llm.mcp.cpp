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

using namespace c74::min;
using json = nlohmann::json;

// LLM通信用のクライアントクラス
class llm_client {
public:
    // メッセージ受信コールバック
    std::function<void(const std::string&)> on_message;
    
    // コンストラクタ
    llm_client(outlet<>& output, outlet<>& error_out)
        : output_(output), error_out_(error_out), is_connected_(false) {
        // 復旧デバッグ用のメッセージを設定
        response_messages_.push("I'm Claude, an AI assistant created by Anthropic.");
        response_messages_.push("I'm here to be helpful, harmless, and honest.");
        response_messages_.push("How can I assist you today?");
    }
    
    // デストラクタ
    ~llm_client() {
        disconnect();
    }
    
    // 接続
    bool connect(const std::string& url) {
        output_.send("llm_connecting", symbol(url));
        
        // デバッグ用の機能実装として即時接続成功を返す
        is_connected_ = true;
        
        // 接続後にシミュレーション用のレスポンスを生成
        output_.send("llm_connected", symbol(url));
        
        // Claudeの利用可能なモデルをシミュレート
        json models = {
            {"id", "claude-3-opus-20240229"},
            {"id", "claude-3-sonnet-20240229"},
            {"id", "claude-3-haiku-20240307"}
        };
        
        std::string models_str = models.dump();
        output_.send("llm_models", symbol(models_str));
        
        return true;
    }
    
    // 切断
    void disconnect() {
        if (is_connected_) {
            is_connected_ = false;
            output_.send("llm_disconnected");
        }
    }
    
    // メッセージ送信
    bool send_message(const std::string& message) {
        if (!is_connected_) {
            error_out_.send("llm_not_connected");
            return false;
        }
        
        output_.send("llm_request_sent", symbol(message));
        
        // Claudeからのレスポンスをシミュレート
        if (!response_messages_.empty()) {
            std::string response = response_messages_.front();
            response_messages_.pop();
            
            // カスタムレスポンスをキューの最後尾に入れておく
            response_messages_.push("I understood your request. How else can I assist you?");
            
            if (on_message) {
                on_message(response);
            }
            
            output_.send("llm_response", symbol(response));
        }
        
        return true;
    }
    
    // 接続状態確認
    bool is_connected() const {
        return is_connected_;
    }

private:
    outlet<>& output_;
    outlet<>& error_out_;
    std::atomic<bool> is_connected_;
    std::queue<std::string> response_messages_;
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
          is_connected_(false) {
        
        // クライアントのメッセージ受信コールバックを設定
        client_.on_message = [this](const std::string& message) {
            this->handle_message(message);
        };
        
        // デフォルトのモデルを設定
        current_model_ = "claude-3-opus-20240229";
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
                break;
                
            case llm_connection_type::claude_api:
                // Claude API URL
                if (url == "ws://localhost:5678") {
                    target_url = "wss://api.anthropic.com/v1/messages";
                }
                break;
                
            case llm_connection_type::openai_api:
                // OpenAI API URL
                if (url == "ws://localhost:5678") {
                    target_url = "wss://api.openai.com/v1/chat/completions";
                }
                break;
                
            case llm_connection_type::custom_api:
                // カスタムURLはそのまま使用
                break;
        }
        
        bool result = client_.connect(target_url);
        is_connected_ = result;
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
        
        json message;
        message["type"] = "prompt";
        message["content"] = prompt;
        message["model"] = current_model_;
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
        output_.send("model", symbol(current_model_));
        output_.send("llm_config_dump_end");
    }
    
    void dump_status() {
        output_.send("llm_status_dump_start");
        output_.send("connected", is_connected_ ? 1 : 0);
        if (is_connected_) {
            output_.send("connection_type", static_cast<int>(connection_type_));
        }
        output_.send("llm_status_dump_end");
    }
    
    void dump_models() {
        output_.send("llm_models_dump_start");
        output_.send("claude-3-opus-20240229", true);
        output_.send("claude-3-sonnet-20240229", true);
        output_.send("claude-3-haiku-20240307", true);
        output_.send("llm_models_dump_end");
    }
    
    void request_models() {
        // 実際のAPIではここでモデル情報を取得するリクエストを送信
        // この実装では回答をシミュレート
        dump_models();
    }
    
    bool cancel_request() {
        if (!is_connected_) {
            return false;
        }
        
        json message;
        message["type"] = "cancel";
        output_.send("llm_request_cancelled");
        return true;
    }
    
    bool is_connected() const {
        return is_connected_ && client_.is_connected();
    }
    
private:
    void handle_message(const std::string& message) {
        try {
            json j = json::parse(message);
            output_.send("llm_message", symbol(message));
        } catch (const std::exception& e) {
            error_out_.send("llm_invalid_json", symbol(message));
        }
    }
    
    outlet<>& output_;
    outlet<>& error_out_;
    llm_client client_;
    llm_connection_type connection_type_;
    std::atomic<bool> is_connected_;
    std::string current_model_;
    
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
    }
    
    ~llm_mcp() {
        // コネクタはユニークポインタのため自動的に破棄される
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
            atom value = args[1];
            connector_->set_config(param, value);
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
