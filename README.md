# Max-Claude パッチジェネレーター

## プロジェクト概要

本プロジェクトは、自然言語の指示から直接Max/MSPパッチファイル(`.maxpat`)やMax for Liveデバイス(`.amxd`)を生成するAIシステムです。音楽制作者やサウンドデザイナーは、プログラミングの詳細な知識がなくても、自分のアイデアを言葉で表現するだけですぐに使えるMaxパッチを入手できます。

## 主な機能

- **自然言語→Maxパッチ変換**: 日常言語での説明から完全なMaxパッチを生成
- **Max for Live対応**: Ableton Live用のデバイス(`.amxd`)ファイル生成
- **AIによるパッチ設計**: 音楽理論やDSP知識に基づいた最適なパッチ構造の提案
- **SQL知識ベースによる支援**: 専門的な接続パターンと最適なオブジェクト選択
- **即時利用可能**: 生成されたパッチファイルはすぐにMaxまたはAbleton Liveで開いて使用可能

## システム構成

システムは以下の主要コンポーネントから構成されています：

1. **Claude Desktop**:
   - 自然言語処理を担当
   - ユーザーの音楽制作・サウンドデザインの指示を理解
   - SQL知識ベースと連携して最適な実装を決定

2. **MCP (Model Context Protocol)**:
   - Claude Desktopとパッチ生成システム間の通信を担当
   - ツール機能を通じてパッチ生成をトリガー
   - 双方向フィードバックを実現

3. **SQL知識ベース**:
   - Max/MSPオブジェクト情報（561種類）
   - 接続パターン（201パターン）
   - 検証ルール（105ルール）
   - 自然言語意図マッピング（160マッピング）

4. **パッチ生成エンジン**:
   - JSONベースのmaxpatファイルを直接生成
   - オブジェクト間の接続を適切に構築
   - UIレイアウトを最適化

## セットアップ方法

### 前提条件

- Node.js 14以降
- Claude Desktop
- Max/MSP 8以降（生成されたパッチを開くため）
- SQLite3
- Python 3.x（データインポート用）

### インストール手順

1. **リポジトリのクローン**
   ```bash
   git clone https://github.com/yasunoritani/manxo.git
   cd manxo
   npm install
   ```

2. **SQL知識ベースの初期化**
   ```bash
   cd src/sql_knowledge_base
   python import_data.py
   ```

3. **MCPサーバーの設定**
   ```bash
   # Claude Desktopの設定ファイルディレクトリ作成（macOS）
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
           "name": "Max Patch Generator",
           "command": "node /path/to/manxo/src/mcp-server.js"
         }
       ]
     }
   }
   ```

4. **MCPサーバーの起動**
   ```bash
   node src/mcp-server.js
   ```

## 使い方

1. **Claude Desktopを起動（または再起動）**
   - 設定を反映させるためにClaude Desktopを再起動します

2. **MCPツールの確認**
   - Claude Desktopの入力欄上部でMCPツールが表示されていることを確認
   - 「Max Patch Generator」が利用可能なMCPツールとして表示されます

3. **Maxパッチの生成を指示**
   - 例：「4つのサイン波オシレーターからなるアディティブシンセサイザーのパッチを作って、ピッチとボリュームをコントロールできるようにして」
   - より詳細な指示も可能：「フィードバックディレイとリバーブを備えたグラニュラーサンプラーを作って、タイムストレッチパラメーターも付けて」

4. **生成されたパッチファイルを使用**
   - AIが指示に基づいて`.maxpat`または`.amxd`ファイルを生成
   - 生成されたファイルはシステムのダウンロードフォルダまたは指定フォルダに保存
   - Max/MSPまたはAbleton Liveで開いてすぐに使用可能

## 技術詳細

### システムフロー

1. **ユーザーの意図理解**:
   - Claude Desktopがユーザーの自然言語指示を解析
   - 必要なオブジェクトと機能の特定

2. **SQL知識ベース問い合わせ**:
   - 必要なMax/MSPオブジェクトの検索
   - 適切な接続パターンの特定
   - パラメータ設定の最適値取得

3. **パッチ構造設計**:
   - 信号フローの論理的構成
   - オブジェクト間の接続設計
   - UIエレメントの配置計画

4. **JSONファイル生成**:
   - Max/MSPのJSON形式に変換
   - オブジェクトとパラメータの設定
   - 接続情報の生成

5. **レイアウト最適化**:
   - UIの使いやすさを考慮した配置
   - 視覚的なグルーピング
   - スケーリングと位置調整

### SQL知識ベース構造

SQLiteデータベースには以下のテーブルが定義されています：

1. **max_objects**: Max/MSPオブジェクト情報
   - id, name, description, category, inlets, outlets, is_ui_object等

2. **connection_patterns**: オブジェクト接続パターン
   - source_object, source_outlet, destination_object, destination_inlet等

3. **validation_rules**: 検証ルール情報
   - rule_type, pattern, description, severity等

4. **api_mapping**: 自然言語意図とパターン関数のマッピング
   - natural_language_intent, pattern_id等

### Max/MSPパッチファイル構造

Maxパッチ(`.maxpat`)とMax for Liveデバイス(`.amxd`)は基本的にJSON形式のテキストファイルです。パッチファイルには以下の要素が含まれます：

- オブジェクト定義（種類、位置、パラメータなど）
- オブジェクト間の接続情報
- UIエレメント（スライダー、ボタンなど）の定義
- パッチの全体設定（サイズ、表示オプションなど）

## MCPプロトコルについて

**Model Context Protocol (MCP)** はAnthropicが開発したオープンスタンダードで、AIモデルと外部データソースやツールを安全に接続するためのプロトコルです。USB-Cが様々な周辺機器を標準化された方法で接続するように、MCPはAIと外部システムの接続を標準化します。

本プロジェクトでは、MCPを通じてClaude DesktopとSQL知識ベースを連携させ、Max/MSPプログラミングに関する専門知識をLLMに提供しています。

## 開発状況

現在進行中の課題:
- より複雑なパッチ構造のサポート
- UI要素の洗練化
- SQL知識ベースの拡充（特に接続パターンと検証ルール）
- 様々な音楽ジャンル向けテンプレートの拡充
- Max for Liveデバイスのカスタムスキン対応

## ドキュメント

詳細は以下のドキュメントを参照してください：
- [シンプル版要件定義](docs/requirements_simplified.md)
- [SQL知識ベース実装詳細](docs/sql_knowledge_base_implementation.md)
- [実装状況](docs/implementation_status.md)
- [パッチ構造リファレンス](docs/patch_structure.md)
- [サポートされているオブジェクト一覧](docs/supported_objects.md)

## ライセンス

本プロジェクトはMITライセンスで提供されています。

生成されたMaxパッチの著作権はユーザーに帰属します。商用利用を含め、生成されたパッチを自由にお使いいただけます。
