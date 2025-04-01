#pragma once

#include "osc_interface.hpp"
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

namespace mcp {
namespace osc {
namespace mock {

// 前方宣言
class client;
class server;
class bridge;

// モックOSCサーバー実装
class server : public server_interface {
public:
    server() : running_(false), port_(0), last_error_(osc_error_code::none) {}
    
    virtual ~server() {
        stop();
    }
    
    bool start(int port) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (running_) return true;
        
        port_ = port;
        
        // 開始エラーをシミュレート可能
        if (simulate_start_error_) {
            last_error_ = osc_error_code::connection_failed;
            return false;
        }
        
        running_ = true;
        last_error_ = osc_error_code::none;
        return true;
    }
    
    void stop() override {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }
    
    bool is_running() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }
    
    void set_port(int port) override {
        std::lock_guard<std::mutex> lock(mutex_);
        port_ = port;
    }
    
    void add_method(const std::string& pattern, message_handler_t handler) override {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[pattern] = handler;
    }
    
    void remove_method(const std::string& pattern) override {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(pattern);
    }
    
    osc_error_code get_last_error() const override {
        return last_error_;
    }
    
    // モックスペシフィックメソッド
    void receive_message(const std::string& address, const atoms& args) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!running_) return;
        
        // 受信メッセージを記録
        received_messages_.push_back({address, args});
        
        // 受信エラーをシミュレート可能
        if (simulate_receive_error_) {
            last_error_ = osc_error_code::receive_failed;
            // エラー発生時はメッセージ処理を中断せずに継続する
            // エラー後の動作確認用にエラーフラグを維持
        }
        
        // パターンマッチングとハンドラー呼び出し
        for (const auto& handler_pair : handlers_) {
            if (pattern_match(handler_pair.first, address)) {
                if (handler_pair.second) {
                    // 独立したスレッドでハンドラーを呼び出す（実際のOSCサーバーに近い挙動）
                    std::thread([handler = handler_pair.second, address, args]() {
                        handler(address, args);
                    }).detach();
                }
            }
        }
        
        last_error_ = osc_error_code::none;
    }
    
    void simulate_start_error(bool simulate) {
        simulate_start_error_ = simulate;
    }
    
    void simulate_receive_error(bool simulate) {
        simulate_receive_error_ = simulate;
    }
    
    const std::vector<std::pair<std::string, atoms>>& get_received_messages() const {
        return received_messages_;
    }
    
    void clear_received_messages() {
        received_messages_.clear();
    }
    
private:
    // 簡易的なパターンマッチング（完全な実装ではない）
    bool pattern_match(const std::string& pattern, const std::string& address) const {
        // 完全一致
        if (pattern == address) return true;
        
        // ワイルドカードマッチング（簡易実装）
        if (pattern.back() == '*') {
            std::string prefix = pattern.substr(0, pattern.size() - 1);
            return address.find(prefix) == 0;
        }
        
        return false;
    }
    
    int port_;
    std::atomic<bool> running_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, message_handler_t> handlers_;
    osc_error_code last_error_;
    std::vector<std::pair<std::string, atoms>> received_messages_;
    bool simulate_start_error_ = false;
    bool simulate_receive_error_ = false;
};

// モックOSCクライアント実装
class client : public client_interface {
public:
    client() : connected_(false), last_error_(osc_error_code::none) {}
    
    virtual ~client() {
        disconnect();
    }
    
    bool connect(const std::string& host, int port) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (connected_) return true;
        
        host_ = host;
        port_ = port;
        
        // 接続エラーをシミュレート可能
        if (simulate_connection_error_) {
            last_error_ = osc_error_code::connection_failed;
            return false;
        }
        
