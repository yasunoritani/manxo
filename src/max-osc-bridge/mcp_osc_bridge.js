/**
 * Max 9 v8ui OSC Bridge
 * Claude DesktopのMCPサーバーとMax 9を接続するブリッジ実装
 * 
 * このスクリプトはMax 9内のv8ui環境で実行され、OSCを介してMCPサーバーと通信します
 */

// MCPブリッジコントローラークラス
class MCPBridge {
    constructor() {
        // 設定
        this.mcpServerIP = "127.0.0.1";
        this.mcpReceivePort = 7500;  // MCP側が受信するポート（Max側が送信するポート）
        this.mcpSendPort = 7400;    // MCP側が送信するポート（Max側が受信するポート）
        this.debugMode = true;

        // 状態管理
        this.isConnected = false;
        this.oscReceiver = null;
        this.oscSender = null;
        this.activePatchIds = {};
        this.activeObjects = {};
        
        // 接続状態の詳細な追跡
        this.connectionStatus = {
            lastConnectedAt: null,
            lastDisconnectedAt: null,
            reconnectAttempts: 0,
            errors: []
        };
        
        // パッチャー参照（post出力用）
        this.patcher = max.patcher;
    }

    // 初期化関数 - Max 9起動時に呼び出される
    initialize() {
        // 設定の読み込み
        this.loadSettings();
        
        // OSC通信の初期化
        this.initOSC();
        
        // デバッグログ
        this.log("Max 9 v8ui OSCブリッジを初期化しました");
        this.log("MCPサーバーへの送信先: " + this.mcpServerIP + ":" + this.mcpReceivePort);
        this.log("MCPサーバーからの受信ポート: " + this.mcpSendPort);
        
        // 接続ステータスを更新
        this.connectionStatus.lastConnectedAt = Date.now();
        this.connectionStatus.reconnectAttempts = 0;
        
        // MCPサーバーに接続通知を送信
        this.sendOSCMessage("/max/response/connected", 1);
        
        return "MCPブリッジ初期化完了";
    }

    // 設定ファイルの読み込み
    loadSettings() {
        try {
            // ここでMax環境から設定値を読み込む
            // 実際の実装では、max.getattr()などを使ってパッチから値を取得する
            if (typeof max !== 'undefined' && max.getenv) {
                this.mcpServerIP = max.getenv("MCP_OSC_IP") || this.mcpServerIP;
                this.mcpReceivePort = parseInt(max.getenv("MCP_RECEIVE_PORT")) || this.mcpReceivePort;
                this.mcpSendPort = parseInt(max.getenv("MCP_SEND_PORT")) || this.mcpSendPort;
                this.debugMode = (max.getenv("DEBUG_MODE") === "true") || this.debugMode;
            }
        } catch (e) {
            this.error("設定読み込みエラー: " + e.message);
            this.connectionStatus.errors.push({
                type: 'config_error',
                message: e.message,
                timestamp: Date.now()
            });
        }
    }
    
    // OSC通信の初期化
    initOSC() {
        try {
            // Max APIを使用して正しいOSCオブジェクトを作成
            
            // パッチャーが存在するか確認
            if (!this.patcher) {
                this.patcher = max.patcher;
                if (!this.patcher) {
                    this.error("パッチャーにアクセスできません");
                    return;
                }
            }
            
            // 既存のオブジェクトがあれば削除
            if (this.oscReceiver) {
                this.patcher.remove(this.oscReceiver);
            }
            if (this.oscSender) {
                this.patcher.remove(this.oscSender);
            }
            
            // 新しいOSCオブジェクトを作成
            this.oscReceiver = this.patcher.newdefault(100, 100, "udpreceive", this.mcpSendPort);
            this.oscSender = this.patcher.newdefault(100, 150, "udpsend", this.mcpServerIP, this.mcpReceivePort);
            
            // 接続確立
            this.oscSender.message("connect");
            
            this.isConnected = true;
            this.log("OSC通信を初期化しました");
        } catch (e) {
            this.error("OSC通信の初期化に失敗しました: " + e.message);
            this.connectionStatus.errors.push({
                type: 'osc_init_error',
                message: e.message,
                timestamp: Date.now()
            });
            this.scheduleReconnect();
        }
    }
    
