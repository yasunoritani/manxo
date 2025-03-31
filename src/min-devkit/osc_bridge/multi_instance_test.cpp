// 複数インスタンステスト
// M4L環境での複数OSC Bridgeインスタンス共存のテスト

#include <random>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>

// ランダムポート生成用ヘルパー関数
int random_port(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

SCENARIO("Multiple OSC Bridge Instances Test") {
    
    GIVEN("Multiple OSC bridges running simultaneously") {
        const int instance_count = 3; // テストするインスタンス数
        
        // テスト用の受信カウンター
        struct ReceiveCounter {
            std::mutex mtx;
            std::unordered_map<int, int> counts;
            
            void increment(int instance_id) {
                std::lock_guard<std::mutex> lock(mtx);
                counts[instance_id]++;
            }
            
            int get_count(int instance_id) {
                std::lock_guard<std::mutex> lock(mtx);
                return counts[instance_id];
            }
        };
        
        ReceiveCounter counter;
        
        // インスタンス管理用コンテナ
        std::vector<mcp::osc::server> servers;
        std::vector<mcp::osc::client> clients;
        std::vector<int> server_ports;
        
        // 複数のインスタンスを初期化
        for (int i = 0; i < instance_count; i++) {
            // 動的ポート割り当てのため、ベースポートを設定
            int base_port = random_port(45000 + i * 1000, 46000 + i * 1000);
            server_ports.push_back(base_port);
            
            // サーバー設定
            mcp::osc::connection_config server_config;
            server_config.host = "localhost";
            server_config.port_in = base_port;
            server_config.port_retry_count = 5; // ポート競合時の試行回数
            server_config.auto_reconnect = true;
            
            // サーバーインスタンス作成
            servers.emplace_back(server_config);
            
            // 最終追加されたサーバーへの参照
            auto& server = servers.back();
            
            // テスト用のハンドラーを登録
            int instance_id = i;
            server.register_handler("/test/multi", [&counter, instance_id](const std::string& addr, const atoms& args) {
                counter.increment(instance_id);
            });
            
            // サーバー起動
            server.connect();
            
            // クライアント設定
            mcp::osc::connection_config client_config;
            client_config.host = "localhost";
            client_config.port_out = server.get_bound_port(); // 実際にバインドされたポートを使用
            client_config.auto_reconnect = true;
            
            // クライアントインスタンス作成
            clients.emplace_back(client_config);
            
            // クライアント接続
            clients.back().connect();
        }
        
        // セットアップ完了まで短く待機
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        WHEN("Sending messages to each instance") {
            const int messages_per_instance = 10;
            
            // 各インスタンスに複数のメッセージを送信
            for (int i = 0; i < instance_count; i++) {
                for (int j = 0; j < messages_per_instance; j++) {
                    clients[i].send("/test/multi", atoms{"test"});
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
            
            // 処理完了待ち
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            
            THEN("Each instance should receive its own messages") {
                for (int i = 0; i < instance_count; i++) {
                    CHECK(counter.get_count(i) == messages_per_instance);
                }
            }
        }
        
        WHEN("One instance is disconnected") {
            // 2番目のインスタンスを切断（インデックスは1）
            if (instance_count >= 2) {
                int disconnected_idx = 1;
                
                // 切断前にテストメッセージを送信
                for (int i = 0; i < instance_count; i++) {
                    clients[i].send("/test/multi", atoms{"before_disconnect"});
                }
                
                // 処理待ち
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                // 切断前のカウントを記録
                std::vector<int> before_counts;
                for (int i = 0; i < instance_count; i++) {
                    before_counts.push_back(counter.get_count(i));
                }
                
                // 2番目のインスタンスを切断
                clients[disconnected_idx].disconnect();
                servers[disconnected_idx].disconnect();
                
                // 他のインスタンスにメッセージを送信
                for (int i = 0; i < instance_count; i++) {
                    if (i != disconnected_idx) {
                        clients[i].send("/test/multi", atoms{"after_disconnect"});
                    }
                }
                
                // 処理待ち
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                
                THEN("Other instances should continue to function") {
                    // 切断されたインスタンスのカウントは変化しないはず
                    CHECK(counter.get_count(disconnected_idx) == before_counts[disconnected_idx]);
                    
                    // 他のインスタンスは新しいメッセージを受信しているはず
                    for (int i = 0; i < instance_count; i++) {
                        if (i != disconnected_idx) {
                            CHECK(counter.get_count(i) == before_counts[i] + 1);
                        }
                    }
                }
            }
        }
        
        WHEN("Instances use conflicting ports") {
            // 意図的にポート競合をシミュレート
            if (instance_count >= 2) {
                // 新しいインスタンスを作成し、既存のポートを使用
                int existing_port = servers[0].get_bound_port();
                
                mcp::osc::connection_config conflict_server_config;
                conflict_server_config.host = "localhost";
                conflict_server_config.port_in = existing_port; // 既に使用中のポート
                conflict_server_config.port_retry_count = 3; // 再試行回数
                
                mcp::osc::server conflict_server(conflict_server_config);
                
                // 接続試行（動的ポート割り当てにより別のポートにバインドされるはず）
                bool connect_result = conflict_server.connect();
                int new_port = conflict_server.get_bound_port();
                
                THEN("New instance should bind to a different port") {
                    CHECK(connect_result);
                    CHECK(new_port != existing_port);
                    CHECK(new_port > existing_port);
                    CHECK(new_port <= existing_port + conflict_server_config.port_retry_count);
                }
                
                // テスト後のクリーンアップ
                conflict_server.disconnect();
            }
        }
        
        // 全インスタンスのクリーンアップ
        for (int i = 0; i < instance_count; i++) {
            clients[i].disconnect();
            servers[i].disconnect();
        }
    }
}
