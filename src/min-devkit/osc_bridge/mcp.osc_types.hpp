#pragma once

#include "c74_min.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <random>
#include <set>
#include <chrono>
#include <thread>

// oscpackヘッダー
#include "ip/UdpSocket.h"

using namespace c74::min;

namespace mcp {
namespace osc {

/**
 * OSCメッセージの基本構造体
 */
struct message {
    std::string address;    // OSCアドレスパターン
    atoms args;             // 引数リスト
    
    // 比較演算子（テスト用）
    bool operator==(const message& other) const {
        return address == other.address && args == other.args;
    }
};

/**
 * OSC接続設定
 */
struct connection_config {
    std::string host = "127.0.0.1";  // ホスト名/IPアドレス
    int port_in = 7500;              // 受信ポート
    int port_out = 7400;             // 送信ポート
    int buffer_size = 4096;          // バッファサイズ
    bool dynamic_ports = true;       // M4L向け動的ポート割り当て
    bool low_latency = false;        // 低レイテンシモード（CPU負荷が増加）
    bool m4l_compatibility = true;   // Max for Live互換モード
};

/**
 * 動的ポート管理ユーティリティ
 */
class port_manager {
public:
    /**
     * 利用可能なポートを見つける
     * @param start_port 探索開始ポート番号
     * @param end_port 探索終了ポート番号
     * @param max_attempts 最大試行回数
     * @return 利用可能なポート番号、見つからない場合は-1
     */
    static int find_available_port(int start_port = 7500, int end_port = 7600, int max_attempts = 50) {
        // 無効なパラメータチェック
        if (start_port < 1024 || end_port > 65535 || start_port > end_port) {
            throw std::invalid_argument("Invalid port range: ports must be between 1024-65535 and start < end");
        }
        
        // 試行回数の制限
        int attempts = 0;
        std::unordered_set<int> tried_ports;
        
        // ランダムポート選択のための乱数生成器
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<int> dist(start_port, end_port);
        
        while (attempts < max_attempts && tried_ports.size() < (end_port - start_port + 1)) {
            // 乱数でポートを選択 (競合状態による要求の衝突を減らす)
            int port = dist(gen);
            
            // 既に試したポートはスキップ
            if (tried_ports.count(port) > 0) {
                continue;
            }
            
            tried_ports.insert(port);
            attempts++;
            
            if (is_port_available(port)) {
                return port;
            }
        }
        
        // 全てのポートが使用中または試行回数上限に達した
        return -1;
    }
    
    /**
     * ポートが利用可能かチェック
     * @param port チェックするポート番号
     * @return 利用可能な場合true
     */
    static bool is_port_available(int port) {
        if (port < 1024 || port > 65535) {
            return false; // 無効なポートレンジ
        }
        
        UdpSocket socket;
        try {
            // タイムアウトを設定するなどの詳細設定が必要な場合はここに追加
            IpEndpointName endpoint(IpEndpointName::ANY_ADDRESS, port);
            socket.Bind(endpoint);
            socket.Close();
            return true;
        } catch (const std::exception& e) {
            // 具体的な例外情報をログに記録できるように
            std::cerr << "Port " << port << " is not available: " << e.what() << std::endl;
            return false;
        } catch (...) {
            std::cerr << "Port " << port << " is not available due to unknown error" << std::endl;
            return false;
        }
    }
    
    /**
     * 新しい接続設定を生成（動的ポート割り当て）
     * @param config 元の設定
     * @return 動的ポートを割り当てた新しい設定
     * @throws std::runtime_error ポート割り当てに失敗した場合
     */
    static connection_config allocate_dynamic_ports(const connection_config& config) {
        if (!config.dynamic_ports) {
            return config;
        }
        
        connection_config new_config = config;
        
        // ポート割り当ての試行回数
        const int max_attempts = 3;
        
        // 入力ポートの割り当て
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            new_config.port_in = find_available_port(7500, 7999);
            if (new_config.port_in != -1) break;
            
            // ポート範囲を変えて再試行
            if (attempt == max_attempts - 1) {
                throw std::runtime_error("Failed to allocate input port after multiple attempts");
            }
            
            // 少し待機してから再試行
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        // 出力ポートの割り当て (入力ポートと競合しないように範囲を変更)
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            new_config.port_out = find_available_port(8000, 8499);
            if (new_config.port_out != -1) break;
            
            if (attempt == max_attempts - 1) {
                throw std::runtime_error("Failed to allocate output port after multiple attempts");
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        
        return new_config;
    }
    
    /**
     * ポート割り当てのキャッシュをクリア
     * 長時間実行している場合、定期的に呼び出すと良い
     */
    static void clear_port_cache() {
        // 将来的にポート使用状況のキャッシュを実装する場合に備えて用意
    }
    
    // ポート関連のスレッドセーフな操作のためのミューテックス
    static std::mutex& get_port_mutex() {
        static std::mutex port_mutex;
        return port_mutex;
    }
};

/**
 * 接続状態
 */
enum class connection_state {
    disconnected,        // 未接続
    connecting,          // 接続中
    connected,           // 接続済み
    error                // エラー状態
};

/**
 * エラーコード
 */
enum class osc_error_code {
    none,                // エラーなし
    connection_failed,   // 接続失敗
    send_failed,         // 送信失敗
    receive_failed,      // 受信失敗
    invalid_address,     // 無効なアドレス
    invalid_args,        // 無効な引数
    timeout              // タイムアウト
};

/**
 * エラー情報
 */
struct error_info {
    osc_error_code code = osc_error_code::none;
    std::string message;
    
