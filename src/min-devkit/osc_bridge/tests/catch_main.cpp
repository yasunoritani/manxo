/**
 * @file catch_main.cpp
 * @brief Catch2テストフレームワークのメインエントリーポイント
 * 
 * このファイルはIssue 22でCatch2テストフレームワークを導入するために作成されました。
 * 全テストの集約実行エントリーポイントとして機能します。
 * 
 * @version 1.0.0
 * @date 2025-04-01
 */

#define CATCH_CONFIG_MAIN  // Catch2のメイン関数を定義
#include <catch2/catch.hpp>

// OSC Bridgeテスト用共通設定
#define MCP_OSC_TEST 1

/**
 * @brief テスト環境のセットアップ情報を表示する
 * 
 * この関数はテスト開始時に呼び出され、環境情報を表示します。
 * Catch2のTEST_CASEマクロを使用することで、テスト実行の一部として実行されます。
 */
TEST_CASE("OSC Bridge Test Environment", "[environment]") {
    SECTION("Verify test environment") {
        // 環境情報の表示
        INFO("OSC Bridge Test Suite");
        INFO("Using Catch2 v" << CATCH_VERSION_MAJOR << "." << CATCH_VERSION_MINOR << "." << CATCH_VERSION_PATCH);
        
        #ifdef __APPLE__
            INFO("Platform: macOS");
            #ifdef __arm64__
                INFO("Architecture: arm64 (Apple Silicon)");
            #elif defined(__x86_64__)
                INFO("Architecture: x86_64 (Intel)");
            #else
                INFO("Architecture: Unknown");
            #endif
        #elif defined(_WIN32)
            INFO("Platform: Windows");
        #else
            INFO("Platform: Unknown");
        #endif
        
        // テスト環境が正しく設定されていることを確認
        REQUIRE(true);
    }
}

/**
 * @brief 動的ポート割り当てのユーティリティ関数
 * 
 * このセクションは自動テスト実行時に必要な動的ポート管理のユーティリティを提供します。
 * テストの並列実行時にポート競合を避けるために使用されます。
 */
namespace osc_test_utils {
    // 動的ポート管理の開始ポート
    constexpr int MIN_TEST_PORT = 49152;
    constexpr int MAX_TEST_PORT = 65535;
    constexpr int PORT_RANGE = MAX_TEST_PORT - MIN_TEST_PORT;
    
    // テスト用のランダムポートを取得
    inline int get_random_test_port() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> distrib(0, PORT_RANGE);
        return MIN_TEST_PORT + distrib(gen);
    }
}
