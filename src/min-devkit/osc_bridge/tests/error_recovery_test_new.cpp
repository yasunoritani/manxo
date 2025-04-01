#include <catch2/catch.hpp>
#include "mocks/osc_interface.hpp"
#include "mocks/osc_mock.hpp"

#include <random>
#include <thread>
#include <chrono>
#include <vector>
#include <string>

// テスト用補助関数
int random_port(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
}

// Catch2テストケース：エラー発生時の回復処理
TEST_CASE("OSC Bridge error recovery", "[error][recovery]") {
    const int client_port = random_port(40000, 45000);
    const int server_port = random_port(45001, 50000);
    
    // エラー状態のチェック
    SECTION("Error state management") {
        mcp::osc::mock::osc_bridge bridge;
        
        // 初期状態はエラーなし
        REQUIRE(bridge.get_last_error() == mcp::osc::osc_error_code::no_error);
        
        // サーバー起動前の接続試行はエラーになる
        bool connected = bridge.connect("127.0.0.1", server_port, client_port);
        REQUIRE_FALSE(connected);
        REQUIRE(bridge.get_last_error() == mcp::osc::osc_error_code::server_not_started);
        
        // エラー発生後のリセット
        bridge.reset_error();
        REQUIRE(bridge.get_last_error() == mcp::osc::osc_error_code::no_error);
    }
    
    // 接続失敗からの回復
    SECTION("Recovery from connection failure") {
        mcp::osc::mock::osc_bridge bridge;
        
        // 初期化と未起動サーバーへの接続試行
        REQUIRE_FALSE(bridge.connect("127.0.0.1", server_port, client_port));
        REQUIRE(bridge.get_last_error() == mcp::osc::osc_error_code::server_not_started);
        
        // サーバー起動
        REQUIRE(bridge.start_server(server_port));
        
        // エラーリセット後の再接続試行
        bridge.reset_error();
        REQUIRE(bridge.connect("127.0.0.1", server_port, client_port));
        REQUIRE(bridge.get_last_error() == mcp::osc::osc_error_code::no_error);
        
        // クリーンアップ
        bridge.disconnect();
        bridge.stop_server();
    }
    
    // サーバーダウンからの回復
    SECTION("Recovery from server down") {
        mcp::osc::mock::osc_bridge bridge;
        
        // サーバー起動と接続
        REQUIRE(bridge.start_server(server_port));
        REQUIRE(bridge.connect("127.0.0.1", server_port, client_port));
        
        // サーバー停止（クラッシュをシミュレート）
        bridge.stop_server();
        
        // メッセージ送信試行（失敗する）
        REQUIRE_FALSE(bridge.send_message("/test/path", "test data"));
        REQUIRE(bridge.get_last_error() != mcp::osc::osc_error_code::no_error);
        
        // サーバー再起動と再接続
        bridge.reset_error();
        REQUIRE(bridge.start_server(server_port));
        REQUIRE(bridge.connect("127.0.0.1", server_port, client_port));
        
        // 復旧後のメッセージ送信
        REQUIRE(bridge.send_message("/test/recovered", "recovery test"));
        REQUIRE(bridge.get_last_error() == mcp::osc::osc_error_code::no_error);
        
        // クリーンアップ
        bridge.disconnect();
        bridge.stop_server();
    }
    
    // ポート競合からの回復
    SECTION("Recovery from port conflict") {
        mcp::osc::mock::osc_bridge bridge1;
        mcp::osc::mock::osc_bridge bridge2;
        
        // 最初のブリッジでサーバー起動
        REQUIRE(bridge1.start_server(server_port));
        
        // 2つ目のブリッジで同じポートのサーバー起動を試行（失敗する）
        REQUIRE_FALSE(bridge2.start_server(server_port));
        REQUIRE(bridge2.get_last_error() == mcp::osc::osc_error_code::port_in_use);
        
        // 別のポートで起動
        const int alt_server_port = server_port + 1000;
        bridge2.reset_error();
        REQUIRE(bridge2.start_server(alt_server_port));
        REQUIRE(bridge2.get_last_error() == mcp::osc::osc_error_code::no_error);
        
        // クリーンアップ
        bridge1.stop_server();
        bridge2.stop_server();
    }
}

// Catch2テストケース：ネットワーク接続の問題からの回復
TEST_CASE("Network issue recovery", "[error][network]") {
    const int client_port = random_port(40000, 45000);
    const int server_port = random_port(45001, 50000);
    
    // ネットワークエラーのシミュレーション
    SECTION("Simulate network disconnection") {
        mcp::osc::mock::osc_bridge bridge;
        
        // サーバー起動と接続
        REQUIRE(bridge.start_server(server_port));
        REQUIRE(bridge.connect("127.0.0.1", server_port, client_port));
        
        // 正常なメッセージ送信
        REQUIRE(bridge.send_message("/test/before", "before disconnect"));
        
        // ネットワーク切断をシミュレート
        bridge.simulate_network_error();
        
        // 切断後のメッセージ送信（失敗する）
        REQUIRE_FALSE(bridge.send_message("/test/during", "during disconnect"));
        REQUIRE(bridge.get_last_error() == mcp::osc::osc_error_code::network_error);
        
        // ネットワーク回復
        bridge.reset_error();
        REQUIRE(bridge.reconnect());
        
        // 回復後のメッセージ送信
        REQUIRE(bridge.send_message("/test/after", "after reconnect"));
        REQUIRE(bridge.get_last_error() == mcp::osc::osc_error_code::no_error);
        
        // クリーンアップ
        bridge.disconnect();
        bridge.stop_server();
    }
    
    // 自動再接続機能
    SECTION("Auto reconnection") {
        mcp::osc::mock::osc_bridge bridge;
        
        // 自動再接続を有効化
        bridge.set_auto_reconnect(true);
        
        // サーバー起動と接続
        REQUIRE(bridge.start_server(server_port));
        REQUIRE(bridge.connect("127.0.0.1", server_port, client_port));
        
        // ネットワーク切断をシミュレート
        bridge.simulate_network_error();
        
        // 自動再接続が行われるまで少し待機
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 自動再接続後のメッセージ送信
        REQUIRE(bridge.send_message("/test/auto", "auto reconnected"));
        REQUIRE(bridge.get_last_error() == mcp::osc::osc_error_code::no_error);
        
        // クリーンアップ
        bridge.disconnect();
        bridge.stop_server();
    }
}
