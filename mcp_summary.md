# Max-Claude Desktop MCP連携プロジェクト調査まとめ

## プロジェクトの目標

- **AbletonMCPをベースモデル**にして、Claude DesktopからMaxを操作するシステムを構築
- Min-DevKitを使用してMax環境向けのMCP連携を実装
- 自然言語によるMax/MSPの制御を可能にする
- Max環境（音楽・マルチメディア制作環境）を自然言語インターフェースで操作可能にする

## 参考プロジェクト: AbletonMCP

### 概要
- **GitHub**: https://github.com/ahujasid/ableton-mcp
- **目的**: Ableton LiveとClaude AIをMCPで連携させ、AIによる音楽制作を実現
- **構成**:
  1. **Ableton Remote Script**: Ableton Live内で動作するPythonスクリプト
  2. **MCP Server**: Claude DesktopとAbleton間の通信を仲介するサーバー

### AbletonMCPの主な特徴
- TCPソケット通信によるJSON形式のメッセージ交換
- Claude Desktop設定ファイルによるMCPサーバー定義
- 具体的な音楽制作ユースケース（トラック作成、エフェクト追加など）
- インストール手順と設定例の詳細な提供

## MCP (Model Context Protocol) の理解

### MCPの基本情報
- Anthropicが開発したLLMと外部システムを連携させるオープンプロトコル
- JSONベースのメッセージ交換を行う
- リソース管理、ツール連携、コンテキスト共有などの機能を提供

### 通信方式
- WebSocket接続（例: `ws://localhost:5678/api/organizations/_/chat_conversations`）
- またはSTDIO（標準入出力）による通信

### 設定方法
- Claude Desktopの設定ファイル: `~/Library/Application Support/Claude/claude_desktop_config.json`
- ここにMCPサーバーの接続情報を記述する例:
  ```json
  {
    "mcpServers": {
      "MaxMCP": {
        "command": "node",
        "args": ["max-mcp-server.js"]
      }
    }
  }
  ```

## Min-DevKitについて

- **Min-DevKit**: C++ベースのMax拡張開発フレームワーク
  - Max 7.x/8.x/9.x向けの開発環境
  - MITライセンスで提供
  - C++17による拡張開発に対応
  - 公式リポジトリ: https://github.com/Cycling74/min-devkit

- **特徴**:
  - ネイティブC++オブジェクト実装
  - 高性能処理とMax内部APIへの直接アクセス
  - クロスプラットフォーム対応（Mac/Windows）
  - CMakeベースのビルドシステム

## 現在の実装アプローチ

### 現在の実装
- `llm.mcp.cpp`: WebSocketを使用してClaude Desktopと通信するクライアント実装
- 複数のLLMタイプ（Claude Desktop、Claude API、OpenAI API）をサポート
- モデル選択機能やAPI認証など高度な機能を実装

### 実装上の課題と方向性
- **モデル指定は不要**: Claude Desktopとの単純なMCP連携に焦点を当てるべき
- **複雑な機能は簡素化**: 現状の`llm.mcp.cpp`にある複数LLM対応などは不要
- **AbletonMCPの構造を参考**: シンプルかつ直感的なツール定義に注力

## 実装アプローチの比較

| 項目 | AbletonMCP（参考） | 現状のMax連携実装 |
|------|-------------------|------------------|
| 通信方式 | TCPソケット通信 | WebSocket + OSCブリッジ |
| MCP接続 | Claude Desktopの設定ファイルに定義 | 同様の設定ファイル方式 |
| クライアント | PythonベースのRemote Script | C++/Min-DevKitベースの実装 |
| サーバー | PythonベースのMCPサーバー | Node.js/FastMCP実装 |
| ユーザー体験 | ツール選択UIによる直感的操作 | 同様のアプローチを目指す |

## Max環境との連携

### アーキテクチャ
- Min-DevKitを用いたC++ベースの実装
- OSCブリッジによるMax環境とMCPの連携
- libwebsocketsライブラリを使用したWebSocket通信

### ユースケース
- テキスト記述からMaxパッチ自動生成
- 自然言語による音楽構造の操作
- インタラクティブな音楽生成とパラメータ制御
- パッチの作成・編集・保存
- オブジェクトの配置と接続
- パラメータの設定と調整

## 次のステップ

1. AbletonMCPのサーバー実装を詳細に分析し、Max環境に適応
2. Min-DevKitベースの実装を、Claude DesktopのMCP仕様に合わせて最適化
3. ユーザーフレンドリーなインターフェースとドキュメントの作成
4. Claude Desktopとの連携に特化したシンプルな実装への改善
5. Max環境特有の要件に合わせたMCPツールの開発

## リファレンス

- AbletonMCP: https://github.com/ahujasid/ableton-mcp
- Min-DevKit: https://github.com/Cycling74/min-devkit
- MCP公式ドキュメント: https://modelcontextprotocol.io/
- libwebsocketsライブラリ: https://libwebsockets.org/
