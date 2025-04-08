# Max-Claude 連携プロジェクト

![プロジェクトロゴ](assets/logo.png)

## このプロジェクトについて

**Max-Claude連携プロジェクト**は、Max/MSPプログラミングの専門知識がなくても、AIの力を使って音楽・サウンド制作を行えるようにするシステムです。

**シンプルな例**:
> 「サイン波オシレーターを作成して、ローパスフィルターにつないで」

このような自然言語の指示だけで、Maxパッチが自動的に作成されます。

## 核となる3つの技術

![システム概要](assets/system_overview.png)

1. **Claude Desktop (LLM)**: ユーザーの意図を理解し、適切な指示に変換します
2. **SQL知識ベース**: Max/MSPとMin-DevKitの専門知識を格納し、AIの判断を支援します
3. **Min-DevKit (C++)**: Max/MSPを直接操作する橋渡しを行います

## 使い方

1. **MCPサーバーをインストール・設定**
   ```bash
   cd v8ui
   npm install
   ```

2. **Claude Desktopでツールを設定**
   - Claude Desktopの設定でMCPツールを追加
   - MCPツールの設定で以下のコマンドを指定:
     ```
     node src/sql_knowledge_base/mcp-server.js
     ```
   - Claude Desktopに指定したツールが表示されていることを確認

3. **Claude Desktopでツールを使用**
   - 新しい会話を開始し、Max/MSPについて質問や指示を入力
   - 例：「サイン波オシレーターを作成して、ローパスフィルターにつないで」
   - Claude Desktopが内部的にMCPツールを呼び出し、SQL知識ベースを参照

4. **結果のMaxパッチを確認・必要に応じて調整**

## 主な機能

- **自然言語→Maxパッチ変換**: 日常言語でMax/MSPプログラミング
- **音楽的意図の理解**: 音楽理論やサウンドデザインの専門用語に対応
- **自動検証**: SQL知識ベースを用いた最適なオブジェクト選択と接続検証
- **修正提案**: 生成したパッチの問題点を検出し改善案を提示

## システムの仕組み

1. **ユーザーの指示** → Claude Desktopが理解
2. **MCPツール呼び出し** → クエリをSQL知識ベースに送信
3. **知識ベース検索** → 適切なAPI・接続パターンを特定
4. **Min-DevKit API選択** → 最適なMax/MSP操作を決定
5. **実行・検証** → パッチの生成と検証
6. **結果表示** → ユーザーに結果を返却

## ドキュメント

- [シンプル版要件定義](docs/requirements_simplified.md) ← **まずはこれを読むことをお勧めします**
- [SQL知識ベース実装詳細](docs/sql_knowledge_base_implementation.md)
- [詳細版要件定義書](docs/specifications/requirements_min_devkit.md)

## 開発状況

現在進行中の課題:
- SQL知識ベースによる検証精度の向上
- より複雑なパッチパターンへの対応
- ユーザーフィードバックの改善

## 貢献方法

1. このリポジトリをフォーク
2. 機能追加やバグ修正のブランチを作成
3. 変更をコミット
4. プルリクエストを送信

## ライセンス

このプロジェクトは[MIT License](LICENSE)のもとで公開されています。

使用している[Min-DevKit](https://github.com/Cycling74/min-devkit)もMITライセンスで提供されています。
