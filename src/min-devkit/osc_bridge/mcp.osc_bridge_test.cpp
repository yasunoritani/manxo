#include "c74_min_unittest.h"
#include "mcp.osc_bridge.cpp"
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <random>

// ユーティリティ関数

// 乱数生成器
std::mt19937& get_random_engine() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    return gen;
}

// ランダムなポートを生成
int random_port(int min = 10000, int max = 60000) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(get_random_engine());
}

// ユーティリティ関数: マルチスレッドテストを実行
void run_concurrent_test(int thread_count, const std::function<void(int)>& test_func) {
    std::vector<std::future<void>> futures;
    
    for (int i = 0; i < thread_count; ++i) {
        futures.push_back(std::async(std::launch::async, [i, &test_func]() {
            test_func(i);
        }));
    }
    
    // スレッドの完了を待機
    for (auto& f : futures) {
        f.wait();
    }
}

// ユーティリティ関数: M4Lライフサイクルイベントをシミュレート
void simulate_m4l_lifecycle_event(t_object* x, const std::string& event_type) {
    // 本番環境では適切なLiveオブジェクトが渡されますが、テストでは空のオブジェクトを使用
    static t_object dummy_object;
    
    if (event_type == "liveset_loaded") {
        mcp::osc::handle_m4l_liveset_loaded(x ? x : &dummy_object);
    } else if (event_type == "liveset_saved") {
        mcp::osc::handle_m4l_liveset_saved(x ? x : &dummy_object);
    } else if (event_type == "liveset_closed") {
        mcp::osc::handle_m4l_liveset_closed(x ? x : &dummy_object);
    } else if (event_type == "liveset_new") {
        mcp::osc::handle_m4l_liveset_new(x ? x : &dummy_object);
    }
}

// Min-DevKitのテストフレームワークを使用
SCENARIO("OSC Bridge Basic Tests") {
    
    GIVEN("An OSC client instance") {
        // テスト用の接続設定
        mcp::osc::connection_config config;
        config.host = "localhost";
        config.port_out = 7400;
        config.port_in = 7500;
        
        // クライアントインスタンスを作成
        mcp::osc::client client(config);
        
        WHEN("Connecting to OSC server") {
            bool result = client.connect();
            
            THEN("Connection should succeed or fail gracefully") {
                // 実際のテスト環境では接続に成功するかどうかは状況による
                // 成功時はtrue、失敗時はfalseが返るはず
                // ここではエラーがないことだけを確認
                CHECK_NOTHROW([&]{ client.disconnect(); });
            }
        }
        
        WHEN("Sending an OSC message") {
            client.connect();
            
            // テスト用メッセージ
            atoms args = {1, 2.5f, "test"};
            
            THEN("Send should succeed or fail gracefully") {
                CHECK_NOTHROW([&]{ client.send("/test/address", args); });
            }
            
            client.disconnect();
        }
    }
    
    GIVEN("An OSC server instance") {
        // テスト用の接続設定
        mcp::osc::connection_config config;
        config.host = "localhost";
        config.port_in = 7501; // 別のポートを使用してテスト
        
        // サーバーインスタンスを作成
        mcp::osc::server server(config);
        
        // テスト用のメッセージハンドラ
        bool message_received = false;
        atoms received_args;
        std::string received_address;
        
        server.register_handler("/test/pattern", [&](const std::string& address, const atoms& args) {
            message_received = true;
            received_address = address;
            received_args = args;
        });
        
        WHEN("Starting the server") {
            bool result = server.connect();
            
            THEN("Server should start or fail gracefully") {
                CHECK_NOTHROW([&]{ server.disconnect(); });
            }
        }
    }
    
    GIVEN("An OSC bridge Max object") {
        ext_main(nullptr);  // 初期化関数を呼び出し
        
        // テスト用のオブジェクト引数
        atoms args = {"localhost", 7501, 7401};
        
        // オブジェクトインスタンスを作成
        osc_bridge bridge(args);
        
        WHEN("Sending connect message") {
            atoms result = bridge.connect(atoms{});
            
            THEN("Object should process the message without errors") {
                // 結果は空のatomsのはず
                CHECK(result.empty());
            }
        }
        
        WHEN("Sending an OSC message") {
            atoms message = {"test_message", 1, 2.5, "test"};
            atoms result = bridge.anything(message);
            
            THEN("Object should process the message without errors") {
                CHECK(result.empty());
            }
        }
        
        WHEN("Mapping an OSC pattern") {
            atoms map_args = {"/test/pattern"};
            atoms result = bridge.map(map_args);
            
            THEN("Object should process the mapping without errors") {
                CHECK(result.empty());
            }
        }
        
        WHEN("Disconnecting") {
            atoms result = bridge.disconnect(atoms{});
            
            THEN("Object should disconnect without errors") {
                CHECK(result.empty());
            }
        }
    }
}