    // エラーがあるかどうか
    bool has_error() const {
        return code != osc_error_code::none;
    }
};

/**
 * OSCメッセージハンドラの型定義
 */
using message_handler = std::function<void(const std::string& address, const atoms& args)>;

/**
 * エラーハンドラの型定義
 */
using error_handler = std::function<void(const error_info& error)>;

/**
 * OSCアドレスパターンハンドラのレジストリ
 */
class handler_registry {
public:
    // ハンドラの登録
    void register_handler(const std::string& pattern, message_handler handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_[pattern] = handler;
    }
    
    // ハンドラの削除
    void unregister_handler(const std::string& pattern) {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.erase(pattern);
    }
    
    // アドレスに対応するハンドラの取得
    message_handler get_handler(const std::string& address) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 完全一致の検索
        auto it = handlers_.find(address);
        if (it != handlers_.end()) {
            return it->second;
        }
        
        // パターンマッチング検索（ワイルドカード対応などを実装可能）
        for (const auto& pair : handlers_) {
            if (pattern_match(address, pair.first)) {
                return pair.second;
            }
        }
        
        // 見つからない場合は空ハンドラを返す
        return nullptr;
    }
    
    // すべてのハンドラを削除
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }
    
private:
    // アドレスパターンのマッチング
    bool pattern_match(const std::string& address, const std::string& pattern) {
        // OSC標準のパターンマッチングを実装
        // 支援するワイルドカード:
        // * - 0個以上の任意の文字列にマッチ
        // ? - 任意の1文字にマッチ
        // [...] - 指定した文字セットのいずれか1文字にマッチ
        // {a,b,c} - カンマ区切りパターンのいずれかにマッチ
        
        return osc_pattern_match(pattern, address);
    }
    
    // OSCパターンマッチングの再帰的実装
    bool osc_pattern_match(const std::string& pattern, const std::string& test, 
                            size_t p_pos = 0, size_t t_pos = 0) {
        // パターンの終了条件
        if (p_pos == pattern.size()) {
            return t_pos == test.size(); // 両方とも終了していればマッチ
        }
        
        // 現在のパターン文字をチェック
        char p = pattern[p_pos];
        
        // スペシャルケースの処理
        if (p == '*') {
            // 1. * は0文字にマッチする場合
            if (osc_pattern_match(pattern, test, p_pos + 1, t_pos)) {
                return true;
            }
            
            // 2. * は1文字以上にマッチする場合
            return t_pos < test.size() && 
                   osc_pattern_match(pattern, test, p_pos, t_pos + 1);
        }
        else if (p == '?') {
            // ? は任意の1文字にマッチ
            return t_pos < test.size() && 
                   osc_pattern_match(pattern, test, p_pos + 1, t_pos + 1);
        }
        else if (p == '[') {
            // [...] の文字クラスマッチング
            if (t_pos >= test.size()) {
                return false; // テスト文字列が終了している
            }
            
            // 文字クラスの終了位置を探す
            size_t end = pattern.find(']', p_pos + 1);
            if (end == std::string::npos) {
                return false; // 閉じ括弧が見つからない場合は不正なパターン
            }
            
            // 文字クラスの内容を取得
            std::string char_class = pattern.substr(p_pos + 1, end - p_pos - 1);
            
            // 否定クラスのチェック ([^...] の形式)
            bool negate = false;
            size_t class_pos = 0;
            
            if (!char_class.empty() && char_class[0] == '^') {
                negate = true;
                class_pos = 1;
            }
            
            // 文字クラス内でのマッチング
            bool match = false;
            while (class_pos < char_class.size()) {
                // 範囲指定 (a-z の形式) をチェック
                if (class_pos + 2 < char_class.size() && char_class[class_pos + 1] == '-') {
                    char range_start = char_class[class_pos];
                    char range_end = char_class[class_pos + 2];
                    
                    if (test[t_pos] >= range_start && test[t_pos] <= range_end) {
                        match = true;
                        break;
                    }
                    
                    class_pos += 3;
                }
                else {
                    // 単一文字のマッチング
                    if (char_class[class_pos] == test[t_pos]) {
                        match = true;
                        break;
                    }
                    
                    class_pos++;
                }
            }
            
            // 否定パターンの場合は結果を反転
            if (negate) {
                match = !match;
            }
            
            return match && osc_pattern_match(pattern, test, end + 1, t_pos + 1);
        }
        else if (p == '{') {
            // {a,b,c} のオルタネートパターンマッチング
            // 終了位置を探す
            size_t end = pattern.find('}', p_pos);
            if (end == std::string::npos) {
                return false; // 閉じ括弧が見つからない場合は不正なパターン
            }
            
            // カンマ区切りのパターンを処理
            size_t subpat_start = p_pos + 1;
            size_t comma_pos;
            
            while ((comma_pos = pattern.find(',', subpat_start)) != std::string::npos && comma_pos < end) {
                // 各パターンを一つずつテスト
                std::string new_pattern = pattern.substr(0, p_pos) + 
                                         pattern.substr(subpat_start, comma_pos - subpat_start) + 
                                         pattern.substr(end + 1);
                
                if (osc_pattern_match(new_pattern, test, 0, t_pos)) {
                    return true;
                }
                
                subpat_start = comma_pos + 1;
            }
            
            // 最後のサブパターンをテスト
            std::string new_pattern = pattern.substr(0, p_pos) + 
                                     pattern.substr(subpat_start, end - subpat_start) + 
                                     pattern.substr(end + 1);
            
            return osc_pattern_match(new_pattern, test, 0, t_pos);
        }
        else {
            // 通常の文字マッチング
            return t_pos < test.size() && p == test[t_pos] && 
                   osc_pattern_match(pattern, test, p_pos + 1, t_pos + 1);
        }
    }
    
    std::unordered_map<std::string, message_handler> handlers_;
    std::mutex mutex_;
};

} // namespace osc
} // namespace mcp
