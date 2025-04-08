# Max 9 v8ui OSC Bridge

Claude DesktopとMax 9を連携するためのOSCブリッジ実装です。このブリッジはMax 9のJavaScriptエンジン（v8ui）上で動作し、OSCプロトコルを介してMCPサーバーと通信します。

## 機能

- OSCを介したMCPサーバーとの双方向通信
- Max 9パッチの作成・編集
- オブジェクトの追加・接続・パラメータ設定
- エラー検出と回復機能

## インストール

1. このディレクトリを Max 9 の Packages ディレクトリにコピーします
2. Max 9 を起動し、`mcp_osc_bridge.maxpat`を開きます
3. パッチ内の「script start」をクリックしてOSCブリッジを起動します

## 設定

`.env`ファイルを作成するか、以下の環境変数を設定することで動作をカスタマイズできます：

```
MCP_OSC_IP=127.0.0.1        # MCPサーバーのIPアドレス
MCP_RECEIVE_PORT=7500       # MCP側が受信するポート（Max側が送信するポート）
MCP_SEND_PORT=7400          # MCP側が送信するポート（Max側が受信するポート）
ACCESS_LEVEL=full           # アクセスレベル（full, restricted, readonly）
DEBUG_MODE=true             # デバッグ出力の有効化
```

## OSCプロトコル

### MCPサーバーからMax 9へのメッセージ

- `/mcp/system/connection [status]` - 接続テスト
- `/mcp/system/status` - 状態情報リクエスト
- `/mcp/patch/create [patchId] [name]` - パッチ作成
- `/mcp/object/create [patchId] [objectId] [type] [x] [y]` - オブジェクト作成
- `/mcp/object/connect [sourceId] [sourceOutlet] [targetId] [targetInlet]` - オブジェクト接続
- `/mcp/object/param [objectId] [paramName] [value]` - パラメータ設定
- `/mcp/patch/save [patchId] [filePath]` - パッチ保存

### Max 9からMCPサーバーへのメッセージ

- `/max/response/connected [status]` - 接続応答
- `/max/response/status [patchCount] [objectCount]` - 状態情報
- `/max/response/patchCreated [patchId] [name]` - パッチ作成応答
- `/max/response/objectCreated [patchId] [objectId] [type] [x] [y]` - オブジェクト作成応答
- `/max/response/objectConnected [sourceId] [sourceOutlet] [targetId] [targetInlet]` - オブジェクト接続応答
- `/max/response/paramSet [objectId] [paramName] [value]` - パラメータ設定応答
- `/max/response/patchSaved [patchId] [filePath]` - パッチ保存応答
- `/max/params/[paramName] [value]` - パラメータ変更通知
- `/max/error/[errorType] [message]` - エラー通知
