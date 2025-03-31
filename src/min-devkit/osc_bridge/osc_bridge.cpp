/// @file
///	@ingroup 	mcpmodules
///	@copyright	Copyright 2025 The Max 9-Claude Desktop Integration Project. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

#include "c74_min.h"
#include <string>
#include <memory>
#include <atomic>
#include <deque>
#include <mutex>
#include <functional>
#include <thread>
#include <map>
#include <sstream>

// OSCpack includes
#include "ip/UdpSocket.h"
#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"

// Local includes
#include "mcp.osc_types.hpp"

using namespace c74::min;

// OSC名前空間
namespace mcp {
namespace osc {

// 前方宣言
class client;
class server;
class handler_registry;
class packet_listener;
class claude_handler;

// メッセージハンドラーの型定義
typedef std::function<void(const std::string&, const atoms&)> message_handler_t;
typedef std::function<void(const std::string&)> error_handler_t;

// OSCパケットリスナークラス - oscpackを拡張
class packet_listener : public ::osc::OscPacketListener {
public:
    packet_listener() = default;
    
    void set_message_callback(message_handler_t handler) {
        message_callback_ = handler;
    }
    
    void set_error_callback(error_handler_t handler) {
        error_callback_ = handler;
    }
    
    // 受信パケットの処理
    virtual void ProcessMessage(const ::osc::ReceivedMessage& message, 
                              const IpEndpointName& remote_endpoint) override {
        try {
            // アドレスパターンを取得
            std::string address = message.AddressPattern();
            
            // 引数をatomsオブジェクトに変換
            atoms args;
            
            // 引数のイテレーション
            for (::osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();
                 arg != message.ArgumentsEnd(); ++arg) {
                if (arg->IsFloat()) {
                    args.push_back(static_cast<float>(arg->AsFloat()));
                }
                else if (arg->IsDouble()) {
                    args.push_back(static_cast<float>(arg->AsDouble()));
                }
                else if (arg->IsInt32()) {
                    args.push_back(static_cast<int>(arg->AsInt32()));
                }
                else if (arg->IsInt64()) {
                    args.push_back(static_cast<int>(arg->AsInt64()));
                }
                else if (arg->IsString()) {
                    args.push_back(symbol(arg->AsString()));
                }
                else if (arg->IsBool()) {
                    args.push_back(arg->AsBool() ? 1 : 0);
                }
                // 他の型は必要に応じて追加
            }
            
            // コールバックを呼び出す
            if (message_callback_) {
                message_callback_(address, args);
            }
        }
        catch (::osc::Exception& e) {
            if (error_callback_) {
                error_callback_(std::string("Error parsing OSC message: ") + e.what());
            }
        }
        catch (const std::exception& e) {
            if (error_callback_) {
                error_callback_(std::string("Unknown error processing OSC message: ") + e.what());
            }
        }
    }
    
private:
    message_handler_t message_callback_;
    error_handler_t error_callback_;
};

// ハンドラーレジストリクラス - 特定のOSCアドレスとハンドラーを関連付ける
class handler_registry {
public:
    handler_registry() = default;
    
    // ハンドラーを登録
    void register_handler(const std::string& pattern, message_handler_t handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[pattern] = handler;
    }
    
    // ハンドラーを削除
    void unregister_handler(const std::string& pattern) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(pattern);
    }
    
    // アドレスパターンにマッチするハンドラーを探す
    bool dispatch_message(const std::string& address, const atoms& args) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // アドレスと完全一致するハンドラーを探す
        auto it = handlers_.find(address);
        if (it != handlers_.end()) {
            it->second(address, args);
            return true;
        }
        
        // ワイルドカードマッチを実装する場合はここにコードを追加
        
        return false;
    }
    
    // 登録されているハンドラーの数を取得
    size_t count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.size();
    }
    
    // 全てのハンドラーをクリア
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }
    
    // 特定のパターンに対応するハンドラーを取得
    message_handler_t get_handler(const std::string& pattern) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = handlers_.find(pattern);
        if (it != handlers_.end()) {
            return it->second;
        }
        return nullptr;
    }
    
