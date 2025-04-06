#pragma once

#include "c74_min.h"
#include "mcp.osc_types.hpp"
#include "mcp.osc_claude_handler.hpp"
#include "mcp.websocket_client.hpp"
#include "mcp.websocket_server.hpp"
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>

// JSON用エイリアス
using json = nlohmann::json;

using namespace c74::min;

namespace mcp {
namespace osc {

/**
 * WebSocket MCP (Model Context Protocol) ハンドラークラス
 * LLM（Claude Desktop）とMaxの連携のためのMCPプロトコル実装
 */
class websocket_mcp_handler {
public:
    /**
     * コンストラクタ
     * @param output メッセージ出力用のMin outlet
     * @param error_out エラー出力用のMin outlet
     */
    websocket_mcp_handler(outlet<>& output, outlet<>& error_out)
        : output_(output), error_out_(error_out),
          claude_handler_(output, error_out),
          websocket_client_(output, error_out),
          websocket_server_(output, error_out),
          is_running_(false) {

        // WebSocketサーバーのメッセージハンドラを設定
        websocket_server_.set_message_handler(
            [this](const std::string& client_id, const std::string& message) {
                handle_websocket_message(client_id, message);
            }
        );

        // WebSocketサーバーのバイナリメッセージハンドラを設定
        websocket_server_.set_binary_handler(
            [this](const std::string& client_id, const char* data, size_t len) {
                handle_websocket_binary(client_id, data, len);
            }
        );

        // WebSocketサーバーの接続ハンドラを設定
        websocket_server_.set_connection_handler(
            [this](const std::string& client_id) {
                handle_client_connected(client_id);
            }
        );

        // WebSocketサーバーの切断ハンドラを設定
        websocket_server_.set_disconnection_handler(
            [this](const std::string& client_id) {
                handle_client_disconnected(client_id);
            }
        );

        // WebSocketクライアントのメッセージハンドラを設定
        websocket_client_.set_message_handler(
            [this](const std::string& message) {
                handle_client_message(message);
            }
        );

        // Claude OSCハンドラのコマンド登録
        register_claude_handlers();
    }

    /**
     * デストラクタ
     */
    ~websocket_mcp_handler() {
        // WebSocketサーバー停止
        if (websocket_server_.is_running()) {
            websocket_server_.stop();
        }

        // WebSocketクライアント切断
        if (websocket_client_.is_connected()) {
            websocket_client_.disconnect();
        }
    }

    /**
     * WebSocketサーバーの起動
     * @param port ポート番号
     * @return 起動成功の場合はtrue
     */
    bool start_server(int port = 8080) {
        return websocket_server_.start(port);
    }

    /**
     * WebSocketサーバーの停止
     */
    void stop_server() {
        websocket_server_.stop();
    }

    /**
     * WebSocketクライアントの接続
     * @param url 接続先URL
     * @param protocols 使用するサブプロトコル
     * @return 接続成功の場合はtrue
     */
    bool connect_client(const std::string& url, const std::string& protocols = "osc") {
        return websocket_client_.connect(url, protocols);
    }

    /**
     * WebSocketクライアントの切断
     */
    void disconnect_client() {
        websocket_client_.disconnect();
    }

    /**
     * Claude Desktopからのメッセージをハンドラーに転送
     * @param address OSCアドレス
     * @param args OSC引数
     */
    void process_claude_message(const std::string& address, const atoms& args) {
        claude_handler_.process_message(address, args);
    }

    /**
     * MCPコマンドの送信 (WebSocket経由)
     * @param command MCPコマンド
     * @param args コマンド引数
     * @return 送信成功の場合はtrue
     */
    bool send_mcp_command(const std::string& command, const atoms& args) {
        // MCPコマンドをフォーマット
        std::string address = "/mcp/" + command;

        // WebSocketクライアントが接続中ならそちらに送信
        if (websocket_client_.is_connected()) {
            return websocket_client_.send_osc(address, args);
        }

        // WebSocketサーバーが実行中ならブロードキャスト
        if (websocket_server_.is_running()) {
            return websocket_server_.broadcast_osc(address, args);
        }

        // どちらも利用不可
        error_out_.send("no_websocket_connection");
        return false;
    }

    /**
     * Maxパッチへのコマンド送信
     * @param command Maxコマンド
     * @param args コマンド引数
     */
    void send_max_command(const std::string& command, const atoms& args) {
        atoms cmd_msg;
        cmd_msg.push_back("/max/" + command);
        cmd_msg.insert(cmd_msg.end(), args.begin(), args.end());
        output_.send(cmd_msg);
    }