    // 再接続をスケジュール
    scheduleReconnect() {
        // 最大再試行回数をチェック
        if (this.connectionStatus.reconnectAttempts >= 5) {
            this.error("最大再接続試行回数に達しました");
            return;
        }
        
        this.connectionStatus.reconnectAttempts++;
        this.log(`再接続を試行します (${this.connectionStatus.reconnectAttempts}/5)`);
        
        // 3秒後に再接続
        setTimeout(() => {
            this.initOSC();
        }, 3000);
    }

    // OSCメッセージを送信
    sendOSCMessage(address, ...args) {
        if (!this.oscSender || !this.isConnected) {
            this.error("OSC送信できません: 接続されていません");
            return;
        }
        
        try {
            // v8ui環境でのOSC送信方法
            let messageArray = [address].concat(args);
            this.oscSender.message("send", messageArray);
            
            if (this.debugMode) {
                this.log("送信 OSC: " + address + " " + args.join(" "));
            }
        } catch (e) {
            this.error("OSCメッセージ送信エラー: " + e.message);
            this.connectionStatus.errors.push({
                type: 'send_error',
                address: address,
                message: e.message,
                timestamp: Date.now()
            });
            
            // 送信エラーの場合、接続状態を確認し、必要に応じて再接続
            this.scheduleReconnect();
        }
    }
    
    // OSCメッセージ受信ハンドラ
    oscMessageHandler(address, ...args) {
        if (this.debugMode) {
            this.log("受信 OSC: " + address + " " + args.join(" "));
        }
        
        // MCPからのメッセージを処理
        try {
            if (address === "/mcp/system/connection") {
                this.handleConnectionMessage(args);
            }
            else if (address === "/mcp/system/status") {
                this.handleStatusRequest();
            }
            else if (address === "/mcp/patch/create") {
                this.handleCreatePatch(args);
            }
            else if (address === "/mcp/object/create") {
                this.handleCreateObject(args);
            }
            else if (address === "/mcp/object/connect") {
                this.handleConnectObjects(args);
            }
            else if (address === "/mcp/object/param") {
                this.handleSetParameter(args);
            }
            else if (address === "/mcp/patch/save") {
                this.handleSavePatch(args);
            }
            else if (address === "/mcp/ping") {
                // Pingに応答
                this.sendOSCMessage("/max/pong", Date.now());
            }
            else {
                this.log("未処理のOSCメッセージ: " + address);
            }
        } catch (e) {
            this.error(`メッセージ処理エラー (${address}): ${e.message}`);
            // エラーメッセージを送信元に返す
            const errorAddress = `/max/error/${address.split('/').pop()}`;
            this.sendOSCMessage(errorAddress, e.message);
        }
    }

    // 接続メッセージの処理
    handleConnectionMessage(args) {
        const connectionStatus = args[0];
        this.log("MCPサーバーからの接続要求: " + connectionStatus);
        
        // 接続状態を更新
        this.isConnected = true;
        this.connectionStatus.lastConnectedAt = Date.now();
        this.connectionStatus.reconnectAttempts = 0;
        
        // 接続成功の応答を返す
        this.sendOSCMessage("/max/response/connected", 1);
    }
    
    // 状態リクエストの処理
    handleStatusRequest() {
        this.log("MCPサーバーからの状態リクエスト");
        
        // 現在のパッチとオブジェクトの数を送信
        const patchCount = Object.keys(this.activePatchIds).length;
        const objectCount = Object.keys(this.activeObjects).length;
        
        this.sendOSCMessage("/max/response/status", patchCount, objectCount);
        
        // 各パッチの情報を送信
        for (var patchId in this.activePatchIds) {
            this.sendOSCMessage("/max/response/patch", patchId, this.activePatchIds[patchId].name);
        }
    }
    
