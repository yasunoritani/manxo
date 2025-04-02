#include <catch2/catch.hpp>
#include "mocks/osc_interface.hpp"
#include "mocks/osc_mock.hpp"
#include "mocks/test_utilities.hpp"

#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <random>

// mcp::osc::test::random_portを使用

// OSC拡張データ型のテスト
SCENARIO("Extended OSC Data Type Tests") {
    
    GIVEN("An OSC bridge with extended type support") {
        // テスト用のブリッジオブジェクトを作成
        auto bridge = std::make_unique<mcp::osc::mock::bridge>();
        
        // サーバー設定
        int server_port = mcp::osc::test::random_port(40000, 45000);
        mcp::osc::connection_config config;
        config.host = "localhost";
        config.port_in = server_port;
        config.port_out = server_port + 1;
        config.dynamic_ports = true;
        
        // 検証用変数
        bool received_bool = false;
        bool received_string1 = false;
        bool received_string2 = false;
        
        // メッセージハンドラーの設定
        bridge->add_method("/test/bool", [&](const std::string& addr, const mcp::osc::atoms& args) {
            if (args.size() > 0 && args.get_type(0) == "bool") {
                received_bool = true;
                REQUIRE(args.get_bool(0) == true);
            }
        });
        
        bridge->add_method("/test/string1", [&](const std::string& addr, const mcp::osc::atoms& args) {
            if (args.size() > 0 && args.get_type(0) == "string") {
                received_string1 = true;
                REQUIRE(args.get_string(0) == "test_string1");
            }
        });
        
        bridge->add_method("/test/string2", [&](const std::string& addr, const mcp::osc::atoms& args) {
            if (args.size() > 0 && args.get_type(0) == "string") {
                received_string2 = true;
                REQUIRE(args.get_string(0) == "Hello OSC");
            }
        });
        
        // ブリッジを接続
        bridge->connect(config.host, config.port_in, config.port_out);
        
        // クライアント設定
        auto client = std::make_unique<mcp::osc::mock::client>();
        
        // クライアントとサーバーを直接接続
        client->set_connected_server(bridge->get_server());
        client->connect(config.host, config.port_in);
        
        WHEN("Sending boolean True value") {
            mcp::osc::atoms bool_args(true);
            
            client->send("/test/bool", bool_args);
            
            // メッセージ処理の時間を確保（長めに設定）
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            THEN("Boolean value should be correctly received") {
                REQUIRE(received_bool == true);
            }
        }
        
        WHEN("Sending String value 1") {
            // 文字列を送信
            mcp::osc::atoms string_args("test_string1");
            
            client->send("/test/string1", string_args);
            
            // メッセージ処理の時間を確保（長めに設定）
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            THEN("String value 1 should be correctly received") {
                REQUIRE(received_string1 == true);
            }
        }
        
        WHEN("Sending String data 2") {
            mcp::osc::atoms string_args("Hello OSC");
            
            client->send("/test/string2", string_args);
            
            // メッセージ処理の時間を確保（長めに設定）
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            THEN("String data 2 should be correctly received") {
                REQUIRE(received_string2 == true);
            }
        }
    }
}
