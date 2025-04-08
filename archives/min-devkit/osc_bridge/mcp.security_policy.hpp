#pragma once

#include "c74_min.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <mutex>
#include <deque>
#include <ctime>
#include <regex>
#include <optional>
#include <functional>

using namespace c74::min;

namespace mcp {
namespace security {

// セキュリティトークン
struct SecurityToken {
    std::string token;
    std::string client_id;
    std::chrono::system_clock::time_point expiry;
    std::unordered_set<std::string> allowed_commands;

    bool is_valid() const {
        return std::chrono::system_clock::now() < expiry;
    }
};

// レート制限用のクライアント状態
struct ClientState {
    std::deque<std::chrono::system_clock::time_point> request_timestamps;
    std::chrono::system_clock::time_point last_reset;
    size_t total_bytes_received{0};
    size_t message_count{0};
};

/**
 * セキュリティポリシークラス
 * WebSocketとOSC通信のセキュリティを管理
 */
class security_policy {
public:
    /**
     * コンストラクタ
     * @param error_out エラー出力用のMin outlet
     */
    security_policy(outlet<>& error_out)
        : error_out_(error_out),
          max_message_size_(1024 * 1024), // デフォルト1MB
          rate_limit_count_(100),         // デフォルト100メッセージ
          rate_limit_period_(60),         // デフォルト1分(60秒)
          min_port_(8000),               // デフォルト最小ポート
          max_port_(9000),               // デフォルト最大ポート
          token_required_(false) {        // デフォルトでトークン認証は無効

        // デフォルトで許可するIPアドレス (ローカルホストのみ)
        allowed_ips_.insert("127.0.0.1");
        allowed_ips_.insert("::1");

        // 禁止コマンドのデフォルト設定
        restricted_commands_.insert("system");
        restricted_commands_.insert("delete");
        restricted_commands_.insert("format");
    }

    /**
     * メッセージサイズ制限の設定
     * @param max_size 最大メッセージサイズ（バイト）
     */
    void set_max_message_size(size_t max_size) {
        max_message_size_ = max_size;
    }

    /**
     * メッセージサイズの検証
     * @param message_size メッセージサイズ
     * @return 許可される場合はtrue、それ以外はfalse
     */
    bool validate_message_size(size_t message_size) const {
        return message_size <= max_message_size_;
    }

    /**
     * レート制限の設定
     * @param count 制限期間内のメッセージ数
     * @param period_seconds 制限期間（秒）
     */
    void set_rate_limit(size_t count, int period_seconds) {
        rate_limit_count_ = count;
        rate_limit_period_ = period_seconds;
    }

    /**
     * レート制限の検証
     * @param client_id クライアントID
     * @param message_size 現在のメッセージサイズ
     * @return 許可される場合はtrue、それ以外はfalse
     */
    bool validate_rate_limit(const std::string& client_id, size_t message_size) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto now = std::chrono::system_clock::now();
        auto& state = client_states_[client_id];

        // 古いタイムスタンプを削除
        auto cutoff = now - std::chrono::seconds(rate_limit_period_);
        while (!state.request_timestamps.empty() && state.request_timestamps.front() < cutoff) {
            state.request_timestamps.pop_front();
        }

        // レート制限をチェック
        if (state.request_timestamps.size() >= rate_limit_count_) {
            error_out_.send("rate_limit_exceeded", symbol(client_id));
            return false;
        }

        // 新しいリクエストを記録
        state.request_timestamps.push_back(now);
        state.total_bytes_received += message_size;
        state.message_count++;

        return true;
    }

    /**
     * ポート範囲の設定
     * @param min_port 最小ポート番号
     * @param max_port 最大ポート番号
     */
    void set_port_range(int min_port, int max_port) {
        min_port_ = min_port;
        max_port_ = max_port;
    }

    /**
     * ポートの検証
     * @param port ポート番号
     * @return 許可される場合はtrue、それ以外はfalse
     */
    bool validate_port(int port) const {
        return port >= min_port_ && port <= max_port_;
    }

