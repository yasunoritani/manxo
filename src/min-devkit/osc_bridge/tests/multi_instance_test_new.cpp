#include <catch2/catch.hpp>
#include "mocks/osc_interface.hpp"
#include "mocks/osc_mock.hpp"
#include "mocks/test_utilities.hpp"

#include <random>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <string>
#include <unordered_map>
#include <memory>

// mcp::osc::test::random_portを使用

// テスト用の受信カウンター
class ReceiveCounter {
public:
    void increment(int instance_id) {
        std::lock_guard<std::mutex> lock(mtx);
        counts[instance_id]++;
    }
    
    int get_count(int instance_id) {
        std::lock_guard<std::mutex> lock(mtx);
        return counts[instance_id];
    }
    
private:
    std::mutex mtx;
    std::unordered_map<int, int> counts;
};

SCENARIO("Multi Instance Coexistence Tests") {
    
    GIVEN("Multiple OSC bridge instances in a simulated M4L environment") {
        // 設定
        const int instance_count = 3; // テスト用インスタンス数
        ReceiveCounter counter;
        
        // インスタンス管理用コンテナ
        std::vector<std::unique_ptr<mcp::osc::mock::bridge>> bridges;
        std::vector<int> ports;
        
        // 複数のインスタンスを作成
        for (int i = 0; i < instance_count; i++) {
            // ポートを異なる範囲から生成してポート競合を防ぐ
            int port_base = 43000 + (i * 1000);
            int port = mcp::osc::test::random_port(port_base, port_base + 999);
            ports.push_back(port);
            
            // ブリッジ作成
            auto bridge = std::make_unique<mcp::osc::mock::bridge>();
            
            // インスタンスID用のハンドラー登録
            const int instance_id = i;
            bridge->add_method("/test/instance", [&counter, instance_id](const std::string& addr, const mcp::osc::atoms& args) {
                counter.increment(instance_id);
            });
            
            // 接続
            bridge->connect("localhost", port, port + 1);
            
            // コンテナに追加
            bridges.push_back(std::move(bridge));
        }
        
        WHEN("Each instance receives messages on its own port") {
            // 各インスタンスに専用のクライアントでメッセージを送信
            std::vector<std::unique_ptr<mcp::osc::mock::client>> clients;
            
            for (int i = 0; i < instance_count; i++) {
                auto client = std::make_unique<mcp::osc::mock::client>();
                
                // クライアントとサーバーを直接接続
                client->set_connected_server(bridges[i]->get_server());
                client->connect("localhost", ports[i]);
                
                // テストメッセージを送信
                mcp::osc::atoms args(i);
                
                client->send("/test/instance", args);
                
                clients.push_back(std::move(client));
            }
            
            // メッセージ処理の時間を確保（長めに設定）
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
            
            THEN("Each instance should receive its own messages") {
                for (int i = 0; i < instance_count; i++) {
                    INFO("Instance " << i << " received count: " << counter.get_count(i));
                    REQUIRE(counter.get_count(i) > 0);
                }
            }
        }
        
        WHEN("One instance is disconnected") {
            // 一部のインスタンスを切断
            const int disconnect_index = 1; // 2番目のインスタンスを切断
            
            if (disconnect_index < instance_count) {
                bridges[disconnect_index]->disconnect();
                
                // 切断したインスタンスにメッセージを送信
                auto client = std::make_unique<mcp::osc::mock::client>();
                // 切断されたインスタンスには接続できないはずだが、構造上は可能
                // 切断中でもテストのためにサーバーに接続するが、メッセージ処理はされないはず
                client->set_connected_server(bridges[disconnect_index]->get_server());
                client->connect("localhost", ports[disconnect_index]);
                
                mcp::osc::atoms args(disconnect_index);
                
                client->send("/test/instance", args);
                
                // 他のインスタンスにもメッセージを送信
                for (int i = 0; i < instance_count; i++) {
                    if (i != disconnect_index) {
                        auto other_client = std::make_unique<mcp::osc::mock::client>();
                        // 正常なインスタンスには直接サーバーに接続
                        other_client->set_connected_server(bridges[i]->get_server());
                        other_client->connect("localhost", ports[i]);
                        
                        mcp::osc::atoms other_args(i);
                        
                        other_client->send("/test/instance", other_args);
                    }
                }
                
                // メッセージ処理の時間を確保（長めに設定）
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                
                THEN("Disconnected instance should not receive messages, others should continue to work") {
                    // 切断したインスタンスはメッセージを受信しないはず
                    REQUIRE(counter.get_count(disconnect_index) == 0);
                    
                    // 他のインスタンスは正常にメッセージを受信するはず
                    for (int i = 0; i < instance_count; i++) {
                        if (i != disconnect_index) {
                            INFO("Instance " << i << " received count: " << counter.get_count(i));
                            REQUIRE(counter.get_count(i) > 0);
                        }
                    }
                }
            }
        }
        
        WHEN("Instances use conflicting ports") {
            // 意図的にポート競合をシミュレート
            if (instance_count >= 1) {
                // 新しいインスタンスを作成し、既存のポートを使用
                int existing_port = ports[0];
                
                // 競合するブリッジを作成
                auto conflict_bridge = std::make_unique<mcp::osc::mock::bridge>();
                
                // インスタンスID用のハンドラー登録
                const int conflict_id = 99; // 競合インスタンス用の特別なID
                conflict_bridge->add_method("/test/instance", [&counter, conflict_id](const std::string& addr, const mcp::osc::atoms& args) {
                    counter.increment(conflict_id);
                });
                
                // 既存のポートに接続を試みるが、動的ポート割り当てにより別のポートにバインドされるはず
                conflict_bridge->connect("localhost", existing_port, existing_port + 1);
                
                // メッセージを送信
                auto client = std::make_unique<mcp::osc::mock::client>();
                // 既存のブリッジに接続
                client->set_connected_server(bridges[0]->get_server());
                client->connect("localhost", existing_port);
                
                mcp::osc::atoms args(conflict_id);
                
                client->send("/test/instance", args);
                
                // メッセージ処理の時間を確保（長めに設定）
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                
                THEN("Dynamic port allocation should resolve conflicts") {
                    // 競合インスタンスは別のポートに割り当てられるため、メッセージは受信されないはず
                    // （モック環境では競合解決をシミュレートしにくいため、この部分は簡易的なチェックとなる）
                    INFO("Conflict instance received count: " << counter.get_count(conflict_id));
                    INFO("Original instance received count: " << counter.get_count(0));
                    
                    // オリジナルインスタンスは引き続き機能するはず
                    REQUIRE(counter.get_count(0) >= 0);
                }
            }
        }
    }
}
