# Max-Claude Desktop MCP連携プロジェクト

## 1. プロジェクト概要

このプロジェクトは、Claude DesktopからMax環境を操作できるようにするための連携システムです。**Min-DevKit**（C++ベース）を介してClaude DesktopとMax/MSPの橋渡しを行い、AIによるMax環境の自動操作を実現することが目的です。

### 主要な目標

- C++ベースのMin-DevKit開発環境の構築（Max 7.x/8.x互換）
- Claude DesktopからMax環境を操作するためのMCP連携基盤
- Max for Live（Ableton Live内）環境対応
- MCPプロトコルを用いたClaude DesktopとMin-DevKit間の通信
- AIによるパッチ作成、DSP操作のための操作API
- MinDevKit実装の正確性検証メカニズム
- SQL知識ベースによる命令変換・検証レイヤーの実装

## 2. MCPプロトコルについて

MCPはAnthropicが開発したModel Context Protocol（モデルコンテキストプロトコル）で、大規模言語モデル（LLM）がローカル環境と安全に連携するためのプロトコルです。本プロジェクトではこのMCPプロトコルを活用し、Claude DesktopからMaxへの命令を実現します。

### 主要なMCP機能

- Max環境の操作や制御を実行するための標準化されたツール群
- Claude AIによるツール実行の理解と管理
- **stdio Transport**（標準入出力トランスポート）を使用した通信
- 局所的かつ安全なプロセス間通信
- エラーハンドリングと実行制御のフレームワーク

## 3. SQL知識ベース

本プロジェクトではSQLite3ベースの知識ベースを実装し、以下の情報を構造化しています：

1. **max_objects**: Max/MSPオブジェクト情報（561レコード）
2. **min_devkit_api**: Min-DevKit API関数情報（169レコード）
3. **connection_patterns**: 接続パターン情報（201レコード）
4. **validation_rules**: 検証ルール情報（105レコード）
5. **api_mapping**: 自然言語意図からAPI関数へのマッピング（160レコード）

この知識ベースにより、コード検証や修正提案などの高度な機能を実現します。

## 4. プロジェクト構造

```
v8ui/
├── src/
│   ├── min-devkit/          # Min-DevKit統合コード
│   │   ├── osc_bridge/      # OSCブリッジ実装
│   │   └── orchestrator/    # オーケストレータ
│   ├── max-osc-bridge/      # Max OSC連携
│   ├── sql_knowledge_base/  # SQL知識ベース実装
│   │   ├── max_claude_kb.db # SQLiteデータベース
│   │   ├── server.js        # MCPサーバー（stdio Transport使用）
│   │   └── integration.js   # Claude Desktop連携
│   └── security/            # セキュリティ関連コード
├── docs/                    # プロジェクトドキュメント
│   ├── overview/            # プロジェクト概要
│   ├── issues/              # Issue詳細定義
│   ├── design/              # 設計ドキュメント
│   ├── specifications/      # 技術仕様
│   ├── development/         # 開発ガイド
│   └── planning/            # 計画と課題管理
└── externals/               # 外部依存ライブラリ
```

## 5. MCP実装詳細

本プロジェクトのMCP実装では、以下の特徴があります：

- **stdio Transport**: 標準入出力を使用したプロセス間通信
- **C++直接連携**: データ構造を直接受け渡し、中間変換なし
- **ローカル実行**: すべての処理はローカル環境で完結
- **高速処理**: 中間フォーマット変換を省略した高パフォーマンス実装

## 6. 重要なIssue

現在のプロジェクトでは、以下のIssueに取り組んでいます：

- **Issue #010** (SQL知識ベースによる命令変換・検証レイヤーの実装)
  - GitHub上の実際のIssue番号: **#149**
  - パス: `/docs/issues/issue_010_sql_knowledge_base.md`

## 7. ドキュメント

主要なドキュメントは以下にあります：

- 要件定義書: `/docs/specifications/requirements_min_devkit.md`
- SQL知識ベース仕様: `/docs/handover_sql_knowledge_base.md`
- 実装Issue: `/docs/issues/` ディレクトリ

## 8. ライセンス情報

### Min-DevKitライセンス

本プロジェクトでは**[Min-DevKit](https://github.com/Cycling74/min-devkit)**を使用しています。

Min-DevKitはMITライセンスの下で提供されています：

```
The MIT License

Copyright 2018, The Min-DevKit Authors. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
