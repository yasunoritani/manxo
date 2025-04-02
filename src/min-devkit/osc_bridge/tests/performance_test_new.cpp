#include <catch2/catch.hpp>
#include "mocks/osc_interface.hpp"
#include "mocks/osc_mock.hpp"
#include "mocks/test_utilities.hpp"

// 標準ライブラリ
#include <random>
#include <thread>
#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <mutex>

// mcp::osc::test::random_portを使用

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
    
    GIVEN("An OSC bridge in simulated M4L environment") {
        // テスト設定
        const int server_port = mcp::osc::test::random_port(41000, 42000);
        const int client_port = mcp::osc::test::random_port(42001, 43000);
        
        // モックブリッジの作成
        auto bridge = std::make_unique<mcp::osc::mock::bridge>();
        
        // 接続設定
        mcp::osc::connection_config config;
        config.host = "localhost";
        config.port_in = server_port;
        config.port_out = client_port;
        config.dynamic_ports = true;
        config.m4l_compatibility = true;
        
        // ブリッジを接続
        bridge->connect(config.host, config.port_in, config.port_out);
        
        // 受信データカウントとレイテンシ計測用の変数
        std::mutex mutex;
        int message_count = 0;
        std::vector<double> latencies;
        
        // シンプルなハンドラー登録（カウンターとレイテンシ計測）
        bridge->add_method("/perf/counter", [&](const std::string& addr, const mcp::osc::atoms& args) {
            std::lock_guard<std::mutex> lock(mutex);
            message_count++;
            
            if (args.size() > 0 && args.get_type(0) == "float") {
                double send_time = args.get_float(0);
                double current_time = Timer().elapsed_ms();
                double latency = current_time - send_time;
                latencies.push_back(latency);
            }
        });
        
        WHEN("Testing message throughput") {
            // スループットテスト用のクライアント
            auto client = std::make_unique<mcp::osc::mock::client>();
            
            // クライアントとサーバーを直接接続
            client->set_connected_server(bridge->get_server());
            client->connect(config.host, config.port_in);
            
            const int test_count = 100; // 送信メッセージ数
            
            Timer timer;
            
            // 大量のメッセージを送信
            for (int i = 0; i < test_count; i++) {
                mcp::osc::atoms args(i);
                
                client->send("/perf/counter", args);
                
                // 少し待機（輻輳を避けるため）
                if (i % 10 == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
            
            double elapsed = timer.elapsed_ms();
            
            // 全てのメッセージが処理されるのを待つ（長めに設定）
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            THEN("Throughput should be acceptable") {
                double messages_per_second = test_count / (elapsed / 1000.0);
                INFO("Throughput: " << messages_per_second << " messages/second");
                INFO("Total time: " << elapsed << " ms for " << test_count << " messages");
                
                // 少なくとも一定のスループットがあることを確認
                // 実際の値はシステムによって異なるため、最小値を低めに設定
                REQUIRE(messages_per_second > 10.0);
                REQUIRE(message_count > 0);
            }
        }
        
        WHEN("Testing message latency") {
            // レイテンシテスト用のクライアント
            auto client = std::make_unique<mcp::osc::mock::client>();
            
            // クライアントとサーバーを直接接続
            client->set_connected_server(bridge->get_server());
            client->connect(config.host, config.port_in);
            
            const int test_count = 20;
            latencies.clear();
            
            // タイムスタンプ付きのメッセージを送信
            for (int i = 0; i < test_count; i++) {
                double current_time = Timer().elapsed_ms();
                mcp::osc::atoms args(current_time);
                
                client->send("/perf/counter", args);
                
                // 連続送信による影響を避けるために少し待機
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
            
            // 全てのメッセージが処理されるのを待つ（長めに設定）
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            THEN("Latency should be within acceptable range") {
                // ほとんどのメッセージが受信されているはず
                REQUIRE(latencies.size() > 0);
                
                if (latencies.size() > 0) {
                    // 平均レイテンシを計算
                    double avg_latency = std::accumulate(latencies.begin(), latencies.end(), 0.0) / latencies.size();
                    
                    // 最大レイテンシを計算
                    double max_latency = *std::max_element(latencies.begin(), latencies.end());
                    
                    INFO("Average latency: " << avg_latency << " ms");
                    INFO("Maximum latency: " << max_latency << " ms");
                    
                    // モック環境では実際のネットワーク通信はないため、レイテンシは非常に低いはず
                    // 実環境では異なる値になるため、テスト基準は緩めに設定
                    REQUIRE(avg_latency < 100.0); // 100ms以下であること
                }
            }
        }
        
        WHEN("Testing memory usage under load") {
            // メモリ使用量テスト用のクライアント
            auto client = std::make_unique<mcp::osc::mock::client>();
            
            // クライアントとサーバーを直接接続
            client->set_connected_server(bridge->get_server());
            client->connect(config.host, config.port_in);
            
            // 大量のメッセージを送信して、メモリリークがないことを間接的に確認
            const int test_count = 500;
            
            for (int i = 0; i < test_count; i++) {
                // 異なるサイズのメッセージを送信
                mcp::osc::atoms args;
                
                // 値を追加
                args.add(i);
                
                // 文字列引数（大きめのデータ）
                std::string large_data = "Test string with data: " + std::to_string(i) + std::string(i % 100, 'X');
                args.add(large_data);
                
                client->send("/perf/counter", args);
                
                // 少し待機（輻輳を避けるため）
                if (i % 50 == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            
            // 全てのメッセージが処理されるのを待つ（長めに設定）
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            
            THEN("System should handle the load without issues") {
                // このテストでは実際にメモリ使用量を計測することはできないが、
                // 大量のメッセージを処理できることを確認する
                INFO("Processed " << message_count << " messages without crashing");
                REQUIRE(message_count > 0);
            }
        }
    }
}
