# Issue #003: WebSocketクライアントの簡略化

## 概要

現在のWebSocketクライアント実装には、複数のLLMとモデル選択をサポートする機能など、Claude Desktop専用途であるこのプロジェクトには不要な機能が含まれています。このイシューでは、Claude Desktop専用の簡略化されたWebSocketクライアントを実装し、エラー処理の改善、安定性の向上、パフォーマンスの最適化を行います。

## 目的

- Claude Desktop専用の簡略化されたWebSocketクライアント実装へ移行する
- 不要な機能（複数LLMサポート、モデル選択など）を削除する
- エラー処理を強化し、接続の安定性を向上させる
- コード可読性とメンテナンス性を向上させる
- クリーンなインターフェースと適切なドキュメントを提供する

## タスク

### 3.1 現状分析と設計

- [ ] 既存WebSocketクライアントの詳細分析と評価
- [ ] 依存関係の特定と最小化計画
- [ ] 簡略化クライアントのクラス設計と方式確定
- [ ] ユニットテスト可能な設計の検討

### 3.2 基本実装

- [ ] 新しいMCPClientクラスの基本実装
- [ ] WebSocketとの基本通信機能の実装
- [ ] エラー処理と回復メカニズムの実装
- [ ] スレッドセーフな実装の確保

### 3.3 拡張機能と最適化

- [ ] メッセージ送受信機能の最適化
- [ ] タイムアウト処理の改善
- [ ] Claude Desktopとの互換性確認
- [ ] ツール呼び出し機能の実装

### 3.4 テストと文書化

- [ ] 単体テストの作成と実行
- [ ] Claude Desktopとの統合テスト
- [ ] コードドキュメントの整備
- [ ] 使用例とチュートリアルの作成

## 技術的詳細

### MCPClientクラス設計

```cpp
namespace v8ui::mcp {

class MCPClient {
public:
    // シングルトンインスタンス
    static MCPClient& getInstance();

    // 接続管理
    bool connect(const std::string& url, int port,
                 const std::string& path = "/");
    void disconnect();
    bool isConnected() const;
    void setConnectionTimeout(std::chrono::milliseconds timeout);

    // メッセージ送信
    bool sendMessage(const std::string& message);
    bool sendMessage(const json& message);

    // ツール呼び出し送信
    bool sendToolCall(const std::string& toolName,
                       const json& parameters);

    // コールバック設定
    void setMessageCallback(std::function<void(const std::string&)> callback);
    void setErrorCallback(std::function<void(const std::string&, int)> callback);
    void setConnectionStatusCallback(std::function<void(bool)> callback);

private:
    MCPClient();
    ~MCPClient();

    // libwebsocketsとの連携
    static int lwsCallback(struct lws* wsi, enum lws_callback_reasons reason,
                           void* user, void* in, size_t len);

    void handleMessage(const std::string& message);
    void handleError(const std::string& error, int code);
    void updateConnectionStatus(bool connected);

    // 内部データ
    struct Impl;
    std::unique_ptr<Impl> pImpl;
};

} // namespace v8ui::mcp
```

### 削除される機能

簡略化の一環として削除される機能:

1. 複数のLLMプロバイダーサポート
2. モデル選択機能
3. API認証と鍵管理
4. 汎用UIコンポーネント
5. 複雑なメッセージ変換レイヤー

### 保持される機能

必須機能として保持されるもの:

1. WebSocket接続管理
2. メッセージ送受信
3. エラー処理と回復メカニズム
4. コールバックベースのイベント通知
5. スレッドセーフな実装
6. 基本的なツール呼び出し機能

### MCPメッセージ例

Claude Desktopとの通信に使用されるMCPメッセージ例:

```json
// Claude Desktopへ送信するメッセージ
{
  "type": "message",
  "data": {
    "content": "Max/MSPで簡単なシンセサイザーパッチを作成してください。",
    "context": {
      "currentPatch": {
        "name": "my_synth.maxpat",
        "objects": []
      }
    }
  }
}

// Claude Desktopから受信するツール呼び出し
{
  "type": "tool_call",
  "data": {
    "tool": "createMaxObject",
    "parameters": {
      "objectType": "cycle~",
      "position": {"x": 100, "y": 100},
      "args": "440"
    }
  }
}
```

## 実装ガイドライン

1. **リーンな設計**:
   - 必要最小限の機能に集中
   - クラス間の依存関係を最小化
   - シンプルで理解しやすいインターフェースを設計

2. **エラー処理**:
   - 堅牢なエラー処理メカニズム
   - 接続の中断と回復プロセス
   - 明確なエラーメッセージとログ記録

3. **スレッド安全性**:
   - スレッドセーフなメッセージ処理
   - 非同期操作のための適切なプロミスとフューチャーの使用
   - スレッド間の適切な同期処理

4. **パフォーマンス**:
   - 効率的なメモリ使用
   - メッセージのバッファリングと最適化
   - 最小限のパケット交換

## テスト計画

### 単体テスト
- 接続と認証機能のテスト
- メッセージ送受信のテスト
- エラー処理と回復のテスト

### 統合テスト
- モックサーバーとの通信テスト
- 実際のClaude Desktopとの統合テスト
- ツール呼び出しの処理テスト

### ストレステスト
- 大量メッセージ送信テスト
- 接続中断シナリオテスト
- 長時間実行安定性テスト

## 関連リソース

- [libwebsocketsドキュメント](https://libwebsockets.org/lws-api-doc-main/html/index.html)
- [MCPプロトコル仕様](https://github.com/anthropics/anthropic-cookbook/tree/main/model_context_protocol)
- [WebSocketプロトコル (RFC 6455)](https://tools.ietf.org/html/rfc6455)

## 完了条件

- Claude Desktop専用の簡略化されたWebSocketクライアントが実装されている
- 不要な機能が削除され、コードベースがクリーンアップされている
- エラー処理が強化され、安定性が向上している
- 単体テストと統合テストが追加され、80%以上のコードカバレッジがある
- Claude Desktopとの通信テストに合格している
- インターフェースとAPIが文書化されている

## 優先度とカテゴリ

**優先度**: 高
**カテゴリ**: コア機能
