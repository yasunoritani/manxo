# Max-Claude パッチジェネレーター

## プロジェクト概要

本プロジェクトは、自然言語の指示から直接Max/MSPパッチファイル(`.maxpat`)やMax for Liveデバイス(`.amxd`)を生成するAIシステムです。ユーザーは音楽制作やサウンドデザインのアイデアを自然言語で表現するだけで、すぐに使えるMaxパッチを入手できます。

## 主な機能

- **自然言語→Maxパッチ変換**: 日常言語での説明から完全なMaxパッチを生成
- **Max for Live対応**: Ableton Live用のデバイス(`.amxd`)ファイル生成
- **AIによるパッチ設計**: 音楽理論やDSP知識に基づいた最適なパッチ構造の提案
- **即時利用可能**: 生成されたパッチファイルはすぐにMaxまたはAbleton Liveで開いて使用可能

## システム構成

システムは以下の主要コンポーネントで構成されています：

1. **MCPサーバー**:
   - Claude DesktopとのMCP通信
   - 自然言語からMaxパッチ構造への変換処理
   - JSONパッチファイル生成エンジン

2. **SQL知識ベース**:
   - Max/MSPオブジェクト情報（パラメータ、接続ポイントなど）
   - 一般的な接続パターンやテンプレート
   - オブジェクト間の互換性情報

3. **Max/MSP構造モデル**:
   - `.maxpat`と`.amxd`のJSON構造テンプレート
   - 一般的なパッチパターンのライブラリ
   - Min-DevKit APIの知識を活用したオブジェクト挙動モデル

## セットアップ方法

### 前提条件

- Node.js 14以降
- Claude Desktop
- Max/MSP 8以降（生成されたパッチを開くため）

### インストール手順

1. **リポジトリのクローン**
   ```bash
   git clone https://github.com/yourusername/max-claude-patchgen.git
   cd max-claude-patchgen
   npm install
   ```

2. **MCPサーバーの設定**
   ```bash
   # Claude Desktopの設定ファイルディレクトリ作成
   mkdir -p ~/Library/Application\ Support/Claude/

   # 設定ファイル編集
   nano ~/Library/Application\ Support/Claude/claude_desktop_config.json
   ```

   以下の内容を設定ファイルに追加：
   ```json
   {
     "mcp": {
       "servers": [
         {
           "name": "Max Patch Generator",
           "command": "node /path/to/max-claude-patchgen/src/mcp-server.js"
         }
       ]
     }
   }
   ```

3. **SQLデータベースの初期化**
   ```bash
   node src/db/init-database.js
   ```

## 使い方

1. **Claude Desktopを起動（または再起動）**
   - 設定を反映させるためにClaude Desktopを再起動します

2. **MCPツールの確認**
   - Claude Desktopの入力欄上部でMCPツールが表示されていることを確認
   - 「Max Patch Generator」が利用可能なMCPツールとして表示されます

3. **Maxパッチの生成を指示**
   - 例：「4つのサイン波オシレーターからなるアディティブシンセサイザーのパッチを作って、ピッチとボリュームをコントロールできるようにして」
   - より詳細な指示も可能：「フィードバックディレイとリバーブを備えたグラニュラーサンプラーを作って、タイムストレッチパラメーターも付けて」

4. **生成されたパッチファイルを使用**
   - AIが指示に基づいて`.maxpat`または`.amxd`ファイルを生成
   - 生成されたファイルはシステムのダウンロードフォルダまたは指定フォルダに保存
   - Max/MSPまたはAbleton Liveで開いてすぐに使用可能

## 技術詳細

### パッチ生成プロセス

1. **自然言語解析**: ユーザーの指示から必要なオブジェクト、接続、パラメータを抽出
2. **パッチ構造設計**: 音響プロセッシングの原則に基づいて最適なオブジェクト構成を決定
3. **JSON構造生成**: Maxパッチファイル形式（JSON）に変換
4. **ポストプロセス**: オブジェクト配置の最適化、自動レイアウト調整

### Max/MSPパッチファイル構造

Maxパッチ(`.maxpat`)とMax for Liveデバイス(`.amxd`)は基本的にJSON形式のテキストファイルです。システムはMin-DevKitの知識を活用し、有効なJSON構造を生成します。パッチファイルには以下の要素が含まれます：

- オブジェクト定義（種類、位置、パラメータなど）
- オブジェクト間の接続情報
- UIエレメント（スライダー、ボタンなど）の定義
- パッチの全体設定（サイズ、表示オプションなど）

## 開発状況

現在進行中の課題:
- より複雑なパッチ構造のサポート
- UI要素の洗練化
- 様々な音楽ジャンル向けテンプレートの拡充
- Max for Liveデバイスのカスタムスキン対応

## ドキュメント

- [パッチ構造リファレンス](docs/patch_structure.md)
- [サポートされているオブジェクト一覧](docs/supported_objects.md)
- [サンプルパッチギャラリー](docs/example_patches.md)
- [API仕様](docs/api_specs.md)

## ライセンス

本プロジェクトはMITライセンスで提供されています。

生成されたMaxパッチの著作権はユーザーに帰属します。商用利用を含め、生成されたパッチを自由にお使いいただけます。
