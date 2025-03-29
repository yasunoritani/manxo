# OSC Bridge Technical Specification

本ドキュメントでは、M4L環境向けに実装されたOSC Bridgeの技術仕様を詳細に解説します。C++/Min-DevKitベースの実装における技術的詳細、プロトコル仕様、インターフェース定義などを提供します。

## 目次

1. [アーキテクチャ概要](#アーキテクチャ概要)
2. [OSCメッセージ仕様](#oscメッセージ仕様)
3. [ポート管理とネットワーク設定](#ポート管理とネットワーク設定)
4. [スレッドモデルと並行処理](#スレッドモデルと並行処理)
5. [エラー処理と回復メカニズム](#エラー処理と回復メカニズム)
6. [セキュリティ考慮事項](#セキュリティ考慮事項)
7. [パフォーマンス最適化](#パフォーマンス最適化)
8. [拡張性と将来の展望](#拡張性と将来の展望)

## アーキテクチャ概要

### コンポーネント構成

OSC Bridgeは以下の主要コンポーネントで構成されています：

1. **OSC Client (`mcp.osc_client.cpp`)**
   - OSCメッセージの送信を担当
   - 接続管理と再接続機能を提供
   - メッセージキューイングと非同期送信

2. **OSC Server (`mcp.osc_server.cpp`)**
   - UDPソケットによるOSCメッセージの受信
   - メッセージの解析とパターンマッチング
   - ハンドラへのディスパッチ

3. **OSC Bridge (`mcp.osc_bridge.cpp`)**
   - Max/M4L環境との統合層
   - ライフサイクル管理
   - 設定およびUI連携

4. **Type System (`mcp.osc_types.hpp`)**
   - OSCデータ型の定義
   - 型変換ユーティリティ
   - アドレスパターンマッチング

### データフロー

```
[Max/M4L Environment]
       ↑ ↓
[mcp.osc_bridge] ←→ [Handler Registry]
       ↑ ↓
[mcp.osc_client] [mcp.osc_server]
       ↑ ↓        ↑ ↓
       UDP        UDP
       ↑ ↓        ↑ ↓
[External Applications/Devices]
```

### キークラスとその役割

1. **OSCClient**
   - UDPソケット管理
   - メッセージ構築と送信
   - 再接続ロジック

2. **OSCServer**
   - UDPリスナー管理
   - メッセージ受信と解析
   - ハンドラディスパッチ

3. **OSCBridge**
   - 設定管理
   - イベントハンドリング
   - M4Lライフサイクル連携

## OSCメッセージ仕様

### サポートされるOSCデータ型

以下のOSCデータ型がサポートされています：

| 型名 | タグ文字 | 説明 | 対応するC++/Max型 |
|-----|---------|------|-----------------|
| Int32 | i | 32ビット整数 | int / int atom |
| Float32 | f | 32ビット浮動小数点 | float / float atom |
| String | s | NUL終端文字列 | std::string / symbol atom |
| Blob | b | バイナリデータ | std::vector<char> / atom array |
| Boolean (True) | T | 真値 | bool(true) / int(1) atom |
| Boolean (False) | F | 偽値 | bool(false) / int(0) atom |
| Nil | N | 空値 | nullptr / null atom |
| Infinitum | I | 無限大 | special value / special atom |
| Time Tag | t | OSCタイムタグ | custom struct / atoms |

### メッセージ形式

OSCメッセージは以下の形式に従います：

```
/<address_pattern> [,<type_tags>] [<arguments>...]
```

例：
```
/mcp/status ,sif "connected" 1 440.0
```

### アドレスパターンマッチング

OSC標準のパターンマッチングをサポートしています：

- `*` - 0個以上の任意の文字列にマッチ
- `?` - 任意の1文字にマッチ
- `[abc]` - 文字クラス（a, b, またはcにマッチ）
- `{foo,bar}` - 代替パターン（fooまたはbarにマッチ）

```cpp
// 実装例（oscpack拡張）
bool pattern_match(const std::string& address, const std::string& pattern) {
    return osc_pattern_match(pattern, address);
}
```

### バンドル

OSCバンドルもサポートされており、複数のメッセージを一つのパケットにまとめて送信できます：

```cpp
// OSCバンドルの構築例
::osc::OutboundPacketStream p(buffer_, buffer_size_);
p << ::osc::BeginBundleImmediate
  << ::osc::BeginMessage("/mcp/status")
  << "connected" << 1 << 440.0f
  << ::osc::EndMessage
  << ::osc::BeginMessage("/mcp/version")
  << "1.0.0"
  << ::osc::EndMessage
  << ::osc::EndBundle;
socket_->Send(p.Data(), p.Size());
```

## ポート管理とネットワーク設定

### 動的ポート割り当て

M4L環境では複数のインスタンスが共存するため、動的ポート割り当てが実装されています：

1. **試行メカニズム**:
   - 指定されたベースポートから開始
   - ポートが使用中の場合、インクリメントして再試行
   - 最大試行回数（デフォルト20回）まで試行

2. **ポート範囲**:
   - デフォルトベースポート: 7400（受信）、7500（送信）
   - 推奨範囲: 49152-65535（動的/プライベートポート範囲）

```cpp
// 動的ポート割り当ての実装例
bool bind_to_available_port(int base_port, int max_attempts = 20) {
    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        int port = base_port + attempt;
        try {
            if (socket_->Bind(IpEndpointName(address_.c_str(), port))) {
                bound_port_ = port;
                return true;
            }
        } catch (...) {
            // 次のポートを試行
            continue;
        }
    }
    return false; // 全てのポートが使用中
}
```

### アドレス解決

ホスト名からIPアドレスへの解決をサポートし、以下の機能を提供：

1. **名前解決**:
   - ホスト名（"localhost"など）のIPアドレスへの解決
   - IPv4/IPv6のサポート

2. **特殊アドレス**:
   - "localhost" - ループバックアドレス (127.0.0.1)
   - "any" - 全インターフェース (0.0.0.0)

```cpp
// アドレス解決の実装例
std::string resolve_address(const std::string& host) {
    if (host == "localhost") {
        return "127.0.0.1";
    } else if (host == "any") {
        return "0.0.0.0";
    }
    
    // その他の名前解決ロジック...
    return host;
}
```

## スレッドモデルと並行処理

### スレッド構成

OSC Bridgeは以下のスレッドモデルを採用しています：

1. **メインスレッド（UIスレッド）**:
   - Max/M4L環境のUIスレッド
   - 属性更新、UIイベント処理
   - `defer()`を介した安全な非同期処理

2. **リスナースレッド**:
   - OSCメッセージの受信
   - メッセージのパースと前処理
   - イベントディスパッチ

3. **送信スレッド（オプション）**:
   - 非同期送信モード時のメッセージ送信
   - キューイングと再試行ロジック

### スレッド間通信

1. **メッセージキュー**:
   - スレッド安全なキュー実装
   - 優先度付きメッセージ対応

2. **イベント通知**:
   - シグナル/スロットモデル
   - コールバック関数登録

3. **共有状態アクセス**:
   - ミューテックスによる保護
   - アトミック変数の使用

```cpp
// スレッド安全なキュー実装例
template<typename T>
class thread_safe_queue {
    std::queue<T> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    
public:
    void push(T value) {
        std::unique_lock<std::mutex> lock(mutex_);
        queue_.push(std::move(value));
        cv_.notify_one();
    }
    
    bool try_pop(T& value) {
        std::unique_lock<std::mutex> lock(mutex_);
        if (queue_.empty()) {
            return false;
        }
        value = std::move(queue_.front());
        queue_.pop();
        return true;
    }
    
    // その他のメソッド...
};
```

## エラー処理と回復メカニズム

### エラーの種類と検出

以下の種類のエラーを検出し、適切に処理します：

1. **ネットワークエラー**:
   - 接続失敗（ポートバインドエラーなど）
   - 送信エラー（ネットワーク到達不能など）
   - タイムアウト

2. **プロトコルエラー**:
   - 不正なOSCメッセージ形式
   - サポートされていないデータ型
   - アドレスパターンエラー

3. **アプリケーションエラー**:
   - リソース枯渇
   - 内部状態エラー
   - ハンドラ例外

### 回復メカニズム

1. **再接続**:
   - 接続失敗時の自動再接続
   - 指数バックオフによる再試行

```cpp
// 再接続ロジックの実装例
void reconnect_with_backoff() {
    int retry_count = 0;
    int max_retries = 5;
    int base_delay_ms = 100;
    
    while (retry_count < max_retries) {
        if (connect()) {
            log("Reconnected successfully");
            return;
        }
        
        // 指数バックオフ
        int delay_ms = base_delay_ms * (1 << retry_count);
        std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
        retry_count++;
    }
    
    log_error("Failed to reconnect after maximum retries");
}
```

2. **メッセージキューイング**:
   - 送信失敗時のメッセージ保持
   - 再接続後の再送

3. **グレースフルデグラデーション**:
   - 部分的な機能制限モード
   - 重要な機能の優先的な維持

## セキュリティ考慮事項

### 入力検証

1. **メッセージ検証**:
   - OSCメッセージの形式検証
   - アドレスパターンの検証
   - 引数の型と範囲の検証

```cpp
// メッセージ検証の実装例
bool validate_message(const ::osc::ReceivedMessage& msg) {
    try {
        // 基本的な構造チェック
        if (!msg.AddressPattern()) {
            return false;
        }
        
        // アドレスパターン形式チェック
        std::string address = msg.AddressPattern();
        if (address.empty() || address[0] != '/') {
            return false;
        }
        
        // 引数の検証
        try {
            for (auto it = msg.ArgumentsBegin(); it != msg.ArgumentsEnd(); ++it) {
                // 各引数の型と範囲の検証
                // ...
            }
        } catch (const ::osc::Exception& e) {
            return false;
        }
        
        return true;
    } catch (...) {
        return false;
    }
}
```

2. **リソース制限**:
   - 最大メッセージサイズの制限
   - レート制限
   - メモリ使用量の制限

### アクセス制御

1. **ホスト制限**:
   - デフォルトでlocalhostのみ許可
   - 明示的な許可リスト

2. **アドレスフィルタリング**:
   - 許可されたアドレスパターンのみ処理
   - 特権アドレスの保護

```cpp
// アドレスフィルタリングの実装例
bool is_address_allowed(const std::string& address) {
    // 許可リスト
    static const std::vector<std::string> allowed_patterns = {
        "/mcp/*",
        "/max/*",
        "/live/*"
    };
    
    for (const auto& pattern : allowed_patterns) {
        if (pattern_match(address, pattern)) {
            return true;
        }
    }
    
    return false;
}
```

## パフォーマンス最適化

### メモリ管理

1. **バッファプーリング**:
   - メッセージバッファの再利用
   - アロケーション最小化

2. **メモリ使用量の最適化**:
   - 適切なバッファサイズの選択
   - 不要なコピーの回避

```cpp
// バッファプーリングの実装例
class buffer_pool {
    std::vector<std::unique_ptr<char[]>> free_buffers_;
    std::mutex mutex_;
    const size_t buffer_size_;
    
public:
    buffer_pool(size_t buffer_size, size_t initial_count = 10) 
        : buffer_size_(buffer_size) {
        for (size_t i = 0; i < initial_count; ++i) {
            free_buffers_.push_back(std::make_unique<char[]>(buffer_size));
        }
    }
    
    std::unique_ptr<char[]> get() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (free_buffers_.empty()) {
            return std::make_unique<char[]>(buffer_size_);
        }
        
        auto buffer = std::move(free_buffers_.back());
        free_buffers_.pop_back();
        return buffer;
    }
    
    void release(std::unique_ptr<char[]> buffer) {
        std::lock_guard<std::mutex> lock(mutex_);
        free_buffers_.push_back(std::move(buffer));
    }
};
```

### 処理効率の最適化

1. **バッチ処理**:
   - メッセージのバッチ送受信
   - OSCバンドルの活用

2. **キャッシングと遅延評価**:
   - パターンマッチング結果のキャッシング
   - 設定変更時のみの再評価

3. **非同期処理**:
   - 時間のかかる処理の非同期実行
   - 処理のスレッド分散

## 拡張性と将来の展望

### 拡張可能な設計

1. **プラグイン構造**:
   - メッセージハンドラの動的登録
   - カスタム型のサポート

2. **設定の柔軟性**:
   - 実行時設定変更
   - 設定の永続化

### 将来の機能拡張

1. **追加サポート予定のOSC型**:
   - MIDIメッセージ型
   - より詳細なTimeTag処理
   - Array型サポート

2. **プロトコル拡張**:
   - OSC 1.1機能のサポート
   - 双方向型クエリのサポート
   - サービスディスカバリー

3. **高度な機能**:
   - 暗号化通信オプション
   - 複数エンドポイント管理
   - 高度なルーティングテーブル

## 実装リファレンス

### 主要なクラスと関数

#### OSC Client

```cpp
class OSCClient {
public:
    // 構築と初期化
    OSCClient();
    ~OSCClient();
    
    // 接続管理
    bool connect(const std::string& host, int port);
    void disconnect();
    bool is_connected() const;
    
    // メッセージ送信
    bool send_message(const std::string& address, const atoms& args);
    bool send_bundle(const std::vector<std::pair<std::string, atoms>>& messages);
    
    // 設定
    void set_buffer_size(size_t size);
    void set_reconnect_policy(reconnect_policy policy);
    
private:
    // 内部実装の詳細
    // ...
};
```

#### OSC Server

```cpp
class OSCServer {
public:
    // 構築と初期化
    OSCServer();
    ~OSCServer();
    
    // リスニング管理
    bool start(const std::string& address, int port);
    void stop();
    bool is_running() const;
    
    // ハンドラ管理
    void add_method(const std::string& pattern, message_handler handler);
    void remove_method(const std::string& pattern);
    
    // 設定
    void set_buffer_size(size_t size);
    void set_max_queue_size(size_t size);
    
private:
    // 内部実装の詳細
    // ...
};
```

#### OSC Bridge

```cpp
class OSCBridge : public object<OSCBridge> {
public:
    // Min-DevKit統合
    MIN_DESCRIPTION {"OSC Bridge for M4L integration"};
    MIN_TAGS {"network", "osc", "m4l"};
    MIN_AUTHOR {"Your Name"};
    
    // インレット/アウトレット
    inlet<> input {this, "(anything) Message to send"};
    outlet<> output {this, "(anything) Received messages"};
    outlet<> status_out {this, "(anything) Status messages"};
    
    // 属性
    attribute<symbol> host {...};
    attribute<int> port_in {...};
    attribute<int> port_out {...};
    attribute<bool> auto_connect {...};
    
    // メッセージハンドラ
    message<> connect {...};
    message<> disconnect {...};
    message<> status {...};
    
    // M4Lライフサイクルハンドラ
    void handle_m4l_liveset_loaded(t_object* x);
    void handle_m4l_liveset_saved(t_object* x);
    void handle_m4l_liveset_closed(t_object* x);
    void handle_m4l_liveset_new(t_object* x);
    
private:
    // 内部実装の詳細
    // ...
};
```

### 設定オプション一覧

| オプション名 | 型 | デフォルト値 | 説明 |
|------------|-----|------------|------|
| host | symbol | "localhost" | OSCサーバーのホスト名またはIPアドレス |
| port_in | int | 7400 | 受信ポート |
| port_out | int | 7500 | 送信ポート |
| auto_connect | bool | true | 起動時の自動接続 |
| reconnect | bool | true | 切断後の自動再接続 |
| retry_count | int | 5 | 再接続試行回数 |
| retry_interval | int | 1000 | 再接続間隔（ミリ秒） |
| buffer_size | int | 4096 | OSCバッファサイズ（バイト） |
| async_send | bool | false | 非同期送信モード |
| queue_size | int | 100 | メッセージキューサイズ |
| m4l_compatibility | bool | true | M4L互換モード |
| address_filter | symbol | "/mcp/*,/max/*" | 許可アドレスフィルタ |
| dynamic_ports | bool | true | 動的ポート割り当て |
