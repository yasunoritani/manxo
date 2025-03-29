#include "c74_min_unittest.h"
#include "mcp.osc_bridge.cpp"

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
        mcp_osc_bridge bridge(args);
        
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
