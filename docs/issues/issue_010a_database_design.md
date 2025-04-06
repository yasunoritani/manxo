# Issue #010a: SQL知識ベースのデータベース設計とスキーマ実装

**ステータス**: 提案
**種別**: 機能拡張
**優先度**: 高
**担当者**: 未割り当て
**リポート日**: 2025-03-29
**親Issue**: #010

## 概要

SQL知識ベースの基盤となるデータベース設計とスキーマを実装する。このIssueはより大きなIssue #010「SQL知識ベースによる命令変換・検証レイヤーの実装」の第一部分として作成されます。

## 目標

1. Max/MSPオブジェクトとMin-DevKit API情報を格納するための最適なデータベース構造を設計する
2. 接続パターン、検証ルール、APIマッピングのためのテーブル設計を行う
3. 効率的なクエリのためのインデックス戦略を実装する
4. 初期データベーススキーマをSQLite形式で実装する

## 成功基準

- スキーマがすべての必要なデータ型と関係を適切に表現していること
- テーブル間の関係が正規化され、データの整合性が確保されていること
- インデックスが適切に設定され、クエリパフォーマンスが最適化されていること
- スキーマがMax/MSPとMin-DevKitの今後の機能拡張に対応できる柔軟性を持つこと

## 詳細仕様

### データベース設計

**Maxオブジェクトテーブル:**
```sql
CREATE TABLE max_objects (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL UNIQUE,
  description TEXT,
  category TEXT,
  min_max_version TEXT,
  max_max_version TEXT,
  inlets INTEGER,
  outlets INTEGER,
  is_ui_object BOOLEAN,
  is_deprecated BOOLEAN,
  recommended_alternative TEXT
);
```

**MinDevKit APIテーブル:**
```sql
CREATE TABLE min_devkit_api (
  id INTEGER PRIMARY KEY,
  function_name TEXT NOT NULL UNIQUE,
  signature TEXT NOT NULL,
  return_type TEXT,
  description TEXT,
  parameters TEXT,
  example_usage TEXT,
  min_version TEXT,
  max_version TEXT,
  notes TEXT
);
```

**接続パターンテーブル:**
```sql
CREATE TABLE connection_patterns (
  id INTEGER PRIMARY KEY,
  source_object TEXT NOT NULL,
  source_outlet INTEGER NOT NULL,
  destination_object TEXT NOT NULL,
  destination_inlet INTEGER NOT NULL,
  description TEXT,
  is_recommended BOOLEAN,
  audio_signal_flow BOOLEAN,
  performance_impact TEXT,
  compatibility_issues TEXT,
  FOREIGN KEY (source_object) REFERENCES max_objects(name),
  FOREIGN KEY (destination_object) REFERENCES max_objects(name)
);
```

**検証ルールテーブル:**
```sql
CREATE TABLE validation_rules (
  id INTEGER PRIMARY KEY,
  rule_type TEXT NOT NULL,
  pattern TEXT NOT NULL,
  description TEXT,
  severity TEXT,
  suggestion TEXT,
  example_fix TEXT
);
```

**API意図マッピングテーブル:**
```sql
CREATE TABLE api_mapping (
  id INTEGER PRIMARY KEY,
  natural_language_intent TEXT NOT NULL,
  min_devkit_function_id INTEGER,
  transformation_template TEXT,
  context_requirements TEXT,
  FOREIGN KEY (min_devkit_function_id) REFERENCES min_devkit_api(id)
);
```

### インデックス戦略

```sql
-- 名前ベースの高速検索のためのインデックス
CREATE INDEX idx_max_objects_name ON max_objects(name);
CREATE INDEX idx_min_devkit_api_function_name ON min_devkit_api(function_name);

-- カテゴリ検索の最適化
CREATE INDEX idx_max_objects_category ON max_objects(category);

-- 接続パターン検索の最適化
CREATE INDEX idx_connection_patterns_source ON connection_patterns(source_object, source_outlet);
CREATE INDEX idx_connection_patterns_destination ON connection_patterns(destination_object, destination_inlet);

-- 検証ルールタイプ検索の最適化
CREATE INDEX idx_validation_rules_type ON validation_rules(rule_type);

-- 自然言語意図検索の最適化
CREATE INDEX idx_api_mapping_intent ON api_mapping(natural_language_intent);
```

## 実装計画

1. データベーススキーマの詳細設計とレビュー (3日)
2. SQLiteスキーマ作成スクリプトの実装 (2日)
3. インデックス戦略の検証とパフォーマンステスト (2日)
4. データベースマイグレーションスクリプトの作成 (1日)
5. サンプルデータによるスキーマ検証 (2日)

## 関連Issue

- 親Issue: #010 SQL知識ベースによる命令変換・検証レイヤーの実装
- 後続Issue: #010b データ収集と初期データセット作成

## 参考資料

1. [SQLite公式ドキュメント](https://www.sqlite.org/docs.html)
2. [データベース正規化ガイド](https://www.guru99.com/database-normalization.html)
3. [SQLiteインデックス最適化](https://www.sqlite.org/optoverview.html)
4. [Min-DevKit公式ドキュメント](https://cycling74.github.io/min-devkit/)
5. [Max/MSP API リファレンス](https://docs.cycling74.com/max8/)
