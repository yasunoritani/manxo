/// @file
///	@ingroup 	mcpmodules
///	@copyright	Copyright 2025 The Max 9-Claude Desktop Integration Project. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

// テストフレームワークのインクルード
#include "c74_min.h"
#include "c74_min_catch.h"

// テスト対象のインクルード
#include "../max_wrapper.hpp"
#include "../mcp.osc_types.hpp"

// 標準ライブラリ
#include <random>

// ランダムポート生成用ヘルパー関数
int random_port(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
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