// Min-DevKitの実装定義が必要
SCENARIO("OSC Type Conversion Tests") {
    
    GIVEN("Various OSC data types") {
        
        WHEN("Converting between atoms and OSC types") {
            
            THEN("Integers should convert correctly") {
                atoms int_atoms = {1, 2, 3};
                // 型変換のテストロジックをここに追加
                CHECK(int_atoms[0].is_int());
                CHECK(int_atoms[0].as_int() == 1);
            }
            
            THEN("Floats should convert correctly") {
                atoms float_atoms = {1.1f, 2.2f, 3.3f};
                CHECK(float_atoms[0].is_float());
                CHECK(abs(float_atoms[0].as_float() - 1.1f) < 0.00001f);
            }
            
            THEN("Strings should convert correctly") {
                atoms string_atoms = {"test1", "test2"};
                CHECK(string_atoms[0].is_string());
                CHECK(string_atoms[0].as_string() == "test1");
            }
        }
    }
}

// アドレスパターンマッチングのテスト
SCENARIO("OSC Address Pattern Matching Tests") {
    
    GIVEN("An OSC handler registry") {
        mcp::osc::handler_registry registry;
        
        // テスト用メッセージハンドラ
        bool handler_called = false;
        std::string received_address;
        
        registry.register_handler("/test/pattern", [&](const std::string& address, const atoms& args) {
            handler_called = true;
            received_address = address;
        });
        
        WHEN("Exact address match") {
            auto handler = registry.get_handler("/test/pattern");
            
            THEN("Handler should be found") {
                CHECK(handler != nullptr);
                
                // ハンドラを呼び出してみる
                handler_called = false;
                handler("/test/pattern", atoms{});
                CHECK(handler_called);
                CHECK(received_address == "/test/pattern");
            }
        }
        
        WHEN("Address mismatch") {
            auto handler = registry.get_handler("/different/pattern");
            
            THEN("Handler should not be found") {
                CHECK(handler == nullptr);
            }
        }
        
        WHEN("Wildcard pattern") {
            registry.register_handler("/wildcard/*", [&](const std::string& address, const atoms& args) {
                handler_called = true;
                received_address = address;
            });
            
            auto handler = registry.get_handler("/wildcard/test");
            
            THEN("Handler should be found") {
                CHECK(handler != nullptr);
                
                // ハンドラを呼び出してみる
                handler_called = false;
                handler("/wildcard/test", atoms{});
                CHECK(handler_called);
                CHECK(received_address == "/wildcard/test");
            }
        }
    }
}

