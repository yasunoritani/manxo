// マルチインスタンスのテスト
SCENARIO("Multi-Instance Tests") {
    
    GIVEN("Multiple OSC bridge instances") {
        ext_main(nullptr);  // 初期化関数を呼び出し
        
        const int instance_count = 3;
        std::vector<std::unique_ptr<osc_bridge>> bridges;
        std::set<int> used_in_ports;
        std::set<int> used_out_ports;
        
        // 基本ポート
        int base_port_in = random_port(20000, 25000);
        int base_port_out = random_port(25001, 30000);
        
        // 複数インスタンスを作成
        for (int i = 0; i < instance_count; i++) {
            atoms args = {"localhost", base_port_in, base_port_out};
            bridges.push_back(std::make_unique<osc_bridge>(args));
            bridges[i]->connect(atoms{});
            
            // 実際に使用されたポートを記録
            used_in_ports.insert(bridges[i]->get_in_port());
            used_out_ports.insert(bridges[i]->get_out_port());
        }
        
        WHEN("All instances are running concurrently") {
            THEN("Each instance should have unique ports") {
                // 使用されたポート数がインスタンス数と一致するか確認
                CHECK(used_in_ports.size() == instance_count);
                CHECK(used_out_ports.size() == instance_count);
            }
        }
        
        WHEN("Sending messages through all instances") {
            bool all_sent = true;
            
            for (int i = 0; i < instance_count; i++) {
                atoms message = {"test_multi", i};
                atoms result = bridges[i]->anything(message);
                
                if (!result.empty()) {
                    all_sent = false;
                }
            }
            
            THEN("All instances should send without errors") {
                CHECK(all_sent);
            }
        }
        
        WHEN("One instance is disconnected") {
            // 2番目のインスタンスを切断
            bridges[1]->disconnect(atoms{});
            
            THEN("Other instances should continue to function") {
                // 1番目と3番目のインスタンスがまだ動作するか確認
                atoms message = {"test_after_disconnect"};
                CHECK(bridges[0]->anything(message).empty());
                CHECK(bridges[2]->anything(message).empty());
            }
        }
        
        WHEN("All instances are restarted") {
            // 全インスタンスを切断
            for (auto& bridge : bridges) {
                bridge->disconnect(atoms{});
            }
            
            // 全インスタンスを再接続
            bool all_reconnected = true;
            for (auto& bridge : bridges) {
                atoms result = bridge->connect(atoms{});
                if (!result.empty()) {
                    all_reconnected = false;
                }
            }
            
            THEN("All instances should reconnect successfully") {
                CHECK(all_reconnected);
                
                // 全インスタンスが機能するか確認
                for (auto& bridge : bridges) {
                    CHECK(bridge->anything(atoms{"test_reconnect"}).empty());
                }
            }
        }
        
        // クリーンアップ
        for (auto& bridge : bridges) {
            bridge->disconnect(atoms{});
        }
    }
}
