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

// M4Lライフサイクルイベントのテスト
SCENARIO("M4L Lifecycle Event Tests") {
    
    GIVEN("An OSC bridge with M4L compatibility mode") {
        // テスト用のブリッジオブジェクトを作成
        auto bridge = std::make_unique<mcp::osc::mock::bridge>();
        
        // テスト用のメッセージハンドラー設定
        std::string last_received_message;
        bool message_received = false;
        
        bridge->add_method("/test/m4l", [&](const std::string& addr, const mcp::osc::atoms& args) {
            if (args.size() > 0) {
                last_received_message = args.get_string(0);
                message_received = true;
            }
        });
        
        // ポート設定
        int server_port = mcp::osc::test::random_port(50000, 55000);
        std::string host = "localhost";
        
        // ブリッジを接続
        CHECK(bridge->connect(host, server_port, server_port + 1));
        
        WHEN("Liveset loaded event is received") {
            // 初期状態の確認
            bool initial_connection = bridge->is_connected();
            
            // ライフサイクルイベントのシミュレーション
            bridge->handle_m4l_event("liveset_loaded");
            
            // 短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // イベント後の通信テスト
            message_received = false;
            bridge->send("/test/m4l", mcp::osc::atoms{"after_liveset_loaded"});
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Connection should remain intact after event") {
                CHECK(initial_connection);
                CHECK(bridge->is_connected());
                CHECK(message_received);
                CHECK(last_received_message == "after_liveset_loaded");
                
                // M4Lイベントが記録されていることを確認
                auto events = bridge->get_m4l_events();
                CHECK(events.size() == 1);
                CHECK(events[0] == "liveset_loaded");
            }
        }
        
        WHEN("Liveset saved event is received") {
            // イベント前のテスト
            bridge->send("/test/m4l", mcp::osc::atoms{"before_liveset_saved"});
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            bool pre_event_received = message_received;
            std::string pre_event_message = last_received_message;
            
            // ライフサイクルイベントのシミュレーション
            bridge->handle_m4l_event("liveset_saved");
            
            // 短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            // イベント後の通信テスト
            message_received = false;
            bridge->send("/test/m4l", mcp::osc::atoms{"after_liveset_saved"});
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Communication should work before and after event") {
                CHECK(pre_event_received);
                CHECK(pre_event_message == "before_liveset_saved");
                CHECK(message_received);
                CHECK(last_received_message == "after_liveset_saved");
                
                // M4Lイベントが記録されていることを確認
                auto events = bridge->get_m4l_events();
                CHECK(events.size() == 1);
                CHECK(events[0] == "liveset_saved");
            }
        }
        
        WHEN("Liveset closed event is received") {
            // イベント前の接続状態確認
            CHECK(bridge->is_connected());
            
            // ライフサイクルイベントのシミュレーション
            bridge->handle_m4l_event("liveset_closed");
            
            // 短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should disconnect") {
                CHECK_FALSE(bridge->is_connected());
                
                // M4Lイベントが記録されていることを確認
                auto events = bridge->get_m4l_events();
                CHECK(events.size() == 1);
                CHECK(events[0] == "liveset_closed");
            }
            
            AND_WHEN("Reconnecting after liveset closed") {
                // 再接続
                CHECK(bridge->connect(host, server_port, server_port + 1));
                
                // 通信テスト
                message_received = false;
                bridge->send("/test/m4l", mcp::osc::atoms{"after_reconnect"});
                
                // 処理待ち
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
                THEN("Bridge should be able to reconnect and communicate") {
                    CHECK(bridge->is_connected());
                    CHECK(message_received);
                    CHECK(last_received_message == "after_reconnect");
                }
            }
        }
        
        WHEN("Multiple consecutive lifecycle events are received") {
            // 連続したイベントをシミュレート
            const std::vector<std::string> events = {
                "liveset_new", "liveset_loaded", "liveset_saved"
            };
            
            for (const auto& event : events) {
                // ライフサイクルイベントをシミュレート
                bridge->handle_m4l_event(event);
                
                // 短い待機
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
                // イベント後の通信テスト
                message_received = false;
                std::string test_message = "after_" + event;
                bridge->send("/test/m4l", mcp::osc::atoms{test_message});
                
                // 処理待ち
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
                // このイベントに対するテスト結果を確認
                DYNAMIC_SECTION("After " << event << " event") {
                    CHECK(bridge->is_connected());
                    CHECK(message_received);
                    CHECK(last_received_message == test_message);
                }
            }
            
            THEN("Bridge should have recorded all events") {
                auto recorded_events = bridge->get_m4l_events();
                CHECK(recorded_events.size() == events.size());
                
                for (size_t i = 0; i < events.size(); ++i) {
                    CHECK(recorded_events[i] == events[i]);
                }
            }
        }
        
        WHEN("Simulating temporary resource constraints") {
            // 高負荷状態をシミュレート
            const int message_count = 100;
            int success_count = 0;
            
            // 短時間に多数のメッセージを送信
            for (int i = 0; i < message_count; i++) {
                std::string msg = "stress_test_" + std::to_string(i);
                if (bridge->send("/test/m4l", mcp::osc::atoms{msg})) {
                    success_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 最終確認メッセージ
            message_received = false;
            bridge->send("/test/m4l", mcp::osc::atoms{"after_stress_test"});
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should handle high load without crashing") {
                // 全てのメッセージが成功するはず（モック実装なので）
                CHECK(success_count == message_count);
                CHECK(message_received);
                CHECK(last_received_message == "after_stress_test");
                CHECK(bridge->is_connected());
            }
        }
    }
}