private:
    std::map<std::string, message_handler_t> handlers_;
    mutable std::mutex mutex_; // スレッドセーフな操作のため
};

// OSCクライアントクラス - OSCメッセージを送信する
class client {
public:
    client() : socket_(nullptr), connected_(false), target_port_(0) {}
    
    ~client() {
        disconnect();
    }
    
    // 接続設定
    bool connect(const std::string& host, int port) {
        try {
            disconnect(); // 既存の接続を切断
            
            // ソケットの作成
            socket_ = std::make_unique<UdpTransmitSocket>(IpEndpointName(host.c_str(), port));
            target_host_ = host;
            target_port_ = port;
            connected_ = true;
            
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "OSC client connect error: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 切断
    void disconnect() {
        socket_.reset();
        connected_ = false;
    }
    
    // 接続状態確認
    bool is_connected() const {
        return connected_ && socket_ != nullptr;
    }
    
    // ターゲット設定を更新
    void set_target(const std::string& host, int port) {
        if (host != target_host_ || port != target_port_) {
            connect(host, port);
        }
    }
    
    // OSCメッセージ送信
    bool send(const std::string& address, const atoms& args) {
        if (!socket_) {
            return false;
        }
        
        try {
            // OSCパケットの準備
            char buffer[1024]; // 適切なバッファサイズ
            ::osc::OutboundPacketStream packet(buffer, 1024);
            
            // パケットの構築開始
            packet << ::osc::BeginMessage(address.c_str());
            
            // 引数の追加
            for (size_t i = 0; i < args.size(); ++i) {
                const auto& arg = args[i];
                
                if (arg.type() == message_type::float_argument) {
                    float value = arg;
                    packet << value;
                }
                else if (arg.type() == message_type::int_argument) {
                    int value = arg;
                    packet << value;
                }
                else if (arg.type() == message_type::symbol_argument) {
                    std::string value = arg;
                    packet << value.c_str();
                }
                // 他の型は必要に応じて追加
            }
            
            // パケットの構築終了
            packet << ::osc::EndMessage;
            
            // パケット送信
            socket_->Send(packet.Data(), packet.Size());
            
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "OSC send error: " << e.what() << std::endl;
            return false;
        }
    }
    
private:
    std::unique_ptr<UdpTransmitSocket> socket_;
    std::string target_host_;
    int target_port_;
    bool connected_;
};

// OSCサーバークラス - OSCメッセージを受信する
class server {
public:
    server() : socket_(nullptr), port_(0), running_(false), low_latency_(false) {}
    
    ~server() {
        stop();
    }
    
    // メッセージコールバック設定
    void set_message_callback(message_handler_t callback) {
        listener_.set_message_callback(callback);
    }
    
    // エラーコールバック設定
    void set_error_callback(error_handler_t callback) {
        listener_.set_error_callback(callback);
    }
    
    // ポート設定を変更
    void set_port(int port) {
        if (port_ != port) {
            port_ = port;
            if (running_) {
                // サーバーが実行中なら再起動
                stop();
                start(port);
            }
        }
    }
    
    // 低レイテンシモード設定
    void set_low_latency(bool enable) {
        low_latency_ = enable;
    }
    
    // サーバー開始
    bool start(int port) {
        // 既に起動していれば停止
        if (running_) {
            stop();
        }
        
        try {
            port_ = port;
            
            // 受信用ソケット作成
            socket_ = std::make_unique<UdpListeningReceiveSocket>(
                IpEndpointName(IpEndpointName::ANY_ADDRESS, port_),
                &listener_);
            
            // リスニングスレッド開始
            receive_thread_ = std::thread([this]() {
                try {
                    if (socket_) {
                        // ソケットのスレッド優先度を設定
                        if (low_latency_) {
                            #ifdef __APPLE__
                            // macOSでの優先度設定
                            pthread_t this_thread = pthread_self();
                            struct sched_param param;
                            param.sched_priority = sched_get_priority_max(SCHED_RR);
                            pthread_setschedparam(this_thread, SCHED_RR, &param);
                            #endif
                        }
                        
                        // パケット受信ループ開始
                        socket_->Run();
                    }
                }
                catch (const std::exception& e) {
                    // エラーコールバックが設定されていれば通知
                    std::string error_msg = "OSC receive thread error: ";
                    error_msg += e.what();
                    listener_.set_error_callback(error_msg);
                }
            });
            
            running_ = true;
            return true;
        }
        catch (const std::exception& e) {
            std::cerr << "OSC server start error: " << e.what() << std::endl;
            stop();
            return false;
        }
    }
    
    // サーバー停止
    void stop() {
        if (socket_) {
            socket_->AsynchronousBreak();
        }
        
        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }
        
        socket_.reset();
        running_ = false;
    }
    
    // サーバーの状態確認
    bool is_running() const {
        return running_;
    }
    
    // 現在のポートを取得
    int get_port() const {
        return port_;
    }
    
    // アドレスとハンドラの関連付けを追加
    void add_handler(const std::string& pattern, message_handler_t handler) {
        registry_.register_handler(pattern, handler);
    }
    
private:
    std::unique_ptr<UdpListeningReceiveSocket> socket_;
    packet_listener listener_;
    std::thread receive_thread_;
    int port_;
    bool running_;
    bool low_latency_;
    handler_registry registry_;
};

// Claude Desktopハンドラークラス
class claude_handler {
public:
    claude_handler(outlet& output, outlet& error) 
        : output_(output), error_out_(error) {}
    
