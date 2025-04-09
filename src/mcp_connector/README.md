# MCP連携モジュール

## 概要

このモジュールは分散型データベース（ベクトルDB、グラフDB、ドキュメントDB）とMCPサーバーを連携する機能を提供します。ClaudeのMCPを介して、複数のデータベース間でシームレスに情報を検索・取得することが可能になります。

## 実装されたMCPツール

1. **semanticSearch** - ベクトルDBを使用した意味的検索
2. **findRelatedObjects** - グラフDBを使用した関連オブジェクト検索
3. **integratedSearch** - 複数のデータベースを横断した統合検索

## ファイル構成

- **index.js** - メインモジュール、MCPツール登録処理
- **vector-client.js** - ベクトルDB（Pinecone）クライアント
- **graph-client.js** - グラフDB（Neo4j）クライアント
- **document-client.js** - ドキュメントDB（MongoDB）クライアント

## 使用方法

```javascript
const { registerDistributedDBTools } = require('./mcp_connector');

// MCPサーバーにツールを登録
registerDistributedDBTools(mcpServer);
```

## ツール詳細

### semanticSearch

自然言語クエリを使ってベクトル検索を実行します。
オブジェクト名や正確な用語を知らなくても、概念や機能の説明から関連するMax/MSPオブジェクトを見つけることができます。

**パラメータ**:
- `query` (string): 検索するための自然言語クエリ
- `limit` (number, optional): 返す結果の最大数（デフォルト: 5）

### findRelatedObjects

グラフデータベースを使用して、あるオブジェクトに関連する他のオブジェクトを見つけます。
接続パターンや依存関係など、オブジェクト間の関係性を探索します。

**パラメータ**:
- `objectName` (string): 関連オブジェクトを探す対象のオブジェクト名
- `relationshipType` (string, optional): 探す関係のタイプ（デフォルト: 'CONNECTS_TO'）
- `limit` (number, optional): 返す結果の最大数（デフォルト: 5）

### integratedSearch

すべての分散データベース（ベクトル、グラフ、ドキュメント）を横断して検索を実行し、結果を統合します。
これにより、単一のクエリで様々な種類の情報を取得することができます。

**パラメータ**:
- `query` (string): すべてのデータベースで実行する検索クエリ
- `limit` (number, optional): 各データベースから返す結果の最大数（デフォルト: 3）
