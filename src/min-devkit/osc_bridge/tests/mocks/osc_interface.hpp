#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>

// Min-DevKit依存のないOSCインターフェース定義
namespace mcp {
namespace osc {

// エラーコード - 実際の実装と互換性を保つ
enum class osc_error_code {
    none,
    connection_failed,
    send_failed,
    receive_failed
};

// 接続設定 - Min-DevKit依存のない純粋なデータ構造
struct connection_config {
    std::string host = "127.0.0.1";
    int port_in = 7400;
    int port_out = 7500;
    bool dynamic_ports = true;
    bool m4l_compatibility = true;
    int max_retry = 5;
};

// atomsのシンプルな代替実装
class atoms {
public:
    atoms() = default;
    
    template<typename T>
    atoms(const T& value) {
        add(value);
    }
    
    template<typename T, typename... Args>
    atoms(const T& value, const Args&... args) : atoms(args...) {
        add(value);
    }
    
    template<typename T>
    void add(const T& value) {
        if constexpr(std::is_same_v<T, int>) {
            values.push_back(std::to_string(value));
            types.push_back("int");
        } else if constexpr(std::is_same_v<T, float> || std::is_same_v<T, double>) {
            values.push_back(std::to_string(value));
            types.push_back("float");
        } else if constexpr(std::is_same_v<T, std::string>) {
            values.push_back(value);
            types.push_back("string");
        } else if constexpr(std::is_same_v<T, bool>) {
            values.push_back(value ? "true" : "false");
            types.push_back("bool");
        } else if constexpr(std::is_same_v<T, const char*> || std::is_array_v<T>) {
            // 文字列リテラルや文字配列の処理
            values.push_back(value);
            types.push_back("string");
        } else {
            values.push_back(std::to_string(value));
            types.push_back("unknown");
        }
    }
    
    std::string get_string(size_t index) const {
        return index < values.size() ? values[index] : std::string();
    }
    
    int get_int(size_t index) const {
        try {
            return index < values.size() ? std::stoi(values[index]) : 0;
        } catch(...) {
            return 0;
        }
    }
    
    float get_float(size_t index) const {
        try {
            return index < values.size() ? std::stof(values[index]) : 0.0f;
        } catch(...) {
            return 0.0f;
        }
    }
    
    bool get_bool(size_t index) const {
        return index < values.size() ? (values[index] == "true") : false;
    }
    
    size_t size() const {
        return values.size();
    }
    
    // 型情報の取得
    std::string get_type(size_t index) const {
        return index < types.size() ? types[index] : "";
    }
    
    // 値と型情報のコピーを作成
    atoms clone() const {
        atoms result;
        for (size_t i = 0; i < values.size(); ++i) {
            if (i < types.size()) {
                if (types[i] == "int") {
                    result.add(get_int(i));
                } else if (types[i] == "float") {
                    result.add(get_float(i));
                } else if (types[i] == "bool") {
                    result.add(get_bool(i));
                } else {
                    result.add(get_string(i));
                }
            } else {
                result.add(get_string(i));
            }
        }
        return result;
    }
    
private:
    std::vector<std::string> values;
    std::vector<std::string> types;
};

// OSCメッセージハンドラ型定義
using message_handler_t = std::function<void(const std::string&, const atoms&)>;

// OSCクライアントインターフェース
class client_interface {
public:
    virtual ~client_interface() = default;
    
    virtual bool connect(const std::string& host, int port) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual void set_target(const std::string& host, int port) = 0;
    virtual bool send(const std::string& address, const atoms& args) = 0;
    virtual bool send_internal(const std::string& address, const std::vector<std::string>& args) = 0;
    virtual osc_error_code get_last_error() const = 0;
};

// OSCサーバーインターフェース
class server_interface {
public:
    virtual ~server_interface() = default;
    
    virtual bool start(int port) = 0;
    virtual void stop() = 0;
    virtual bool is_running() const = 0;
    virtual void set_port(int port) = 0;
    virtual void add_method(const std::string& pattern, message_handler_t handler) = 0;
    virtual void remove_method(const std::string& pattern) = 0;
    virtual osc_error_code get_last_error() const = 0;
};

// OSCブリッジインターフェース
class bridge_interface {
public:
    virtual ~bridge_interface() = default;
    
    virtual bool connect(const std::string& host, int port_in, int port_out) = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual bool send(const std::string& address, const atoms& args) = 0;
    virtual void add_method(const std::string& pattern, message_handler_t handler) = 0;
    virtual void remove_method(const std::string& pattern) = 0;
    virtual osc_error_code get_last_error() const = 0;
    virtual void handle_m4l_event(const std::string& event_name) = 0;
};

} // namespace osc
} // namespace mcp
