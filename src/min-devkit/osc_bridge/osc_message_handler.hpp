#pragma once

// Issue 19: Min-DevKitヘッダーの重複インクルード対応
// Min-DevKit型とOSC独自型の変換機能を提供する中間レイヤー

// Min-DevKitヘッダーをインクルード
#include "c74_min.h"

// OSC独自型定義をインクルード
#include "mcp.osc_types.hpp"

namespace mcp {
namespace osc {

// Min-DevKit型とOSC独自型の変換ユーティリティ

/**
 * OSC独自型（osc_args）からMin-DevKit型（atoms）への変換
 */
inline c74::min::atoms convert_to_atoms(const osc_args& args) {
    c74::min::atoms result;
    for (const auto& arg : args) {
        // すべての値をsymbolとして追加（実際のアプリケーションでは型情報を解析する必要あり）
        result.push_back(c74::min::symbol(arg));
    }
    return result;
}

/**
 * Min-DevKit型（atoms）からOSC独自型（osc_args）への変換
 */
inline osc_args convert_to_osc_args(const c74::min::atoms& args) {
    osc_args result;
    for (const auto& arg : args) {
        if (arg.type() == c74::min::message_type::symbol_argument) {
            std::string value = arg;
            result.push_back(value);
        } else if (arg.type() == c74::min::message_type::float_argument) {
            float value = arg;
            result.push_back(std::to_string(value));
        } else if (arg.type() == c74::min::message_type::int_argument) {
            int value = arg;
            result.push_back(std::to_string(value));
        } else {
            // 他の型は空文字列として扱う
            result.push_back("");
        }
    }
    return result;
}

// Min-DevKit型向けのOSCメッセージハンドラアダプタ
template<typename T>
class message_handler_adapter {
public:
    message_handler_adapter(T* instance, void (T::*handler)(const std::string&, const osc_args&))
        : instance_(instance), handler_(handler) {}
    
    void operator()(const std::string& address, const c74::min::atoms& args) {
        // Min-DevKit型からOSC独自型に変換して呼び出す
        (instance_->*handler_)(address, convert_to_osc_args(args));
    }
    
private:
    T* instance_;
    void (T::*handler_)(const std::string&, const osc_args&);
};

// OSC独自型向けのMin-DevKitメッセージハンドラアダプタ
template<typename T>
class min_message_handler_adapter {
public:
    min_message_handler_adapter(T* instance, void (T::*handler)(const std::string&, const c74::min::atoms&))
        : instance_(instance), handler_(handler) {}
    
    void operator()(const std::string& address, const osc_args& args) {
        // OSC独自型からMin-DevKit型に変換して呼び出す
        (instance_->*handler_)(address, convert_to_atoms(args));
    }
    
private:
    T* instance_;
    void (T::*handler_)(const std::string&, const c74::min::atoms&);
};

} // namespace osc
} // namespace mcp
