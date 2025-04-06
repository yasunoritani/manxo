# v8uiプロジェクト ドキュメント整理実行プラン

## 概要

v8uiプロジェクトのドキュメントが増加し、一部のファイルは古くなったり重複したりしているため、整理が必要になっています。このドキュメントは、不要なファイルの削除と必要なファイルの整理のための実行プランを提供します。

## 削除対象ドキュメント

以下のドキュメントは不要または最新版に統合されているため、削除します：

### 古いIssue関連（新Issue体系に統合済み）
```bash
rm docs/issue13_osc_migration_plan.md
rm docs/issue21_implementation_plan.md
rm docs/issue21_summary.md
rm docs/issue21_to_issue22_handover.md
rm docs/issue22_to_issue23_handover.md
rm docs/issue27_to_issue28_handover.md
rm docs/issue4_implementation.md
rm docs/issue_25_handover.md
```

### 重複または古い分析ドキュメント
```bash
rm docs/issue_analysis.md
rm docs/design_change_analysis.md
rm docs/implementation_analysis.md
rm docs/implementation_plan_revised.md
rm docs/project_restructuring_plan.md
rm docs/issue_unification_plan.md
```

### テスト結果（アーカイブへ移動）
```bash
mkdir -p docs/archives/test_results
mv docs/test_results/* docs/archives/test_results/
rmdir docs/test_results
```

## 保持するドキュメントのリファクタリング

以下のドキュメントは重要なため保持しますが、最新の情報に更新し、統一された形式に整えます：

### コア設計・実装ドキュメント
```bash
mkdir -p docs/design
mv docs/architecture_min_devkit_v8ui_node.md docs/design/
mv docs/project_current_state.md docs/design/
```

### 技術仕様書
```bash
mkdir -p docs/specifications
mv docs/osc_bridge_technical_specification.md docs/specifications/
mv docs/min_devkit_knowledge.md docs/specifications/
mv docs/min_devkit_license_info.md docs/specifications/
```

## 新規ドキュメントの配置

新たに作成したドキュメントを適切なディレクトリに配置します：

```bash
# 概要ドキュメント
mkdir -p docs/overview
mv docs/v8ui_overview.md docs/overview/

# Issue詳細
mkdir -p docs/issues
mv docs/issue_005_cross_platform.md docs/issues/
mv docs/issue_007_security.md docs/issues/
mv docs/issue_009_performance.md docs/issues/

# 開発ガイド
mkdir -p docs/development
mv docs/development_roadmap.md docs/development/
mv docs/directory_structure.md docs/development/
mv docs/build_instructions.md docs/development/

# 不明点
mkdir -p docs/planning
mv docs/unresolved_questions.md docs/planning/
```

## ディレクトリ構造の標準化

以下の新しいディレクトリ構造に整理します：

```
docs/
├── overview/          # プロジェクト概要と全体像
├── issues/            # Issue詳細定義
├── design/            # 設計ドキュメント
├── specifications/    # 技術仕様
├── development/       # 開発ガイドとルール
├── planning/          # 計画と課題管理
├── api/               # API仕様（今後追加）
└── archives/          # アーカイブされた古いドキュメント
    └── test_results/  # 古いテスト結果
```

## README.mdの更新

プロジェクトルートのREADME.mdを更新して、新しいドキュメント構造を反映させます：

```bash
# READMEの更新（手動で編集する必要があります）
code README.md
```

README.mdに以下の内容を追加します：

```markdown
## ドキュメント

プロジェクトの詳細なドキュメントは以下のディレクトリにあります：

- [プロジェクト概要](docs/overview/v8ui_overview.md)
- [開発ロードマップ](docs/development/development_roadmap.md)
- [ビルド手順](docs/development/build_instructions.md)
- [Issue詳細](docs/issues/)
```

## 後処理

最後に、不要なディレクトリを削除し、Git履歴から削除したファイルを除外します：

```bash
# 空のディレクトリを削除
find docs -type d -empty -delete

# Gitからの削除を確定
git add docs
git commit -m "整理: ドキュメント構造の整理と古いファイルの削除"
```

## 実行計画のタイムライン

1. **バックアップ**：実行前に全ドキュメントのバックアップを作成
2. **ディレクトリ作成**：新しいディレクトリ構造の作成
3. **ファイル移動**：保持するドキュメントの適切な場所への移動
4. **不要ファイル削除**：古いドキュメントの削除
5. **README更新**：メインREADMEの更新
6. **Git操作**：変更のコミット

## リスク対策

- **重要なドキュメントの誤削除**：実行前にバックアップを取り、削除前に内容を確認
- **リンク切れ**：ドキュメント間の相互参照を更新
- **履歴の喪失**：GitのBLOBは保持されるため、必要に応じて復元可能