        connected_ = true;
        last_error_ = osc_error_code::none;
        return true;
    }
    
    void disconnect() override {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = false;
    }
    
    bool is_connected() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return connected_;
    }
    
    void set_target(const std::string& host, int port) override {
        std::lock_guard<std::mutex> lock(mutex_);
        host_ = host;
        port_ = port;
    }
    
    bool send(const std::string& address, const atoms& args) override {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!connected_) {
            last_error_ = osc_error_code::send_failed;
            return false;
        }
        
        // 送信エラーをシミュレート可能
        if (simulate_send_error_) {
            last_error_ = osc_error_code::send_failed;
            return false;
        }
        
        // 送信履歴を記録
        sent_messages_.push_back({address, args});
        
        // 相互接続されたサーバーがあれば、そのサーバーにメッセージを転送
        if (connected_server_) {
            // clone() メソッドを使用して型情報を保持した新しいインスタンスを作成
            connected_server_->receive_message(address, args.clone());
        }
        
        last_error_ = osc_error_code::none;
        return true;
    }
    
    bool send_internal(const std::string& address, const std::vector<std::string>& args) override {
        atoms converted_args;
        for (const auto& arg : args) {
            converted_args.add(arg);
        }
        return send(address, converted_args);
    }
    
    osc_error_code get_last_error() const override {
        return last_error_;
    }
    
    // モックスペシフィックメソッド
    void set_connected_server(server* server) {
        connected_server_ = server;
    }
    
    void simulate_connection_error(bool simulate) {
        simulate_connection_error_ = simulate;
    }
    
    void simulate_send_error(bool simulate) {
        simulate_send_error_ = simulate;
    }
    
    const std::vector<std::pair<std::string, atoms>>& get_sent_messages() const {
        return sent_messages_;
    }
    
    void clear_sent_messages() {
        sent_messages_.clear();
    }
    
private:
    std::string host_;
    int port_;
    std::atomic<bool> connected_;
    mutable std::mutex mutex_;
    osc_error_code last_error_;
    std::vector<std::pair<std::string, atoms>> sent_messages_;
    server* connected_server_ = nullptr;
    bool simulate_connection_error_ = false;
    bool simulate_send_error_ = false;
};

// モックOSCブリッジ実装
class bridge : public bridge_interface {
public:
    bridge() : client_(new client()), server_(new server()), last_error_(osc_error_code::none) {
        // クライアントとサーバーを相互接続
        client_->set_connected_server(server_.get());
    }
    
    virtual ~bridge() {
        disconnect();
    }
    
    bool connect(const std::string& host, int port_in, int port_out) override {
        bool server_started = server_->start(port_in);
        bool client_connected = client_->connect(host, port_out);
        
        if (!server_started || !client_connected) {
            last_error_ = server_started ? client_->get_last_error() : server_->get_last_error();
            return false;
        }
        
        last_error_ = osc_error_code::none;
        return true;
    }
    
    void disconnect() override {
        server_->stop();
        client_->disconnect();
    }
    
    bool is_connected() const override {
        return server_->is_running() && client_->is_connected();
    }
    
    bool send(const std::string& address, const atoms& args) override {
        // 型情報を正確に保持するためにクローンを作成して送信
        bool result = client_->send(address, args.clone());
        if (!result) {
            last_error_ = client_->get_last_error();
        }
        return result;
    }
    
    void add_method(const std::string& pattern, message_handler_t handler) override {
        server_->add_method(pattern, handler);
    }
    
    void remove_method(const std::string& pattern) override {
        server_->remove_method(pattern);
    }
    
    osc_error_code get_last_error() const override {
        return last_error_;
    }
    
    void handle_m4l_event(const std::string& event_name) override {
        // M4Lライフサイクルイベントの処理をシミュレート
        m4l_events_.push_back(event_name);
        
        if (event_name == "liveset_closed") {
            // Live Setが閉じられたときの処理をシミュレート
            disconnect();
        } else if (event_name == "liveset_new" || event_name == "liveset_loaded") {
            // 新しいLive Setが作成されたときの処理をシミュレート
            // 必要に応じて再接続などの処理を行う
        }
    }
    
    // モックスペシフィックメソッド
    client* get_client() {
        return client_.get();
    }
    
    server* get_server() {
        return server_.get();
    }
    
    const std::vector<std::string>& get_m4l_events() const {
        return m4l_events_;
    }
    
    void clear_m4l_events() {
        m4l_events_.clear();
    }
    
private:
    std::unique_ptr<client> client_;
    std::unique_ptr<server> server_;
    osc_error_code last_error_;
    std::vector<std::string> m4l_events_;
};

} // namespace mock
} // namespace osc
} // namespace mcp
