# v8uiプロジェクト ドキュメント整理計画

## 1. 保持すべき重要ドキュメント

### コア設計・実装ドキュメント
- `docs/summary.md` - 最終実装計画と推奨事項（最新の統合情報）
- `docs/cross_platform_analysis.md` - クロスプラットフォーム対応の詳細分析
- `docs/issues/issue_001_min_devkit_claude_integration.md` - Issue #001の詳細
- `docs/issues/issue_002_project_restructuring.md` - Issue #002の詳細
- `docs/issues/issue_003_websocket_client_simplification.md` - Issue #003の詳細
- `docs/issues/issue_004_mcp_tools_implementation.md` - Issue #004の詳細
- `docs/architecture_min_devkit_v8ui_node.md` - アーキテクチャ概要
- `docs/project_current_state.md` - 現在の実装状態

### 技術仕様書
- `docs/osc_bridge_technical_specification.md` - OSCブリッジの技術仕様
- `docs/min_devkit_knowledge.md` - Min-DevKitに関する知識ベース
- `docs/min_devkit_license_info.md` - ライセンス情報

## 2. 削除推奨ドキュメント

### 古いIssue関連（新Issue体系に統合済み）
- `docs/issue13_osc_migration_plan.md`
- `docs/issue21_implementation_plan.md`
- `docs/issue21_summary.md`
- `docs/issue21_to_issue22_handover.md`
- `docs/issue22_to_issue23_handover.md`
- `docs/issue27_to_issue28_handover.md`
- `docs/issue4_implementation.md`
- `docs/issue_25_handover.md`

### 重複または古い分析
- `docs/issue_analysis.md` - 新しい分析に置き換え済み
- `docs/design_change_analysis.md` - 最新の設計に統合済み
- `docs/implementation_analysis.md` - 最新の実装計画に統合済み
- `docs/implementation_plan_revised.md` - summary.mdに統合済み
- `docs/project_restructuring_plan.md` - issue_002に統合済み
- `docs/issue_unification_plan.md` - 実行済みプラン

### テスト結果（アーカイブへ移動推奨）
- `docs/test_results/` ディレクトリ内のすべてのファイル
  - 整理してアーカイブに移動するか、不要なら削除

## 3. 重要度が低いが保持可能なドキュメント
- `docs/osc_analysis.md` - OSC実装の詳細分析
- `docs/osc_bridge_test_criteria.md` - テスト基準
- `docs/osc_bridge_testing_guide.md` - テストガイド
- `docs/mcp_authentication_analysis.md` - 認証分析
- `docs/requirements_min_devkit.md` - Min-DevKit要件

## 4. 新規作成が必要なドキュメント

### 優先度高
- `docs/v8ui_overview.md` - プロジェクト概要と最新状態（簡潔版）
- `docs/issue_005_cross_platform.md` - Issue #005詳細
- `docs/issue_007_security.md` - Issue #007詳細
- `docs/issue_009_performance.md` - Issue #009詳細
- `docs/development_roadmap.md` - 16週間の実装ロードマップ詳細

### 優先度中
- `docs/directory_structure.md` - 整理されたディレクトリ構造の説明
- `docs/build_instructions.md` - ビルド手順（クロスプラットフォーム対応）
