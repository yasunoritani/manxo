# Issue 27: MCP WebSocketサポートの実装

## 概要
OSC BridgeにWebSocketサポートを追加することで、ブラウザベースのアプリケーションやWebSocket対応のクライアントから直接OSCメッセージを送受信できるようにします。これにより、Max/MSPとWeb技術の統合が容易になります。

## 詳細
- WebSocketサーバー機能の実装
- WebSocketクライアント機能の実装
- JSON-OSC相互変換機能
- MCP（Message Control Protocol）対応

### 技術的要件
1. libwebsocketsライブラリの統合
2. nlohmann/jsonライブラリによるJSON処理
3. arm64とx86_64の両アーキテクチャ対応（ユニバーサルバイナリ）
4. 動的ポート割り当て（M4L環境向け）

## 再現手順
該当なし（新機能実装）

## 解決案
1. **WebSocketサーバー/クライアントクラスの実装**
   - `mcp.websocket_server.hpp`
   - `mcp.websocket_client.hpp`
   - `mcp.websocket_mcp_handler.hpp`

2. **OSC-JSON変換**
   - OSCメッセージをJSON形式に変換
   - JSONメッセージをOSC形式に変換

3. **Max/MSPインターフェース**
   - `websocket_server <port>` - WebSocketサーバーを起動
   - `websocket_client <ws_url>` - WebSocketサーバーに接続
   - `send_osc <address> [args...]` - OSCメッセージを送信

4. **テスト環境**
   - Pythonテストクライアント
   - Pythonテストサーバー
   - Maxテストパッチ

## 優先度: 高

## カテゴリ: OSCブリッジ

## 担当者
実装済み

## 見積もり工数
3人日（完了）
