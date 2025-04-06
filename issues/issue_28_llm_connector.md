# Issue 28: Claude/LLM MCPコネクタの実装

## 概要
WebSocketサポート（Issue 27）を基盤として、OSC BridgeとClaude Desktop（またはその他のLLM）をMCPプロトコルで連携する機能を実装します。これにより、MaxパッチからLLMに直接アクセスし、AI機能を音楽・メディアアプリケーションに統合できるようになります。

## 詳細
- Claude Desktopやその他のLLM APIと連携するためのMCPコネクタ機能の実装
- プロンプト送信と応答受信のための標準化されたメッセージフォーマットの定義
- Max内でプロンプトを構築し、応答を処理するためのインターフェースの提供
- 既存のWebSocket機能を活用した効率的な実装

### 技術的要件
1. Claude Desktop APIとのWebSocket接続実装
2. JSON形式でのLLMプロンプト/レスポンス構造の設計
3. LLM応答の非同期処理とコールバック管理
4. エラーハンドリングと再接続機能
5. サンプルMax/MSPパッチの作成

## 再現手順
該当なし（新機能実装）

## 解決案
1. **MCPメッセージング拡張**
   - LLM関連のMCPコマンドを追加（`llm_prompt`, `llm_response`など）
   - JSONメッセージ構造の拡張

2. **LLMコネクタクラスの実装**
   ```cpp
   class llm_connector {
   public:
       bool connect(const std::string& api_endpoint);
       bool send_prompt(const std::string& prompt, const json& options = {});
       void register_response_callback(std::function<void(const json&)> callback);
       // ...
   };
   ```

3. **Max/MSPインターフェース**
   - `llm_prompt [text] [options]` - LLMにプロンプトを送信
   - `llm_config [param] [value]` - LLM設定の構成
   - `llm_model [model_name]` - 使用するLLMモデルの選択

4. **認証とセキュリティ**
   - APIキーの安全な管理
   - 通信の暗号化と検証

## 優先度: 最高

## カテゴリ: OSCブリッジ

## 担当者
未割り当て

## 見積もり工数
3人日
