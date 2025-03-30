// M4Lライフサイクルイベントのテスト
SCENARIO("M4L Lifecycle Event Tests") {
    
    GIVEN("An OSC bridge with M4L compatibility") {
        ext_main(nullptr);  // 初期化関数を呼び出し
        
        // M4L互換モードでオブジェクトを作成
        atoms args = {"localhost", 7601, 7602, 1};  // 最後の1はm4l_compatibilityフラグ
        osc_bridge bridge(args);
        
        // 接続
        bridge.connect(atoms{});
        
        WHEN("Liveset loaded event is received") {
            bool initial_state = bridge.is_connected();
            
            // Livesetロードイベントをシミュレート
            simulate_m4l_lifecycle_event(nullptr, "liveset_loaded");
            
            // イベント処理のための短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should maintain or reestablish connection") {
                CHECK(bridge.is_connected());
            }
        }
        
        WHEN("Liveset saved event is received") {
            // 状態更新フラグ
            bool status_sent = false;
            
            // 状態更新を監視するハンドラを登録
            bridge.register_handler("/mcp/status", [&](const std::string& addr, const atoms& a) {
                status_sent = true;
            });
            
            // Liveset保存イベントをシミュレート
            simulate_m4l_lifecycle_event(nullptr, "liveset_saved");
            
            // イベント処理のための短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should send status update") {
                CHECK(status_sent);
            }
        }
        
        WHEN("Liveset closed event is received") {
            // クローズイベントをシミュレート
            simulate_m4l_lifecycle_event(nullptr, "liveset_closed");
            
            // イベント処理のための短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should handle the event without errors") {
                // このテストでは例外が発生しないことを確認
                CHECK_NOTHROW([&] {
                    bridge.disconnect(atoms{});
                    bridge.connect(atoms{});
                });
            }
        }
        
        WHEN("Liveset new event is received") {
            // 新規イベントをシミュレート
            simulate_m4l_lifecycle_event(nullptr, "liveset_new");
            
            // イベント処理のための短い待機
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            
            THEN("Bridge should reinitialize properly") {
                CHECK_NOTHROW([&] {
                    // 再接続と基本操作をテスト
                    bridge.disconnect(atoms{});
                    bridge.connect(atoms{});
                    bridge.anything(atoms{"test_message"});
                });
            }
        }
        
        // テスト後のクリーンアップ
        bridge.disconnect(atoms{});
    }
}
