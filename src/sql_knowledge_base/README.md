# Max/MSP SQL知識ベース

Max/MSPとMin-DevKitの情報を格納したSQL知識ベースと、それにアクセスするための各種インターフェースを提供します。

## 特徴

- **204種のMaxオブジェクト情報**：名前、説明、カテゴリ、入出力情報など
- **169のMin-DevKit API関数情報**：関数名、シグネチャ、使用例など
- **201の接続パターン**：オブジェクト間の接続妥当性情報
- **105の検証ルール**：コード検証のためのパターン
- **160のAPI意図マッピング**：自然言語からAPI関数への変換情報

## インターフェース

このパッケージでは以下の方法で知識ベースにアクセスできます：

1. **RESTful API**：HTTPリクエストを通じたクエリ
2. **OSC連携**：Max/MSPパッチからのOSCメッセージを通じたクエリ
3. **MCPサーバー**：Claude DesktopからのModel Context Protocolを通じたアクセス

## インストール

```bash
# 必要なパッケージをインストール
npm install

# データベースが存在しない場合、ダンプから復元
cd src/sql_knowledge_base
sqlite3 max_claude_kb.db < max_claude_kb_dump.sql
```

## 使用方法

### RESTful APIサーバーの起動

```bash
npm start
# または
node server.js
```

サーバーは `http://localhost:3000` で起動します。

### Max/MSP OSC連携サーバーの起動

Max/MSPからOSCを通じてアクセスする場合：

```bash
node max-db-server.js
```

OSCサーバーはポート7400で起動します。

### MCPサーバーの起動

Claude Desktopから利用する場合：

```bash
npm run mcp
# または
node mcp-server.js
```

## APIリファレンス

### RESTful API

| エンドポイント | メソッド | 説明 |
|--------------|---------|------|
| /api/health | GET | サーバー稼働状況確認 |
| /api/max-objects | GET | Maxオブジェクト一覧取得 |
| /api/max-objects/:name | GET | 特定のMaxオブジェクト情報取得 |
| /api/max-objects/categories | GET | カテゴリ一覧取得 |
| /api/min-devkit | GET | Min-DevKit API関数一覧取得 |
| /api/min-devkit/:functionName | GET | 特定の関数情報取得 |
| /api/connection-patterns | GET | 接続パターン検索 |
| /api/validation-rules | GET | 検証ルール検索 |
| /api/api-mapping | GET | API意図マッピング検索 |
| /api/search | POST | 全文検索 |

### OSCアドレス

| アドレス | 引数 | 説明 |
|---------|------|------|
| /claude/query/get_max_object | object_name | Maxオブジェクト情報取得 |
| /claude/query/get_min_api | function_name | Min-DevKit関数情報取得 |
| /claude/query/check_connection | source, source_outlet, destination, destination_inlet | 接続パターン検証 |
| /claude/query/validate_code | code | コード検証 |
| /claude/query/get_api_mapping | intent | 意図からAPI検索 |

### MCPツール

| ツール名 | 説明 |
|---------|------|
| getMaxObject | Maxオブジェクト情報取得 |
| searchMaxObjects | Maxオブジェクト検索 |
| getMinDevkitFunction | Min-DevKit関数情報取得 |
| checkConnectionPattern | 接続パターン検証 |
| findApiByIntent | 意図からAPI検索 |

## Claude Desktopとの連携

Claude Desktopで知識ベースを利用するには、MCP構成ファイルに以下を追加します：

```json
{
  "mcpServers": {
    "maxKnowledgeBase": {
      "command": "node",
      "args": ["PATH_TO_PROJECT/src/sql_knowledge_base/mcp-server.js"]
    }
  }
}
```

## 実装詳細

- データベース: SQLite3
- APIサーバー: Express.js
- OSC連携: node-osc
- MCP実装: @model-context-protocol/server-node

## ライセンス

MIT
