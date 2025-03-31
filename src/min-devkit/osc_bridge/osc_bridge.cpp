/// @file
///	@ingroup 	mcpmodules
///	@copyright	Copyright 2025 The Max 9-Claude Desktop Integration Project. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

// Issue 19: Min-DevKitヘッダーの重複インクルード対応
// Min-DevKitの設計思想に従い、単一ファイル内に実装を完結
// Min-DevKitヘッダーは各ソースファイルで一度だけインクルード
#include "c74_min.h"

// 標準ライブラリ
#include <string>
#include <memory>
#include <atomic>
#include <deque>
#include <mutex>
#include <functional>
#include <thread>
#include <map>
#include <sstream>
#include <random>

// OSCpack includes
#include "ip/UdpSocket.h"
#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"

// Issue 19: 独自のラッパーヘッダー
#include "max_wrapper.hpp"

// Min-DevKitの名前空間を使用
using namespace c74::min;

// OSC関連の独自型定義とユーティリティ
namespace mcp {
namespace osc {

// OSC独自型定義
using osc_value = std::string;  // 演算のための簡略型
 using osc_args = std::vector<osc_value>;

/**
 * OSC接続設定
 */
struct connection_config {
    std::string host = "127.0.0.1";  // ホスト名/IPアドレス
    int port_in = 7500;              // 受信ポート
    int port_out = 7400;             // 送信ポート
    int buffer_size = 4096;          // バッファサイズ
    bool dynamic_ports = true;       // M4L向け動的ポート割り当て
    bool low_latency = false;        // 低レイテンシモード（CPU負荷が増加）
    bool m4l_compatibility = true;   // Max for Live互換モード
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
    timeout,             // タイムアウト
    unknown_error        // 不明なエラー
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
using message_handler = std::function<void(const std::string& address, const osc_args& args)>;

/**
 * エラーハンドラの型定義
 */
using error_handler = std::function<void(const error_info& error)>;

// Min-DevKit型とOSC独自型の変換関数

/**
 * OSC独自型（osc_args）からMin-DevKit型（atoms）への変換
 */
inline atoms convert_to_atoms(const osc_args& args) {
    atoms result;
    for (const auto& arg : args) {
        // 型ごとの適切な変換を実装
        // 実装簡略化のため、現在は文字列として扱う
        result.push_back(symbol(arg));
    }
    return result;
}

/**
 * Min-DevKit型（atoms）からOSC独自型（osc_args）への変換
 */
inline osc_args convert_to_osc_args(const atoms& args) {
    osc_args result;
    for (const auto& arg : args) {
        if (arg.type() == message_type::symbol_argument) {
            std::string value = arg;
            result.push_back(value);
        } else if (arg.type() == message_type::float_argument) {
            float value = arg;
            result.push_back(std::to_string(value));
        } else if (arg.type() == message_type::int_argument) {
            int value = arg;
            result.push_back(std::to_string(value));
        } else {
            // 他の型は空文字列として扱う
            result.push_back("");
        }
    }
    return result;
}

// Min-DevKitメッセージハンドラ用アダプタ

/**
 * OSC独自型向けメッセージハンドラをMin-DevKit型のハンドラとして使用するアダプタ
 */
class message_adapter {
public:
    message_adapter(message_handler handler) : handler_(handler) {}
    
    // Min-DevKitからの呼び出し用インターフェース
    void operator()(const std::string& address, const atoms& args) {
        handler_(address, convert_to_osc_args(args));
    }
    
    // OSC独自型からの吹き出し用インターフェース
    void invoke_with_osc_args(const std::string& address, const osc_args& args) {
        handler_(address, args);
    }
    
private:
    message_handler handler_;
};

/**
 * ハンドラーレジストリクラス - OSCアドレスパターンとハンドラーの関連付けを管理
 */
class handler_registry {
public:
    // パターンとハンドラーの登録
    void register_handler(const std::string& pattern, message_handler handler) {
        handlers_[pattern] = handler;
    }
    