    // 新規パッチ作成の処理
    handleCreatePatch(args) {
        const patchId = args[0];
        const patchName = args[1] || "Untitled";
        
        this.log("新規パッチ作成リクエスト: " + patchName + " (ID: " + patchId + ")");
        
        try {
            // 新規パッチを作成
            var newPatcher = new max.Patcher();
            
            // パッチ情報を記録
            this.activePatchIds[patchId] = {
                name: patchName,
                maxPatch: newPatcher,
                objects: {}
            };
            
            // 成功応答を送信
            this.sendOSCMessage("/max/response/patchCreated", patchId, patchName);
        } catch (e) {
            this.error("パッチ作成エラー: " + e.message);
            this.sendOSCMessage("/max/error/patchCreate", "パッチ作成に失敗しました: " + e.message);
            this.connectionStatus.errors.push({
                type: 'patch_creation_error',
                message: e.message,
                timestamp: Date.now()
            });
        }
    }
    
    // オブジェクト作成の処理
    handleCreateObject(args) {
        const patchId = args[0];
        const objectId = args[1];
        const objectType = args[2];
        const x = args[3] || 100;
        const y = args[4] || 100;
        
        this.log("オブジェクト作成リクエスト: " + objectType + " (ID: " + objectId + ")");
        
        if (!this.activePatchIds[patchId]) {
            this.error("パッチが見つかりません: " + patchId);
            this.sendOSCMessage("/max/error/objectCreate", "パッチが見つかりません: " + patchId);
            return;
        }
        
        try {
            var patch = this.activePatchIds[patchId].maxPatch;
            
            // オブジェクトを作成
            var newObj = patch.newdefault(x, y, objectType);
            
            // オブジェクト情報を記録
            this.activePatchIds[patchId].objects[objectId] = newObj;
            this.activeObjects[objectId] = {
                patchId: patchId,
                type: objectType,
                object: newObj,
                position: { x, y }
            };
            
            // 成功応答を送信
            this.sendOSCMessage("/max/response/objectCreated", patchId, objectId, objectType, x, y);
        } catch (e) {
            this.error("オブジェクト作成エラー: " + e.message);
            this.sendOSCMessage("/max/error/objectCreate", "オブジェクト作成に失敗しました: " + e.message);
            this.connectionStatus.errors.push({
                type: 'object_creation_error',
                message: e.message,
                timestamp: Date.now()
            });
        }
    }
    
    // オブジェクト接続の処理
    handleConnectObjects(args) {
        const sourceId = args[0];
        const sourceOutlet = args[1] || 0;
        const targetId = args[2];
        const targetInlet = args[3] || 0;
        
        this.log("オブジェクト接続リクエスト: " + sourceId + "[" + sourceOutlet + "] -> " + targetId + "[" + targetInlet + "]");
        
        if (!this.activeObjects[sourceId] || !this.activeObjects[targetId]) {
            this.error("オブジェクトが見つかりません");
            this.sendOSCMessage("/max/error/objectConnect", "オブジェクトが見つかりません");
            return;
        }
        
        try {
            var sourcePatchId = this.activeObjects[sourceId].patchId;
            var targetPatchId = this.activeObjects[targetId].patchId;
            
            if (sourcePatchId !== targetPatchId) {
                this.error("異なるパッチ間の接続はサポートされていません");
                this.sendOSCMessage("/max/error/objectConnect", "異なるパッチ間の接続はサポートされていません");
                return;
            }
            
            var patch = this.activePatchIds[sourcePatchId].maxPatch;
            
            // オブジェクトを接続
            var sourceObj = this.activeObjects[sourceId].object;
            var targetObj = this.activeObjects[targetId].object;
            
            patch.connect(sourceObj, sourceOutlet, targetObj, targetInlet);
            
            // 成功応答を送信
            this.sendOSCMessage("/max/response/objectConnected", sourceId, sourceOutlet, targetId, targetInlet);
        } catch (e) {
            this.error("オブジェクト接続エラー: " + e.message);
            this.sendOSCMessage("/max/error/objectConnect", "オブジェクト接続に失敗しました: " + e.message);
            this.connectionStatus.errors.push({
                type: 'connection_error',
                message: e.message,
                timestamp: Date.now()
            });
        }
    }