    void process_message(const std::string& address, const atoms& args) {
        // Claude Desktopメッセージ特有の処理
        if (address == "/claude/message") {
            handle_claude_message(args);
        }
        else if (address == "/claude/status") {
            handle_claude_status(args);
        }
        else if (address == "/claude/error") {
            handle_claude_error(args);
        }
        else {
            // 不明なメッセージはそのまま転送
            atoms out_args;
            out_args.push_back(symbol(address));
            for (const auto& arg : args) {
                out_args.push_back(arg);
            }
            output_.send(out_args);
        }
    }
    
private:
    outlet& output_;
    outlet& error_out_;
    
    // Claudeからのメッセージ処理
    void handle_claude_message(const atoms& args) {
        if (args.size() < 1) {
            error_out_.send("invalid_claude_message", "No message content");
            return;
        }
        
        // メッセージを出力
        atoms message_atoms;
        message_atoms.push_back(symbol("claude_message"));
        
        // 全ての引数を追加
        for (const auto& arg : args) {
            message_atoms.push_back(arg);
        }
        
        output_.send(message_atoms);
    }
    
    // Claudeの状態変更処理
    void handle_claude_status(const atoms& args) {
        if (args.size() < 1) {
            error_out_.send("invalid_claude_status", "No status information");
            return;
        }
        
        // 状態情報を出力
        atoms status_atoms;
        status_atoms.push_back(symbol("claude_status"));
        
        // 全ての引数を追加
        for (const auto& arg : args) {
            status_atoms.push_back(arg);
        }
        
        output_.send(status_atoms);
    }
    
    // Claudeのエラー処理
    void handle_claude_error(const atoms& args) {
        if (args.size() < 1) {
            error_out_.send("invalid_claude_error", "No error information");
            return;
        }
        
        // エラー情報を出力
        std::string error_type = "unknown_error";
        if (args.size() > 0 && args[0].type() == message_type::symbol_argument) {
            error_type = args[0];
        }
        
        std::string error_message = "";
        if (args.size() > 1) {
            for (size_t i = 1; i < args.size(); ++i) {
                if (i > 1) error_message += " ";
                error_message += std::string(args[i]);
            }
        }
        
        error_out_.send("claude_error", error_type, error_message);
    }
};

// メインの外部オブジェクトクラス
class osc_bridge : public object<osc_bridge> {
public:
    MIN_DESCRIPTION {"OSC Bridge for MCP-Max integration"};
    MIN_TAGS		{"mcp-bridge"};
    MIN_AUTHOR		{"Max9-Claude Desktop Project"};
    MIN_RELATED		{"udpsend, udpreceive, oscformat, oscparse"};
    