    /**
     * M4Lパッチへのコマンド送信
     * @param command M4Lコマンド
     * @param args コマンド引数
     */
    void send_m4l_command(const std::string& command, const atoms& args) {
        atoms cmd_msg;
        cmd_msg.push_back("/m4l/" + command);
        cmd_msg.insert(cmd_msg.end(), args.begin(), args.end());
        output_.send(cmd_msg);
    }

private:
    /**
     * WebSocketメッセージの処理
     * @param client_id クライアントID
     * @param message 受信したメッセージ
     */
    void handle_websocket_message(const std::string& client_id, const std::string& message) {
        try {
            // nlohmann/jsonライブラリを使用してJSONをパース
            json j = json::parse(message);

            // OSCメッセージフォーマット（address, args）の確認
            if (j.contains("address") && j.contains("args") && j["address"].is_string() && j["args"].is_array()) {
                // アドレスと引数の取得
                std::string address = j["address"].get<std::string>();
                atoms args;

                // 引数の配列を処理
                for (const auto& arg : j["args"]) {
                    if (arg.is_string()) {
                        args.push_back(arg.get<std::string>());
                    } else if (arg.is_number_float()) {
                        args.push_back(arg.get<float>());
                    } else if (arg.is_number_integer()) {
                        args.push_back(static_cast<int>(arg.get<int64_t>()));
                    } else if (arg.is_boolean()) {
                        args.push_back(arg.get<bool>() ? 1 : 0);
                    } else if (arg.is_null()) {
                        args.push_back(0); // Maxではnullは整数0として扱う
                    } else {
                        // その他の型は文字列に変換
                        args.push_back(arg.dump());
                    }
                }

                // MCPプレフィックスの処理
                if (address.substr(0, 5) == "/mcp/") {
                    // MCPコマンドの処理
                    handle_mcp_command(address.substr(5), args);
                } else if (address.substr(0, 8) == "/claude/") {
                    // Claudeメッセージの処理
                    claude_handler_.process_message(address, args);
                } else {
                    // その他のOSCメッセージ
                    atoms osc_msg;
                    osc_msg.push_back(address);
                    osc_msg.insert(osc_msg.end(), args.begin(), args.end());
                    output_.send(osc_msg);
                }
            } else {
                // 形式が正しくないJSONメッセージ
                atoms text_msg;
                text_msg.push_back("websocket_json");
                text_msg.push_back(client_id);
                text_msg.push_back(message);
                output_.send(text_msg);
            }
        } catch (const json::parse_error& e) {
            // JSONパースエラー
            error_out_.send("json_parse_error", symbol(e.what()));

            // 通常のテキストメッセージとして処理
            atoms text_msg;
            text_msg.push_back("websocket_text");
            text_msg.push_back(client_id);
            text_msg.push_back(message);
            output_.send(text_msg);
        }
    }

    /**
     * クライアント接続時の処理
     * @param client_id クライアントID
     */
    void handle_client_connected(const std::string& client_id) {
        atoms connection_info;
        connection_info.push_back("client_connected");
        connection_info.push_back(client_id);
        output_.send(connection_info);
    }

    /**
     * クライアント切断時の処理
     * @param client_id クライアントID
     */
    void handle_client_disconnected(const std::string& client_id) {
        atoms disconnection_info;
        disconnection_info.push_back("client_disconnected");
        disconnection_info.push_back(client_id);
        output_.send(disconnection_info);
    }

    /**
     * MCPコマンドの処理
     * @param command MCPコマンド（/mcp/プレフィックスを除いたもの）
     * @param args コマンド引数
     */
    void handle_mcp_command(const std::string& command, const atoms& args) {
        if (command == "status") {
            // ステータスコマンドの処理
            send_status_response();
        } else if (command == "max_command") {
            // Maxコマンドの処理
            if (args.size() >= 1) {
                std::string max_cmd = static_cast<std::string>(args[0]);
                atoms cmd_args;
                if (args.size() > 1) {
                    cmd_args.insert(cmd_args.end(), args.begin() + 1, args.end());
                }
                send_max_command(max_cmd, cmd_args);
            }
        } else if (command == "m4l_command") {
            // M4Lコマンドの処理
            if (args.size() >= 1) {
                std::string m4l_cmd = static_cast<std::string>(args[0]);
                atoms cmd_args;
                if (args.size() > 1) {
                    cmd_args.insert(cmd_args.end(), args.begin() + 1, args.end());
                }
                send_m4l_command(m4l_cmd, cmd_args);
            }
        } else {
            // 未知のコマンド
            error_out_.send("unknown_mcp_command", symbol(command));
        }
    }

    /**
     * ステータス応答の送信
     */
    void send_status_response() {
        atoms status;
        status.push_back("active");
        status.push_back("websocket_mcp");
        status.push_back(symbol(__DATE__ " " __TIME__));

        // WebSocketクライアントが接続中ならそちらに送信
        if (websocket_client_.is_connected()) {
            websocket_client_.send_osc("/mcp/status_response", status);
        }

        // WebSocketサーバーが実行中ならブロードキャスト
        if (websocket_server_.is_running()) {
            websocket_server_.broadcast_osc("/mcp/status_response", status);
        }

        // Maxパッチにも通知
        atoms status_max;
        status_max.push_back("/mcp/status_response");
        status_max.insert(status_max.end(), status.begin(), status.end());
        output_.send(status_max);
    }

private:
    outlet<>& output_;
    outlet<>& error_out_;
    claude_handler claude_handler_;
    websocket_client websocket_client_;
    websocket_server websocket_server_;
    bool is_running_;

    // Claudeハンドラーの登録（実装省略）
    void register_claude_handlers() {
        // Claude特有のコマンドハンドラーを登録
    }

    // WebSocketクライアントからのメッセージ処理
    void handle_client_message(const std::string& message) {
        // クライアントからのメッセージを処理
        try {
            json j = json::parse(message);
            // 処理ロジック
        } catch (const json::parse_error& e) {
            error_out_.send("client_json_parse_error", symbol(e.what()));
        }
    }

    // バイナリメッセージの処理
    void handle_websocket_binary(const std::string& client_id, const char* data, size_t len) {
        // バイナリメッセージの処理ロジック
    }
};

} // namespace osc
} // namespace mcp
