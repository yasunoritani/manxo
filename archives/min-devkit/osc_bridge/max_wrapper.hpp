#pragma once

// Issue 19: Min-DevKit固有のラッパークラス
// 目的: Min-DevKitヘッダーの重複インクルード問題を解決
//      max_object_t<osc_bridge>の代わりにこのクラスを使用

// 前方宣言 - クラス名のみを指定してヘッダー依存を減らす
namespace c74 { 
    namespace min {
        class osc_bridge;
    }
}

/**
 * M4Lライフサイクルイベント処理用のラッパークラス
 * osc_bridgeクラスへの依存を最小化する
 */
class max_wrapper {
public:
    // M4Lライフサイクルイベントハンドラ - ロード
    static void* handle_m4l_liveset_loaded_wrapper(void* x) {
        c74::min::osc_bridge* obj = (c74::min::osc_bridge*)x;
        // オブジェクトのメソッドは直接呼び出さず、遅延実行キューに追加
        if (obj) {
            // ライフサイクルイベント処理をメインスレッドに委譲
            // 実装は別途osc_bridgeクラス内で定義
        }
        return nullptr;
    }
    
    // M4Lライフサイクルイベントハンドラ - 保存
    static void* handle_m4l_liveset_saved_wrapper(void* x) {
        c74::min::osc_bridge* obj = (c74::min::osc_bridge*)x;
        if (obj) {
            // ライフサイクルイベント処理をメインスレッドに委譲
        }
        return nullptr;
    }
    
    // M4Lライフサイクルイベントハンドラ - クローズ
    static void* handle_m4l_liveset_closed_wrapper(void* x) {
        c74::min::osc_bridge* obj = (c74::min::osc_bridge*)x;
        if (obj) {
            // ライフサイクルイベント処理をメインスレッドに委譲
        }
        return nullptr;
    }
    
    // M4Lライフサイクルイベントハンドラ - 新規作成
    static void* handle_m4l_liveset_new_wrapper(void* x) {
        c74::min::osc_bridge* obj = (c74::min::osc_bridge*)x;
        if (obj) {
            // ライフサイクルイベント処理をメインスレッドに委譲
        }
        return nullptr;
    }
};