    // インレットとアウトレット
    inlet<>  input  { this, "(anything) Command to send via OSC" };
    outlet<> output { this, "(anything) Received OSC messages" };
    outlet<> error_out { this, "(anything) Error messages" };
    
    // 遅延処理用のキュー
    queue<> task_queue { this,
        MIN_FUNCTION {
            if (!deferred_tasks.empty()) {
                auto task = deferred_tasks.front();
                deferred_tasks.pop_front();
                task();
            }
            return {};
        }
    };

    // 接続設定の属性
    attribute<symbol> host { this, "host", "localhost",
        description {"OSCサーバーのホスト名またはIPアドレス"} };
        
    attribute<int> port_in { this, "port_in", 7500,
        description {"OSCメッセージ受信ポート"} };
        
    attribute<int> port_out { this, "port_out", 7400,
        description {"OSCメッセージ送信ポート"} };
        
    attribute<int> buffer_size { this, "buffer_size", 4096,
        description {"OSCバッファサイズ"} };
        
    // 接続状態（読み取り専用）
    attribute<bool> connected { this, "connected", false,
        description {"接続状態"} };
        
    // Max for Live互換性モード
    attribute<bool> m4l_compatibility { this, "m4l_compatibility", true,
        description {"Max for Live環境での動作を最適化"} };
        
    // 動的ポート割り当て
    attribute<bool> dynamic_ports { this, "dynamic_ports", true,
        description {"ポート競合を回避するための動的ポート割り当て"} };
        
    // 低レイテンシモード（高CPU負荷）
    attribute<bool> low_latency { this, "low_latency", false,
        description {"レイテンシを最小化（CPU負荷増加）"} };

    // コンストラクタ
    osc_bridge(const atoms& args = {}) {
        // 初期化は遅延させてMaxの初期化が完了してから行う
        
        // 引数の処理
        if (!args.empty()) {
            if (args.size() >= 1) {
                host = args[0];
            }
            if (args.size() >= 2) {
                port_in = args[1];
            }
            if (args.size() >= 3) {
                port_out = args[2];
            }
        }
    }
    
    // デストラクタ
    ~osc_bridge() {
        // 接続を閉じる
        disconnect_client_server();
    }
    
    // 接続メッセージ
    message<> connect { this, "connect", "Connect to OSC server",
        MIN_FUNCTION {
            // 現在の設定を反映
            update_connection_config();
            
            // 接続を開始
            bool success = connect_client_server();
            
            // 接続状態を更新
            connected = success;
            
            // クライアント、サーバーの接続状態を出力
            cout << "Client: " << (client_ ? "initialized" : "not initialized") << endl;
            cout << "Server: " << (server_ ? "initialized" : "not initialized") << endl;
            
            // 結果を出力
            if (success) {
                cout << "Connected to OSC server: " << host.get() << " in:" << port_in << " out:" << port_out << endl;
            } else {
                error_out.send("connect_failed");
            }
            
            return {};
        }
    };
    
    // 切断メッセージ
    message<> disconnect { this, "disconnect", "Disconnect from OSC server",
        MIN_FUNCTION {
            // 接続を閉じる
            disconnect_client_server();
            
            // 接続状態を更新
            connected = false;
            
            cout << "Disconnected from OSC server" << endl;
            
            return {};
        }
    };
    
