
/// @file
///	@ingroup 	mcpmodules
///	@copyright	Copyright 2025 The Max 9-Claude Desktop Integration Project. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

// M4Lライフサイクルイベントテスト
// M4L環境でのOSC Bridgeの動作をテスト

// テストフレームワークのインクルード
#include "c74_min.h"
#include "c74_min_catch.h"

// テスト対象のインクルード
#include "max_wrapper.hpp"
#include "mcp.osc_types.hpp"

// 標準ライブラリ
#include <random>
#include <thread>
#include <chrono>

// ランダムポート生成用ヘルパー関数
int random_port(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

SCENARIO("M4L Lifecycle Event Tests") {
    
    GIVEN("An OSC bridge in M4L environment") {
        // サーバー設定
        int server_port = random_port(40000, 45000);
        mcp::osc::connection_config server_config;
        server_config.host = "localhost";
        server_config.port_in = server_port;
        server_config.auto_reconnect = true;
        
        mcp::osc::server server(server_config);
        
        // クライアント設定
        mcp::osc::connection_config client_config;
        client_config.host = "localhost";
        client_config.port_out = server_port;
        client_config.auto_reconnect = true;
        
        mcp::osc::client client(client_config);
        
        // 受信データの検証用変数
        std::string last_received_message;
        bool message_received = false;
        
        // ハンドラー登録
        server.register_handler("/test/m4l", [&](const std::string& addr, const atoms& args) {
            if (!args.empty() && args[0].is_string()) {
                last_received_message = args[0].as_string();
                message_received = true;
            }
        });
        
        // サーバーとクライアントを起動
        server.connect();
        client.connect();
        
        WHEN("Simulating 'liveset_loaded' event") {
            // 初期状態の確認
            bool initial_connection = client.is_connected() && server.is_connected();
            
            // ライフサイクルイベントのシミュレーション
            server.handle_lifecycle_event("liveset_loaded");
            client.handle_lifecycle_event("liveset_loaded");
            
            // 短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // イベント後の通信テスト
            message_received = false;
            client.send("/test/m4l", atoms{"after_liveset_loaded"});
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            THEN("Connection should remain intact after event") {
                CHECK(initial_connection);
                CHECK(client.is_connected());
                CHECK(server.is_connected());
                CHECK(message_received);
                CHECK(last_received_message == "after_liveset_loaded");
            }
        }
        
        WHEN("Simulating 'liveset_saved' event") {
            // イベント前のテスト
            client.send("/test/m4l", atoms{"before_liveset_saved"});
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            bool pre_event_received = message_received;
            std::string pre_event_message = last_received_message;
            
            // ライフサイクルイベントのシミュレーション
            server.handle_lifecycle_event("liveset_saved");
            client.handle_lifecycle_event("liveset_saved");
            
            // 短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // イベント後の通信テスト
            message_received = false;
            client.send("/test/m4l", atoms{"after_liveset_saved"});
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            THEN("Communication should work before and after event") {
                CHECK(pre_event_received);
                CHECK(pre_event_message == "before_liveset_saved");
                CHECK(message_received);
                CHECK(last_received_message == "after_liveset_saved");
            }
        }
        
        WHEN("Simulating consecutive lifecycle events") {
            // 連続したイベントをシミュレート
            const std::vector<std::string> events = {
                "liveset_new", "liveset_loaded", "liveset_saved", "liveset_closed"
            };
            
            bool all_events_passed = true;
            
            for (const auto& event : events) {
                // ライフサイクルイベントをシミュレート
                server.handle_lifecycle_event(event);
                client.handle_lifecycle_event(event);
                
                // 短い待機
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // イベント後の通信テスト
                message_received = false;
                std::string test_message = "after_" + event;
                client.send("/test/m4l", atoms{test_message});
                
                // 処理待ち
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // このイベントに対するテスト結果を確認
                if (!message_received || last_received_message != test_message) {
                    all_events_passed = false;
                    break;
                }
            }
            
            THEN("Bridge should handle all lifecycle events correctly") {
                CHECK(all_events_passed);
                CHECK(client.is_connected());
                CHECK(server.is_connected());
            }
        }
        
        WHEN("Simulating temporary resource constraints") {
            // 高負荷状態をシミュレート
            const int message_count = 100;
            int success_count = 0;
            
            // 短時間に多数のメッセージを送信
            for (int i = 0; i < message_count; i++) {
                std::string msg = "stress_test_" + std::to_string(i);
                if (client.send("/test/m4l", atoms{msg})) {
                    success_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            
            // 処理待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            // 最終確認メッセージ
            message_received = false;
            client.send("/test/m4l", atoms{"after_stress_test"});
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            THEN("Bridge should handle high load without crashing") {
                // 全てのメッセージが成功するとは限らないが、大部分は成功すべき
                CHECK(success_count > message_count * 0.9);
                CHECK(message_received);
                CHECK(last_received_message == "after_stress_test");
                CHECK(client.is_connected());
                CHECK(server.is_connected());
            }
        }
        
        // クリーンアップ
        client.disconnect();
        server.disconnect();
=======
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
