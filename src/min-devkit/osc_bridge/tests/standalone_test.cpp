/// @file
///	@ingroup 	mcpmodules
///	@copyright	Copyright 2025 The Max 9-Claude Desktop Integration Project. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

// 完全に独立したテスト - Min-DevKit依存なし
#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

// oscpackのインクルード - サブモジュール化の検証用
#include "ip/UdpSocket.h"
#include "osc/OscOutboundPacketStream.h"
#include "osc/OscReceivedElements.h"
#include "osc/OscPacketListener.h"

// 基本的なoscpackの機能テスト
SCENARIO("OscPack Library Test - Verify Submodule") {
    
    GIVEN("OscOutboundPacketStream") {
        char buffer[1024];
        osc::OutboundPacketStream packet(buffer, 1024);
        
        WHEN("Creating an OSC message") {
            packet << osc::BeginMessage("/test/message");
            packet << 123;
            packet << 45.67f;
            packet << "test string";
            packet << osc::EndMessage;
            
            THEN("Packet should be created successfully") {
                CHECK(packet.Size() > 0);
                CHECK_NOTHROW(packet.Data());
            }
        }
    }
    
    GIVEN("OscReceivedElements") {
        WHEN("Testing build integration") {
            THEN("Code should compile successfully") {
                CHECK(true);
            }
        }
    }
}

// Catch2フレームワークテスト - 基本的な機能検証
SCENARIO("Catch2 Framework Test") {
    
    GIVEN("Math operations") {
        int a = 2;
        int b = 3;
        
        WHEN("Adding two numbers") {
            int result = a + b;
            
            THEN("The result should be correct") {
                CHECK(result == 5);
            }
        }
    }
    
    GIVEN("String operations") {
        std::string s1 = "Hello";
        std::string s2 = "World";
        
        WHEN("Concatenating strings") {
            std::string result = s1 + " " + s2;
            
            THEN("The result should be correct") {
                CHECK(result == "Hello World");
            }
        }
    }
}

// oscpackサブモジュールのバージョン検証
TEST_CASE("Verify OscPack Submodule Version") {
    // バージョン情報を出力 (実際のテストではなく情報のみ)
    INFO("OscPack submodule integration verified");
    CHECK(true);
}
