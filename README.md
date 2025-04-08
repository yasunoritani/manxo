# Max-Claude 連携プロジェクト

![プロジェクトロゴ](assets/logo.png)

## このプロジェクトについて

**Max-Claude連携プロジェクト**は、Max/MSPプログラミングの専門知識がなくても、AIの力を使って音楽・サウンド制作を行えるようにするシステムです。

**シンプルな例**:
> 「サイン波オシレーターを作成して、ローパスフィルターにつないで」

このような自然言語の指示だけで、Maxパッチが自動的に作成されます。

## 核となる3つの技術

![システム概要](assets/system_overview.png)

1. **Claude Desktop (LLM)**: ユーザーの意図を理解し、適切な指示に変換します
2. **SQL知識ベース**: Max/MSPとMin-DevKitの専門知識を格納し、AIの判断を支援します
3. **Min-DevKit (C++)**: Max/MSPを直接操作する橋渡しを行います

## 使い方

### 1. MCPサーバーの設定

1. **リポジトリのクローン**
   ```bash
   git clone https://github.com/yourusername/v8ui.git
   cd v8ui
   npm install
   ```

2. **MCPサーバーの設定ファイルを作成**
   Claude Desktopの設定ファイルにMCPサーバーを追加します：

   Macの場合:
   ```bash
   # 設定ディレクトリがない場合は作成
   mkdir -p ~/Library/Application\ Support/Claude/

   # 設定ファイルを編集
   nano ~/Library/Application\ Support/Claude/claude_desktop_config.json
   ```

   以下の内容を追加:
   ```json
   {
     "mcp": {
       "servers": [
         {
           "name": "Max Knowledge Base",
           "command": "node /path/to/v8ui/src/sql_knowledge_base/mcp-server.js"
         }
       ]
     }
   }
   ```

### 2. Claude Desktopでの使用

1. **Claude Desktopを起動（または再起動）**
   - 設定を反映させるためにClaude Desktopを再起動します

2. **MCPツールの確認**
   - メッセージの入力欄の上部でMCPツールが表示されていることを確認します
   - 「Max Knowledge Base」が利用可能なMCPツールとして表示されます

3. **Max/MSPに関する質問や指示を入力**
   - 例：「サイン波オシレーターを作成して、ローパスフィルターにつないで」
   - Claude Desktopが自動的にSQLデータベースを参照して適切な回答を生成します

## 主な機能

- **自然言語→Maxパッチ変換**: 日常言語でMax/MSPプログラミング
- **音楽的意図の理解**: 音楽理論やサウンドデザインの専門用語に対応
- **自動検証**: SQL知識ベースを用いた最適なオブジェクト選択と接続検証
- **修正提案**: 生成したパッチの問題点を検出し改善案を提示

## MCPプロトコルについて

**Model Context Protocol (MCP)** はAnthropicが開発したオープンスタンダードで、AIモデルと外部データソースやツールを安全に接続するためのプロトコルです。USB-Cが様々な周辺機器を標準化された方法で接続するように、MCPはAIと外部システムの接続を標準化します。

本プロジェクトでは、MCPを通じてClaude DesktopとSQL知識ベースを連携させ、Max/MSPプログラミングに関する専門知識をLLMに提供しています。

## システムの仕組み

1. **ユーザー** → Claude Desktopで質問や指示を入力
2. **Claude** → 自動的にMCPを通じてSQL知識ベースに問い合わせ
3. **知識ベース** → Max/MSPとMin-DevKitの情報を提供
4. **Claude** → 情報を活用してパッチ生成
5. **結果** → ユーザーにMaxパッチを提案

## ドキュメント

- [シンプル版要件定義](docs/requirements_simplified.md) ← **まずはこれを読むことをお勧めします**
- [SQL知識ベース実装詳細](docs/sql_knowledge_base_implementation.md)
- [詳細版要件定義書](docs/specifications/requirements_min_devkit.md)

## 開発状況

現在進行中の課題:
- SQL知識ベースによる検証精度の向上
- より複雑なパッチパターンへの対応
- ユーザーフィードバックの改善

## 貢献方法

1. このリポジトリをフォーク
2. 機能追加やバグ修正のブランチを作成
3. 変更をコミット
4. プルリクエストを送信

## ライセンス

このプロジェクトは[MIT License](LICENSE)のもとで公開されています。

使用している[Min-DevKit](https://github.com/Cycling74/min-devkit)もMITライセンスで提供されています。
