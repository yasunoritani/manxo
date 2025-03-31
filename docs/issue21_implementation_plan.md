# OSC Bridge Issue 21: M4L環境でのテスト実装計画

## 1. 現状整理

### 現在のブランチ状態
- PR #63（Issue 19 & 20の修正）がマージ完了
- 新しいブランチ `feature/issue-21-m4l-osc-bridge-tests` を作成済み
- 開発環境は最新のmainブランチに基づいている

### ファイル構造調査結果
- **テスト関連ファイル**: 
  - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/error_recovery_test.cpp`
  - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/extended_types_test.cpp`
  - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/new_test_snippet.cpp`

- **ヘッダーファイル**:
  - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/mcp.osc_claude_handler.hpp`
  - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/mcp.osc_client.hpp`
  - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/mcp.osc_server.hpp`
  - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/osc_message_handler.hpp`

- **バックアップファイル**: 
  - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/backup/`内に複数のバリエーション
  - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/backup_issue19/`内にバックアップ

- **ビルド成果物**:
  - `/Users/mymac/v8ui/externals/osc_bridge.mxo` (正式版)
  - `/Users/mymac/v8ui/externals/mcp.osc_bridge.mxo` (古いバージョン)

## 2. Issue 21の目的と計画

### 目的
Issue 21は「M4L環境でのOSC Bridgeテスト実装」を目的としています。Ableton Live環境内でのOSC Bridge動作検証のためのテストパッチとテストの実装が必要です。

### 具体的な実装内容
1. **M4L特有のテストケース設計**:
   - ライフサイクルイベント対応（Live起動/終了、セットの読み込み/保存）
   - 複数インスタンス間の通信検証
   - ポート競合時の動作確認
   - エラー回復メカニズムの検証

2. **テストパッチの作成**:
   - `/Users/mymac/v8ui/src/min-devkit/osc_bridge/m4l-patches/`にテストパッチを配置
   - 基本機能テスト用パッチ
   - 高負荷テスト用パッチ
   - エラー条件テスト用パッチ

3. **テストスクリプトの実装**:
   - 自動化テストスクリプトの作成
   - 手動テスト手順の文書化

## 3. 実装手順

### ステップ1: テストケースの詳細設計
- 既存のテストファイル（error_recovery_test.cpp, extended_types_test.cpp）を参考に
- M4L環境特有の要件を特定
- エッジケースとコーナーケースの明確化

### ステップ2: テストパッチの作成
- Max for Live環境用の基本テンプレート作成
- OSC Bridgeオブジェクトの適切な配置
- 検証用UIの実装

### ステップ3: テストコードの実装
- 新規テストファイルの作成
- Min-DevKitテストフレームワークの活用
- エラー条件のシミュレーション機能

### ステップ4: 検証と文書化
- テスト結果の収集と分析
- 問題点の特定と修正
- テスト手順と結果のドキュメント化

## 4. 技術的考慮事項

### M4L環境の特性
- Max 8.x系との互換性（8.0.2, 8.1.0, 8.2.0）
- Ableton Liveのライフサイクルイベント
- リソース制約とパフォーマンス要件

### テスト自動化の課題
- M4L環境でのテスト自動化の制限
- 手動テストとの併用戦略

### 環境設定
- テスト環境の標準化
- 再現性の確保

## 5. 注意点と制約
- `/tools`ディレクトリは絶対にGitHubにアップロードしない
- PRはTools経由（`create_pr.py`）で作成する
- 機密情報は`.env`で管理し、`.gitignore`で保護する

## 6. 次のアクション
1. 既存のテストファイルの詳細分析
2. M4L環境テスト要件の確定
3. テストパッチのプロトタイプ作成
4. テストコードの実装開始
