#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import json
import re
import glob
import requests
from requests.exceptions import RequestException
import time
import markdown
import argparse
from pathlib import Path

# GitHub APIのベースURL
GITHUB_API_BASE = "https://api.github.com"

def get_github_token():
    """GitHub APIトークンを環境変数から取得"""
    token = os.environ.get("GITHUB_TOKEN")
    if not token:
        print("エラー: GITHUB_TOKENが設定されていません。")
        print("export GITHUB_TOKEN=your_token をターミナルで実行してください。")
        sys.exit(1)
    return token

def get_repo_info():
    """リポジトリ情報を取得"""
    owner = os.environ.get("GITHUB_OWNER", "yasunoritani")
    repo = os.environ.get("GITHUB_REPO", "v8ui")
    return owner, repo

def parse_issue_file(file_path):
    """Issueファイルからタイトルと本文を解析"""
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()

    # タイトルは最初の行から抽出（# Issue #NNN: Title形式）
    title_match = re.search(r'^# Issue #(\d+): (.+)$', content, re.MULTILINE)
    if not title_match:
        print(f"警告: {file_path}からタイトルを抽出できませんでした。")
        issue_number = Path(file_path).stem.split('_')[1]
        title = f"Issue #{issue_number}"
    else:
        issue_number = title_match.group(1)
        title = title_match.group(2)

    # 優先度とカテゴリを抽出
    priority_match = re.search(r'\*\*優先度\*\*: (.+)', content)
    category_match = re.search(r'\*\*カテゴリ\*\*: (.+)', content)

    priority = priority_match.group(1) if priority_match else "未設定"
    category = category_match.group(1) if category_match else "未分類"

    # Markdownをそのまま本文として使用
    body = content

    return {
        "issue_number": issue_number,
        "title": f"#{issue_number}: {title}",
        "body": body,
        "priority": priority,
        "category": category
    }

def create_github_issue(owner, repo, title, body, token, labels=None):
    """GitHubにIssueを作成"""
    url = f"{GITHUB_API_BASE}/repos/{owner}/{repo}/issues"

    # GitHubの推奨する認証方法に変更
    headers = {
        "Authorization": f"token {token}",
        "Accept": "application/vnd.github.v3+json"
    }

    data = {
        "title": title,
        "body": body
    }

    if labels:
        data["labels"] = labels

    try:
        response = requests.post(url, headers=headers, json=data)
        response.raise_for_status()
        return response.json()
    except RequestException as e:
        print(f"エラー: GitHubへのIssue作成中にエラーが発生しました: {e}")
        if hasattr(e, 'response') and e.response:
            print(f"ステータスコード: {e.response.status_code}")
            print(f"レスポンス: {e.response.text}")
        return None

def main():
    parser = argparse.ArgumentParser(description='GitHubにIssueをアップロード')
    parser.add_argument('--dry-run', action='store_true', help='実際にアップロードせずに内容を表示')
    args = parser.parse_args()

    # GitHubトークンとリポジトリ情報を取得
    token = get_github_token()
    owner, repo = get_repo_info()

    # issue_00X_*.mdファイルを検索
    issue_files = sorted(glob.glob("docs/issues/issue_*.md"))

    if not issue_files:
        print("アップロード対象のIssueファイルが見つかりませんでした。")
        sys.exit(1)

    # 各Issueファイルを処理
    for file_path in issue_files:
        issue_data = parse_issue_file(file_path)
        print(f"処理中: {file_path} -> {issue_data['title']}")

        # ラベルの設定
        labels = [f"priority:{issue_data['priority']}", f"category:{issue_data['category']}"]

        if args.dry_run:
            print(f"[DRY-RUN] Issueをアップロード: {issue_data['title']}")
            print(f"  ラベル: {labels}")
            print(f"  内容の先頭100文字: {issue_data['body'][:100]}...")
            print()
        else:
            # GitHubにIssueを作成
            result = create_github_issue(owner, repo, issue_data['title'], issue_data['body'], token, labels)

            if result:
                print(f"成功: Issue #{result['number']} を作成しました: {result['html_url']}")
            else:
                print(f"失敗: {file_path} のアップロードに失敗しました")

            # API制限を避けるための小休止
            time.sleep(1)

    print("すべてのIssueのアップロードが完了しました。")

if __name__ == "__main__":
    main()