    // パターンに一致するハンドラーを検索
    message_handler find_handler(const std::string& address) const {
        // 完全一致の検索
        auto it = handlers_.find(address);
        if (it != handlers_.end()) {
            return it->second;
        }
        
        // 将来的にはOSCパターンマッチング（ワイルドカード等）を実装
        // 現在は完全一致のみサポート
        
        return nullptr; // 一致するハンドラーなし
    }
    
    // 登録されているハンドラーの数を取得
    size_t size() const {
        return handlers_.size();
    }
    
    // イテレーター処理のサポート
    auto begin() const { return handlers_.begin(); }
    auto end() const { return handlers_.end(); }
    
private:
    // パターンとハンドラーのマップ
    std::map<std::string, message_handler> handlers_;
};

// OSCパケットリスナークラス - oscpackを拡張
class packet_listener : public ::osc::OscPacketListener {
public:
    packet_listener() = default;
    
    void set_message_callback(message_handler handler) {
        message_callback_ = handler;
    }
    
    void set_error_callback(std::function<void(const error_info&)> handler) {
        error_callback_ = handler;
    }
    
    // 受信パケットの処理
    // Issue 19: OSC独自型(osc_args)を使用するように変更
    virtual void ProcessMessage(const ::osc::ReceivedMessage& message, 
                              const IpEndpointName& remote_endpoint) override {
        try {
            // アドレスパターンを取得
            std::string address = message.AddressPattern();
            
            // 引数をOSC独自型のosc_argsオブジェクトに変換
            osc_args osc_arguments;
            
            // 引数のイテレーション
            for (::osc::ReceivedMessage::const_iterator arg = message.ArgumentsBegin();
                 arg != message.ArgumentsEnd(); ++arg) {
                if (arg->IsFloat()) {
                    osc_arguments.push_back(std::to_string(arg->AsFloat()));
                }
                else if (arg->IsDouble()) {
                    osc_arguments.push_back(std::to_string(arg->AsDouble()));
                }
                else if (arg->IsInt32()) {
                    osc_arguments.push_back(std::to_string(arg->AsInt32()));
                }
                else if (arg->IsInt64()) {
                    osc_arguments.push_back(std::to_string(arg->AsInt64()));
                }
                else if (arg->IsString()) {
                    osc_arguments.push_back(arg->AsString());
                }
                else if (arg->IsBool()) {
                    osc_arguments.push_back(arg->AsBool() ? "true" : "false");
                }
                // 他の型は必要に応じて追加
            }
            
            // コールバックを呼び出す
            if (message_callback_) {
                // OSC独自型を使用してコールバックを呼び出す
                message_callback_(address, osc_arguments);
            }
        }
        catch (::osc::Exception& e) {
            if (error_callback_) {
                error_info err;
                err.code = osc_error_code::receive_failed;
                err.message = std::string("Error parsing OSC message: ") + e.what();
                error_callback_(err);
            }
        }
        catch (const std::exception& e) {
            if (error_callback_) {
                error_info err;
                err.code = osc_error_code::unknown_error;
                err.message = std::string("Unknown error processing OSC message: ") + e.what();
                error_callback_(err);
            }
        }
    }
    
private:
    message_handler message_callback_;
    std::function<void(const error_info&)> error_callback_;
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
    
    // OSCメッセージ送信 - Min-DevKit型を使用するバージョン
    // Issue 19: Min-DevKit依存を最小化するためMin-DevKit型とOSC独自型の両方をサポート
    bool send(const std::string& address, const atoms& args) {
        // 独自型に変換して内部関数を呼び出す
        return send_internal(address, convert_to_osc_args(args));
    }
    
    // OSCメッセージ送信 - OSC独自型を使用するバージョン
    bool send_osc(const std::string& address, const osc_args& args) {
        return send_internal(address, args);
    }
    
