/// @file
///	@ingroup 	mcpmodules
///	@copyright	Copyright 2025 The Max 9-Claude Desktop Integration Project. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

// テストフレームワークのインクルード
#include "c74_min.h"
#include "c74_min_catch.h"

// テスト対象のインクルード
#include "max_wrapper.hpp"
#include "mcp.osc_types.hpp"

// OSC拡張型テスト用補助関数
int random_port(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(min, max);
    return distrib(gen);
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
