#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import re
import json

# toolsディレクトリのパスを追加して、new_osc_bridge_issues.pyをインポート可能にする
sys.path.append(os.path.join(os.path.dirname(os.path.abspath(__file__)), "tools"))
from new_osc_bridge_issues import create_issue, get_github_token

def read_markdown_file(file_path):
    """マークダウンファイルを読み込み、タイトルと本文を抽出する"""
    with open(file_path, "r", encoding="utf-8") as f:
        content = f.read()

    # タイトルを抽出（最初の見出し行）
    title_match = re.search(r'^# (.+)$', content, re.MULTILINE)
    if not title_match:
        return None, None

    title = title_match.group(1)
    # 本文は最初の見出し以降の全て
    body = content[title_match.end():].strip()

    return title, body

def extract_labels(body):
    """本文からラベル情報を抽出する"""
    labels = []

    # 優先度を抽出
    priority_match = re.search(r'優先度: (最高|高|中|低)', body)
    if priority_match:
        labels.append(f"優先度:{priority_match.group(1)}")

    # カテゴリを抽出
    category_match = re.search(r'カテゴリ: (.+)$', body, re.MULTILINE)
    if category_match:
        labels.append(category_match.group(1))

    return labels

def main():
    """docsディレクトリのissuesサブディレクトリのマークダウンファイルをGitHubにアップロード"""
    # 変更：issuesディレクトリの場所をdocs/issuesに変更
    issues_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "docs", "issues")

    if not os.path.exists(issues_dir):
        print(f"エラー: {issues_dir} ディレクトリが見つかりません。")
        return

    issue_files = [f for f in os.listdir(issues_dir) if f.startswith("issue_") and f.endswith(".md")]

    if not issue_files:
        print("アップロードするIssueファイルが見つかりません。")
        return

    token = get_github_token()
    if not token:
        print("エラー: GitHubトークンが設定されていません。")
        return

    for file_name in issue_files:
        file_path = os.path.join(issues_dir, file_name)
        title, body = read_markdown_file(file_path)

        if not title or not body:
            print(f"警告: {file_name} からタイトルまたは本文を抽出できませんでした。")
            continue

        labels = extract_labels(body)

        print(f"アップロード準備中: {title}")
        print(f"  ラベル: {labels}")

        try:
            issue = create_issue(title, body, labels)
            print(f"アップロード成功: Issue #{issue.number} ({title})")
        except Exception as e:
            print(f"エラー: {file_name} のアップロードに失敗しました。 - {str(e)}")

if __name__ == "__main__":
    main()
