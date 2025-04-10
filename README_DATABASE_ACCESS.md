# Manxoプロジェクト データベースアクセスガイド

このドキュメントでは、Manxoプロジェクトにおける分散型データベースとMaxパッチパターンへのアクセス方法について説明します。

## 概要

Manxoプロジェクトでは、以下の2種類のデータにアクセスするための統合ツールを提供しています：

1. **分散型データベース** - ベクトルDB、グラフDB、ドキュメントDBを統合的に操作
2. **Maxパッチパターン** - 収集・分析したMax/MSPパッチのパターンデータにアクセス

これらのデータは、MCPサーバーを通じてLLM（Claude）に提供されます。

## 1. 分散型データベースアクセス

### 利用可能なアクション

`databaseAccess`ツールは、以下のアクションをサポートしています：

- **semanticSearch**: ベクトルDBを使用して自然言語クエリに基づくセマンティック検索
- **findRelatedObjects**: グラフDBを使用して関連オブジェクトを検索
- **documentSearch**: ドキュメントDBを使用してテキスト検索
- **integratedSearch**: 上記すべてのDBを並列に検索し、結果を統合

### 使用例

```javascript
// semanticSearch の例
const result = await databaseAccess({
  action: 'semanticSearch',
  query: 'サイン波を生成する方法',
  limit: 5
});

// findRelatedObjects の例
const result = await databaseAccess({
  action: 'findRelatedObjects',
  objectName: 'cycle~',
  relationshipType: 'CONNECTS_TO',
  limit: 10
});

// integratedSearch の例
const result = await databaseAccess({
  action: 'integratedSearch',
  query: 'フィルターエフェクト',
  limit: 3
});
```

## 2. Maxパッチパターンアクセス

### 利用可能なアクション

`maxPatternAccess`ツールは、以下のアクションをサポートしています：

- **search**: キーワードに基づいてパターンを検索
- **recommend**: 特定の目的（合成、エフェクト等）に合わせたパターンを推奨
- **findRelated**: 特定のMax/MSPオブジェクトに関連するパターンを検索

### 使用例

```javascript
// search の例
const result = await maxPatternAccess({
  action: 'search',
  query: 'ディレイエフェクト',
  category: 'audio',
  limit: 10
});

// recommend の例
const result = await maxPatternAccess({
  action: 'recommend',
  purpose: 'synthesis',
  complexity: 3,
  limit: 5
});

// findRelated の例
const result = await maxPatternAccess({
  action: 'findRelated',
  query: 'delay~',
  limit: 10
});
```

## 返り値の構造

両方のツールから返される結果は一貫した形式を持ちます：

```javascript
{
  action: 'アクション名',
  // 入力パラメータ
  query: '入力されたクエリ',
  // その他のパラメータ...
  
  // 結果オブジェクト（アクションにより異なる構造）
  results: {
    // アクション固有の結果データ
  }
}
```

## エラー処理

エラーが発生した場合、以下の形式でエラー情報が返されます：

```javascript
{
  error: 'エラーメッセージ',
  action: 'エラーが発生したアクション'
}
```

## MCPサーバーでの設定

これらのツールはMCPサーバー起動時に自動的に登録されます。`src/mcp_connector/index.js`の`registerDistributedDBTools`関数がこの処理を担当します。

## 内部設計の注意点

1. 両方のツールは一貫したインターフェースとエラー処理を持つように設計されています。
2. `databaseAccess`は分散DB操作をまとめ、`maxPatternAccess`はMaxパターン操作をまとめています。
3. すべての操作は適切なエラーハンドリングと結果検証を行います。

## 開発向け情報

ツールの機能を拡張する場合：

1. 対応する関数（`databaseAccess`または`maxPatternAccess`）に新しいケースを追加
2. 適切なスキーマ定義をツール登録部分に追加
3. エラー処理とnullチェックを適切に実装 