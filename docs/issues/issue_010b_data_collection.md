# Issue #010b: SQL知識ベースのデータ収集と初期データセット作成

**ステータス**: 提案
**種別**: 機能拡張
**優先度**: 高
**担当者**: 未割り当て
**リポート日**: 2025-03-29
**親Issue**: #010

## 概要

SQL知識ベースに必要なデータを収集し、初期データセットを作成する。このIssueはより大きなIssue #010「SQL知識ベースによる命令変換・検証レイヤーの実装」の第二部分として作成されます。

## 目標

1. Max/MSPオブジェクトの包括的なリストとその詳細情報を収集する
2. Min-DevKit APIの関数、シグネチャ、戻り値タイプなどの情報を体系的に整理する
3. オブジェクト間の標準的な接続パターンとベストプラクティスをまとめる
4. 一般的なコード検証ルールとAPIマッピングのデータセットを構築する
5. 収集したデータをSQLiteデータベースに効率的にインポートする方法を実装する

## 成功基準

- 少なくとも500のMax/MSPオブジェクトとその詳細情報がデータベースに格納されていること
- Min-DevKit APIの主要な関数が100%カバーされていること
- 標準的な接続パターンが少なくとも200パターン記録されていること
- データの正確性が専門家によって検証されていること
- データのインポート処理が自動化され、更新が容易に行えること

## 詳細仕様

### データ収集方法

1. **Maxオブジェクト情報:**
   - Max/MSP公式リファレンスページからのデータスクレイピング
   - Max SDKのリファレンスXMLファイルからの情報抽出
   - コミュニティドキュメントからの追加情報収集

2. **MinDevKit API情報:**
   - Min-DevKit公式ヘッダーファイルからの関数シグネチャ抽出
   - API実装ファイルからのドキュメント抽出
   - サンプルコードからの使用例収集

3. **接続パターン:**
   - 標準的なMax/MSPパッチの分析
   - 公式チュートリアルやサンプルからのパターン抽出
   - オーディオとデータフローの標準パターンの体系化

4. **検証ルール:**
   - 一般的なC++コーディング標準からのルール抽出
   - Min-DevKit固有のベストプラクティス収集
   - パフォーマンスとメモリ管理に関するルールの定義

### データフォーマット

**CSVインポートフォーマット例 (max_objects.csv):**
```
name,description,category,min_max_version,max_max_version,inlets,outlets,is_ui_object,is_deprecated,recommended_alternative
number,数値を保存してメッセージを出力します,Math,5.0,8.3,2,1,FALSE,FALSE,
slider,スライダーUIコントロール,UI,5.0,8.3,1,1,TRUE,FALSE,
cycle~,サイン波を生成します,MSP,5.0,8.3,2,1,FALSE,FALSE,
[...]
```

**CSVインポートフォーマット例 (min_devkit_api.csv):**
```
function_name,signature,return_type,description,parameters,example_usage,min_version,max_version,notes
inlet_int,void inlet_int(int inlet, int value),void,整数値を特定のインレットに送信します,"inlet: インレット番号, value: 送信する整数値","inlet_int(0, 42);",1.0,current,
atom_getfloat,double atom_getfloat(const t_atom* a),double,アトムから浮動小数点値を取得します,"a: 浮動小数点値を含むアトムへのポインタ","double x = atom_getfloat(argv);",1.0,current,
[...]
```

### データインポートプロセス

1. CSVファイルからのデータ読み込み
2. データの検証と標準化（重複の排除、形式の統一）
3. データベースへの一括インポート
4. インポート結果の検証と報告

## 実装計画

1. データ収集スクリプトの開発 (5日)
   - Max/MSP公式ドキュメント用スクレーパー
   - Min-DevKit APIヘッダーパーサー
   - 接続パターン抽出ツール

2. データ形式の標準化とクリーニング (3日)
   - CSVフォーマット定義
   - データ正規化ルールの実装
   - 不完全データの処理方法

3. インポートスクリプトの開発 (4日)
   - 高効率なSQLite一括インポート処理
   - データ検証と整合性チェック
   - エラー処理とログ記録

4. データ検証と品質保証 (3日)
   - 専門家によるサンプルデータのレビュー
   - 自動テストケースの作成
   - データカバレッジの評価

## 関連Issue

- 親Issue: #010 SQL知識ベースによる命令変換・検証レイヤーの実装
- 先行Issue: #010a データベース設計とスキーマ実装
- 後続Issue: #010c クエリエンジンとAPIの実装

## 参考資料

1. [Max/MSP公式リファレンス](https://docs.cycling74.com/max8/)
2. [Min-DevKit GitHubリポジトリ](https://github.com/Cycling74/min-devkit)
3. [データスクレイピングベストプラクティス](https://www.scrapingbee.com/blog/web-scraping-best-practices/)
4. [CSVデータ処理ガイド](https://realpython.com/python-csv/)
5. [SQLite一括インポートガイド](https://www.sqlite.org/import.html)
