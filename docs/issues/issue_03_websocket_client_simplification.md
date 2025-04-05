# Issue #3: WebSocketクライアントの簡略化

## 概要

現在のWebSocketクライアント実装（`llm.mcp.cpp`）は複数のLLM対応やモデル選択機能など、不要な機能を含んでいます。このイシューでは、Claude Desktop専用のシンプルで効率的なWebSocketクライアントに実装を簡略化します。

## 目的

- 現行の複雑なクライアント実装からClaude Desktop専用のシンプルな実装へ移行
- 余分な機能（複数LLM対応、モデル選択など）を削除
- エラーハンドリングと安定性の強化
- パフォーマンスの最適化
- コードの保守性と可読性の向上

## タスク

### 3.1 WebSocketクライアントのコード分析

- [x] 現行の`llm.mcp.cpp`の詳細分析
- [x] 必須機能と余分な機能の特定
- [x] WebSocket通信部分のコード品質評価
- [x] 依存関係の洗い出し

### 3.2 シンプルなWebSocketクライアントの設計

- [ ] Claude Desktop専用の通信仕様に合わせた設計
- [ ] クラス構造とインターフェースの設計
- [ ] 状態管理とエラーハンドリングの設計
- [ ] ユニットテスト可能な設計の考慮

### 3.3 基本通信機能の実装

- [ ] WebSocket接続管理（接続、切断）
- [ ] メッセージ送受信
- [ ] リクエスト/レスポンス処理
- [ ] 非同期通信のサポート

### 3.4 エラーハンドリングの強化

- [ ] ネットワークエラー処理
- [ ] プロトコルエラー処理
- [ ] 回復メカニズム実装
- [ ] エラーログとフィードバック
- [ ] タイムアウト処理の実装

### 3.5 テストとドキュメント

- [ ] 単体テストの作成
- [ ] 統合テストの実装
- [ ] APIドキュメントの作成
- [ ] サンプルコードの提供

## 技術的詳細

### シンプル化されたWebSocketクライアント設計

```cpp
// mcp_client.hpp
class MCPClient {
public:
    // コンストラクタと初期化
    MCPClient(outlet<>& output, outlet<>& error_out);
    ~MCPClient();

    // 接続管理
    bool connect(const std::string& url);
    void disconnect();
    bool is_connected() const;

    // メッセージ送受信
    bool send_message(const json& message);
    bool send_tool_call(const std::string& tool_name, const json& parameters, int request_id);

    // コールバック設定
    void set_message_handler(std::function<void(const json&)> handler);
    void set_connection_handler(std::function<void(bool)> handler);
    void set_error_handler(std::function<void(const std::string&)> handler);
    void set_tool_response_handler(std::function<void(int, const json&)> handler);

    // タイムアウト設定
    void set_timeout(std::chrono::milliseconds timeout);

private:
    // WebSocketコールバック
    static int callback_function(struct lws *wsi, enum lws_callback_reasons reason,
                               void *user, void *in, size_t len);

    // 内部処理メソッド
    void on_connection_established();
    void on_connection_closed();
    void on_message_received(const char* data, size_t len);
    void on_connection_error(const std::string& error);
    void on_writeable();

    // サービススレッド
    void service_thread_func();

    // タイムアウト監視
    void timeout_monitor_func();

    // 内部状態・変数
    outlet<>& output_;
    outlet<>& error_out_;
    std::atomic<bool> is_connected_;
    std::atomic<bool> should_stop_;
    std::string url_;
    std::chrono::milliseconds timeout_{10000}; // デフォルト10秒

    struct lws_context* context_;
    struct lws* wsi_;

    std::thread service_thread_;
    std::thread timeout_thread_;
    std::mutex queue_mutex_;
    std::mutex pending_requests_mutex_;
    std::queue<std::string> outgoing_messages_;
    std::map<int, std::chrono::steady_clock::time_point> pending_requests_;

    // コールバック
    std::function<void(const json&)> message_handler_;
    std::function<void(bool)> connection_handler_;
    std::function<void(const std::string&)> error_handler_;
    std::function<void(int, const json&)> tool_response_handler_;
};
```

