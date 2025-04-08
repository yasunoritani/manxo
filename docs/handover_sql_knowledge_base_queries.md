# SQL知識ベースクエリエンジンとAPI 引き継ぎドキュメント

## 1. 概要

Issue #010cの実装として、Max/MSPとMin-DevKit情報を格納したSQL知識ベースへアクセスするための複数のインターフェースを開発しました。このシステムはMax/MSPパッチ開発時の情報検索や検証を効率化するためのものです。

実装した主な機能:
- RESTful APIサーバー
- OSC経由のMax/MSP連携
- MCPサーバーによるClaude Desktop連携
- パフォーマンス最適化（キャッシュ、非同期処理など）

## 2. システム構成

### 2.1 ファイル構成

```
src/sql_knowledge_base/
├── max_claude_kb.db        # SQLiteデータベース（メインデータ）
├── server.js               # RESTful APIサーバー
├── max-db-server.js        # Max/MSP OSC連携ブリッジ
├── mcp-server.js           # Claude Desktop MCP連携サーバー
├── package.json            # 依存関係定義
└── README.md               # 詳細ドキュメント
```

### 2.2 データベース構造

- `max_objects`: Max/MSPオブジェクト情報（204レコード）
- `min_devkit_api`: Min-DevKit API関数情報（169レコード）
- `connection_patterns`: 接続パターン情報（201レコード）
- `validation_rules`: 検証ルール情報（105レコード）
- `api_mapping`: 自然言語意図からAPI関数へのマッピング（160レコード）

## 3. 環境構築と実行方法

### 3.1 必要条件

- **Node.js**: バージョン20推奨（Node 23ではsqlite3モジュールに互換性問題あり）
- **npm**: 依存パッケージ管理用

### 3.2 セットアップ

```bash
# 依存パッケージのインストール
cd src/sql_knowledge_base
npm install
```

### 3.3 各サーバーの起動方法

**RESTful APIサーバー**:
```bash
node server.js
# サーバーは http://localhost:3000 で起動
```

**Max/MSP OSCブリッジ**:
```bash
node max-db-server.js
# OSCサーバーはポート7400でリッスン
```

**MCPサーバー**:
```bash
node mcp-server.js
# 標準入出力経由でClaude Desktopと通信
```

## 4. APIリファレンス

### 4.1 RESTful API

| エンドポイント | メソッド | 説明 | 例 |
|-------------|--------|------|-----|
| `/api/health` | GET | サーバー状態確認 | `curl http://localhost:3000/api/health` |
| `/api/max-objects` | GET | Maxオブジェクト一覧取得 | `curl http://localhost:3000/api/max-objects?category=time&limit=10` |
| `/api/max-objects/:name` | GET | 特定のオブジェクト情報取得 | `curl http://localhost:3000/api/max-objects/metro` |
| `/api/min-devkit` | GET | Min-DevKit API一覧取得 | `curl http://localhost:3000/api/min-devkit?return_type=void` |
| `/api/connection-patterns` | GET | 接続パターン検索 | `curl http://localhost:3000/api/connection-patterns?source=metro` |
| `/api/validate-connection` | GET | 接続パターン検証 | `curl http://localhost:3000/api/validate-connection?source=metro&destination=bang` |
| `/api/search` | POST | 全文検索 | `curl -X POST -H "Content-Type: application/json" -d '{"query":"timing"}' http://localhost:3000/api/search` |

### 4.2 OSC通信

| アドレス | 引数 | 説明 |
|---------|------|------|
| `/claude/query/get_max_object` | object_name | Maxオブジェクト情報取得 |
| `/claude/query/check_connection` | source, source_outlet, destination, destination_inlet | 接続パターン検証 |
| `/claude/query/validate_code` | code | コード検証 |

### 4.3 MCP連携

Claude Desktopの設定ファイル（`claude_desktop_config.json`）に以下を追加:

```json
{
  "mcpServers": {
    "maxKnowledgeBase": {
      "command": "node",
      "args": ["/path/to/v8ui/src/sql_knowledge_base/mcp-server.js"]
    }
  }
}
```

使用可能なMCPツール:
- `getMaxObject`: Maxオブジェクト情報取得
- `searchMaxObjects`: オブジェクト検索
- `getMinDevkitFunction`: Min-DevKit関数情報取得
- `checkConnectionPattern`: 接続パターン検証
- `findApiByIntent`: 自然言語意図からAPI関数検索

## 5. 実装詳細と注意点

### 5.1 パフォーマンス最適化

- キャッシュメカニズム: リクエスト結果を1時間メモリ内にキャッシュ
- プリペアドステートメント: 頻繁に使用されるクエリはプリコンパイル
- 非同期処理: すべてのDB操作は非同期で実行

### 5.2 既知の問題

- Node.js 23以上ではsqlite3モジュールのビルドが失敗する→Node.js 20を使用すること
- 大量のデータに対する検索は時間がかかる場合がある→limit引数で制限すること
- MCPサーバーは現在stdio通信のみサポート→WebSocket対応は今後の課題

### 5.3 セキュリティ上の注意

- 現在のRESTful APIには認証機能がない→内部ネットワークでのみ使用すること
- クエリインジェクション対策としてパラメータ化クエリを使用
- 本番環境ではHTTPS、認証、レート制限の追加を検討すること

## 6. 今後のタスク

- **高度な検索機能**: 自然言語から複合クエリへの変換
- **ユーザーインターフェース**: ウェブUIの開発
- **リアルタイム更新**: WebSocketによる更新通知
- **分散キャッシュ**: Redis連携によるキャッシュの共有
- **データ更新API**: データベース更新用のセキュアなAPI
- **WebSocket対応**: MCPサーバーのWebSocket通信対応

## 7. トラブルシューティング

### 7.1 よくあるエラー

- **EADDRINUSE**: 指定ポートが既に使用中→別のポートを指定または既存プロセスを終了
- **SQLite3 build failure**: Node.jsバージョンの互換性問題→Node.js 20を使用
- **No such module 'sqlite3'**: NPMインストールの失敗→`npm ci`で再インストール

### 7.2 ログの確認

- サーバーログは標準出力に出力
- 詳細なデバッグログを有効化: `NODE_ENV=development node server.js`

## 8. 連絡先とリソース

- 実装者: [担当者名]
- 関連ドキュメント: `docs/issues/issue_010c_implementation_summary.md`
- GitHub PR: [Pull Request #XX](https://github.com/project/repo/pull/XX)

---

このドキュメントは、SQL知識ベースクエリエンジンとAPIの引き継ぎのためのガイドです。詳細な情報は各ソースコードのコメントやREADME.mdを参照してください。
