#!/usr/bin/env python
# WebSocket Test Client for OSC Bridge
# OSC Bridge プロジェクト用 WebSocketテストクライアント

import asyncio
import websockets
import json
import argparse
import time

async def send_osc_message(websocket, address, args):
    """OSCメッセージをJSON形式でWebSocketに送信"""
    message = {
        "type": "osc",
        "address": address,
        "args": args
    }
    await websocket.send(json.dumps(message))
    print(f"送信: {json.dumps(message)}")

async def send_mcp_command(websocket, command, params=None):
    """MCPコマンドをJSON形式でWebSocketに送信"""
    message = {
        "type": "mcp",
        "command": command
    }
    if params:
        message.update(params)
    await websocket.send(json.dumps(message))
    print(f"送信: {json.dumps(message)}")

async def run_test_sequence(uri):
    """テストシーケンスを実行"""
    print(f"WebSocket接続を開始: {uri}")
    try:
        async with websockets.connect(uri) as websocket:
            print("接続成功")
            
            # 1. クライアント登録
            await send_mcp_command(websocket, "register", {"client_name": "test_client"})
            response = await websocket.recv()
            print(f"応答: {response}")
            
            # 2. Pingテスト
            await send_mcp_command(websocket, "ping")
            response = await websocket.recv()
            print(f"応答: {response}")
            
            # 3. Claude OSCメッセージ送信
            await send_osc_message(websocket, "/claude/test", ["value1", 123, True])
            try:
                response = await asyncio.wait_for(websocket.recv(), timeout=1.0)
                print(f"応答: {response}")
            except asyncio.TimeoutError:
                print("応答タイムアウト (正常: 一部のメッセージは応答を返さない)")
            
            # 4. エコーテスト
            await send_osc_message(websocket, "/echo", ["echo_test"])
            try:
                response = await asyncio.wait_for(websocket.recv(), timeout=1.0)
                print(f"応答: {response}")
            except asyncio.TimeoutError:
                print("応答タイムアウト (エコーがサポートされていない可能性があります)")
            
            print("テスト完了")
    except Exception as e:
        print(f"エラー: {e}")

def main():
    parser = argparse.ArgumentParser(description='OSC Bridge WebSocketテストクライアント')
    parser.add_argument('--host', default='localhost', help='ホスト名 (デフォルト: localhost)')
    parser.add_argument('--port', type=int, default=8081, help='ポート番号 (デフォルト: 8081)')
    args = parser.parse_args()
    
    uri = f"ws://{args.host}:{args.port}"
    asyncio.run(run_test_sequence(uri))

if __name__ == "__main__":
    main()