### 削除すべき機能

以下の機能は新しい実装から削除します：

1. **複数LLM対応**: Claude Desktop以外のAPI（Claude API、OpenAI API）サポート
2. **モデル選択機能**: モデル一覧取得・選択機能
3. **API認証**: APIキー管理と認証機能
4. **設定パラメータ管理**: 各種LLM設定パラメータ
5. **汎用UIコンポーネント**: 不要なUI要素と設定画面

### 残すべき機能

以下の機能は新しい実装に残します：

1. **WebSocket接続管理**: 接続・切断・状態確認
2. **メッセージ送受信**: JSON形式のメッセージ送受信
3. **エラーハンドリング**: ネットワークエラーと回復メカニズム
4. **スレッド安全性**: マルチスレッド対応と同期処理
5. **ツール呼び出し**: MCPツール呼び出しの簡易インターフェース

### Claude DesktopとのMCP通信例

```javascript
// Claude Desktopに送信するMCPメッセージ例
{
  "method": "mcp/tools/call",
  "params": {
    "name": "createSynthPatch",
    "parameters": {
      "synthType": "fm",
      "complexity": 3,
      "voices": 4
    }
  },
  "id": 1
}

// Claude Desktopからの応答例
{
  "result": {
    "status": "success",
    "patchCreated": true,
    "patchId": "synth_fm_001",
    "objectCount": 12
  },
  "id": 1
}

// エラー応答例
{
  "error": {
    "code": -32603,
    "message": "Internal error occurred while creating synth patch"
  },
  "id": 1
}
```

## 実装ガイドライン

1. **コード整理**:
   - 現行の`llm.mcp.cpp`からWebSocket通信部分を抽出・整理
   - 単一責任の原則に従ったクラス設計
   - Min-DevKitの作法に従った実装

2. **インターフェース設計**:
   - シンプルで一貫性のあるAPIを設計
   - 直感的な命名規則の採用
   - 拡張性を考慮したインターフェース

3. **エラーハンドリング**:
   - すべての外部API呼び出しをtry-catchで囲む
   - ネットワークエラーに対する適切なリトライロジック（最大3回、指数バックオフ）
   - 明確なエラーメッセージとログ
   - タイムアウト処理の実装（応答がない場合のフォールバック）

4. **パフォーマンス考慮**:
   - メッセージバッファリングと非同期処理
   - スレッドセーフな実装（ミューテックスによる同期）
   - リソースリーク防止（RAII原則の徹底）
   - メモリ使用量の最適化

5. **テスト容易性**:
   - モックオブジェクトを利用するためのインターフェース抽象化
   - 依存性注入パターンの採用
   - テスト可能なコンポーネント分割

## テスト計画

1. **単体テスト**:
   - 接続・切断シナリオ
   - メッセージ送受信
   - エラー条件
   - タイムアウト処理
   - リトライロジック
   - スレッド安全性

2. **統合テスト**:
   - Claude Desktopとの実際の通信
   - エッジケースとエラー回復
   - 長時間安定性テスト
   - 高負荷テスト（多数のメッセージ処理）

3. **モック環境でのテスト**:
   - WebSocketサーバーモックを使用した自動テスト
   - さまざまなエラー状況のシミュレーション

## 関連リソース

- [libwebsocketsドキュメント](https://libwebsockets.org/lws-api-doc-main/html/index.html)
- [MCP通信仕様](https://modelcontextprotocol.io/)
- [nlohmann/json ライブラリ](https://github.com/nlohmann/json)
- [Min-DevKit リファレンス](https://github.com/Cycling74/min-devkit)
- [v8uiリポジトリ](https://github.com/yasunoritani/v8ui)

## 完了条件

- Claude Desktop専用のシンプルなWebSocketクライアントが実装されている
- 余分な機能（複数LLM対応、モデル選択）が削除されている
- エラーハンドリングが強化され、安定した通信が可能
- Claude Desktopとの通信テストが成功している
- テストカバレッジが80%以上
- コードドキュメントが完備されている
- サンプルコードが提供されている