    // ステータスメッセージ
    message<> status { this, "status", "Report current status",
        MIN_FUNCTION {
            // 現在の状態を出力
            cout << "OSC Bridge Status:" << endl;
            cout << "Host: " << host.get() << endl;
            cout << "Port In: " << port_in << endl;
            cout << "Port Out: " << port_out << endl;
            cout << "Connected: " << (connected ? "yes" : "no") << endl;
            cout << "Client: " << (client_ ? "initialized" : "not initialized") << endl;
            cout << "Server: " << (server_ ? "initialized" : "not initialized") << endl;
            cout << "Mappings: " << osc_mappings_.size() << endl;
            
            // いくつかのマッピングを表示
            int count = 0;
            for (const auto& mapping : osc_mappings_) {
                if (count++ < 5) { // 最初の5つだけ表示
                    cout << "  " << mapping.first << " -> " << mapping.second << endl;
                }
            }
            if (count > 5) {
                cout << "  ... and " << (count - 5) << " more" << endl;
            }
            
            return {};
        }
    };
    
    // マッピング登録メッセージ
    message<> map { this, "map", "Register OSC address pattern mapping",
        MIN_FUNCTION {
            if (args.size() < 2) {
                error_out.send("invalid_mapping", "Need pattern and callback");
                return {};
            }
            
            std::string pattern = args[0];
            std::string callback = args[1];
            
            if (map_address(pattern, callback)) {
                cout << "Mapped OSC pattern: " << pattern << " -> " << callback << endl;
            } else {
                error_out.send("mapping_failed", pattern);
            }
            
            return {};
        }
    };
    
    // OSCメッセージ送信
    message<> anything { this, "anything", "Send OSC message",
        MIN_FUNCTION {
            // 接続状態を確認
            if (!connected || !client_) {
                // 自動再接続を試みる
                update_connection_config();
                bool success = connect_client_server();
                connected = success;
                
                if (!connected || !client_) {
                    error_out.send("not_connected");
                    return {};
                }
            }
            
            // 最初の引数をアドレスとして使用
            symbol address = args[0];
            
            // 残りの引数をOSCメッセージの引数として使用
            atoms message_args;
            if (args.size() > 1) {
                message_args = atoms(args.begin() + 1, args.end());
            }
            
            // メッセージを送信
            bool success = client_->send(address, message_args);
            
            // 送信失敗時はエラーを報告
            if (!success) {
                error_out.send("send_failed", address);
            }
            
            return {};
        }
    };
    
    // 初期化完了時の処理
    message<> maxclass_setup { this, "maxclass_setup",
        MIN_FUNCTION {
            // M4L環境のライフサイクルイベントを登録
            void* c = args[0];
            c74::max::class_addmethod(
                (c74::max::t_class*)c,
                (c74::max::method)handle_m4l_liveset_loaded_wrapper,
                "liveset_loaded",
                c74::max::A_CANT,
                0
            );
            
            c74::max::class_addmethod(
                (c74::max::t_class*)c,
                (c74::max::method)handle_m4l_liveset_saved_wrapper,
                "liveset_saved",
                c74::max::A_CANT,
                0
            );
            
            c74::max::class_addmethod(
                (c74::max::t_class*)c,
                (c74::max::method)handle_m4l_liveset_closed_wrapper,
                "liveset_closed",
                c74::max::A_CANT,
                0
            );
            
            c74::max::class_addmethod(
                (c74::max::t_class*)c,
                (c74::max::method)handle_m4l_liveset_new_wrapper,
                "liveset_new",
                c74::max::A_CANT,
                0
            );
            
            // 初期化処理
            defer_task([this]() {
                init_client_server();
                connect_client_server();
                
                // Claude Desktop用のデフォルトマッピングを作成
                map_address("/claude/*", "/claude/*");
                
                cout << "MCP OSC Bridge initialized with M4L compatibility mode" << endl;
            });
            
            return {};
        }
    };
    
private:
    // クライアントとサーバー
    std::unique_ptr<client> client_;
    std::unique_ptr<server> server_;
    
