# SQL知識ベース実装の引き継ぎドキュメント

## プロジェクト概要

Max/MSP及びMin-DevKit APIの情報を格納するSQL知識ベースを実装し、AI支援によるコード生成・検証プロセスを強化するプロジェクトです。Issue #010を5つのサブタスクに分割して実施しています。

## 進捗状況

### 完了済み（プルリクエスト #155 作成済み）

- **Issue #010a: データベース設計とスキーマ実装**
  - SQLiteデータベースのスキーマ設計と実装
  - 各テーブル定義とリレーションシップの確立
  - インデックス戦略の実装
  - データインポートスクリプトの作成
  - データベースフロントエンドの基本実装

### 実装中

- **Issue #010b: データ収集と初期データセット作成**
  - 小規模サンプルデータセットはすでに作成済み（各テーブル20-21件ずつ）
  - 本格的なデータセット作成は未着手
  - Max/MSPオブジェクト情報、Min-DevKit API情報を収集するプロセスの設計が必要

### 未着手

- **Issue #010c: クエリエンジンとAPIの実装**
- **Issue #010d: 検証エンジンの開発**
- **Issue #010e: Max-Claudeデスクトップとの統合**

## ディレクトリ構成

```
/src/sql_knowledge_base/           # SQLite知識ベースの実装ディレクトリ
  ├── create_database.sql          # データベーススキーマ定義
  ├── import_data.py               # CSVデータインポートスクリプト
  ├── max_knowledge.sqlite         # SQLiteデータベースファイル
  ├── server.js                    # Node.jsベースのAPIサーバー
  ├── package.json                 # 依存関係の定義
  └── public/                      # フロントエンドファイル
      ├── index.html               # 知識ベース検索UI
      ├── css/style.css            # スタイルシート
      └── js/main.js               # フロントエンドロジック

/docs/                             # プロジェクトドキュメント
  ├── MaxObjects.csv               # Max/MSPオブジェクト情報（大規模データ：561行）
  ├── Min-DevKit_API.csv           # Min-DevKit API情報（大規模データ：170行）
  ├── ConnectionPatterns.csv       # 接続パターン情報（大規模データ：55行）
  ├── ValidationRules.csv          # 検証ルール情報（大規模データ：106行）
  ├── API_Intent_Mapping.csv       # API意図マッピング情報（大規模データ：161行）
  ├── max_objects.csv              # サンプルデータ（小規模：20行）
  ├── min_devkit_api.csv           # サンプルデータ（小規模：21行）
  ├── connection_patterns.csv      # サンプルデータ（小規模：20行）
  ├── validation_rules.csv         # サンプルデータ（小規模：20行）
  ├── api_mapping.csv              # サンプルデータ（小規模：21行）
  └── issues/                      # 各Issue定義ファイル
      ├── issue_010a_database_design.md
      ├── issue_010b_data_collection.md
      ├── issue_010c_query_engine.md
      ├── issue_010d_validation_engine.md
      └── issue_010e_claude_integration.md
```

## データベーススキーマ

SQLite3データベースには以下のテーブルが定義されています：

1. **max_objects** - Max/MSPオブジェクト情報
   - id, name, description, category, min_max_version, max_max_version, inlets, outlets, is_ui_object, is_deprecated, recommended_alternative

2. **min_devkit_api** - Min-DevKit API関数情報
   - id, function_name, signature, return_type, description, parameters, example_usage, min_version, max_version, notes

3. **connection_patterns** - オブジェクト接続パターン
   - id, source_object, source_outlet, destination_object, destination_inlet, description, is_recommended, audio_signal_flow, performance_impact, compatibility_issues

4. **validation_rules** - コード検証ルール
   - id, rule_type, pattern, description, severity, suggestion, example_fix

5. **api_mapping** - 自然言語意図とAPI関数のマッピング
   - id, natural_language_intent, min_devkit_function_id, transformation_template, context_requirements

各テーブルには適切なインデックスが設定されています。詳細は`create_database.sql`ファイルを参照してください。

## データインポートプロセス

データインポートはPythonスクリプト`import_data.py`で実装されています。以下の機能があります：

1. CSVファイルからデータを読み込み
2. 重複エントリの処理（スキップまたは更新）
3. データベースへの一括インポート
4. インポートレポートの生成

現在、サンプルデータと大規模データの両方がインポート可能です。
サンプルデータは小さなCSVファイル（20-21行）ですでにインポートされています。
大規模データ（`MaxObjects.csv`など）のインポート準備が完了しています。

## 実行方法

1. データベースの初期化とサンプルデータのインポート:
   ```
   cd /src/sql_knowledge_base
   python import_data.py
   ```

2. 大規模データセットのインポート:
   ```
   # import_data.pyのCSVパスを修正する必要があります
   # 例：max_objects_csv = "../../docs/MaxObjects.csv"
   python import_data.py
   ```

3. APIサーバーの起動:
   ```
   cd /src/sql_knowledge_base
   npm install
   node server.js
   ```

4. フロントエンドへのアクセス:
   ```
   http://localhost:3000/
   ```

## 次の作業ステップ

### 優先度の高い課題

1. **Issue #010b: データ収集と初期データセット作成**
   - 大規模CSVファイルのデータ検証と整形
   - 不足しているデータフィールドの補完
   - インポートスクリプトのエラー処理強化

2. **Issue #010c: クエリエンジンとAPIの実装**
   - RESTful APIエンドポイントの完全実装
   - 複雑なクエリのサポート
   - パフォーマンス最適化（キャッシング、インデックス活用）

### 技術的な課題

1. **大規模データセットの処理**
   - 一部のCSVファイルにはヘッダーが不足しているため、適切なカラム名を追加する必要があります
   - `Min-DevKit_API.csv`などのファイルでは、関数シグネチャとパラメータ記述にカンマが含まれており、CSVパース時の特別な処理が必要です

2. **データベースパフォーマンス**
   - 大規模データセットに対するクエリパフォーマンスの最適化
   - SQLiteの全文検索（FTS）機能の活用を検討する必要があります

## GitHubリソース

- **Issues**:
  - #150 - Issue #010a: SQL知識ベースのデータベース設計とスキーマ実装
  - #151 - Issue #010b: SQL知識ベースのデータ収集と初期データセット作成
  - #152 - Issue #010c: SQL知識ベースのクエリエンジンとAPIの実装
  - #153 - Issue #010d: SQL知識ベースの検証エンジンの開発
  - #154 - Issue #010e: SQL知識ベースのMax-Claudeデスクトップとの統合

- **プルリクエスト**:
  - #155 - SQL知識ベースのデータベース設計とスキーマ実装 (Issue #010a)

## 依存関係

- Node.js (v14以上)
- SQLite3
- Express.js
- Python 3.x
- better-sqlite3（Node.jsのSQLiteクライアント）

## 将来の拡張性

1. **データモデルの拡張**
   - 将来的に新しいフィールドやテーブルが必要になる可能性があるため、マイグレーション戦略を検討すべきです

2. **APIの拡張**
   - GraphQLインターフェースの追加などで、より柔軟なクエリが可能になります

3. **Max/MSPとの統合**
   - Node for Max経由でMax/MSPパッチから直接SQLite知識ベースにアクセスするための連携機能
