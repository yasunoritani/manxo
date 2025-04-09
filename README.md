# ✨ Manxo - AI駆動型Maxパッチジェネレーター ✨

![Max Meets AI](https://img.shields.io/badge/Max%20Meets%20AI-%F0%9F%8E%B5%20%F0%9F%A4%96-blueviolet)
![Built with Love](https://img.shields.io/badge/Built%20with-%E2%9D%A4%EF%B8%8F-red)
![Kawaii Factor](https://img.shields.io/badge/Kawaii%20Factor-%E2%98%85%E2%98%85%E2%98%85%E2%98%85%E2%98%85-pink)

## 💫 プロジェクト概要

Manxoは、ClaudeのAI能力とMax/MSPの創造性を組み合わせた魔法のようなプロジェクト！🪄✨

自然言語からMaxパッチを自動生成するAI駆動型システムで、音楽制作やメディアアートの世界に革命を起こします。

### 🌟 主な機能

- 💬 **自然言語からパッチ生成**: 「ボイスエフェクト付きシンセサイザー作って」だけでパッチ完成！
- 🔍 **高度なセマンティック検索**: 複数のデータベースを活用した意味理解型検索
- 🧩 **パターン認識**: よく使われる接続パターンを学習して最適な構成を提案
- 🛠️ **テンプレート**: オーディオ・ビデオ処理用の特製テンプレートを用意
- 🔄 **自己修正機能**: 生成したパッチを自動検証&修正
- 📊 **分散型データベース**: ベクトルDB、グラフDB、ドキュメントDBによる高性能なナレッジベース

## 🧠 分散型データベース構造

Manxoは最近、単一のSQLiteデータベースから分散型データベース構造に移行し、MCPを通じて統合されました：

### 🔤 ベクトルDB (Pinecone)
- セマンティック検索を実現する意味理解基盤
- 自然言語クエリからMax/MSPオブジェクトを探索
- 類似度に基づく高精度な検索結果

### 🕸️ グラフDB (Neo4j)
- オブジェクト間の複雑な関係性を表現
- カテゴリや接続パターンの関係を効率的に探索
- 関連オブジェクトの発見や依存関係の視覚化

### 📑 ドキュメントDB (MongoDB)
- 柔軟なスキーマでMax/MSPオブジェクトのメタデータを管理
- 豊富な例とコードサンプルのライブラリ
- カテゴリごとのパターン集と使用例

### 🔌 MCP連携モジュール
- Claude DesktopとManxoをシームレスに接続
- 複数データベースを横断した統合検索機能
- グラフデータベースの関係性探索機能

## 🔍 システム要件

- Node.js 18.x以上
- npm 8.x以上
- Max/MSP 8.5以上
- **注意**: このプロジェクトはmacOSでの動作を前提に開発されており、WindowsおよびLinuxでの動作検証は行っていません。これらの環境での動作に関しては保証できません。

## 🚀 はじめ方

### 📋 必要なもの

- Node.js 14.x以上
- Max/MSP 8.x以上（推奨）
- Claude Desktop（3.5 Sonnet以上推奨）

### 💻 インストール方法

1. リポジトリをクローン:
   ```bash
   git clone https://github.com/yasunoritani/manxo.git
   cd manxo
   ```

2. 依存パッケージをインストール:
   ```bash
   npm install
   ```

3. Claude Desktop用の設定ファイルを自動更新:
   ```bash
   node src/setup-claude-config.js
   ```
   このスクリプトは自動的にClaude Desktopの設定ファイルを更新し、Manxo MCPサーバーを登録します。

4. または、Claude Desktop設定ファイルを手動で編集:
   ```json
   {
     "mcpServers": {
       "manxo": {
         "command": "node",
         "args": [
           "/絶対パスを入力/manxo/src/index.js"
         ],
         "cwd": "/絶対パスを入力/manxo"
       }
     }
   }
   ```

   > **注意**: `/絶対パスを入力/` の部分は、あなたの環境での実際のパスに置き換えてください。
   > - macOSの例： `/Users/あなたのユーザー名/manxo/src/index.js`
   > - Windowsの例： `C:\\Users\\あなたのユーザー名\\manxo\\src\\index.js`（Windowsではバックスラッシュをエスケープするために二重に記述）

### 🎮 使い方

Claude Desktop起動時に自動的にManxo MCPサーバーが起動します。Claude Desktopでこれらのツールを使用するには：

1. Claude Desktopを起動します
2. 新しい会話を開始すると、画面右上にハンマーアイコン（🛠️）が表示されます
   - 初回使用時は「このサーバーへのアクセスを許可しますか？」というプロンプトが表示されるので「許可」をクリック
3. Max/MSPに関連する質問をする際、Claudeは自動的に適切なツールを選択して使用します
   - 例：「Max/MSPでリアルタイムオーディオエフェクトを作るには？」と質問すると、Claudeは複数のDBを横断検索

**具体的な使用例**:
- 「シンプルなサイン波を生成するMaxパッチを作って」→ Claudeが自動的にMaxパッチを生成
- 「オーディオ処理の基本概念を教えて」→ 分散型DBから最適な情報を取得して回答
- 「delay~とreverb~の接続について教えて」→ グラフDBとベクトルDBを活用して推奨接続を表示

**ツールを手動で呼び出す**:
1. 会話中に画面右上のハンマーアイコン（🛠️）をクリック
2. 表示されるツール一覧から使用したいツールを選択
3. 必要なパラメータを入力して「実行」をクリック

> **注意**: Claudeは会話の文脈から適切なツールを選択できるため、通常は手動でツールを呼び出す必要はありません。自然に質問するだけで機能します。

## 🛠️ MCPツール一覧

Manxo MCPサーバーは以下のツールを提供します:

### 🔍 ナレッジベースツール

- `getMaxObject` → Maxオブジェクトの詳細情報を取得 📚
- `searchMaxObjects` → オブジェクトを名前や説明で検索 🔎
- `checkConnectionPattern` → オブジェクト間の接続を検証 ✅
- `searchConnectionPatterns` → よく使われる接続パターンを検索 🔄

### 🔮 分散型DB検索ツール（新機能）

- `semanticSearch` → 意味的な質問からベクトル検索を実行 💡
- `findRelatedObjects` → グラフ構造から関連オブジェクトを探索 🕸️
- `integratedSearch` → 複数データベースを横断した統合検索 🔄

### 🎨 パッチジェネレーターツール

- `generateMaxPatch` → テンプレートからパッチを生成 ✨
- `listTemplates` → 利用可能なテンプレート一覧を取得 📋
- `createAdvancedPatch` → カスタムオブジェクトで高度なパッチを生成 🔧

## 📁 プロジェクト構造

```
manxo/
├── src/                          # ソースコード
│   ├── index.js                  # メインMCPサーバーエントリーポイント
│   ├── setup-claude-config.js    # Claude設定自動更新スクリプト
│   ├── maxpat_generator/         # Maxパッチ生成エンジン
│   │   ├── templates/            # パッチテンプレート
│   │   ├── schemas/              # JSONスキーマ定義
│   │   ├── lib/                  # ユーティリティライブラリ
│   │   └── mcp-tool.js           # パッチ生成MCPツール
│   ├── mcp_connector/            # 分散型データベース連携
│   │   ├── index.js              # MCPツール登録
│   │   ├── vector-client.js      # ベクトルDB接続クライアント
│   │   ├── graph-client.js       # グラフDB接続クライアント
│   │   └── document-client.js    # ドキュメントDB接続クライアント
│   ├── max-bridge/               # Max/MSP連携
│   ├── mcp-server/               # MCPサーバーコア
│   ├── protocol/                 # プロトコル定義
│   ├── sql_knowledge_base/       # SQLiteレガシーシステム
│   ├── document_knowledge_base/  # ドキュメントDB接続モジュール
│   ├── graph_knowledge_base/     # グラフDB接続モジュール
│   └── vector_knowledge_base/    # ベクトルDB接続モジュール
├── data/                         # データファイル
│   ├── vector_db/                # ベクトルDBデータ
│   ├── graph_db/                 # グラフDBデータ
│   ├── document_db/              # ドキュメントDBデータ
│   ├── extracted/                # SQLiteから抽出データ
│   ├── transformed/              # 変換済みデータ
│   └── backups/                  # バックアップデータ
├── config/                       # 設定ファイル
│   ├── database.js               # データベース設定
│   ├── migration.js              # データ移行設定
│   └── mcp.js                    # MCP設定
├── scripts/                      # ユーティリティスクリプト
│   ├── extract_sql_data.js       # SQLデータ抽出
│   ├── convert_vector_data.js    # ベクトル変換
│   ├── import_vector_db.js       # ベクトルDBインポート
│   ├── import_graph_db.js        # グラフDBインポート
│   ├── import_document_db.js     # ドキュメントDBインポート
│   └── verify_migration.js       # 移行検証
├── tests/                        # テストファイル
│   ├── unit/                     # ユニットテスト
│   └── llm_connector/            # LLM連携テスト
├── externals/                    # 外部依存ファイル
│   ├── llm.mcp.mxo               # LLM連携Max外部オブジェクト
│   └── osc_bridge.mxo            # OSC連携Max外部オブジェクト
├── assets/                       # アセットファイル
├── node_modules/                 # Node.js依存パッケージ
├── .env                          # 環境変数
├── .gitignore                    # Git除外設定
├── package.json                  # プロジェクト設定
├── package-lock.json             # 依存関係ロック
└── README.md                     # このファイル
```

## 🔧 トラブルシューティング

- **MCPサーバーに接続できない場合**:
  - Claude Desktopのログを確認してください:
    - macOS: `~/Library/Logs/Claude/mcp*.log`
    - Windows: `%APPDATA%\Claude\logs\mcp*.log`
  - `claude_desktop_config.json`の構文エラーがないか確認
  - 絶対パスが正しく指定されているか確認（相対パスは使用できません）
  - Node.jsが正しくインストールされているか確認（`node -v` コマンドで確認）

- **Claude Desktopにツールが表示されない場合**:
  - `node src/setup-claude-config.js`を実行して設定を更新
  - Claude Desktopを完全に再起動してください
  - MCPサーバーの起動ステータスを確認

- **パッチ生成に失敗する場合**:
  - Max/MSPのバージョンが互換性があるか確認
  - テンプレートファイルが存在するか確認
  - データベースが正しく移行されているか確認

- **分散型DB検索に問題がある場合**:
  - `npm run migrate:verify`を実行してデータの整合性を確認
  - MCPツールがClaude Desktopから認識されているか確認
  - 詳細ログを確認

## 🔄 アップデート方法

最新版へのアップデートは以下のコマンドで行います:

```bash
git pull
npm install
node src/setup-claude-config.js
```

## 💌 ライセンス

このプロジェクトはMITライセンスの下で公開されています。詳細はLICENSEファイルをご覧ください。

## 🙏 謝辞

このプロジェクトは、Max/MSPコミュニティの知恵と、AIの可能性を組み合わせることで実現しました。
すべての音楽家、アーティスト、開発者に感謝します！🎵✨

## 🛳️ よくある質問

### Q: Claude Desktopの設定ファイルはどこにありますか？
A: 設定ファイルは以下の場所にあります:
  - macOS: `~/Library/Application Support/Claude/claude_desktop_config.json`
  - Windows: `%APPDATA%\Claude\claude_desktop_config.json`

### Q: setup-claude-config.jsの実行に失敗する場合は？
A: 手動で設定ファイルを編集することもできます。上記の「インストール方法」セクションのJSONサンプルを参考にしてください。

### Q: 分散型データベースで何が改善されましたか？
A: 以下の点が大幅に向上しました:
  - 自然言語ベースのセマンティック検索精度
  - 複雑な関係性に基づく推奨結果
  - 検索パフォーマンス
  - 豊富なメタデータと例の提供

### Q: MCPサーバーの仕組みはどうなっていますか？
A: Model Context Protocol (MCP)を使用して、Claude DesktopとManxoサーバーが通信します。Claudeは質問内容に応じて適切なMCPツールを呼び出し、Manxoサーバーが処理して結果を返します。