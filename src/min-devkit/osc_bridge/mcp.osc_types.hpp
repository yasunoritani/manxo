#pragma once

#include "c74_min.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>

using namespace c74::min;

namespace mcp {
namespace osc {

/**
 * OSCメッセージの基本構造体
 */
struct message {
    std::string address;    // OSCアドレスパターン
    atoms args;             // 引数リスト
    
    // 比較演算子（テスト用）
    bool operator==(const message& other) const {
        return address == other.address && args == other.args;
    }
};

/**
 * OSC接続設定
 */
struct connection_config {
    std::string host = "127.0.0.1";  // ホスト名/IPアドレス
    int port_in = 7500;              // 受信ポート
    int port_out = 7400;             // 送信ポート
    int buffer_size = 4096;          // バッファサイズ
};

/**
 * 接続状態
 */
enum class connection_state {
    disconnected,        // 未接続
    connecting,          // 接続中
    connected,           // 接続済み
    error                // エラー状態
};

/**
 * エラーコード
 */
enum class osc_error_code {
    none,                // エラーなし
    connection_failed,   // 接続失敗
    send_failed,         // 送信失敗
    receive_failed,      // 受信失敗
    invalid_address,     // 無効なアドレス
    invalid_args,        // 無効な引数
    timeout              // タイムアウト
};

/**
 * エラー情報
 */
struct error_info {
    osc_error_code code = osc_error_code::none;
    std::string message;
    
    // エラーがあるかどうか
    bool has_error() const {
        return code != osc_error_code::none;
    }
};

/**
 * OSCメッセージハンドラの型定義
 */
using message_handler = std::function<void(const std::string& address, const atoms& args)>;

/**
 * エラーハンドラの型定義
 */
using error_handler = std::function<void(const error_info& error)>;

/**
 * OSCアドレスパターンハンドラのレジストリ
 */
class handler_registry {
public:
    // ハンドラの登録
    void register_handler(const std::string& pattern, message_handler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[pattern] = handler;
    }
    
    // ハンドラの削除
    void unregister_handler(const std::string& pattern) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(pattern);
    }
    
    // アドレスに対応するハンドラの取得
    message_handler get_handler(const std::string& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 完全一致の検索
        auto it = handlers_.find(address);
        if (it != handlers_.end()) {
            return it->second;
        }
        
        // パターンマッチング検索（ワイルドカード対応などを実装可能）
        for (const auto& pair : handlers_) {
            if (pattern_match(address, pair.first)) {
                return pair.second;
            }
        }
        
        // 見つからない場合は空ハンドラを返す
        return nullptr;
    }
    
    // すべてのハンドラを削除
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }
    
private:
    // アドレスパターンのマッチング
    bool pattern_match(const std::string& address, const std::string& pattern) {
        // TODO: アドレスパターンのワイルドカードマッチング実装
        // 例: /foo/* が /foo/bar にマッチするなど
        
        // 現在は単純な前方一致のみ実装
        if (pattern.back() == '*') {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            return address.compare(0, prefix.size(), prefix) == 0;
        }
        
        return address == pattern;
    }
    
    std::unordered_map<std::string, message_handler> handlers_;
    std::mutex mutex_;
};

} // namespace osc
} // namespace mcp
