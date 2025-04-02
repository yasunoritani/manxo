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
                handle_client_disconnected(client_id);
            }
        );
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
        // 簡易的なJSON解析（実際の実装ではより堅牢なJSONパーサーを使用）
        if (message.find("\"address\":") != std::string::npos && 
            message.find("\"args\":") != std::string::npos) {
            // OSCメッセージとして処理
            
            // アドレスの抽出
            size_t addr_start = message.find("\"address\":\"") + 11;
            size_t addr_end = message.find("\"", addr_start);
            std::string address = message.substr(addr_start, addr_end - addr_start);
            
            // 引数の処理（簡易的な実装）
            atoms args;
            size_t args_start = message.find("\"args\":[") + 8;
            size_t args_end = message.find("]", args_start);
            std::string args_str = message.substr(args_start, args_end - args_start);
            
            // カンマで分割（非常に簡易的、実際の実装ではJSONパーサーを使用）
            size_t pos = 0;
            while ((pos = args_str.find(",")) != std::string::npos) {
                std::string arg = args_str.substr(0, pos);
                args_str.erase(0, pos + 1);
                
                // 引数を適切な型に変換（簡易的）
                if (arg.front() == '"' && arg.back() == '"') {
                    // 文字列
                    args.push_back(arg.substr(1, arg.length() - 2));
                } else {
                    // 数値
                    try {
                        float val = std::stof(arg);
                        args.push_back(val);
                    } catch (...) {
                        args.push_back(arg);
                    }
                }
            }
            
            // 最後の引数
            if (!args_str.empty()) {
                if (args_str.front() == '"' && args_str.back() == '"') {
                    args.push_back(args_str.substr(1, args_str.length() - 2));
                } else {
                    try {
                        float val = std::stof(args_str);
                        args.push_back(val);
                    } catch (...) {
                        args.push_back(args_str);
                    }
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
};

} // namespace osc
} // namespace mcp
