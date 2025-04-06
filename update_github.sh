#!/bin/bash

# ステータスの確認
echo "=== GitHubステータスの確認 ==="
git status

# 変更をステージングに追加
echo -e "\n=== 変更をステージングに追加 ==="
git add .

# 変更内容を確認
echo -e "\n=== 変更内容の確認 ==="
git status

# コミットメッセージの作成
echo -e "\n=== コミットの作成 ==="
git commit -m "docs: ドキュメント構造の整理と古いファイルの削除

- 古いIssue関連ドキュメントを削除
- 重複または古い分析ドキュメントを削除
- テスト結果をアーカイブへ移動
- ドキュメントを以下のディレクトリに整理:
  - overview: プロジェクト概要
  - issues: Issue詳細
  - design: 設計ドキュメント
  - specifications: 技術仕様
  - development: 開発ガイドとルール
  - planning: 計画と課題管理
- READMEにドキュメント構造情報を追加"

# GitHubにプッシュ
echo -e "\n=== GitHubへのプッシュ ==="
git push origin main || git push origin master

# GitHub Issuesの更新
echo -e "\n=== GitHub Issuesの更新 ==="
echo "注: GitHub Issuesの更新はWebインターフェースで行う必要があります。"
echo "以下の更新を行ってください:"
echo "1. Issue #001 (Min-DevKit/Claude Desktop統合) のラベルを「最高優先度」に更新"
echo "2. Issue #002 (プロジェクト構造の再設計) のラベルを「高優先度」に更新"
echo "3. Issue #003 (WebSocketクライアントの簡素化) のラベルを「最高優先度」に更新"
echo "4. Issue #004 (MCPツールセットの実装) のラベルを「中優先度」に更新"
echo "5. 新規Issue #005 (クロスプラットフォーム実装基盤) を作成し、「高優先度」ラベルを付与"
echo "6. 新規Issue #007 (セキュリティ対策) を作成し、「中優先度」ラベルを付与"
echo "7. 新規Issue #009 (パフォーマンス最適化) を作成し、「低~中優先度」ラベルを付与"

echo -e "\n=== 処理完了 ==="