    // Claude Desktopハンドラー
    std::unique_ptr<claude_handler> claude_handler_;
    
    // 遅延タスク用キュー
    std::deque<std::function<void()>> deferred_tasks;
    
    // OSCアドレスパターンマッピング
    std::map<std::string, std::string> osc_mappings_;
    
    // タスクの遅延実行
    void defer_task(std::function<void()> task) {
        deferred_tasks.push_back(task);
        task_queue.set();
    }
    
    // 静的ラッパー関数（M4Lライフサイクルイベント用）
    static void* handle_m4l_liveset_loaded_wrapper(void* x) {
        osc_bridge* self = (osc_bridge*)x;
        if (self) {
            self->handle_m4l_liveset_loaded(x);
        }
        return nullptr;
    }
    
    static void* handle_m4l_liveset_saved_wrapper(void* x) {
        osc_bridge* self = (osc_bridge*)x;
        if (self) {
            self->handle_m4l_liveset_saved(x);
        }
        return nullptr;
    }
    
    static void* handle_m4l_liveset_closed_wrapper(void* x) {
        osc_bridge* self = (osc_bridge*)x;
        if (self) {
            self->handle_m4l_liveset_closed(x);
        }
        return nullptr;
    }
    
    static void* handle_m4l_liveset_new_wrapper(void* x) {
        osc_bridge* self = (osc_bridge*)x;
        if (self) {
            self->handle_m4l_liveset_new(x);
        }
        return nullptr;
    }
    
    // M4Lライフサイクルイベントハンドラ（Liveセット読み込み時）
    void handle_m4l_liveset_loaded(void* x) {
        cout << "Max for Live: Liveset loaded event received" << endl;
        
        // 切断されていれば再接続
        if (!connected) {
            defer_task([this]() {
                cout << "Reconnecting after liveset loaded..." << endl;
                connect_client_server();
            });
        }
        
        // 接続状態を更新
        defer_task([this]() {
            update_connection_config();
            send_status_update("liveset_loaded");
        });
    }
    
    // M4Lライフサイクルイベントハンドラ（Liveセット保存時）
    void handle_m4l_liveset_saved(void* x) {
        cout << "Max for Live: Liveset saved event received" << endl;
        
        // 現在の接続状態を保存できるように状態更新
        defer_task([this]() {
            send_status_update("liveset_saved");
        });
    }
    
    // M4Lライフサイクルイベントハンドラ（Liveセットクローズ時）
    void handle_m4l_liveset_closed(void* x) {
        cout << "Max for Live: Liveset closed event received" << endl;
        
        // グレースフルに接続を閉じる
        defer_task([this]() {
            // 終了状態を送信してから接続を閉じる
            send_status_update("liveset_closed");
            disconnect_client_server();
        });
    }
    
    // M4Lライフサイクルイベントハンドラ（Liveセット新規作成時）
    void handle_m4l_liveset_new(void* x) {
        cout << "Max for Live: Liveset new event received" << endl;
        
        // 新規セットの初期化
        defer_task([this]() {
            // 接続設定を更新
            update_connection_config();
            
            // 新規セットの場合は再接続を試みる
            if (!connected) {
                cout << "Reconnecting for new liveset..." << endl;
                connect_client_server();
            }
            
            // 状態更新を送信
            send_status_update("liveset_new");
        });
    }
    
    // Claude Desktopメッセージの処理
    void handle_claude_message(const std::string& address, const atoms& args) {
        // Claude Desktopハンドラーが初期化されていなければ作成
        if (!claude_handler_) {
            claude_handler_ = std::make_unique<claude_handler>(output, error_out);
        }
        
        // Claude Desktopハンドラーにメッセージを渡す
        claude_handler_->process_message(address, args);
    }
    
