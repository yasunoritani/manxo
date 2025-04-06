#!/bin/bash
# GitHubリポジトリのIssuesを更新するスクリプト

set -e  # エラーが発生したら終了

# 設定
GITHUB_REPO="yasunoritani/v8ui"
ISSUES_DIR="docs/issues"
GITHUB_API="https://api.github.com"

# GitHubトークンが設定されているか確認
if [ -z "$GITHUB_TOKEN" ]; then
    echo "エラー: GITHUB_TOKENが設定されていません。"
    echo "GitHubのパーソナルアクセストークンを環境変数として設定してください："
    echo "export GITHUB_TOKEN=your_token_here"
    exit 1
fi

# リポジトリ内のIssuesディレクトリが存在するか確認
if [ ! -d "$ISSUES_DIR" ]; then
    echo "エラー: $ISSUES_DIR ディレクトリが見つかりません。"
    exit 1
fi

# Issueファイルを探す
issue_files=$(find "$ISSUES_DIR" -name "issue_*.md" -type f)

if [ -z "$issue_files" ]; then
    echo "更新するIssueファイルが見つかりません。"
    exit 0
fi

echo "GitHubのIssues更新を開始します..."

# 各Issueファイルを処理
for file in $issue_files; do
    echo "処理中: $file"

    # Issueナンバーを抽出
    issue_number=$(basename "$file" | sed -E 's/issue_([0-9]+)_.*/\1/')

    if [ -z "$issue_number" ]; then
        echo "  警告: $file からIssue番号を抽出できませんでした。スキップします。"
        continue
    fi

    echo "  Issue #$issue_number を更新します"

    # タイトルを抽出 (最初の# から行末まで)
    title=$(grep -m 1 "^# " "$file" | sed 's/^# //')

    if [ -z "$title" ]; then
        echo "  警告: $file からタイトルを抽出できませんでした。スキップします。"
        continue
    fi

    # 本文を抽出 (タイトル行以降のすべて)
    body=$(tail -n +2 "$file")

    # 優先度とカテゴリのラベルを抽出
    labels=()
    priority=$(grep -o "優先度: \(最高\|高\|中\|低\)" "$file" | sed 's/優先度: /優先度:/')
    if [ ! -z "$priority" ]; then
        labels+=("$priority")
    fi

    category=$(grep -o "カテゴリ: [^,]*" "$file" | sed 's/カテゴリ: //')
    if [ ! -z "$category" ]; then
        labels+=("$category")
    fi

    # ラベルをJSON形式に変換
    labels_json="[]"
    if [ ${#labels[@]} -gt 0 ]; then
        labels_json=$(printf '"%s",' "${labels[@]}" | sed 's/,$//')
        labels_json="[$labels_json]"
    fi

    # IssueのJSONを作成
    issue_json=$(cat <<EOF
{
  "title": "$title",
  "body": $(echo "$body" | jq -Rs .),
  "labels": $labels_json
}
EOF
)

    # GitHubのIssueを更新
    update_response=$(curl -s -X PATCH \
      -H "Authorization: token $GITHUB_TOKEN" \
      -H "Accept: application/vnd.github.v3+json" \
      -d "$issue_json" \
      "$GITHUB_API/repos/$GITHUB_REPO/issues/$issue_number")

    # レスポンスからエラーを確認
    error=$(echo "$update_response" | jq -r '.message // empty')
    if [ ! -z "$error" ]; then
        echo "  エラー: $error"
        echo "  レスポンス: $update_response"
        continue
    fi

    updated_title=$(echo "$update_response" | jq -r '.title')
    echo "  更新成功: $updated_title"
done

echo "すべてのIssuesの更新が完了しました！"
