# インストール手順

このドキュメントでは、Max/MSP SQL知識ベース統合型開発システムのインストール方法について説明します。

## 前提条件

- Max/MSP 8.x以降
- Node.js 14.x以降
- SQLite 3.35以降
- MinDevKit（最新版）
- C++17対応コンパイラ（XcodeまたはVisual Studio）

## 1. 基本コンポーネントのインストール

### A. osc_bridge.mxoのインストール

1. `osc_bridge.mxo`を以下のいずれかの場所にコピーします：

**macOS:**
```
/Users/<ユーザー名>/Documents/Max 8/Externals/
```

**Windows:**
```
C:\Users\<ユーザー名>\Documents\Max 8\Externals\
```

### B. SQL知識ベースのセットアップ

1. `max-knowledge.sqlite`ファイルをプロジェクトディレクトリにコピーします。
2. 環境変数の設定：

```
MAX_DB_PATH=/path/to/max-knowledge.sqlite
```

### C. Node for Maxのインストール

1. Node for Maxがインストールされていない場合は、Max Package Managerからインストールします：
   - Max Patcherで「Help > Package Manager」を選択
   - 「Node for Max」を検索
   - 「Install」をクリック

2. 必要なNode.jsパッケージをインストール：

```bash
cd <プロジェクトディレクトリ>
npm install sqlite3 express socket.io
```

## 2. 知識ベース関連ファイルの配置

以下のファイルをプロジェクトディレクトリに配置します：

1. `max-db-server.js` - SQLデータベースサーバー
2. `max-codegen.js` - コード生成エンジン
3. `max-knowledge.sqlite` - メインデータベースファイル

## 3. Maxパッチのセットアップ

1. `sql_knowledge_bridge.maxpat`をMaxで開きます
2. パッチ内の以下のオブジェクトが正しく設定されていることを確認します：
   - `[osc_bridge 7400 7500]`
   - `[node.script max-db-server.js]`
   - `[node.script max-codegen.js]`

## 4. MinDevKit環境の設定

1. MinDevKitをまだインストールしていない場合はインストールします：
   - [github.com/Cycling74/min-devkit](https://github.com/Cycling74/min-devkit)からクローン
   - インストール手順に従ってセットアップ

2. 環境変数の設定：

```
MIN_DEVKIT_PATH=/path/to/min-devkit
```

## 5. ビルドスクリプトの設定

1. `build-external.sh`（macOS/Linux）または`build-external.bat`（Windows）を以下のディレクトリに配置します：
   - `<プロジェクトディレクトリ>/scripts/`

2. スクリプトに実行権限を付与（macOS/Linux）：

```bash
chmod +x scripts/build-external.sh
```

## 6. 動作確認

1. Maxを起動し、`sql_knowledge_bridge.maxpat`を開きます
2. 「connect」メッセージをクリックして接続を確立します
3. テスト用のプロンプトを送信し、SQL知識ベースからの応答を確認します

## トラブルシューティング

- **データベース接続エラー**: データベースのパスが正しく設定されているか確認
- **Node for Maxのエラー**: 必要なnpmパッケージがすべてインストールされているか確認
- **ビルドエラー**: MinDevKitのパスと環境変数が正しく設定されているか確認

## 更新方法

データベースの更新や最新のMax文書を反映させるには：

```bash
node scripts/update-max-knowledge.js
```

このスクリプトは、最新のMax文書を解析し、SQLiteデータベースを更新します。
