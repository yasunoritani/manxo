#include "c74_min.h"
#include "mcp.osc_types.hpp"

// 前方宣言
namespace mcp {
namespace osc {
    class client;
    class server;
}
}

// 実装ファイルのインクルード
#include "mcp.osc_client.cpp"
#include "mcp.osc_server.cpp"

using namespace c74::min;

/**
 * MCP OSC ブリッジクラス
 * Min-DevKitオブジェクトとして実装
 */
class mcp_osc_bridge : public object<mcp_osc_bridge> {
public:
    // メタデータ
    MIN_DESCRIPTION {"OSC Bridge for MCP-Max integration"};
    MIN_TAGS        {"mcp", "osc", "communication"};
    MIN_AUTHOR      {"Max9-Claude Desktop Project"};
    MIN_RELATED     {"udpsend, udpreceive, oscformat, oscparse"};

    // インレットとアウトレット
    inlet<>  input  { this, "(anything) Command to send via OSC" };
    outlet<> output { this, "(anything) Received OSC messages" };
    outlet<> error_out { this, "(anything) Error messages" };

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
        description {"接続状態"}, readonly_property };
    
    // コンストラクタ
    mcp_osc_bridge(const atoms& args = {}) {
        // クライアントとサーバーの初期化
        init_client_server();
        
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
    ~mcp_osc_bridge() {
        // 接続を閉じる
        disconnect();
    }
    
    // 接続メッセージ
    message<> connect { this, "connect", "Connect to OSC server",
        MIN_FUNCTION {
            // 現在の設定を反映
            update_connection_config();
            
            // 接続を開始
            bool success = connect_client_server();
            
            // 接続状態を更新
            connected = (client_->get_connection_state() == mcp::osc::connection_state::connected && 
                         server_->get_connection_state() == mcp::osc::connection_state::connected);
            
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
            
            // クライアントとサーバーの状態
            if (client_) {
                auto state = client_->get_connection_state();
                cout << "Client state: " << connection_state_to_string(state) << endl;
                
                auto error = client_->get_last_error();
                if (error.has_error()) {
                    cout << "Client error: " << error.message << endl;
                }
            }
            
            if (server_) {
                auto state = server_->get_connection_state();
                cout << "Server state: " << connection_state_to_string(state) << endl;
                
                auto error = server_->get_last_error();
                if (error.has_error()) {
                    cout << "Server error: " << error.message << endl;
                }
            }
            
            return {};
        }
    };
    
    // OSCメッセージ送信
    message<> anything { this, "anything", "Send OSC message",
        MIN_FUNCTION {
            // 接続状態を確認
            if (!connected) {
                // 自動再接続を試みる
                connect();
                
                if (!connected) {
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
            cout << "MCP OSC Bridge initialized" << endl;
            return {};
        }
    };
    
    // OSCアドレスパターンのマッピング設定
    message<> map { this, "map", "Map OSC address pattern to handler",
        MIN_FUNCTION {
            // 引数チェック
            if (args.size() < 1) {
                error_out.send("map_requires_address");
                return {};
            }
            
            // アドレスパターンを取得
            symbol pattern = args[0];
            
            // ハンドラを登録
            server_->register_handler(pattern, [this](const std::string& address, const atoms& args) {
                // メインスレッドで処理するために遅延実行
                defer([this, address, args]() {
                    // アドレスとともに引数を出力
                    atoms message_args = {symbol(address)};
                    message_args.insert(message_args.end(), args.begin(), args.end());
                    
                    output.send(message_args);
                });
            });
            
            cout << "Mapped OSC address pattern: " << pattern << endl;
            
            return {};
        }
    };
    
    // OSCアドレスパターンのマッピング解除
    message<> unmap { this, "unmap", "Unmap OSC address pattern",
        MIN_FUNCTION {
            // 引数チェック
            if (args.size() < 1) {
                error_out.send("unmap_requires_address");
                return {};
            }
            
            // アドレスパターンを取得
            symbol pattern = args[0];
            
            // ハンドラを削除
            server_->unregister_handler(pattern);
            
            cout << "Unmapped OSC address pattern: " << pattern << endl;
            
            return {};
        }
    };
    
    // 属性変更時の処理
    message<> notify { this, "notify", "Receive notifications from attribute changes",
        MIN_FUNCTION {
            // 接続設定に関わる属性が変更された場合
            symbol attr_name = args[0];
            
            if (attr_name == "host" || attr_name == "port_in" || attr_name == "port_out" || attr_name == "buffer_size") {
                // 接続中なら再接続
                if (connected) {
                    // 現在の設定を反映
                    update_connection_config();
                    
                    // 再接続
                    bool success = connect_client_server();
                    
                    // 接続状態を更新
                    connected = (client_->get_connection_state() == mcp::osc::connection_state::connected && 
                                server_->get_connection_state() == mcp::osc::connection_state::connected);
                                
                    // 結果を出力
                    if (success) {
                        cout << "Reconnected with new settings: " << host.get() << " in:" << port_in << " out:" << port_out << endl;
                    } else {
                        error_out.send("reconnect_failed");
                    }
                }
            }
            
            return {};
        }
    };
    
private:
    // クライアントとサーバーの初期化
    void init_client_server() {
        // 接続設定を作成
        mcp::osc::connection_config config;
        config.host = host.get();
        config.port_in = port_in;
        config.port_out = port_out;
        config.buffer_size = buffer_size;
        
        // クライアントを作成
        client_ = std::make_unique<mcp::osc::client>(config);
        
        // クライアントのエラーハンドラを設定
        client_->set_error_handler([this](const mcp::osc::error_info& error) {
            // メインスレッドで処理するために遅延実行
            defer([this, error]() {
                // エラーを出力
                error_out.send("client_error", error.message);
                
                // 接続状態を更新
                connected = (client_->get_connection_state() == mcp::osc::connection_state::connected && 
                            server_->get_connection_state() == mcp::osc::connection_state::connected);
            });
        });
        
        // サーバーを作成
        server_ = std::make_unique<mcp::osc::server>(config);
        
        // サーバーのエラーハンドラを設定
        server_->set_error_handler([this](const mcp::osc::error_info& error) {
            // メインスレッドで処理するために遅延実行
            defer([this, error]() {
                // エラーを出力
                error_out.send("server_error", error.message);
                
                // 接続状態を更新
                connected = (client_->get_connection_state() == mcp::osc::connection_state::connected && 
                            server_->get_connection_state() == mcp::osc::connection_state::connected);
            });
        });
    }
    
    // 接続設定の更新
    void update_connection_config() {
        // 接続設定を作成
        mcp::osc::connection_config config;
        config.host = host.get();
        config.port_in = port_in;
        config.port_out = port_out;
        config.buffer_size = buffer_size;
        
        // クライアントとサーバーの設定を更新
        if (client_) {
            client_->update_config(config, false);
        }
        
        if (server_) {
            server_->update_config(config, false);
        }
    }
    
    // クライアントとサーバーの接続
    bool connect_client_server() {
        bool client_success = false;
        bool server_success = false;
        
        // クライアントの接続
        if (client_) {
            client_success = client_->connect();
        }
        
        // サーバーの接続
        if (server_) {
            server_success = server_->connect();
        }
        
        return client_success && server_success;
    }
    
    // クライアントとサーバーの切断
    void disconnect_client_server() {
        // クライアントの切断
        if (client_) {
            client_->disconnect();
        }
        
        // サーバーの切断
        if (server_) {
            server_->disconnect();
        }
    }
    
    // 接続状態を文字列に変換
    std::string connection_state_to_string(mcp::osc::connection_state state) {
        switch (state) {
            case mcp::osc::connection_state::disconnected:
                return "disconnected";
            case mcp::osc::connection_state::connecting:
                return "connecting";
            case mcp::osc::connection_state::connected:
                return "connected";
            case mcp::osc::connection_state::error:
                return "error";
            default:
                return "unknown";
        }
    }
    
    std::unique_ptr<mcp::osc::client> client_;  // OSCクライアント
    std::unique_ptr<mcp::osc::server> server_;  // OSCサーバー
};

// オブジェクトの登録
MIN_EXTERNAL(mcp_osc_bridge);