    // 状態更新を送信するヘルパー関数
    void send_status_update(const std::string& event_type) {
        atoms status_info;
        status_info.push_back(symbol("status"));
        status_info.push_back(symbol(event_type));
        status_info.push_back(connected ? 1 : 0);
        status_info.push_back(static_cast<int>(port_in));
        status_info.push_back(static_cast<int>(port_out));
        status_info.push_back(symbol(host.get()));
        
        output.send(status_info);
    }
    
    // atomsオブジェクトを文字列に変換するユーティリティ関数
    std::string atoms_to_string(const atoms& args) {
        std::ostringstream ss;
        for (size_t i = 0; i < args.size(); ++i) {
            const auto& a = args[i];
            if (i > 0) {
                ss << " ";
            }
            
            if (a.type() == message_type::float_argument) {
                ss << a;
            }
            else if (a.type() == message_type::int_argument) {
                ss << a;
            }
            else if (a.type() == message_type::symbol_argument) {
                ss << a;
            }
            else {
                ss << "[" << a.type() << "]";
            }
        }
        return ss.str();
    }
    
    // 接続設定を更新
    void update_connection_config() {
        // ホスト名とポートの設定を更新
        auto host_str = std::string(host.get());
        int in_port = port_in;
        int out_port = port_out;
        
        // M4L環境で動的ポート割り当てが有効な場合、使用可能なポートを探す
        if (m4l_compatibility && dynamic_ports) {
            // M4L互換性モードでは、49152～65535の範囲でポートを動的に割り当てる
            if (in_port < 49152 || in_port > 65535) {
                // 標準的な動的ポート範囲（IANA推奨）を使用
                in_port = find_available_port(49152, 65535);
                cout << "Dynamic port allocation: Using port " << in_port << " for input" << endl;
                port_in = in_port;
            }
            
            if (out_port < 49152 || out_port > 65535) {
                // 受信ポートとは別のポートを使用
                out_port = find_available_port(49152, 65535, in_port);
                cout << "Dynamic port allocation: Using port " << out_port << " for output" << endl;
                port_out = out_port;
            }
        }
        
        // クライアントとサーバーの設定を更新
        if (client_) {
            client_->set_target(host_str, out_port);
        }
        
        if (server_) {
            server_->set_port(in_port);
        }
    }
    
    // 使用可能なポートを探す
    int find_available_port(int min_port, int max_port, int exclude_port = -1) {
        // このシンプルな実装では、ポート範囲からランダムに選択する
        // 実際の実装では、UDPソケットを開いてバインドをテストするべき
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(min_port, max_port);
        
        int port = distrib(gen);
        
        // 除外ポートと同じ場合は別のポートを選択
        if (port == exclude_port) {
            port = (port + 1) % (max_port - min_port + 1) + min_port;
        }
        
        return port;
    }
    
    // クライアントとサーバーを初期化
    void init_client_server() {
        try {
            // クライアントの作成（存在しない場合のみ）
            if (!client_) {
                client_ = std::make_unique<client>();
                cout << "OSC client initialized" << endl;
            }
            
            // サーバーの作成（存在しない場合のみ）
            if (!server_) {
                server_ = std::make_unique<server>();
                
                // OSCメッセージ受信時のコールバック設定
                server_->set_message_callback([this](const std::string& address, const atoms& args) {
                    // メインスレッドでメッセージ処理を行う
                    defer_task([this, address, args]() {
                        handle_incoming_message(address, args);
                    });
                });
                
                // エラー発生時のコールバック設定
                server_->set_error_callback([this](const std::string& error_msg) {
                    defer_task([this, error_msg]() {
                        error_out.send("server_error", error_msg);
                        cout << "OSC Server error: " << error_msg << endl;
                    });
                });
                
                cout << "OSC server initialized" << endl;
            }
        }
        catch (const std::exception& e) {
            error_out.send("init_error", e.what());
            cout << "Error initializing OSC client/server: " << e.what() << endl;
        }
    }
    
