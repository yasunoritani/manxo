#!/usr/bin/env python
# Simple WebSocket Server for testing OSC Bridge
import asyncio
import websockets
import json
import logging

# ロギングの設定
logging.basicConfig(
    format="%(asctime)s %(message)s",
    level=logging.INFO,
)

# クライアント接続を保持
CLIENTS = set()

async def echo(websocket):
    CLIENTS.add(websocket)
    logging.info(f"新しいクライアント接続: {websocket.remote_address}")
    
    try:
        # クライアントに接続確認メッセージを送信
        welcome_msg = {
            "type": "mcp",
            "command": "welcome",
            "message": "OSC Bridge Test Server"
        }
        await websocket.send(json.dumps(welcome_msg))
        
        async for message in websocket:
            try:
                # JSONメッセージとして解析
                data = json.loads(message)
                logging.info(f"受信: {message}")
                
                # MCPコマンド処理
                if data.get("type") == "mcp":
                    if data.get("command") == "ping":
                        response = {
                            "type": "mcp",
                            "command": "pong",
                            "timestamp": data.get("timestamp", 0)
                        }
                        await websocket.send(json.dumps(response))
                    elif data.get("command") == "register":
                        response = {
                            "type": "mcp",
                            "command": "register_response",
                            "status": "success",
                            "client_name": data.get("client_name", "unknown")
                        }
                        await websocket.send(json.dumps(response))
                
                # OSCメッセージ処理
                elif data.get("type") == "osc":
                    if data.get("address") == "/echo":
                        response = {
                            "type": "osc",
                            "address": "/echo_response",
                            "args": data.get("args", [])
                        }
                        await websocket.send(json.dumps(response))
                    elif data.get("address").startswith("/claude/"):
                        # Claude宛のメッセージは処理するだけで応答しない
                        logging.info(f"Claude宛メッセージ: {data.get('address')}")
                
            except json.JSONDecodeError:
                logging.warning(f"無効なJSONメッセージ: {message}")
    
    except websockets.exceptions.ConnectionClosed:
        logging.info("接続が閉じられました")
    finally:
        CLIENTS.remove(websocket)

async def main(port=8081):
    # プロジェクトルールに従って動的ポート範囲（49152～65535）を使用するオプション
    # デフォルトでは8081を使用（8080はMaxパッチで使用中の可能性があるため）
    
    async with websockets.serve(echo, "localhost", port):
        logging.info(f"WebSocketサーバーを起動しました: ws://localhost:{port}")
        try:
            await asyncio.Future()  # サーバーを無期限に実行
        except KeyboardInterrupt:
            logging.info("サーバー停止 (Ctrl+C)")

if __name__ == "__main__":
    import argparse
    
    # コマンドライン引数の処理
    parser = argparse.ArgumentParser(description='OSC Bridge WebSocketテストサーバー')
    parser.add_argument('--port', type=int, default=8081,
                        help='WebSocketサーバーのポート（デフォルト: 8081）')
    parser.add_argument('--dynamic', action='store_true',
                        help='動的ポート範囲を使用（49152-65535）')
    args = parser.parse_args()
    
    # 動的ポート範囲が指定された場合
    if args.dynamic:
        import random
        port = random.randint(49152, 65535)
        logging.info(f"動的ポート範囲から選択: {port}")
    else:
        port = args.port
    
    try:
        asyncio.run(main(port))
    except KeyboardInterrupt:
        print("\nサーバーを停止します...")
    except OSError as e:
        if e.errno == 48:  # アドレス使用中のエラー
            logging.error(f"ポート {port} は既に使用されています。別のポートを試してください。")
            if not args.dynamic:
                logging.info("--dynamic オプションを使用して動的ポート割り当てを試すことができます。")
        else:
            logging.error(f"OSエラー: {e}")
