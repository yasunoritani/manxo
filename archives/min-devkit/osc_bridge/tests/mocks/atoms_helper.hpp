#pragma once

#include "osc_interface.hpp"

namespace mcp {
namespace osc {
namespace mock {

// ローレベルのOSCメッセージ処理用クラス
class osc_message {
public:
    std::string address;
    std::vector<std::string> values;
    std::vector<std::string> types;
};

// シンボル型をサポートするクライアント
class symbol_client : public mock::client {
public:
    symbol_client() : client() {}
    
    // シンボル型を含むメッセージを送信
    bool send_symbol(const std::string& address, const std::string& symbol_value) {
        // サーバーに直接メッセージを送信
        if (connected_server_) {
            osc_message msg;
            msg.address = address;
            msg.values.push_back(symbol_value);
            msg.types.push_back("symbol");
            
            // カスタムメッセージ送信
            return send_custom_message(msg);
        }
        return false;
    }
    
private:
    bool send_custom_message(const osc_message& msg) {
        if (!connected_server_) {
            return false;
        }
        
        // atomsに変換
        atoms converted_args;
        for (size_t i = 0; i < msg.values.size(); ++i) {
            converted_args.add(msg.values[i]);
        }
        
        // カスタムメッセージハンドリングを実装
        connected_server_->add_method(msg.address, [&, msg](const std::string& addr, const atoms& args) {
            // 型情報をカスタマイズして処理
            if (args.size() > 0) {
                for (const auto& handler_pair : connected_server_->get_handlers()) {
                    if (connected_server_->pattern_match(handler_pair.first, addr)) {
                        if (handler_pair.second) {
                            // 型の上書きをシミュレート
                            atoms modified_args;
                            for (size_t i = 0; i < args.size(); ++i) {
                                if (i < msg.types.size()) {
                                    // シンボル型の場合は型情報をカスタマイズ
                                    handler_pair.second(addr, args);
                                }
                            }
                        }
                    }
                }
            }
        });
        
        // 実際の送信は通常のメソッドで行う
        return send(msg.address, converted_args);
    }
};

} // namespace mock
} // namespace osc
} // namespace mcp