    // 内部実装 - OSC独自型のみを使用
    bool send_internal(const std::string& address, const osc_args& args) {
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
            for (const auto& arg : args) {
                // 現在はosc_valueは全て文字列として処理
                // 実际の実装ではデータ型を解析し適切な型で送信する必要がある
                packet << arg.c_str();
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
    void set_message_callback(message_handler callback) {
        listener_.set_message_callback(callback);
    }
    
    // エラーコールバック設定
    void set_error_callback(std::function<void(const error_info&)> callback) {
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
                    error_info err;
                    err.code = osc_error_code::receive_failed;
                    err.message = std::string("OSC receive thread error: ") + e.what();
                    error_callback_(err);
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
    void add_handler(const std::string& pattern, message_handler handler) {
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
    
    void error_callback_(const error_info& error) {
        // 空の実装、サーバークラスで使用される場合は別途設定される
    }
};

// Claude Desktopハンドラークラス
class claude_handler {
public:
    claude_handler(outlet<>& output, outlet<>& error_out) 
        : output_(output), error_out_(error_out) {}
    
    // Issue 19: Min-DevKit型を使用するためのインターフェース
    void process_message(const std::string& address, const atoms& args) {
        // Min-DevKit型からOSC独自型に変換
        process_message_internal(address, convert_to_osc_args(args));
    }
    
    // Issue 19: OSC独自型を使用するためのインターフェース
    void process_message_osc(const std::string& address, const osc_args& args) {
        process_message_internal(address, args);
    }
    
    // 内部実装 - OSC独自型を使用
    void process_message_internal(const std::string& address, const osc_args& args) {
        // Claude Desktopメッセージ特有の処理
        if (address == "/claude/message") {
            handle_claude_message_internal(args);
        }
        else if (address == "/claude/status") {
            handle_claude_status_internal(args);
        }
        else if (address == "/claude/error") {
            handle_claude_error_internal(args);
        }
        else {
            // 不明なメッセージはそのまま転送
            atoms out_args;
            out_args.push_back(symbol(address));
            
            // OSC独自型からMin-DevKit型に変換
            atoms converted_args = convert_to_atoms(args);
            for (const auto& arg : converted_args) {
                out_args.push_back(arg);
            }
            output_.send(out_args);
        }
    }
    
private:
    outlet<>& output_;
    outlet<>& error_out_;
    
    // Issue 19: Min-DevKit型から独自型に変換するメソッドを追加
    
    // Claudeからのメッセージ処理 - Min-DevKit型バージョン
    void handle_claude_message(const atoms& args) {
        // Min-DevKit型からOSC独自型に変換して内部処理を呼び出す
        handle_claude_message_internal(convert_to_osc_args(args));
    }
    
    // Claudeからのメッセージ処理 - OSC独自型バージョン
    void handle_claude_message_internal(const osc_args& args) {
        if (args.size() < 1) {
            error_out_.send("invalid_claude_message", "No message content");
            return;
        }
        
        // メッセージを出力
        atoms message_atoms;
        message_atoms.push_back(symbol("claude_message"));
        
        // 全ての引数を追加（OSC独自型からMin-DevKit型に変換）
        atoms converted_args = convert_to_atoms(args);
        for (const auto& arg : converted_args) {
            message_atoms.push_back(arg);
        }
        
        output_.send(message_atoms);
    }
    
    // Claudeの状態変更処理 - Min-DevKit型バージョン
    void handle_claude_status(const atoms& args) {
        // Min-DevKit型からOSC独自型に変換して内部処理を呼び出す
        handle_claude_status_internal(convert_to_osc_args(args));
    }
    
    // Claudeの状態変更処理 - OSC独自型バージョン
    void handle_claude_status_internal(const osc_args& args) {
        if (args.size() < 1) {
            error_out_.send("invalid_claude_status", "No status information");
            return;
        }
        
        // 状態情報を出力
        atoms status_atoms;
        status_atoms.push_back(symbol("claude_status"));
        
        // 全ての引数を追加（OSC独自型からMin-DevKit型に変換）
        atoms converted_args = convert_to_atoms(args);
        for (const auto& arg : converted_args) {
            status_atoms.push_back(arg);
        }
        
        output_.send(status_atoms);
    }
    
    // Claudeのエラー処理 - Min-DevKit型バージョン
    void handle_claude_error(const atoms& args) {
        // Min-DevKit型からOSC独自型に変換して内部処理を呼び出す
        handle_claude_error_internal(convert_to_osc_args(args));
    }
    
    // Claudeのエラー処理 - OSC独自型バージョン
    void handle_claude_error_internal(const osc_args& args) {
        if (args.size() < 1) {
            error_out_.send("invalid_claude_error", "No error information");
            return;
        }
        
        // エラー情報を出力
        std::string error_type = "unknown_error";
        if (args.size() > 0) {
            error_type = args[0];
        }
        
        std::string error_message = "";
        if (args.size() > 1) {
            for (size_t i = 1; i < args.size(); ++i) {
                if (i > 1) error_message += " ";
                error_message += args[i];
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
        // Issue 20: 非常に目立つデバッグ情報を追加
        cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
        cout << "!!!!! OSC BRIDGE コンストラクタ開始 !!!!!" << endl;
        cout << "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" << endl;
        error_out.send("osc_bridge_init_start");
        
        // 初期化は遅延させてMaxの初期化が完了してから行う
        // 引数の処理
        if (!args.empty()) {
            if (args.size() >= 1) {
                host = args[0];
                cout << "Debug: ホスト設定: " << static_cast<string>(host.get()) << endl;
            }
            if (args.size() >= 2) {
                port_in = args[1];
                cout << "Debug: 受信ポート設定: " << port_in << endl;
            }
            if (args.size() >= 3) {
                port_out = args[2];
                cout << "Debug: 送信ポート設定: " << port_out << endl;
            }
        }
        
        // 属性の初期値を表示
        cout << "Debug: 初期設定 - host: " << static_cast<string>(host.get()) << ", port_in: " << port_in 
             << ", port_out: " << port_out << ", dynamic_ports: " << (dynamic_ports ? "true" : "false") << endl;
        
        // 初期化処理を遅延させる
        cout << "Debug: 初期化を遅延キューに追加" << endl;
        
        // Min-DevKitの遅延処理メカニズムを使用
        // 遅延タスクを登録
        deferred_tasks.push_back([this]{
            cout << "Debug: 遅延初期化処理開始" << endl;
            init_client_server(); 
            cout << "Debug: 遅延初期化処理完了" << endl;
        });
        
        // キューの処理をトリガー
        task_queue.set();
        
        cout << "Debug: osc_bridge コンストラクタ完了" << endl;
    }
    
    // デストラクタ
    ~osc_bridge() {
        // 接続を閉じる
        disconnect_client_server();
    }
    
    // 接続メッセージ - Issue 20: 超シンプル化版
    message<> m_connect { this, "connect", "Connect to OSC server",
        MIN_FUNCTION {
            // 超シンプルな出力のみにしてテスト
            cout << "===== メッセージ受信確認: CONNECT =====" << endl;
            
            // メッセージを出力ポートへ送信
            output.send("connect_received");
            
            // エラーポートにもテストメッセージを送信
            error_out.send("connect_test");
            
            // 完了の出力
            cout << "===== CONNECTメッセージ処理完了 =====" << endl;
            
            return {};
        }
    };
    
    // 切断メッセージ
    message<> m_disconnect { this, "disconnect", "Disconnect from OSC server",
        MIN_FUNCTION {
            // Issue 20: デバッグ情報と安全なエラーハンドリングを追加
            cout << "Debug: disconnect メッセージ受信" << endl;
            
            try {
                // オブジェクトの状態を確認
                cout << "Debug: 切断前の状態 - client_: " << (client_ ? "初期化済み" : "null") 
                     << ", server_: " << (server_ ? "初期化済み" : "null")
                     << ", connected: " << (connected ? "true" : "false") << endl;
                
                // 接続を閉じる
                cout << "Debug: disconnect_client_server() 呼び出し" << endl;
                disconnect_client_server();  // 独自のエラーハンドリング付き
                cout << "Debug: disconnect_client_server() 完了" << endl;
                
                // 接続状態を確実に更新 (冗長だが安全のため)
                connected = false;
                cout << "Debug: connected属性をfalseに設定" << endl;
                
                // 特にクラッシュする場所を確認
                cout << "Debug: 切断処理完了後の状態 - client_: " << (client_ ? "残存" : "nullまたはリセット済み") 
                     << ", server_: " << (server_ ? "残存" : "nullまたはリセット済み")
                     << ", connected: " << (connected ? "true" : "false") << endl;
                
                cout << "Debug: Disconnected from OSC server" << endl;
            } catch (const std::exception& e) {
                // 例外発生時は確実に接続状態を更新してエラーを報告
                cout << "Debug: disconnectメッセージ処理中に例外発生: " << e.what() << endl;
                connected = false;
                error_out.send("disconnect_error", e.what());
            } catch (...) {
                // 不明な例外もキャッチ
                cout << "Debug: disconnectメッセージ処理中に不明な例外発生" << endl;
                connected = false;
                error_out.send("disconnect_unknown_error");
            }
            
            return {};
        }
    };
    
    // ステータスメッセージ
    message<> m_status { this, "status", "Report current status",
        MIN_FUNCTION {
            // Issue 20: デバッグ情報を追加
            cout << "Debug: status メッセージ受信" << endl;
            
            // 現在の状態を出力
            cout << "Debug: OSC Bridge Status:" << endl;
            cout << "Debug: Host: " << host.get() << endl;
            cout << "Debug: Port In: " << port_in << endl;
            cout << "Debug: Port Out: " << port_out << endl;
            cout << "Debug: Connected: " << (connected ? "yes" : "no") << endl;
            cout << "Debug: Client: " << (client_ ? "initialized" : "not initialized") << endl;
            cout << "Debug: Server: " << (server_ ? "initialized" : "not initialized") << endl;
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
    message<> m_map { this, "map", "Register OSC address pattern mapping",
        MIN_FUNCTION {
            // Issue 20: デバッグ情報を追加
            cout << "Debug: map メッセージ受信 - 引数数: " << args.size() << endl;
            
            if (args.size() < 2) {
                cout << "Debug: 引数不足 - error_out.send(\"invalid_mapping\")呼び出し" << endl;
                error_out.send("invalid_mapping", "Need pattern and callback");
                return {};
            }
            
            std::string pattern = args[0];
            std::string callback = args[1];
            cout << "Debug: マッピング設定 - pattern: " << pattern << ", callback: " << callback << endl;
            
            cout << "Debug: map_address() 呼び出し" << endl;
            if (map_address(pattern, callback)) {
                cout << "Debug: マッピング成功 - OSC pattern: " << pattern << " -> " << callback << endl;
            } else {
                cout << "Debug: マッピング失敗 - error_out.send(\"mapping_failed\")呼び出し" << endl;
                error_out.send("mapping_failed", pattern);
            }
            
            return {};
        }
    };
    
    // OSCメッセージ送信
    message<> m_anything { this, "anything", "Send OSC message",
        MIN_FUNCTION {
            // Issue 20: デバッグ情報を追加
            cout << "Debug: anything メッセージ受信 - 引数数: " << args.size() << endl;
            if (args.size() > 0) {
                cout << "Debug: 最初の引数: " << string(args[0]) << endl;
            }
            
            // 接続状態を確認
            cout << "Debug: 接続状態の確認 - connected: " << (connected ? "true" : "false") << ", client_: " << (client_ ? "存在する" : "存在しない") << endl;
            if (!connected || !client_) {
                cout << "Debug: 接続されていないかクライアントが初期化されていないため、自動再接続を試みる" << endl;
                
                // 自動再接続を試みる
                cout << "Debug: update_connection_config() 呼び出し" << endl;
                update_connection_config();
                cout << "Debug: connect_client_server() 呼び出し" << endl;
                bool success = connect_client_server();
                connected = success;
                cout << "Debug: 自動再接続結果: " << (success ? "成功" : "失敗") << endl;
                
                if (!connected || !client_) {
                    cout << "Debug: 再接続失敗 - error_out.send(\"not_connected\")呼び出し" << endl;
                    error_out.send("not_connected");
                    return {};
                }
            }
            
            // 最初の引数をアドレスとして使用
            symbol address = args[0];
            cout << "Debug: 送信アドレス: " << string(address) << endl;
            
            // 残りの引数をOSCメッセージの引数として使用
            atoms message_args;
            if (args.size() > 1) {
                message_args = atoms(args.begin() + 1, args.end());
                cout << "Debug: 送信引数数: " << message_args.size() << endl;
            } else {
                cout << "Debug: 送信引数なし" << endl;
            }
            
            // メッセージを送信
            cout << "Debug: client_->send() 呼び出し" << endl;
            bool success = client_->send(address, message_args);
            cout << "Debug: 送信結果: " << (success ? "成功" : "失敗") << endl;
            
            // 送信失敗時はエラーを報告
            if (!success) {
                cout << "Debug: 送信失敗 - error_out.send(\"send_failed\")呼び出し" << endl;
                error_out.send("send_failed", address);
            }
            
            return {};
        }
    };
    
    // 初期化完了時の処理
    message<> m_maxclass_setup { this, "maxclass_setup",
        MIN_FUNCTION {
            // M4L環境のライフサイクルイベントを登録
            // Issue 19: max_object_t<osc_bridge>の代わりにmax_wrapper::を使用
            void* c = args[0];
            c74::max::class_addmethod(
                (c74::max::t_class*)c,
                (c74::max::method)max_wrapper::handle_m4l_liveset_loaded_wrapper,
                "liveset_loaded",
                c74::max::A_CANT,
                0
            );
            
            c74::max::class_addmethod(
                (c74::max::t_class*)c,
                (c74::max::method)max_wrapper::handle_m4l_liveset_saved_wrapper,
                "liveset_saved",
                c74::max::A_CANT,
                0
            );
            
            c74::max::class_addmethod(
                (c74::max::t_class*)c,
                (c74::max::method)max_wrapper::handle_m4l_liveset_closed_wrapper,
                "liveset_closed",
                c74::max::A_CANT,
                0
            );
            
            c74::max::class_addmethod(
                (c74::max::t_class*)c,
                (c74::max::method)max_wrapper::handle_m4l_liveset_new_wrapper,
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
    // クライアントとサーバーのインスタンス
    std::unique_ptr<client> client_;
    std::unique_ptr<server> server_;
    std::unique_ptr<claude_handler> claude_handler_;
    
    // アドレスパターンマッピング
    std::map<std::string, std::string> osc_mappings_;
    std::mutex osc_mutex_;
    
    // 遅延タスクキュー
    std::deque<std::function<void()>> deferred_tasks;
    
    // クライアント/サーバー初期化
    void init_client_server() {
        // 既存のインスタンスをクリア
        client_.reset();
        server_.reset();
        claude_handler_.reset();
        
        // 新しいインスタンスを作成
        client_ = std::make_unique<client>();
        server_ = std::make_unique<server>();
        claude_handler_ = std::make_unique<claude_handler>(output, error_out);
        
        // サーバーの設定
        if (server_) {
            // メッセージハンドラを設定
            // Issue 19: atoms型からosc_args型に変更して型一致を確保
            server_->set_message_callback([this](const std::string& address, const osc_args& args) {
                handle_osc_message_osc(address, args);
            });
            
            // エラーハンドラを設定
            server_->set_error_callback([this](const error_info& error) {
                handle_error(error);
            });
            
            // 低レイテンシモード設定
            server_->set_low_latency(low_latency);
        }
    }
    
    // 接続設定を更新
    void update_connection_config() {
        if (m4l_compatibility && dynamic_ports) {
            // M4L互換性モードで動的ポート設定が有効な場合は動的ポートを使用
            if (port_in <= 0) {
                port_in = generate_dynamic_port();
            }
        }
    }
    
    // 接続確立
    bool connect_client_server() {
        bool success = true;
        
        // クライアント/サーバーが存在しない場合は初期化
        if (!client_ || !server_) {
            init_client_server();
        }
        
        // クライアント接続
        if (client_) {
            std::string host_str = static_cast<std::string>(host.get());
            if (!client_->connect(host_str, port_in.get())) {
                error_out.send("client_connect_failed", host_str, port_in.get());
                success = false;
            }
        }
        
        // サーバー接続
        if (server_) {
            if (!server_->start(port_in.get())) {
                error_out.send("server_start_failed", port_in.get());
                success = false;
            }
        }
        
        // 接続状態を更新
        connected = success;
        
        return success;
    }
    
    // 接続切断
    void disconnect_client_server() {
        // Issue 20: クラッシュ防止のためのエラーハンドリング追加
        try {
            cout << "Debug: disconnect_client_server()内 - client_の切断処理開始" << endl;
            if (client_) {
                client_->disconnect();
                cout << "Debug: client_->disconnect()完了" << endl;
            } else {
                cout << "Debug: client_はnull" << endl;
            }
            
            cout << "Debug: disconnect_client_server()内 - server_の停止処理開始" << endl;
            if (server_) {
                // 万が一の場合のクラッシュ回避メカニズム
                // M4L環境での問題を回避するため、複数のステップに分ける
                try {
                    server_->stop();
                    cout << "Debug: server_->stop()完了" << endl;
                } catch (const std::exception& e) {
                    // サーバー停止時の例外をキャッチ
                    cout << "Debug: server_->stop()で例外発生: " << e.what() << endl;
                    error_out.send("server_stop_error", e.what());
                }
            } else {
                cout << "Debug: server_はnull" << endl;
            }
            
            // 万が一の場合に備えて、メモリ参照前にチェック
            cout << "Debug: 接続状態をfalseに設定" << endl;
            connected = false;
            
            cout << "Debug: disconnect_client_server()処理完了" << endl;
        } catch (const std::exception& e) {
            // 例外発生時も確実に接続状態を更新
            cout << "Debug: disconnect_client_server()で例外発生: " << e.what() << endl;
            connected = false;
            error_out.send("disconnect_error", e.what());
        } catch (...) {
            // 不明な例外もキャッチ
            cout << "Debug: disconnect_client_server()で不明な例外発生" << endl;
            connected = false;
            error_out.send("disconnect_unknown_error");
        }
    }
    
    // OSCメッセージ処理 - Min-DevKit型バージョン
    // Issue 19: Min-DevKit型とOSC独自型の両方をサポート
    void handle_osc_message(const std::string& address, const atoms& args) {
        // OSC独自型に変換して内部関数を呼び出す
        handle_osc_message_internal(address, convert_to_osc_args(args));
    }
    
    // OSCメッセージ処理 - OSC独自型バージョン
    void handle_osc_message_osc(const std::string& address, const osc_args& args) {
        handle_osc_message_internal(address, args);
    }
    
    // OSCメッセージ処理 - 内部実装
    void handle_osc_message_internal(const std::string& address, const osc_args& args) {
        // アドレスパターンマッピングを適用
        std::string mapped_address = apply_address_mapping(address);
        
        // Claude Desktop固有のメッセージ処理
        if (address.find("/claude/") == 0 && claude_handler_) {
            // Claudeハンドラーを呼び出す前にMin-DevKit型に変換
            claude_handler_->process_message(address, convert_to_atoms(args));
            return;
        }
        
        // 一般的なOSCメッセージ処理
        atoms out_args;
        out_args.push_back(symbol(mapped_address));
        
        // 引数の追加（OSC独自型からMin-DevKit型に変換）
        atoms converted_args = convert_to_atoms(args);
        for (const auto& arg : converted_args) {
            out_args.push_back(arg);
        }
        
        // 出力
        defer_task([this, out_args]() {
            output.send(out_args);
        });
    }
    
    // エラー処理
    void handle_error(const error_info& error) {
        // エラー情報の構築
        atoms error_args;
        error_args.push_back(symbol("osc_error"));
        
        // エラーコードをシンボルに変換
        std::string error_code;
        switch (error.code) {
            case osc_error_code::receive_failed:
                error_code = "receive_failed";
                break;
            case osc_error_code::send_failed:
                error_code = "send_failed";
                break;
            case osc_error_code::connection_failed:  // socket_errorの代わり
                error_code = "socket_error";
                break;
            case osc_error_code::invalid_args:  // format_errorの代わり
                error_code = "format_error";
                break;
            default:
                error_code = "unknown_error";
                break;
        }
        
        error_args.push_back(symbol(error_code));
        error_args.push_back(symbol(error.message));
        
        // エラー出力
        defer_task([this, error_args]() {
            error_out.send(error_args);
        });
    }
    
    // アドレスマッピングを登録
    bool map_address(const std::string& pattern, const std::string& callback) {
        std::lock_guard<std::mutex> lock(osc_mutex_);
        osc_mappings_[pattern] = callback;
        return true;
    }
    
    // マッピングを適用
    std::string apply_address_mapping(const std::string& address) {
        std::lock_guard<std::mutex> lock(osc_mutex_);
        
        // 完全一致優先
        if (osc_mappings_.find(address) != osc_mappings_.end()) {
            return osc_mappings_[address];
        }
        
        // ワイルドカードマッチング
        for (const auto& mapping : osc_mappings_) {
            if (mapping.first.find("*") != std::string::npos) {
                // 単純なワイルドカード置換
                if (mapping.first.find("/*/") != std::string::npos) {
                    std::string pattern = mapping.first;
                    std::string replacement = mapping.second;
                    
                    // /*/の置換
                    size_t wildcard_pos = pattern.find("/*/");
                    if (wildcard_pos != std::string::npos) {
                        std::string prefix = pattern.substr(0, wildcard_pos);
                        if (address.find(prefix) == 0) {
                            // プレフィックスから始まるアドレスにマッチ
                            std::string suffix = pattern.substr(wildcard_pos + 3);
                            size_t suffix_pos = address.find(suffix, prefix.length());
                            
                            if (suffix.empty() || suffix_pos != std::string::npos) {
                                // サフィックスが空か、サフィックスにマッチする場合
                                // ワイルドカード部分を抽出
                                std::string wildcard_part = address.substr(prefix.length(),
                                    suffix.empty() ? std::string::npos : suffix_pos - prefix.length());
                                
                                // 置換パターンの適用
                                size_t repl_wildcard_pos = replacement.find("/*/");
                                if (repl_wildcard_pos != std::string::npos) {
                                    return replacement.substr(0, repl_wildcard_pos) + wildcard_part +
                                           replacement.substr(repl_wildcard_pos + 3);
                                } else {
                                    return replacement;
                                }
                            }
                        }
                    }
                }
                
                // 末尾ワイルドカード
                if (mapping.first.back() == '*') {
                    std::string pattern = mapping.first.substr(0, mapping.first.length() - 1);
                    if (address.find(pattern) == 0) {
                        // パターンの末尾を置換
                        std::string wildcard_part = address.substr(pattern.length());
                        std::string replacement = mapping.second;
                        
                        if (replacement.back() == '*') {
                            return replacement.substr(0, replacement.length() - 1) + wildcard_part;
                        } else {
                            return replacement;
                        }
                    }
                }
            }
        }
        
        // マッチするマッピングが見つからない場合は元のアドレスを返す
        return address;
    }
    
    // 遅延タスクを追加
    void defer_task(const std::function<void()>& task) {
        deferred_tasks.push_back(task);
        task_queue.set();
    }
    
    // 動的ポートの生成（M4L互換性モード用）
    int generate_dynamic_port() {
        // 動的ポート範囲（49152～65535のうちの一つをランダムに生成）
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(49152, 65535);
        return dis(gen);
    }
    
    // Max for Liveライフサイクルハンドラ
    void handle_m4l_liveset_loaded() {
        defer_task([this]() {
            connect_client_server();
            cout << "M4L liveset loaded, reconnecting OSC" << endl;
        });
    }
    
    void handle_m4l_liveset_saved() {
        defer_task([this]() {
            cout << "M4L liveset saved" << endl;
        });
    }
    
    void handle_m4l_liveset_closed() {
        defer_task([this]() {
            disconnect_client_server();
            cout << "M4L liveset closed, disconnecting OSC" << endl;
        });
    }
    
    void handle_m4l_liveset_new() {
        defer_task([this]() {
            cout << "M4L new liveset created" << endl;
        });
    }
};

// Min外部オブジェクトをクラスから生成
} // namespace osc
} // namespace mcp

// 名前空間の外でMIN_EXTERNALを呼び出す必要があります
MIN_EXTERNAL(mcp::osc::osc_bridge);
