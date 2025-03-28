# Max 9 OSC通信機能詳細分析

## 1. 基本OSCオブジェクト

### 1.1 主要OSCオブジェクト
| オブジェクト名 | 機能 | 使用例 |
|--------------|------|--------|
| **udpsend** | UDP経由でOSCメッセージを送信 | `udpsend 127.0.0.1 7400` |
| **udpreceive** | UDP経由でOSCメッセージを受信 | `udpreceive 7500` |
| **oscformat** | Maxメッセージをプロトコルに準拠したOSCパケットに変換 | `oscformat /max/parameter` |
| **oscparse** | 受信したOSCパケットをMaxメッセージに変換 | `oscparse` |
| **param.osc** | パラメータ操作を自動的にOSCメッセージに変換 | `param.osc @export 1 @prefix /max` |
| **osc.codebox** | JavaScriptでのOSC処理を可能にする | `osc.codebox @param osc_handler` |

### 1.2 OSCアドレスパターンと型タグ
- OSCアドレスは `/` で始まり、階層構造を表現（例：`/synth/filter/cutoff`）
- 型タグ：`i`（整数）、`f`（浮動小数点）、`s`（文字列）、`b`（バイナリブロブ）など
- バンドル機能：複数のメッセージをまとめて送信可能

## 2. Max 9でのOSC通信の基本パターン

### 2.1 基本的な送信パターン
```
[message /address 123]
    |
[oscformat /address]
    |
[udpsend 127.0.0.1 8000]
```

### 2.2 基本的な受信パターン
```
[udpreceive 7500]
    |
[oscparse]
    |
[route /address]
    |
[出力]
```

### 2.3 双方向通信の設定
```
# 送信側
[message /max/request getValue]
    |
[oscformat /max/request]
    |
[udpsend 127.0.0.1 8000]

# 受信側
[udpreceive 7500]
    |
[oscparse]
    |
[route /max/response]
    |
[処理ロジック]
```

## 3. param.oscの高度な機能

### 3.1 パラメータのエクスポート
パラメータを自動的にOSC経由で公開・制御可能にする機能：

```
[param.osc @export 1 @prefix /max/params]
```

これにより、以下のような自動的なOSCアドレスマッピングが生成されます：
- `/max/params/param1` - パラメータ1の制御用
- `/max/params/param2` - パラメータ2の制御用

### 3.2 パラメータマッピングとフィルタリング
- 選択的なパラメータのエクスポート：`@parameter param1 param2`
- パラメータのリマッピング：`@map param1:/custom/address`
- 値の範囲変換：`@min 0. @max 1.`

### 3.3 双方向同期
- 値の変更の自動通知：`@notify 1`
- 接続時の初期状態送信：`@initialquery 1`

## 4. osc.codeboxによる高度なOSC処理

### 4.1 基本的なJavaScriptハンドラ
```javascript
function osc_handler(address, args) {
    if (address === "/control/volume") {
        outlet(0, "volume", args[0]);
    } else if (address === "/control/frequency") {
        outlet(0, "frequency", args[0]);
    }
}
```

### 4.2 動的なアドレスパターン処理
```javascript
function osc_handler(address, args) {
    // パターンマッチング: /synth/1/param, /synth/2/param など
    var match = address.match(/\/synth\/(\d+)\/(\w+)/);
    if (match) {
        var synthId = parseInt(match[1]);
        var paramName = match[2];
        outlet(0, "synth", synthId, paramName, args[0]);
    }
}
```

### 4.3 複雑なデータ構造の変換
```javascript
function osc_handler(address, args) {
    if (address === "/control/matrix") {
        // 2次元配列として解釈 (例: 4x4マトリックス)
        var matrix = [];
        for (var i = 0; i < 4; i++) {
            var row = [];
            for (var j = 0; j < 4; j++) {
                row.push(args[i * 4 + j]);
            }
            matrix.push(row);
        }
        // マトリックスデータをJSON文字列として出力
        outlet(0, "matrix", JSON.stringify(matrix));
    }
}
```

## 5. Min-DevKitでのOSC実装パターン

### 5.1 OSCメッセージの送信
```cpp
// osc_args をatoms型からOSCメッセージに変換
atoms osc_args(args.begin() + 1, args.end());

// OSCパケットの構築
char buffer[1024];
osc::OutboundPacketStream packet(buffer, 1024);
packet << osc::BeginMessage(address.c_str());

// 引数の型に応じた追加
for (auto& arg : osc_args) {
    if (arg.is_int())
        packet << arg.as_int();
    else if (arg.is_float())
        packet << arg.as_float();
    else if (arg.is_string())
        packet << arg.as_string().c_str();
}

packet << osc::EndMessage;

// UDPで送信
socket.Send(packet.Data(), packet.Size());
```

### 5.2 OSCメッセージの受信と処理
```cpp
// OSCメッセージ受信ハンドラ
void message_handler(const osc::ReceivedMessage& msg, 
                     const IpEndpointName& remote_endpoint) {
    try {
        // アドレスパターンの取得
        string address = msg.AddressPattern();
        
        // 引数の処理
        atoms args;
        osc::ReceivedMessage::const_iterator arg = msg.ArgumentsBegin();
        
        while (arg != msg.ArgumentsEnd()) {
            if (arg->IsInt32())
                args.push_back(arg->AsInt32());
            else if (arg->IsFloat())
                args.push_back(arg->AsFloat());
            else if (arg->IsString())
                args.push_back(string(arg->AsString()));
            
            arg++;
        }
        
        // Maxオブジェクトに通知
        defer([this, address, args]() {
            // MaxのUI/メインスレッドでの処理
            output.send(address, args);
        });
    }
    catch (osc::Exception& e) {
        cerr << "Error parsing OSC message: " << e.what() << endl;
    }
}
```