    // パラメータ設定の処理
    handleSetParameter(args) {
        const objectId = args[0];
        const paramName = args[1];
        const paramValue = args[2];
        
        this.log("パラメータ設定リクエスト: " + objectId + "." + paramName + " = " + paramValue);
        
        if (!this.activeObjects[objectId]) {
            this.error("オブジェクトが見つかりません: " + objectId);
            this.sendOSCMessage("/max/error/paramSet", "オブジェクトが見つかりません: " + objectId);
            return;
        }
        
        try {
            var obj = this.activeObjects[objectId].object;
            
            // パラメータを設定
            obj.setattr(paramName, paramValue);
            
            // 成功応答を送信
            this.sendOSCMessage("/max/response/paramSet", objectId, paramName, paramValue);
            
            // パラメータ変更通知を送信
            this.sendOSCMessage("/max/params/" + paramName, paramValue);
        } catch (e) {
            this.error("パラメータ設定エラー: " + e.message);
            this.sendOSCMessage("/max/error/paramSet", "パラメータ設定に失敗しました: " + e.message);
            this.connectionStatus.errors.push({
                type: 'param_error',
                message: e.message,
                timestamp: Date.now()
            });
        }
    }
    
    // パッチ保存の処理
    handleSavePatch(args) {
        const patchId = args[0];
        const filePath = args[1];
        
        this.log("パッチ保存リクエスト: " + patchId + " -> " + filePath);
        
        if (!this.activePatchIds[patchId]) {
            this.error("パッチが見つかりません: " + patchId);
            this.sendOSCMessage("/max/error/patchSave", "パッチが見つかりません: " + patchId);
            return;
        }
        
        try {
            var patch = this.activePatchIds[patchId].maxPatch;
            
            // パッチを保存
            patch.filepath = filePath;
            patch.save();
            
            // 成功応答を送信
            this.sendOSCMessage("/max/response/patchSaved", patchId, filePath);
        } catch (e) {
            this.error("パッチ保存エラー: " + e.message);
            this.sendOSCMessage("/max/error/patchSave", "パッチ保存に失敗しました: " + e.message);
            this.connectionStatus.errors.push({
                type: 'save_error',
                message: e.message,
                timestamp: Date.now()
            });
        }
    }
    
    // メモリ最適化処理 - 長時間実行時に呼び出す
    optimizeMemory() {
        // エラーログの古いエントリを削除
        if (this.connectionStatus.errors.length > 100) {
            this.connectionStatus.errors = this.connectionStatus.errors.slice(-50);
        }
        
        this.log("メモリ最適化完了");
        return true;
    }
    
    // システム状態の取得
    getStatus() {
        return {
            connected: this.isConnected,
            patches: Object.keys(this.activePatchIds).length,
            objects: Object.keys(this.activeObjects).length,
            connectionStatus: this.connectionStatus,
            errors: this.connectionStatus.errors.length
        };
    }
    
    // ヘルパー関数：ログ出力
    log(message) {
        if (typeof max !== 'undefined' && max.post) {
            max.post("MCP-Bridge: " + message + "\n");
        } else if (this.patcher) {
            this.patcher.message("post", "MCP-Bridge: " + message);
        }
    }
    
    // ヘルパー関数：エラー出力
    error(message) {
        if (typeof max !== 'undefined' && max.error) {
            max.error("MCP-Bridge-ERROR: " + message);
        } else if (this.patcher) {
            this.patcher.message("error", "MCP-Bridge-ERROR: " + message);
        }
        
        // エラーログを記録
        if (this.connectionStatus && this.connectionStatus.errors) {
            this.connectionStatus.errors.push({
                type: 'general_error',
                message: message,
                timestamp: Date.now()
            });
        }
    }
    
    // v8ui環境から利用できるインターフェースメソッド
    init() {
        this.initialize();
        return "MCPブリッジ初期化完了";
    }
    
    // OSCメッセージを受信した際に呼び出されるエントリポイント
    // v8uiからの呼び出し用
    onOscMessage(address, ...args) {
        this.oscMessageHandler(address, ...args);
    }
}

// グローバルインスタンスの作成
const mcpBridge = new MCPBridge();

// Max 9へのエクスポート
exports.init = () => mcpBridge.init();
exports.onOscMessage = (address, ...args) => mcpBridge.onOscMessage(address, ...args);