    // クライアントとサーバーを接続
    bool connect_client_server() {
        bool success = true;
        
        try {
            // 初期化（必要な場合のみ）
            init_client_server();
            
            // 接続設定を更新
            update_connection_config();
            
            // クライアントを接続
            if (client_) {
                success = client_->connect(std::string(host.get()), port_out);
            } else {
                success = false;
            }
            
            // サーバーを開始
            if (server_ && success) {
                success = server_->start(port_in);
            } else {
                success = false;
            }
            
            // 低レイテンシモードの設定
            if (server_ && low_latency && success) {
                server_->set_low_latency(true);
            }
            
            // 接続状態を更新
            connected = success;
            
            // 既存のマッピングを再登録
            if (success) {
                for (const auto& mapping : osc_mappings_) {
                    server_->add_handler(mapping.first, [this, callback=mapping.second](const std::string& address, const atoms& args) {
                        defer_task([this, callback, address, args]() {
                            handle_incoming_message(callback, address, args);
                        });
                    });
                }
            }
            
            return success;
        }
        catch (const std::exception& e) {
            error_out.send("connect_error", e.what());
            cout << "Error connecting OSC client/server: " << e.what() << endl;
            return false;
        }
    }
    
    // クライアントとサーバーを切断
    void disconnect_client_server() {
        try {
            // サーバーを停止
            if (server_) {
                server_->stop();
            }
            
            // クライアントを切断
            if (client_) {
                client_->disconnect();
            }
            
            // 接続状態を更新
            connected = false;
        }
        catch (const std::exception& e) {
            error_out.send("disconnect_error", e.what());
            cout << "Error disconnecting OSC client/server: " << e.what() << endl;
        }
    }
    
    // 受信メッセージの処理
    void handle_incoming_message(const std::string& address, const atoms& args) {
        // アドレスパターンの確認
        if (address.rfind("/claude/", 0) == 0) {
            // Claude Desktop専用メッセージの処理
            handle_claude_message(address, args);
            return;
        }
        
        // 通常のOSCメッセージとして出力
        atoms message_atoms;
        message_atoms.push_back(symbol(address));
        
        // 残りの引数を追加
        for (size_t i = 0; i < args.size(); ++i) {
            message_atoms.push_back(args[i]);
        }
        
        // メッセージをアウトレットに送信
        output.send(message_atoms);
    }
    
    // マッピング用の受信メッセージ処理
    void handle_incoming_message(const std::string& callback_pattern, const std::string& address, const atoms& args) {
        // コールバックパターンに基づいて処理
        // /claude/* パターンの場合はClaude Desktop関連の処理を行う
        if (callback_pattern == "/claude/*") {
            handle_claude_message(address, args);
            return;
        }
        
        // メッセージの構築
        atoms message_args;
        message_args.push_back(symbol(callback_pattern));
        message_args.push_back(symbol(address));
        
        // 引数を追加
        for (const auto& arg : args) {
            message_args.push_back(arg);
        }
        
        // メッセージをログに出力
        if (false) { // デバッグモードの場合のみ
            cout << "Mapped OSC message: " << callback_pattern << " " << address << " " << atoms_to_string(args) << endl;
        }
        
        // アウトレットに送信
        output.send(message_args);
    }
    
    // OSCアドレスパターンからハンドラーへのマッピングを登録
    bool map_address(const std::string& pattern, const std::string& callback_pattern) {
        // 接続状態を確認
        if (!server_) {
            return false;
        }
        
        // マッピングをサーバーに登録
        server_->add_handler(pattern, [this, callback=callback_pattern](const std::string& address, const atoms& args) {
            // メインスレッドで処理するために遅延実行
            defer_task([this, callback, address, args]() {
                handle_incoming_message(callback, address, args);
            });
        });
        
        // マッピングを保存（サーバー再接続時に使用）
        osc_mappings_[pattern] = callback_pattern;
        
        return true;
    }
};

} // namespace osc
} // namespace mcp

// 外部オブジェクトとして登録
MIN_EXTERNAL(mcp::osc::osc_bridge);
