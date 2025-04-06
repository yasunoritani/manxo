#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
LLM Mock Server - Claude Desktop API互換のモックWebSocketサーバー
Issue 28: Claude/LLM MCPコネクタのテスト用
"""

import asyncio
import json
import logging
import time
import uuid
import websockets
import argparse
import random
from typing import Dict, List, Optional, Any

# ロギング設定
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger('llm_mock_server')

# 利用可能なモデル
AVAILABLE_MODELS = [
    {
        "id": "claude-3-opus-20240229",
        "name": "Claude 3 Opus",
        "provider": "Anthropic",
        "description": "最も高性能なモデル"
    },
    {
        "id": "claude-3-sonnet-20240229",
        "name": "Claude 3 Sonnet",
        "provider": "Anthropic",
        "description": "バランスの取れたモデル"
    },
    {
        "id": "claude-3-haiku-20240307",
        "name": "Claude 3 Haiku",
        "provider": "Anthropic",
        "description": "最速のモデル"
    }
]

# テストレスポンス
TEST_RESPONSES = {
    "こんにちは": [
        "こんにちは！Max for Liveでの音楽制作のお手伝いができて嬉しいです。何かお手伝いできることはありますか？",
        "音楽制作に関する質問や、Ableton Liveの使い方、音作りのアドバイスなど、お気軽にお聞きください。"
    ],
    "Abletonについて教えて": [
        "Ableton Liveは、音楽制作とライブパフォーマンスのための人気のあるデジタル・オーディオ・ワークステーション（DAW）です。",
        "主な特徴として、セッションビューとアレンジメントビューの2つの作業環境、内蔵の高品質エフェクトとインストゥルメント、Max for Liveによる拡張性などがあります。",
        "Ableton Liveは、電子音楽制作、ライブパフォーマンス、レコーディング、編集、ミキシングなど、さまざまな音楽制作のワークフローに対応しています。"
    ],
    "default": [
        "ご質問ありがとうございます。Max for Live環境でのお手伝いができて嬉しいです。",
        "音楽制作に関する詳細な情報が必要でしたら、もう少し具体的に教えていただけると助かります。"
    ]
}

class LLMMockServer:
    """Claude Desktop APIと互換性のあるモックサーバー"""
    
    def __init__(self, port: int = 5678):
        """初期化
        
        Args:
            port: サーバーが使用するポート番号
        """
        self.port = port
        self.clients = {}  # 接続クライアント
        self.active_requests = {}  # 進行中のリクエスト
        self.server = None
        
    async def start(self):
        """サーバーを起動"""
        self.server = await websockets.serve(
            self.handle_client,
            "localhost",
            self.port
        )
        logger.info(f"LLM Mock Server running on ws://localhost:{self.port}")
    
    async def handle_client(self, websocket, path):
        """クライアント接続を処理
        
        Args:
            websocket: WebSocket接続
            path: リクエストパス
        """
        # クライアントIDを割り当て
        client_id = str(uuid.uuid4())
        self.clients[client_id] = websocket
        logger.info(f"Client {client_id} connected from {path}")
        
        try:
            async for message in websocket:
                await self.handle_message(client_id, message)
        except websockets.exceptions.ConnectionClosed:
            logger.info(f"Client {client_id} disconnected")
        finally:
            # クライアント切断時のクリーンアップ
            if client_id in self.clients:
                del self.clients[client_id]
                
            # 進行中のリクエストをキャンセル
            reqs_to_cancel = [req_id for req_id, req in self.active_requests.items() 
                             if req["client_id"] == client_id]
            for req_id in reqs_to_cancel:
                if req_id in self.active_requests:
                    del self.active_requests[req_id]
    
    async def handle_message(self, client_id: str, message: str):
        """メッセージを処理
        
        Args:
            client_id: クライアントID
            message: 受信したメッセージ
        """
        try:
            data = json.loads(message)
            logger.info(f"Received message from client {client_id}: {data.get('type', 'unknown')}")
            
            # メッセージタイプに基づいて処理
            if data.get("type") == "prompt":
                await self.handle_prompt(client_id, data)
            elif data.get("type") == "cancel":
                await self.handle_cancel(client_id)
            elif data.get("type") == "list_models":
                await self.handle_list_models(client_id)
            else:
                await self.send_error(client_id, "unknown_request_type", 
                                     f"Unknown request type: {data.get('type', 'undefined')}")
        
        except json.JSONDecodeError:
            await self.send_error(client_id, "invalid_json", "Invalid JSON format")
        except Exception as e:
            logger.error(f"Error handling message: {str(e)}", exc_info=True)
            await self.send_error(client_id, "server_error", f"Server error: {str(e)}")
    
    async def handle_prompt(self, client_id: str, data: Dict[str, Any]):
        """プロンプトリクエストを処理
        
        Args:
            client_id: クライアントID
            data: リクエストデータ
        """
        # リクエストIDを取得または生成
        req_id = data.get("id", str(uuid.uuid4()))
        
        # プロンプトと設定を取得
        prompt = data.get("prompt", "")
        model = data.get("model", "claude-3-sonnet-20240229")
        config = data.get("config", {})
        
        # モデルが存在するか確認
        model_exists = False
        for m in AVAILABLE_MODELS:
            if m["id"] == model:
                model_exists = True
                break
        
        if not model_exists:
            await self.send_error(client_id, "unknown_model", f"Unknown model: {model}")
            return
        
        # リクエストを記録
        self.active_requests[req_id] = {
            "client_id": client_id,
            "prompt": prompt,
            "model": model,
            "config": config,
            "start_time": time.time()
        }
        
        # ストリーミングモードかどうか
        stream = config.get("stream", True)
        
        # テスト用のレスポンスを準備
        if prompt in TEST_RESPONSES:
            response_parts = TEST_RESPONSES[prompt]
        else:
            response_parts = TEST_RESPONSES["default"]
        
        # ストリーミングレスポンスを送信
        if stream:
            for i, part in enumerate(response_parts):
                is_final = (i == len(response_parts) - 1)
                
                # 途中でキャンセルされたかチェック
                if req_id not in self.active_requests:
                    logger.info(f"Request {req_id} was cancelled")
                    return
                
                # レスポンスを送信
                response = {
                    "type": "stream",
                    "id": req_id,
                    "content": part,
                    "final": is_final
                }
                
                await self.clients[client_id].send(json.dumps(response))
                await asyncio.sleep(1.0)  # 現実的な遅延をシミュレート
        else:
            # 非ストリーミングモードでは全体を一度に送信
            full_response = " ".join(response_parts)
            response = {
                "type": "response",
                "id": req_id,
                "content": full_response,
                "final": True
            }
            
            # 少し遅延してから送信
            await asyncio.sleep(2.0)
            await self.clients[client_id].send(json.dumps(response))
        
        # リクエスト完了時にクリーンアップ
        if req_id in self.active_requests:
            del self.active_requests[req_id]
    
    async def handle_cancel(self, client_id: str):
        """進行中のリクエストをキャンセル
        
        Args:
            client_id: クライアントID
        """
        # クライアントの進行中リクエストを探す
        reqs_to_cancel = [req_id for req_id, req in self.active_requests.items() 
                         if req["client_id"] == client_id]
        
        if not reqs_to_cancel:
            await self.send_error(client_id, "no_active_request", "No active request to cancel")
            return
        
        # リクエストをキャンセル
        for req_id in reqs_to_cancel:
            del self.active_requests[req_id]
        
        # キャンセル成功を通知
        response = {
            "type": "cancel_success",
            "message": "Request cancelled successfully"
        }
        await self.clients[client_id].send(json.dumps(response))
    
    async def handle_list_models(self, client_id: str):
        """利用可能なモデル一覧を送信
        
        Args:
            client_id: クライアントID
        """
        response = {
            "type": "models",
            "models": AVAILABLE_MODELS
        }
        await self.clients[client_id].send(json.dumps(response))
    
    async def send_error(self, client_id: str, code: str, message: str):
        """エラーメッセージを送信
        
        Args:
            client_id: クライアントID
            code: エラーコード
            message: エラーメッセージ
        """
        error = {
            "type": "error",
            "code": code,
            "message": message
        }
        if client_id in self.clients:
            await self.clients[client_id].send(json.dumps(error))
    
    def stop(self):
        """サーバーを停止"""
        if self.server:
            self.server.close()
            logger.info("Server stopped")


async def main():
    """メイン実行関数"""
    parser = argparse.ArgumentParser(description='LLM Mock Server')
    parser.add_argument('--port', type=int, default=5678, help='Port to run the server on')
    args = parser.parse_args()
    
    server = LLMMockServer(port=args.port)
    await server.start()
    
    try:
        # 永続的に実行
        await asyncio.Future()
    except KeyboardInterrupt:
        logger.info("Server stopping due to keyboard interrupt")
    finally:
        server.stop()


if __name__ == "__main__":
    asyncio.run(main())
