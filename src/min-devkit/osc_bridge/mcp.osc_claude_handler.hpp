#pragma once

#include "c74_min.h"
#include "mcp.osc_types.hpp"
#include <string>
#include <functional>
#include <vector>
#include <map>

using namespace c74::min;

namespace mcp {
namespace osc {

/**
 * Claude Desktop OSCハンドラークラス
 * Claude Desktopから送信されたOSCメッセージを処理
 */
class claude_handler {
public:
    /**
     * コンストラクタ
     * @param output メッセージ出力用のMin outlet
     * @param error_out エラー出力用のMin outlet
     */
    claude_handler(outlet<>& output, outlet<>& error_out)
        : output_(output), error_out_(error_out) {
        // デフォルトのコマンドハンドラを登録
        register_default_handlers();
    }
    
    /**
     * OSCアドレスとハンドラの対応表
     */
    using command_handler = std::function<void(const atoms&)>;
    using handler_map = std::map<std::string, command_handler>;
    
    /**
     * OSCメッセージ処理
     * @param address OSCアドレス
     * @param args OSC引数
     */
    void process_message(const std::string& address, const atoms& args) {
        // /claude/で始まるアドレスのみ処理
        if (address.substr(0, 8) != "/claude/") {
            return;
        }
        
        // コマンド部分の抽出（/claude/command → command）
        std::string command = address.substr(8);
        
        // コマンドに対応するハンドラが登録されているか確認
        auto it = handlers_.find(command);
        if (it != handlers_.end()) {
            // ハンドラを呼び出し
            it->second(args);
        } else {
            // 未知のコマンド
            error_out_.send("unknown_claude_command", symbol(command));
        }
    }
    
    /**
     * コマンドハンドラの登録
     * @param command コマンド名
     * @param handler ハンドラ関数
     */
    void register_handler(const std::string& command, command_handler handler) {
        handlers_[command] = handler;
    }
    
private:
    /**
     * デフォルトのコマンドハンドラを登録
     */
    void register_default_handlers() {
        // コマンド: ping
        register_handler("ping", [this](const atoms& args) {
            // pongを返す
            atoms response;
            response.push_back(symbol("/claude/pong"));
            if (args.size() > 0) {
                response.insert(response.end(), args.begin(), args.end());
            }
            output_.send(response);
        });
        
        // コマンド: get_status
        register_handler("get_status", [this](const atoms& args) {
            // 状態情報を返す
            atoms response;
            response.push_back(symbol("/claude/status"));
            response.push_back("active");
            response.push_back("m4l_bridge");
            response.push_back(symbol(__DATE__ " " __TIME__));
            output_.send(response);
        });
        
        // コマンド: ableton_command
        register_handler("ableton_command", [this](const atoms& args) {
            if (args.size() < 1) {
                error_out_.send("missing_ableton_command");
                return;
            }
            
            // Abletonコマンドを処理
            process_ableton_command(args);
        });
    }
    
    /**
     * Abletonコマンドの処理
     * @param args コマンド引数
     */
    void process_ableton_command(const atoms& args) {
        // コマンド名を取得
        std::string command = static_cast<std::string>(args[0]);
        
        // Abletonコマンドを出力
        // 実際のAbletonコマンド処理はMaxパッチ側で実装
        atoms cmd_args;
        cmd_args.push_back(symbol("/ableton/command"));
        cmd_args.push_back(symbol(command));
        
        // 追加パラメータがあれば転送
        if (args.size() > 1) {
            cmd_args.insert(cmd_args.end(), args.begin() + 1, args.end());
        }
        
        output_.send(cmd_args);
    }
    
private:
    outlet<>& output_;
    outlet<>& error_out_;
    handler_map handlers_;
};

} // namespace osc
} // namespace mcp
