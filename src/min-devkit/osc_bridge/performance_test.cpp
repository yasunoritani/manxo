/// @file
///	@ingroup 	mcpmodules
///	@copyright	Copyright 2025 The Max 9-Claude Desktop Integration Project. All rights reserved.
///	@license	Use of this source code is governed by the MIT License found in the License.md file.

// パフォーマンスとリソース管理テスト
// M4L環境でのOSC Bridgeのパフォーマンスと資源効率をテスト

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
#include <vector>
#include <numeric>
#include <ctime>
#include <algorithm>
#include <functional>

// ランダムポート生成用ヘルパー関数
int random_port(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

// 時間計測用ヘルパークラス
class Timer {
public:
    Timer() : start_time_(std::chrono::high_resolution_clock::now()) {}
    
    double elapsed_ms() {
        auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration<double, std::milli>(now - start_time_).count();
    }
    
    void reset() {
        start_time_ = std::chrono::high_resolution_clock::now();
    }
    
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time_;
};

SCENARIO("Performance and Resource Management Tests") {
    
    GIVEN("An OSC bridge in M4L environment") {
        // サーバー設定
        int server_port = random_port(46000, 47000);
        mcp::osc::connection_config server_config;
        server_config.host = "localhost";
        server_config.port_in = server_port;
        
        mcp::osc::server server(server_config);
        
        // クライアント設定
        mcp::osc::connection_config client_config;
        client_config.host = "localhost";
        client_config.port_out = server_port;
        
        mcp::osc::client client(client_config);
        
        // 受信データカウント用変数
        int message_count = 0;
        
        // シンプルなハンドラー登録（カウンターのみ）
        server.register_handler("/test/perf", [&](const std::string& addr, const atoms& args) {
            message_count++;
        });
        
        // サーバーとクライアントを起動
        server.connect();
        client.connect();
        
        WHEN("Sending messages at high frequency") {
            const int test_duration_ms = 1000; // 1秒間のテスト
            const int message_interval_ms = 1; // 1msごとにメッセージ送信
            const int expected_messages = test_duration_ms / message_interval_ms;
            
            // 送信成功カウンター
            int send_success_count = 0;
            
            // 送信開始
            Timer timer;
            while (timer.elapsed_ms() < test_duration_ms) {
                if (client.send("/test/perf", atoms{"highfreq"})) {
                    send_success_count++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(message_interval_ms));
            }
            
            // 全メッセージの処理を待機
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            THEN("Bridge should handle high message frequency") {
                // スループットのレポート
                // 送信の成功率は環境に依存するため、厳密なチェックは行わない
                INFO("Sent messages: " << send_success_count << " / " << expected_messages);
                INFO("Received messages: " << message_count);
                INFO("Success rate (send): " << (100.0 * send_success_count / expected_messages) << "%");
                INFO("Success rate (end-to-end): " << (100.0 * message_count / expected_messages) << "%");
                
                // M4L環境では処理能力に制限があることを考慮
                // ある程度の成功率が確保されていればOKとする
                CHECK(send_success_count > expected_messages * 0.7); // 送信成功率70%以上
                CHECK(message_count > expected_messages * 0.5);     // 受信成功率50%以上
            }
        }
        
        WHEN("Sending large messages") {
            // 大きなメッセージペイロードのテスト
            const int test_count = 100;
            const int max_payload_size = 1024; // 1KBのペイロード
            
            // 送受信成功カウンター
            int send_success_count = 0;
            message_count = 0;
            
            // 大きなペイロードを送信
            for (int i = 0; i < test_count; i++) {
                // 大きな文字列を生成
                std::string large_payload(max_payload_size, 'X');
                large_payload[0] = 'A' + (i % 26); // 区別のために先頭文字を変更
                
                if (client.send("/test/perf", atoms{large_payload})) {
                    send_success_count++;
                }
                
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            // 処理を待機
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            THEN("Bridge should handle large messages") {
                INFO("Large messages sent: " << send_success_count << " / " << test_count);
                INFO("Large messages received: " << message_count);
                
                // 送信と受信の成功率をチェック
                CHECK(send_success_count > test_count * 0.8); // 送信成功率80%以上
                CHECK(message_count > test_count * 0.7);      // 受信成功率70%以上
            }
        }
        
        WHEN("Testing message latency") {
            const int test_count = 50;
            std::vector<double> latencies;
            
            // レイテンシ計測用のコールバック関数
            std::atomic<bool> message_received{false};
            std::atomic<double> last_latency{0.0};
            
            // レイテンシ計測用の特別なハンドラー
            server.register_handler("/test/latency", [&](const std::string& addr, const atoms& args) {
                message_received = true;
            });
            
            for (int i = 0; i < test_count; i++) {
                // タイマーリセット
                Timer timer;
                message_received = false;
                
                // メッセージ送信
                client.send("/test/latency", atoms{"ping"});
                
                // 受信待機（最大100ms）
                int timeout_ms = 100;
                bool timed_out = false;
                
                while (!message_received && timer.elapsed_ms() < timeout_ms) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                
                if (message_received) {
                    latencies.push_back(timer.elapsed_ms());
                }
                
                // 次のテストまで少し待機
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            
            THEN("Latency should be within acceptable range for M4L") {
                // 統計情報の計算
                size_t received_count = latencies.size();
                
                if (received_count > 0) {
                    double avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / received_count;
                    
                    // 最小・最大レイテンシを計算
                    double min_latency = *std::min_element(latencies.begin(), latencies.end());
                    double max_latency = *std::max_element(latencies.begin(), latencies.end());
                    
                    INFO("Latency tests completed: " << received_count << " / " << test_count);
                    INFO("Average latency: " << avg_latency << "ms");
                    INFO("Min latency: " << min_latency << "ms, Max latency: " << max_latency << "ms");
                    
                    // M4L環境では多少のレイテンシは許容される
                    // 10ms以下を目標とするが、環境により変動する可能性あり
                    CHECK(received_count > test_count * 0.8); // 80%以上成功
                    CHECK(avg_latency < 20.0);                // 平均20ms未満
                }
                else {
                    FAIL("No latency measurements received");
                }
            }
        }
        
        WHEN("Testing long-running stability") {
            const int test_duration_sec = 5; // 実際のテストでは長時間（数分〜数時間）を想定
            const int check_interval_ms = 500;
            const int messages_per_check = 10;
            
            // 定期的なメッセージ送信と受信確認
            Timer run_timer;
            int total_sent = 0;
            message_count = 0;
            
            while (run_timer.elapsed_ms() < test_duration_sec * 1000) {
                // 複数のメッセージを送信
                for (int i = 0; i < messages_per_check; i++) {
                    if (client.send("/test/perf", atoms{"stability"})) {
                        total_sent++;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
                
                // 次のチェックまで待機
                std::this_thread::sleep_for(std::chrono::milliseconds(check_interval_ms));
            }
            
            // 最終確認メッセージ
            bool final_message_sent = client.send("/test/perf", atoms{"final"});
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            THEN("Bridge should maintain stability over time") {
                INFO("Test duration: " << test_duration_sec << " seconds");
                INFO("Total messages sent: " << total_sent);
                INFO("Total messages received: " << message_count);
                
                // 安定性のチェック
                CHECK(client.is_connected());
                CHECK(server.is_connected());
                CHECK(final_message_sent);
                CHECK(message_count > 0);
                CHECK(message_count >= total_sent * 0.7); // 70%以上の受信率
            }
        }
        
        // クリーンアップ
        client.disconnect();
        server.disconnect();
    }
}