    /**
     * IPアドレスの許可
     * @param ip IPアドレス
     */
    void allow_ip(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        allowed_ips_.insert(ip);
    }

    /**
     * IPアドレスの禁止
     * @param ip IPアドレス
     */
    void deny_ip(const std::string& ip) {
        std::lock_guard<std::mutex> lock(mutex_);
        allowed_ips_.erase(ip);
    }

    /**
     * IPアドレスの検証
     * @param ip IPアドレス
     * @return 許可される場合はtrue、それ以外はfalse
     */
    bool validate_ip(const std::string& ip) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return allowed_ips_.find(ip) != allowed_ips_.end();
    }

    /**
     * トークンの生成
     * @param client_id クライアントID
     * @param expiry_seconds 有効期間（秒）
     * @return 生成されたトークン
     */
    std::string generate_token(const std::string& client_id, int expiry_seconds = 3600) {
        std::lock_guard<std::mutex> lock(mutex_);

        // トークン生成（簡易的な実装）
        std::string token = client_id + "_" + std::to_string(std::time(nullptr));

        // トークン登録
        SecurityToken security_token;
        security_token.token = token;
        security_token.client_id = client_id;
        security_token.expiry = std::chrono::system_clock::now() + std::chrono::seconds(expiry_seconds);

        tokens_[token] = security_token;
        return token;
    }

    /**
     * トークンの検証
     * @param token トークン
     * @return 有効なトークンの場合はtrue、それ以外はfalse
     */
    bool validate_token(const std::string& token) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = tokens_.find(token);
        if (it == tokens_.end()) {
            return false;
        }

        const auto& security_token = it->second;
        if (!security_token.is_valid()) {
            tokens_.erase(it);
            return false;
        }

        return true;
    }

    /**
     * トークン認証の有効/無効設定
     * @param required trueの場合、トークン認証を要求
     */
    void require_token(bool required) {
        token_required_ = required;
    }

    /**
     * トークン認証が必要かどうか
     * @return トークン認証が必要な場合はtrue
     */
    bool is_token_required() const {
        return token_required_;
    }

    /**
     * コマンドの制限
     * @param command コマンド名
     */
    void restrict_command(const std::string& command) {
        std::lock_guard<std::mutex> lock(mutex_);
        restricted_commands_.insert(command);
    }

    /**
     * コマンドの許可
     * @param command コマンド名
     */
    void allow_command(const std::string& command) {
        std::lock_guard<std::mutex> lock(mutex_);
        restricted_commands_.erase(command);
    }

    /**
     * コマンドの検証
     * @param command コマンド名
     * @return 許可される場合はtrue、それ以外はfalse
     */
    bool validate_command(const std::string& command) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return restricted_commands_.find(command) == restricted_commands_.end();
    }

    /**
     * JSONメッセージ構造の検証
     * @param json_str JSONメッセージ
     * @param schema_checker スキーマチェック関数
     * @return 有効な場合はtrue、それ以外はfalse
     */
    bool validate_json_structure(const std::string& json_str, const std::function<bool(const std::string&)>& schema_checker) {
        try {
            return schema_checker(json_str);
        } catch (...) {
            return false;
        }
    }

private:
    outlet<>& error_out_;

    // セキュリティ設定
    size_t max_message_size_;
    size_t rate_limit_count_;
    int rate_limit_period_;
    int min_port_;
    int max_port_;
    bool token_required_;

    // スレッド安全のためのミューテックス
    mutable std::mutex mutex_;

    // IPアドレス許可リスト
    std::unordered_set<std::string> allowed_ips_;

    // 制限コマンドリスト
    std::unordered_set<std::string> restricted_commands_;

    // クライアント状態
    std::unordered_map<std::string, ClientState> client_states_;

    // セキュリティトークン
    std::unordered_map<std::string, SecurityToken> tokens_;
};

}}  // namespace mcp::security
