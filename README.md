# Max-Claude連携プロジェクト

## プロジェクト概要

本プロジェクトは、Claude DesktopのAI機能とMax/MSPのサウンドプログラミング環境を連携させるシステムです。ユーザーは自然言語でMax/MSPの操作や音楽生成の指示を出すことができます。

## システム構成

システムは以下の3つの主要コンポーネントで構成されています：

1. **MCPサーバー（Node.js）**：
   - Claude DesktopとのMCP（Model Context Protocol）通信を担当
   - JSON形式でのデータやり取り
   - SQL知識ベースへのクエリと結果処理

2. **SQL知識ベース**：
   - Max/MSPのオブジェクト情報
   - 接続パターンのバリデーション
   - 修正提案の生成

3. **Min-DevKit（C++）**：
   - Max/MSPへの直接アクセス
   - MCPサーバーからの命令をMax/MSP操作に変換
   - C++ベースの高速処理

## セットアップ方法

### 前提条件

- Max/MSP 8以降
- Node.js 14以降
- C++開発環境（Min-DevKit用）
- Claude Desktop

### インストール手順

1. **リポジトリのクローン**
   ```bash
   git clone https://github.com/yourusername/max-claude-integration.git
   cd max-claude-integration
   npm install
   ```

2. **MCPサーバーの設定**
   ```bash
   # Claude Desktopの設定ファイルディレクトリ作成
   mkdir -p ~/Library/Application\ Support/Claude/

   # 設定ファイル編集
   nano ~/Library/Application\ Support/Claude/claude_desktop_config.json
   ```

   以下の内容を設定ファイルに追加：
   ```json
   {
     "mcp": {
       "servers": [
         {
           "name": "Max Knowledge Base",
           "command": "node /path/to/max-claude-integration/src/sql_knowledge_base/mcp-server.js"
         }
       ]
     }
   }
   ```

3. **Min-DevKitのビルド（開発者向け）**
   ```bash
   cd src/min-devkit
   cmake .
   make
   ```

## 使い方

1. **Claude Desktopを起動（または再起動）**
   - 設定を反映させるためにClaude Desktopを再起動します

2. **MCPツールの確認**
   - Claude Desktopの入力欄上部でMCPツールが表示されていることを確認
   - 「Max Knowledge Base」が利用可能なMCPツールとして表示されます

3. **Max/MSPに関する質問や指示を入力**
   - 例：「サイン波オシレーターを作成して、ローパスフィルターにつないで」
   - Claude Desktopが自動的にSQL知識ベースと連携して回答・操作を行います

## 技術詳細

### MCPプロトコル

Model Context Protocol (MCP) はAnthropicが開発したオープンスタンダードで、AIモデルと外部ツールを安全に接続するプロトコルです。本プロジェクトでは、MCPを使用してClaude DesktopとMax/MSP関連の知識・操作機能を連携させています。

### Min-DevKit

Min-DevKitはCycling '74が提供する現代的なC++フレームワークで、Max/MSPの拡張開発を容易にします。本プロジェクトでは、Min-DevKitを使用してClaude Desktopからの命令をMax/MSP内部で実行可能な形式に変換し、直接操作を行います。

### データフロー

1. ユーザーがClaude Desktopに自然言語で指示
2. Claude Desktopが指示を理解し、MCPプロトコルを通じてサーバーに問い合わせ
3. MCPサーバーがSQL知識ベースを参照して適切な操作を決定
4. 操作命令がMin-DevKitを通じてMax/MSPに伝達
5. Max/MSPが操作を実行し、結果をフィードバック
6. 結果情報がClaude Desktopに返され、ユーザーに提示

## 開発状況

現在進行中の課題:
- SQL知識ベースの拡充
- Min-DevKit連携の安定化
- ユーザーインターフェースの改善

## ドキュメント

- [技術仕様書](docs/specifications/technical_specs.md)
- [Min-DevKit実装詳細](docs/min-devkit_implementation.md)
- [SQL知識ベース構造](docs/sql_database_schema.md)

## ライセンス

本プロジェクトはMITライセンスで提供されています。

Min-DevKitはCycling '74によるMITライセンスのソフトウェアです。
Copyright © 2016-2022, Cycling '74