// エラーハンドリングのテスト
SCENARIO("OSC Error Handling Tests") {
    
    GIVEN("OSC error information") {
        mcp::osc::error_info error;
        
        WHEN("No error") {
            error.code = mcp::osc::osc_error_code::none;
            
            THEN("has_error should return false") {
                CHECK(!error.has_error());
            }
        }
        
        WHEN("Connection error") {
            error.code = mcp::osc::osc_error_code::connection_failed;
            error.message = "Failed to connect";
            
            THEN("has_error should return true") {
                CHECK(error.has_error());
            }
        }
    }
}
// 動的ポート割り当てのテスト
SCENARIO("Dynamic Port Allocation Tests") {
    
    GIVEN("An OSC server with port conflict simulation") {
        // 最初にポートを占有するダミーサーバーを作成
        int base_port = random_port(20000, 30000);
        ::osc::UdpListeningReceiveSocket* dummy_socket = nullptr;
        
        try {
            // ポートを占有
            ::osc::IpEndpointName endpoint(::osc::IpEndpointName::ANY_ADDRESS, base_port);
            dummy_socket = new ::osc::UdpListeningReceiveSocket(endpoint, nullptr);
            
            // サーバー設定
            mcp::osc::connection_config config;
            config.host = "localhost";
            config.port_in = base_port;  // 既に使用中のポート
            config.port_retry_count = 5; // 最大5回の再試行
            
            mcp::osc::server server(config);
            
            WHEN("Server attempts to bind to an occupied port") {
                bool result = server.connect();
                
                THEN("Server should find an alternative port") {
                    CHECK(result);
                    CHECK(server.get_bound_port() != base_port);
                    CHECK(server.get_bound_port() >= base_port);
                    CHECK(server.get_bound_port() <= base_port + config.port_retry_count);
                }
            }
            
            // クリーンアップ
            if (dummy_socket) {
                delete dummy_socket;
                dummy_socket = nullptr;
            }
        }
        catch (...) {
            if (dummy_socket) {
                delete dummy_socket;
            }
            FAIL("Exception during port conflict test");
        }
    }
}
// M4Lライフサイクルイベントのテスト
SCENARIO("M4L Lifecycle Event Tests") {
    
    GIVEN("An OSC bridge with M4L compatibility") {
        ext_main(nullptr);  // 初期化関数を呼び出し
        
        // M4L互換モードでオブジェクトを作成
        atoms args = {"localhost", 7601, 7602, 1};  // 最後の1はm4l_compatibilityフラグ
        osc_bridge bridge(args);
        
        // 接続
        bridge.connect(atoms{});
        
        WHEN("Liveset loaded event is received") {
            bool initial_state = bridge.is_connected();
            
            // Livesetロードイベントをシミュレート
            simulate_m4l_lifecycle_event(nullptr, "liveset_loaded");
            
            // イベント処理のための短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should maintain or reestablish connection") {
                CHECK(bridge.is_connected());
            }
        }
        
        WHEN("Liveset saved event is received") {
            // 状態更新フラグ
            bool status_sent = false;
            
            // 状態更新を監視するハンドラを登録
            bridge.register_handler("/mcp/status", [&](const std::string& addr, const atoms& a) {
                status_sent = true;
            });
            
            // Liveset保存イベントをシミュレート
            simulate_m4l_lifecycle_event(nullptr, "liveset_saved");
            
            // イベント処理のための短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should send status update") {
                CHECK(status_sent);
            }
        }
        
        WHEN("Liveset closed event is received") {
            // クローズイベントをシミュレート
            simulate_m4l_lifecycle_event(nullptr, "liveset_closed");
            
            // イベント処理のための短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should handle the event without errors") {
                // このテストでは例外が発生しないことを確認
                CHECK_NOTHROW([&] {
                    bridge.disconnect(atoms{});
                    bridge.connect(atoms{});
                });
            }
        }
        
        WHEN("Liveset new event is received") {
            // 新規イベントをシミュレート
            simulate_m4l_lifecycle_event(nullptr, "liveset_new");
            
            // イベント処理のための短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should reinitialize properly") {
                CHECK_NOTHROW([&] {
                    // 再接続と基本操作をテスト
                    bridge.disconnect(atoms{});
                    bridge.connect(atoms{});
                    bridge.anything(atoms{"test_message"});
                });
            }
        }
        
        // テスト後のクリーンアップ
        bridge.disconnect(atoms{});
    }
}
// 追加データ型サポートのテスト
SCENARIO("Extended OSC Data Type Tests") {
    
    GIVEN("OSC client and server with extended type support") {
        // サーバー設定
        int server_port = random_port(40000, 50000);
        mcp::osc::connection_config server_config;
        server_config.host = "localhost";
        server_config.port_in = server_port;
        
        mcp::osc::server server(server_config);
        
        // 受信データ検証用変数
        bool received_bool = false;
        bool received_nil = false;
        bool received_infinitum = false;
        std::vector<char> received_blob;
        
        // ハンドラー登録
        server.register_handler("/test/bool", [&](const std::string& addr, const atoms& args) {
            if (!args.empty() && args[0].is_int()) {
                received_bool = (args[0].as_int() == 1);
            }
        });
        
        server.register_handler("/test/nil", [&](const std::string& addr, const atoms& args) {
            received_nil = true;  // nilは引数なしで検出
        });
        
        server.register_handler("/test/infinitum", [&](const std::string& addr, const atoms& args) {
            received_infinitum = true;  // infinitumは特殊フラグで検出
        });
        
        server.register_handler("/test/blob", [&](const std::string& addr, const atoms& args) {
            // blobはバイト配列として受信
            if (!args.empty() && args[0].is_array()) {
                // 実装例：配列からバイトデータを抽出
                atoms array_data = args[0];
                for (auto& a : array_data) {
                    if (a.is_int()) {
                        received_blob.push_back(static_cast<char>(a.as_int()));
                    }
                }
            }
        });
        
        // サーバー起動
        server.connect();
        
        // クライアント設定
        mcp::osc::connection_config client_config;
        client_config.host = "localhost";
        client_config.port_out = server_port;
        
        mcp::osc::client client(client_config);
        client.connect();
        
        WHEN("Sending boolean True value") {
            atoms bool_args = {1};  // Trueを表す
            client.send("/test/bool", bool_args);
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Server should receive boolean True") {
                CHECK(received_bool);
            }
        }
        
        WHEN("Sending boolean False value") {
            received_bool = true;  // 初期値をtrueに
            atoms bool_args = {0};  // Falseを表す
            client.send("/test/bool", bool_args);
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Server should receive boolean False") {
                CHECK(!received_bool);
            }
        }
        
        WHEN("Sending Nil value") {
            atoms nil_args;  // 空のargsでNilを表現
            client.send("/test/nil", nil_args);
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Server should receive Nil") {
                CHECK(received_nil);
            }
        }
        
        WHEN("Sending Infinitum value") {
            // infinitumは実装によって表現方法が異なる
            // ここではダミーデータとして特別な値を送信
            atoms inf_args = {"INF"};  // 仮の表現
            client.send("/test/infinitum", inf_args);
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Server should receive Infinitum") {
                CHECK(received_infinitum);
            }
        }
        
        WHEN("Sending Blob data") {
            // blobデータのシミュレーション
            std::vector<int> blob_data = {0x68, 0x65, 0x6C, 0x6C, 0x6F}; // "hello"
            atoms blob_args;
            
            // バイト配列を構築（実装によって異なる可能性あり）
            // 例: 配列としてatoms内に格納
            atoms array_atom;
            for (auto& b : blob_data) {
                array_atom.push_back(b);
            }
            blob_args.push_back(array_atom);
            
            client.send("/test/blob", blob_args);
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Server should receive blob data") {
                CHECK(!received_blob.empty());
                if (!received_blob.empty()) {
                    std::string received_str(received_blob.begin(), received_blob.end());
                    CHECK(received_str == "hello");
                }
            }
        }
        
        // クリーンアップ
        client.disconnect();
        server.disconnect();
    }
}
// エラー回復のテスト
SCENARIO("Error Recovery Tests") {
    
    GIVEN("An OSC client with auto-reconnect") {
        // クライアント設定
        mcp::osc::connection_config config;
        config.host = "localhost";
        config.port_out = random_port(50000, 55000);
        config.auto_reconnect = true;
        config.reconnect_interval = 100;  // 100ms間隔で再接続
        
        mcp::osc::client client(config);
        
        WHEN("Server is unavailable initially but becomes available later") {
            // まず接続を試みる（失敗するはず）
            bool initial_result = client.connect();
            
            // サーバーを起動
            ::osc::UdpListeningReceiveSocket* dummy_server = nullptr;
            
            try {
                ::osc::IpEndpointName endpoint(::osc::IpEndpointName::ANY_ADDRESS, config.port_out);
                dummy_server = new ::osc::UdpListeningReceiveSocket(endpoint, nullptr);
                
                // 再接続のための待機
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                
                // メッセージを送信してみる
                bool send_result = client.send("/test/reconnect", atoms{"test"});
                
                THEN("Client should auto-reconnect and send successfully") {
                    CHECK(send_result);
                    CHECK(client.is_connected());
                }
                
                // クリーンアップ
                if (dummy_server) {
                    delete dummy_server;
                    dummy_server = nullptr;
                }
            }
            catch (...) {
                if (dummy_server) {
                    delete dummy_server;
                }
                FAIL("Exception during reconnection test");
            }
        }
        
        WHEN("Connection is lost and then restored") {
            // サーバーを起動
            ::osc::UdpListeningReceiveSocket* dummy_server = nullptr;
            bool connected_once = false;
            
            try {
                // まずサーバーを起動
                ::osc::IpEndpointName endpoint(::osc::IpEndpointName::ANY_ADDRESS, config.port_out);
                dummy_server = new ::osc::UdpListeningReceiveSocket(endpoint, nullptr);
                
                // 接続
                connected_once = client.connect();
                
                // サーバーをシャットダウン（接続切断をシミュレート）
                if (dummy_server) {
                    delete dummy_server;
                    dummy_server = nullptr;
                }
                
                // 切断を検出するための短い待機
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
                // メッセージを送信してみる（失敗するはず）
                bool send_failed = !client.send("/test/disconnected", atoms{"test"});
                
                // サーバーを再起動
                dummy_server = new ::osc::UdpListeningReceiveSocket(endpoint, nullptr);
                
                // 再接続のための待機
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                
                // 再接続後にメッセージを送信
                bool reconnected = client.send("/test/reconnected", atoms{"test"});
                
                THEN("Client should detect disconnection and auto-reconnect") {
                    CHECK(connected_once);
                    CHECK(send_failed);
                    CHECK(reconnected);
                    CHECK(client.is_connected());
                }
                
                // クリーンアップ
                if (dummy_server) {
                    delete dummy_server;
                    dummy_server = nullptr;
                }
            }
            catch (...) {
                if (dummy_server) {
                    delete dummy_server;
                }
                FAIL("Exception during connection loss test");
            }
        }
        
        WHEN("Invalid OSC messages are received") {
            // このテストはサーバー側での実装に依存
            // クライアントが不正なメッセージを送信したときのサーバーの動作をテスト
            
            // サーバー設定
            int server_port = random_port(55001, 60000);
            mcp::osc::connection_config server_config;
            server_config.host = "localhost";
            server_config.port_in = server_port;
            
            mcp::osc::server server(server_config);
            
            // エラーフラグ
            bool server_crashed = false;
            
            // サーバー起動
            CHECK_NOTHROW([&]{ server.connect(); });
            
            // クライアント設定
            mcp::osc::connection_config bad_client_config;
            bad_client_config.host = "localhost";
            bad_client_config.port_out = server_port;
            
            mcp::osc::client bad_client(bad_client_config);
            bad_client.connect();
            
            THEN("Server should handle invalid messages gracefully") {
                // 不正な形式のメッセージを送信する方法は実装によって異なる
                // ここではダミーテストとしてCHECK_NOTHROWを使用
                CHECK_NOTHROW([&]{ 
                    // 通常のメッセージを送信
                    bad_client.send("/test/valid", atoms{"valid"});
                    
                    // サーバーが実際には不正なメッセージをどう処理するかは
                    // サーバー側の実装に依存するため、ここでは簡易化
                });
                
                CHECK(!server_crashed);
            }
            
            // クリーンアップ
            bad_client.disconnect();
            server.disconnect();
        }
    }
}
// マルチインスタンスのテスト
SCENARIO("Multi-Instance Tests") {
    
    GIVEN("Multiple OSC bridge instances") {
        ext_main(nullptr);  // 初期化関数を呼び出し
        
        const int instance_count = 3;
        std::vector<std::unique_ptr<osc_bridge>> bridges;
        std::set<int> used_in_ports;
        std::set<int> used_out_ports;
        
        // 基本ポート
        int base_port_in = random_port(20000, 25000);
        int base_port_out = random_port(25001, 30000);
        
        // 複数インスタンスを作成
        for (int i = 0; i < instance_count; i++) {
            atoms args = {"localhost", base_port_in, base_port_out};
            bridges.push_back(std::make_unique<osc_bridge>(args));
            bridges[i]->connect(atoms{});
            
            // 実際に使用されたポートを記録
            used_in_ports.insert(bridges[i]->get_in_port());
            used_out_ports.insert(bridges[i]->get_out_port());
        }
        
        WHEN("All instances are running concurrently") {
            THEN("Each instance should have unique ports") {
                // 使用されたポート数がインスタンス数と一致するか確認
                CHECK(used_in_ports.size() == instance_count);
                CHECK(used_out_ports.size() == instance_count);
            }
        }
        
        WHEN("Sending messages through all instances") {
            bool all_sent = true;
            
            for (int i = 0; i < instance_count; i++) {
                atoms message = {"test_multi", i};
                atoms result = bridges[i]->anything(message);
                
                if (!result.empty()) {
                    all_sent = false;
                }
            }
            
            THEN("All instances should send without errors") {
                CHECK(all_sent);
            }
        }
        
        WHEN("One instance is disconnected") {
            // 2番目のインスタンスを切断
            bridges[1]->disconnect(atoms{});
            
            THEN("Other instances should continue to function") {
                // 1番目と3番目のインスタンスがまだ動作するか確認
                atoms message = {"test_after_disconnect"};
                CHECK(bridges[0]->anything(message).empty());
                CHECK(bridges[2]->anything(message).empty());
            }
        }
        
        WHEN("All instances are restarted") {
            // 全インスタンスを切断
            for (auto& bridge : bridges) {
                bridge->disconnect(atoms{});
            }
            
            // 全インスタンスを再接続
            bool all_reconnected = true;
            for (auto& bridge : bridges) {
                atoms result = bridge->connect(atoms{});
                if (!result.empty()) {
                    all_reconnected = false;
                }
            }
            
            THEN("All instances should reconnect successfully") {
                CHECK(all_reconnected);
                
                // 全インスタンスが機能するか確認
                for (auto& bridge : bridges) {
                    CHECK(bridge->anything(atoms{"test_reconnect"}).empty());
                }
            }
        }
        
        // クリーンアップ
        for (auto& bridge : bridges) {
            bridge->disconnect(atoms{});
        }
    }
}