## 6. MCPサーバーとMax 9のOSC連携パターン

### 6.1 基本的な連携アーキテクチャ
```
[MCP Server] <--JSON--> [Max 9 MCP Bridge] <--OSC--> [Max 9 Patch]
```

### 6.2 MCPブリッジの役割
1. **プロトコル変換**: JSON ⟷ OSC
2. **アドレスマッピング**: MCPリソース/ツールをOSCアドレスにマッピング
3. **型変換**: JSONの型とOSCの型タグの相互変換
4. **セッション管理**: 接続状態の監視と回復

### 6.3 インプリメンテーションアプローチ
1. **Python FastMCP + python-osc**:
   ```python
   @mcp.tool()
   def send_to_max(ctx: Context, address: str, value: Any) -> str:
       """MaxにOSCメッセージを送信"""
       client = udp_client.SimpleUDPClient("127.0.0.1", 7400)
       client.send_message(address, value)
       return f"OSCメッセージを送信: {address} {value}"
   ```

2. **JavaScript (Node.js) + osc.js**:
   ```javascript
   const { FastMCP } = require('fastmcp');
   const osc = require('osc');
   
   const mcp = new FastMCP({ name: "Max 9 MCP Bridge" });
   const oscPort = new osc.UDPPort({ localAddress: "0.0.0.0", localPort: 7500 });
   
   mcp.tool({
     name: "sendToMax",
     handler: (ctx, { address, value }) => {
       oscPort.send({ address, args: [value] });
       return `OSCメッセージを送信: ${address} ${value}`;
     }
   });
   ```

## 7. OSC通信のベストプラクティス

### 7.1 アドレス設計
- 明確な階層構造: `/app/category/item/action`
- 一貫した命名規則: キャメルケースやスネークケースの統一
- アプリケーション固有プレフィックス: `/max/mcp/...`

### 7.2 エラーハンドリング
- 接続エラーの検出と回復メカニズム
- タイムアウト処理の実装
- エラー通知専用OSCアドレス: `/error/...`

### 7.3 パフォーマンス最適化
- メッセージバンドルの活用
- ポーリングの最小化
- 不要なメッセージのフィルタリング

### 7.4 デバッグ手法
- `osc.codebox`を使ったメッセージのロギング
- OSC監視ツール（例：OSC-Route, OSCulator）の活用
- テスト用エコーサーバーの実装

## 8. Max 9とMCPの統合実装例

### 8.1 Max 9 OSCレシーバーパッチ
```
[udpreceive 7500]
    |
[oscparse]
    |
[route /mcp/action /mcp/query /mcp/notification]
    |       |           |
[アクション処理] [クエリ処理] [通知処理]
```

### 8.2 MCPサーバーでのOSC実装
```python
# OSC送信用の単一インスタンス
osc_client = None

@mcp.tool()
def connect_to_max(ctx: Context, host: str = "127.0.0.1", port: int = 7400) -> str:
    """Max 9にOSC接続する"""
    global osc_client
    try:
        from pythonosc import udp_client
        osc_client = udp_client.SimpleUDPClient(host, port)
        
        # 接続テストメッセージを送信
        osc_client.send_message("/mcp/system/connected", 1)
        return f"Max 9にOSC接続しました: {host}:{port}"
    except Exception as e:
        return f"OSC接続エラー: {str(e)}"

@mcp.tool()
def create_max_object(ctx: Context, patch_id: str, obj_type: str, x: int, y: int) -> str:
    """MaxパッチにオブジェクトをOSC経由で作成"""
    if not osc_client:
        return "エラー: OSC接続がありません"
    
    try:
        # オブジェクト作成コマンドを送信
        osc_client.send_message(f"/mcp/patch/{patch_id}/create", [obj_type, x, y])
        return f"オブジェクト作成コマンドを送信: {obj_type} at ({x}, {y})"
    except Exception as e:
        return f"OSCコマンド送信エラー: {str(e)}"
```

### 8.3 双方向通信実装
```python
# OSC受信設定
from pythonosc import dispatcher, osc_server
import threading

# OSCディスパッチャー
osc_dispatcher = dispatcher.Dispatcher()

# 受信ハンドラ
def handle_max_response(address, *args):
    """Max 9からのOSCレスポンスを処理"""
    print(f"受信: {address} - {args}")
    # MCPステートの更新など

# OSCサーバー起動
def start_osc_receiver(host="0.0.0.0", port=7500):
    """OSC受信サーバーを開始"""
    osc_dispatcher.map("/max/*", handle_max_response)
    
    server = osc_server.ThreadingOSCUDPServer((host, port), osc_dispatcher)
    server_thread = threading.Thread(target=server.serve_forever)
    server_thread.daemon = True
    server_thread.start()
    
    print(f"OSC受信サーバーを開始: {host}:{port}")
    return server
```

## 9. 実装戦略とロードマップ

### 9.1 フェーズ1: 基本的なOSC通信
- Max 9のOSC送受信テンプレートパッチの作成
- 単純なコマンド（値の設定/取得）の実装
- 接続状態の監視と管理

### 9.2 フェーズ2: 高度な機能
- パッチ操作（作成、保存、ロード）
- オブジェクト操作（作成、接続、削除）
- パラメータの双方向同期

### 9.3 フェーズ3: 統合とテスト
- MCPサーバーとの完全統合
- エラーケースのテスト
- パフォーマンス最適化

## 10. 参考リソース
- Max 9 OSCリファレンス
- python-oscライブラリドキュメント
- Min-DevKit OSC実装例
- OSCプロトコル仕様（http://opensoundcontrol.org/spec-1_0/）
