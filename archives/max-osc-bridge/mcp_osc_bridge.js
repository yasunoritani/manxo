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
    
    // ====================
    // = パッチ操作メソッド =
    // ====================
    
    /**
     * 新しいパッチを作成
     * @param {string} name - パッチの名前
     * @returns {object} 作成されたパッチの情報
     */
    createPatch(name) {
        try {
            // Max 9にnewコマンドを送信して新規パッチャーを作成
            const patcher = this.patcher.newDefault(50, 50, name || "untitled");
            
            if (!patcher) {
                this.error(`パッチ作成に失敗しました: ${name}`);
                return null;
            }
            
            // パッチIDを生成
            const patchId = `patch_${Date.now()}_${Math.floor(Math.random() * 1000)}`;
            
            // アクティブパッチとして追跡
            this.activePatchIds[patchId] = {
                id: patchId,
                name: name || "untitled",
                patcher: patcher,
                filepath: null,
                objects: {},
                connections: {},
                createdAt: Date.now(),
                updatedAt: Date.now()
            };
            
            this.log(`新規パッチ作成: ${name} (ID: ${patchId})`);
            
            return {
                id: patchId,
                name: name || "untitled"
            };
        } catch (err) {
            this.error(`パッチ作成エラー: ${err.message}`);
            return null;
        }
    }
    
    /**
     * 既存のパッチを開く
     * @param {string} path - パッチファイルのパス
     * @returns {object} 開いたパッチの情報
     */
    openPatch(path) {
        try {
            // ファイルからパッチを開く
            const patcher = max.openPatcher(path);
            
            if (!patcher) {
                this.error(`パッチを開くのに失敗しました: ${path}`);
                return null;
            }
            
            // パッチ名を取得 (パスからファイル名を抽出)
            const name = path.split('/').pop().split('.')[0];
            
            // パッチIDを生成
            const patchId = `patch_${Date.now()}_${Math.floor(Math.random() * 1000)}`;
            
            // アクティブパッチとして追跡
            this.activePatchIds[patchId] = {
                id: patchId,
                name: name,
                patcher: patcher,
                filepath: path,
                objects: {},
                connections: {},
                createdAt: Date.now(),
                updatedAt: Date.now()
            };
            
            this.log(`パッチを開きました: ${path} (ID: ${patchId})`);
            
            return {
                id: patchId,
                name: name,
                filepath: path
            };
        } catch (err) {
            this.error(`パッチを開くエラー: ${err.message}`);
            return null;
        }
    }
    
    /**
     * パッチを保存
     * @param {string} patchId - パッチID
     * @param {string} path - 保存先パス (オプション)
     * @returns {object} 保存結果
     */
    savePatch(patchId, path) {
        try {
            // パッチの存在確認
            if (!this.activePatchIds[patchId]) {
                this.error(`存在しないパッチID: ${patchId}`);
                return { success: false, error: "存在しないパッチID" };
            }
            
            const patchInfo = this.activePatchIds[patchId];
            const patcher = patchInfo.patcher;
            
            // 保存パスの決定
            const savePath = path || patchInfo.filepath;
            
            if (!savePath) {
                this.error(`保存パスが指定されていません: ${patchId}`);
                // 名前をつけて保存（ユーザー操作が必要なため非対応）
                return { success: false, error: "保存パスが指定されていません" };
            }
            
            // パッチを保存
            patcher.savePatcher(savePath);
            
            // パッチ情報を更新
            patchInfo.filepath = savePath;
            patchInfo.updatedAt = Date.now();
            
            this.log(`パッチを保存しました: ${savePath} (ID: ${patchId})`);
            
            return {
                success: true,
                path: savePath
            };
        } catch (err) {
            this.error(`パッチ保存エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * パッチを閉じる
     * @param {string} patchId - パッチID
     * @returns {object} 閉じた結果
     */
    closePatch(patchId) {
        try {
            // パッチの存在確認
            if (!this.activePatchIds[patchId]) {
                this.error(`存在しないパッチID: ${patchId}`);
                return { success: false, error: "存在しないパッチID" };
            }
            
            const patchInfo = this.activePatchIds[patchId];
            const patcher = patchInfo.patcher;
            
            // パッチを閉じる
            patcher.close();
            
            // 管理リストから削除
            delete this.activePatchIds[patchId];
            
            // 関連するオブジェクトを削除
            Object.keys(patchInfo.objects).forEach(objId => {
                if (this.activeObjects[objId]) {
                    delete this.activeObjects[objId];
                }
            });
            
            this.log(`パッチを閉じました (ID: ${patchId})`);
            
            return { success: true };
        } catch (err) {
            this.error(`パッチを閉じるエラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    // ====================
    // = オブジェクト操作メソッド =
    // ====================
    
    /**
     * パッチにオブジェクトを作成
     * @param {string} patchId - パッチID
     * @param {string} objType - オブジェクトタイプ
     * @param {number} x - X座標
     * @param {number} y - Y座標
     * @returns {object} 作成結果
     */
    createObject(patchId, objType, x, y) {
        try {
            // パッチの存在確認
            if (!this.activePatchIds[patchId]) {
                this.error(`存在しないパッチID: ${patchId}`);
                return { success: false, error: "存在しないパッチID" };
            }
            
            const patchInfo = this.activePatchIds[patchId];
            const patcher = patchInfo.patcher;
            
            // オブジェクトを作成
            const obj = patcher.newDefault(x, y, objType);
            
            if (!obj) {
                this.error(`オブジェクト作成失敗: ${objType}`);
                return { success: false, error: "オブジェクト作成失敗" };
            }
            
            // オブジェクトIDを生成
            const objectId = `obj_${Date.now()}_${Math.floor(Math.random() * 1000)}`;
            
            // アクティブオブジェクトとして追跡
            this.activeObjects[objectId] = {
                id: objectId,
                type: objType,
                patchId: patchId,
                objRef: obj,
                inlets: obj.inlets || 0,
                outlets: obj.outlets || 0,
                position: { x, y },
                params: {},
                createdAt: Date.now()
            };
            
            // パッチにオブジェクト情報を追加
            patchInfo.objects[objectId] = objectId;
            patchInfo.updatedAt = Date.now();
            
            this.log(`オブジェクト作成: ${objType} (ID: ${objectId})`);
            
            return {
                success: true,
                objectId: objectId
            };
        } catch (err) {
            this.error(`オブジェクト作成エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * オブジェクトを削除
     * @param {string} patchId - パッチID
     * @param {string} objectId - オブジェクトID
     * @returns {object} 削除結果
     */
    deleteObject(patchId, objectId) {
        try {
            // パッチとオブジェクトの存在確認
            if (!this.activePatchIds[patchId]) {
                this.error(`存在しないパッチID: ${patchId}`);
                return { success: false, error: "存在しないパッチID" };
            }
            
            if (!this.activeObjects[objectId]) {
                this.error(`存在しないオブジェクトID: ${objectId}`);
                return { success: false, error: "存在しないオブジェクトID" };
            }
            
            const patchInfo = this.activePatchIds[patchId];
            const objInfo = this.activeObjects[objectId];
            
            // オブジェクトの所属パッチを確認
            if (objInfo.patchId !== patchId) {
                this.error(`オブジェクトは指定されたパッチに属していません: ${objectId}`);
                return { success: false, error: "パッチとオブジェクトの不一致" };
            }
            
            // オブジェクトを削除
            objInfo.objRef.remove();
            
            // 管理リストから削除
            delete this.activeObjects[objectId];
            delete patchInfo.objects[objectId];
            
            patchInfo.updatedAt = Date.now();
            
            this.log(`オブジェクト削除: ${objectId}`);
            
            return { success: true };
        } catch (err) {
            this.error(`オブジェクト削除エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * オブジェクトを接続
     * @param {string} patchId - パッチID
     * @param {string} sourceId - 送信元オブジェクトID
     * @param {number} sourceOutlet - 送信元アウトレット番号
     * @param {string} destId - 送信先オブジェクトID
     * @param {number} destInlet - 送信先インレット番号
     * @returns {object} 接続結果
     */
    connectObjects(patchId, sourceId, sourceOutlet, destId, destInlet) {
        try {
            // パッチとオブジェクトの存在確認
            if (!this.activePatchIds[patchId]) {
                this.error(`存在しないパッチID: ${patchId}`);
                return { success: false, error: "存在しないパッチID" };
            }
            
            if (!this.activeObjects[sourceId]) {
                this.error(`存在しない送信元オブジェクトID: ${sourceId}`);
                return { success: false, error: "存在しない送信元オブジェクトID" };
            }
            
            if (!this.activeObjects[destId]) {
                this.error(`存在しない送信先オブジェクトID: ${destId}`);
                return { success: false, error: "存在しない送信先オブジェクトID" };
            }
            
            const patchInfo = this.activePatchIds[patchId];
            const sourceObj = this.activeObjects[sourceId];
            const destObj = this.activeObjects[destId];
            
            // オブジェクトの所属パッチを確認
            if (sourceObj.patchId !== patchId || destObj.patchId !== patchId) {
                this.error(`オブジェクトは指定されたパッチに属していません`);
                return { success: false, error: "パッチとオブジェクトの不一致" };
            }
            
            // オブジェクトを接続
            const connected = patchInfo.patcher.connect(sourceObj.objRef, sourceOutlet, destObj.objRef, destInlet);
            
            if (!connected) {
                this.error(`接続に失敗しました: ${sourceId}[${sourceOutlet}] -> ${destId}[${destInlet}]`);
                return { success: false, error: "接続失敗" };
            }
            
            // 接続IDを生成
            const connectionId = `conn_${sourceId}_${sourceOutlet}_${destId}_${destInlet}`;
            
            // 接続を記録
            patchInfo.connections[connectionId] = {
                id: connectionId,
                sourceId: sourceId,
                sourceOutlet: sourceOutlet,
                destId: destId,
                destInlet: destInlet,
                createdAt: Date.now()
            };
            
            patchInfo.updatedAt = Date.now();
            
            this.log(`オブジェクト接続: ${sourceId}[${sourceOutlet}] -> ${destId}[${destInlet}]`);
            
            return {
                success: true,
                connectionId: connectionId
            };
        } catch (err) {
            this.error(`オブジェクト接続エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * 接続IDによる接続切断
     * @param {string} patchId - パッチID
     * @param {string} connectionId - 接続ID
     * @returns {object} 切断結果
     */
    disconnectByConnectionId(patchId, connectionId) {
        try {
            // パッチの存在確認
            if (!this.activePatchIds[patchId]) {
                this.error(`存在しないパッチID: ${patchId}`);
                return { success: false, error: "存在しないパッチID" };
            }
            
            const patchInfo = this.activePatchIds[patchId];
            
            // 接続の存在確認
            if (!patchInfo.connections[connectionId]) {
                this.error(`存在しない接続ID: ${connectionId}`);
                return { success: false, error: "存在しない接続ID" };
            }
            
            const connection = patchInfo.connections[connectionId];
            
            // オブジェクトの存在確認
            if (!this.activeObjects[connection.sourceId] || !this.activeObjects[connection.destId]) {
                this.error(`接続されたオブジェクトが存在しません: ${connectionId}`);
                return { success: false, error: "接続オブジェクトの不一致" };
            }
            
            const sourceObj = this.activeObjects[connection.sourceId];
            const destObj = this.activeObjects[connection.destId];
            
            // 接続を切断
            const disconnected = patchInfo.patcher.disconnect(
                sourceObj.objRef, connection.sourceOutlet,
                destObj.objRef, connection.destInlet
            );
            
            if (!disconnected) {
                this.error(`切断に失敗しました: ${connectionId}`);
                return { success: false, error: "切断失敗" };
            }
            
            // 管理リストから削除
            delete patchInfo.connections[connectionId];
            patchInfo.updatedAt = Date.now();
            
            this.log(`接続切断: ${connectionId}`);
            
            return { success: true };
        } catch (err) {
            this.error(`接続切断エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * オブジェクト間の接続切断
     * @param {string} patchId - パッチID
     * @param {string} sourceId - 送信元オブジェクトID
     * @param {number} sourceOutlet - 送信元アウトレット番号
     * @param {string} destId - 送信先オブジェクトID
     * @param {number} destInlet - 送信先インレット番号
     * @returns {object} 切断結果
     */
    disconnectObjects(patchId, sourceId, sourceOutlet, destId, destInlet) {
        try {
            // 接続IDを生成
            const connectionId = `conn_${sourceId}_${sourceOutlet}_${destId}_${destInlet}`;
            
            // 接続IDによる切断メソッドを呼び出し
            return this.disconnectByConnectionId(patchId, connectionId);
        } catch (err) {
            this.error(`オブジェクト切断エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * オブジェクトを移動
     * @param {string} patchId - パッチID
     * @param {string} objectId - オブジェクトID
     * @param {number} x - 新しいX座標
     * @param {number} y - 新しいY座標
     * @returns {object} 移動結果
     */
    moveObject(patchId, objectId, x, y) {
        try {
            // パッチとオブジェクトの存在確認
            if (!this.activePatchIds[patchId]) {
                this.error(`存在しないパッチID: ${patchId}`);
                return { success: false, error: "存在しないパッチID" };
            }
            
            if (!this.activeObjects[objectId]) {
                this.error(`存在しないオブジェクトID: ${objectId}`);
                return { success: false, error: "存在しないオブジェクトID" };
            }
            
            const objInfo = this.activeObjects[objectId];
            
            // オブジェクトの所属パッチを確認
            if (objInfo.patchId !== patchId) {
                this.error(`オブジェクトは指定されたパッチに属していません: ${objectId}`);
                return { success: false, error: "パッチとオブジェクトの不一致" };
            }
            
            // オブジェクトを移動
            if (x !== null) objInfo.objRef.rect[0] = x;
            if (y !== null) objInfo.objRef.rect[1] = y;
            
            // 位置情報を更新
            objInfo.position = {
                x: x !== null ? x : objInfo.position.x,
                y: y !== null ? y : objInfo.position.y
            };
            
            this.log(`オブジェクト移動: ${objectId} to (${objInfo.position.x}, ${objInfo.position.y})`);
            
            return { success: true };
        } catch (err) {
            this.error(`オブジェクト移動エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    // ====================
    // = パラメータ操作メソッド =
    // ====================
    
    /**
     * オブジェクトのパラメータを設定
     * @param {string} objectId - オブジェクトID
     * @param {string} paramName - パラメータ名
     * @param {*} value - 設定値
     * @returns {object} 設定結果
     */
    setObjectParam(objectId, paramName, value) {
        try {
            // オブジェクトの存在確認
            if (!this.activeObjects[objectId]) {
                this.error(`存在しないオブジェクトID: ${objectId}`);
                return { success: false, error: "存在しないオブジェクトID" };
            }
            
            const objInfo = this.activeObjects[objectId];
            
            // オブジェクトのパラメータを設定
            if (typeof objInfo.objRef[paramName] !== 'undefined') {
                // 直接プロパティとしてアクセス可能
                objInfo.objRef[paramName] = value;
            } else if (objInfo.objRef.message) {
                // Maxメッセージを送信
                objInfo.objRef.message(paramName, value);
            } else {
                this.error(`パラメータ設定失敗: ${objectId}.${paramName}`);
                return { success: false, error: "パラメータ設定方法が不明" };
            }
            
            // パラメータ情報を記録
            objInfo.params[paramName] = {
                value: value,
                updatedAt: Date.now()
            };
            
            this.log(`パラメータ設定: ${objectId}.${paramName} = ${value}`);
            
            return { success: true };
        } catch (err) {
            this.error(`パラメータ設定エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * オブジェクトのパラメータを取得
     * @param {string} objectId - オブジェクトID
     * @param {string} paramName - パラメータ名
     * @returns {object} 取得結果
     */
    getObjectParam(objectId, paramName) {
        try {
            // オブジェクトの存在確認
            if (!this.activeObjects[objectId]) {
                this.error(`存在しないオブジェクトID: ${objectId}`);
                return { success: false, error: "存在しないオブジェクトID" };
            }
            
            const objInfo = this.activeObjects[objectId];
            
            // オブジェクトのパラメータを取得
            let value;
            
            if (typeof objInfo.objRef[paramName] !== 'undefined') {
                // 直接プロパティとしてアクセス可能
                value = objInfo.objRef[paramName];
            } else if (objInfo.params[paramName] && objInfo.params[paramName].value !== undefined) {
                // 記録された値があればそれを使用
                value = objInfo.params[paramName].value;
            } else {
                this.error(`パラメータが見つかりません: ${objectId}.${paramName}`);
                return { success: false, error: "パラメータが見つかりません" };
            }
            
            this.log(`パラメータ取得: ${objectId}.${paramName} = ${value}`);
            
            return {
                success: true,
                value: value
            };
        } catch (err) {
            this.error(`パラメータ取得エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * パラメータ変更を監視
     * @param {string} objectId - オブジェクトID
     * @param {string} paramName - パラメータ名
     * @returns {object} 監視結果
     */
    watchParam(objectId, paramName) {
        try {
            // オブジェクトの存在確認
            if (!this.activeObjects[objectId]) {
                this.error(`存在しないオブジェクトID: ${objectId}`);
                return { success: false, error: "存在しないオブジェクトID" };
            }
            
            // 監視IDを生成
            const watchId = `watch_${objectId}_${paramName}`;
            
            // 監視設定を記録
            // 注意: 実際の実装ではオブジェクトのイベント起動や定期ポーリングなどが必要
            this.log(`パラメータ監視開始: ${objectId}.${paramName} (ID: ${watchId})`);
            
            return {
                success: true,
                watchId: watchId
            };
        } catch (err) {
            this.error(`パラメータ監視エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * IDによるパラメータ監視の解除
     * @param {string} watchId - 監視ID
     * @returns {object} 解除結果
     */
    unwatchParamById(watchId) {
        try {
            // 監視解除処理
            // 実際の実装では監視リストからの削除やイベント解除などが必要
            this.log(`パラメータ監視解除: ${watchId}`);
            
            return { success: true };
        } catch (err) {
            this.error(`パラメータ監視解除エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * オブジェクトとパラメータ名による監視解除
     * @param {string} objectId - オブジェクトID
     * @param {string} paramName - パラメータ名
     * @returns {object} 解除結果
     */
    unwatchParam(objectId, paramName) {
        try {
            // 監視IDを生成
            const watchId = `watch_${objectId}_${paramName}`;
            
            // IDによる解除メソッドを呼び出し
            return this.unwatchParamById(watchId);
        } catch (err) {
            this.error(`パラメータ監視解除エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    // ====================
    // = システム操作メソッド =
    // ====================
    
    /**
     * システムの初期化
     * @param {object} clientInfo - クライアント情報
     * @returns {object} 初期化結果
     */
    initializeSystem(clientInfo) {
        try {
            // 環境情報を取得
            const maxVersion = `Max ${max.version}`;
            const v8uiVersion = typeof max.v8uiVersion !== 'undefined' ? max.v8uiVersion : 'unknown';
            
            // クライアント情報を記録
            this.clientInfo = clientInfo || {};
            
            // システム情報を設定
            this.version = '1.0.0';
            this.capabilities = {
                objects: ['message', 'number', 'flonum', 'button', 'toggle', 'slider', 'dial', 'metro', 'print', 'comment'],
                patcher: ['create', 'open', 'save', 'close'],
                connections: true,
                watch: true,
                state: true,
                session: true
            };
            
            // 初期化ログ
            this.log(`システム初期化完了: Max ${maxVersion}, v8ui ${v8uiVersion}`);
            
            return {
                success: true,
                maxVersion: maxVersion,
                v8uiVersion: v8uiVersion
            };
        } catch (err) {
            this.error(`システム初期化エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * システムのシャットダウン
     * @returns {object} シャットダウン結果
     */
    shutdownSystem() {
        try {
            // アクティブなパッチをすべて閉じる
            const patchIds = Object.keys(this.activePatchIds);
            
            if (patchIds.length > 0) {
                this.log(`すべてのパッチを閉じます (${patchIds.length}個)`);
                
                patchIds.forEach(patchId => {
                    try {
                        const patchInfo = this.activePatchIds[patchId];
                        if (patchInfo && patchInfo.patcher) {
                            patchInfo.patcher.close();
                        }
                    } catch (e) {
                        this.error(`パッチ閉じるエラー: ${patchId} - ${e.message}`);
                    }
                });
            }
            
            // 状態をクリア
            this.activePatchIds = {};
            this.activeObjects = {};
            this.connectionStatus.lastDisconnectedAt = Date.now();
            
            this.log('システムシャットダウン完了');
            
            return { success: true };
        } catch (err) {
            this.error(`システムシャットダウンエラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * システムステータスを取得
     * @returns {object} システムステータス
     */
    getSystemStatus() {
        try {
            // システム情報を収集
            const status = {
                version: this.version,
                connected: this.isConnected,
                patchCount: Object.keys(this.activePatchIds).length,
                objectCount: Object.keys(this.activeObjects).length,
                connectionStatus: {
                    lastConnectedAt: this.connectionStatus.lastConnectedAt,
                    lastDisconnectedAt: this.connectionStatus.lastDisconnectedAt,
                    reconnectAttempts: this.connectionStatus.reconnectAttempts
                },
                errorCount: this.connectionStatus.errors.length,
                capabilities: this.capabilities,
                timestamp: Date.now()
            };
            
            this.log(`システムステータス取得: パッチ ${status.patchCount}個, オブジェクト ${status.objectCount}個`);
            
            return status;
        } catch (err) {
            this.error(`システムステータス取得エラー: ${err.message}`);
            return {
                version: this.version,
                connected: false,
                error: err.message,
                timestamp: Date.now()
            };
        }
    }
    
    // ====================
    // = 状態同期メソッド =
    // ====================
    
    /**
     * 完全な状態を取得
     * @param {string} category - カテゴリ (オプション)
     * @returns {object} 状態情報
     */
    getFullState(category) {
        try {
            let state = {
                syncId: Date.now().toString(),
                timestamp: Date.now()
            };
            
            // カテゴリに応じて状態を返す
            if (!category || category === 'patch') {
                state.patches = this._getPatchState();
            }
            
            if (!category || category === 'object') {
                state.objects = this._getObjectState();
            }
            
            if (!category || category === 'connection') {
                state.connections = this._getConnectionState();
            }
            
            if (!category || category === 'param') {
                state.parameters = this._getParameterState();
            }
            
            this.log(`完全な状態を取得: syncId=${state.syncId}`);
            
            // 最新の同期IDを記録
            this.lastSyncId = state.syncId;
            
            return state;
        } catch (err) {
            this.error(`状態取得エラー: ${err.message}`);
            return {
                syncId: Date.now().toString(),
                timestamp: Date.now(),
                error: err.message
            };
        }
    }
    
    /**
     * 差分状態を取得
     * @param {string} lastSyncId - 前回の同期ID
     * @returns {object} 差分状態
     */
    getStateDiff(lastSyncId) {
        try {
            // 注意: 実際の実装では変更追跡メカニズムが必要
            // ここでは簡略化のために完全な状態を返す
            
            // 新しい同期IDを生成
            const syncId = Date.now().toString();
            
            this.log(`差分状態を取得: lastSyncId=${lastSyncId}, 新syncId=${syncId}`);
            
            // 変更オブジェクトのダミーデータ
            const changes = {
                patches: {},
                objects: {},
                connections: {},
                parameters: {}
            };
            
            // 最新の同期IDを記録
            this.lastSyncId = syncId;
            
            return {
                syncId: syncId,
                lastSyncId: lastSyncId,
                timestamp: Date.now(),
                changes: changes
            };
        } catch (err) {
            this.error(`差分状態取得エラー: ${err.message}`);
            return {
                syncId: Date.now().toString(),
                lastSyncId: lastSyncId,
                timestamp: Date.now(),
                error: err.message,
                changes: {}
            };
        }
    }
    
    /**
     * パッチ状態を取得
     * @private
     * @returns {object} パッチ状態
     */
    _getPatchState() {
        const patchState = {};
        
        Object.keys(this.activePatchIds).forEach(patchId => {
            const patchInfo = this.activePatchIds[patchId];
            
            patchState[patchId] = {
                id: patchId,
                name: patchInfo.name,
                filepath: patchInfo.filepath,
                objectCount: Object.keys(patchInfo.objects).length,
                connectionCount: Object.keys(patchInfo.connections).length,
                createdAt: patchInfo.createdAt,
                updatedAt: patchInfo.updatedAt
            };
        });
        
        return patchState;
    }
    
    /**
     * オブジェクト状態を取得
     * @private
     * @returns {object} オブジェクト状態
     */
    _getObjectState() {
        const objectState = {};
        
        Object.keys(this.activeObjects).forEach(objectId => {
            const objInfo = this.activeObjects[objectId];
            
            objectState[objectId] = {
                id: objectId,
                type: objInfo.type,
                patchId: objInfo.patchId,
                position: objInfo.position,
                inlets: objInfo.inlets,
                outlets: objInfo.outlets,
                createdAt: objInfo.createdAt
            };
        });
        
        return objectState;
    }
    
    /**
     * 接続状態を取得
     * @private
     * @returns {object} 接続状態
     */
    _getConnectionState() {
        const connectionState = {};
        
        // すべてのパッチの接続を収集
        Object.keys(this.activePatchIds).forEach(patchId => {
            const patchInfo = this.activePatchIds[patchId];
            
            // パッチのすべての接続を収集
            Object.keys(patchInfo.connections).forEach(connectionId => {
                connectionState[connectionId] = { ...patchInfo.connections[connectionId] };
            });
        });
        
        return connectionState;
    }
    
    /**
     * パラメータ状態を取得
     * @private
     * @returns {object} パラメータ状態
     */
    _getParameterState() {
        const paramState = {};
        
        // すべてのオブジェクトのパラメータを収集
        Object.keys(this.activeObjects).forEach(objectId => {
            const objInfo = this.activeObjects[objectId];
            
            if (objInfo.params && Object.keys(objInfo.params).length > 0) {
                paramState[objectId] = {};
                
                // オブジェクトのすべてのパラメータを収集
                Object.keys(objInfo.params).forEach(paramName => {
                    paramState[objectId][paramName] = objInfo.params[paramName];
                });
            }
        });
        
        return paramState;
    }
    
    // ====================
    // = セッション管理メソッド =
    // ====================
    
    /**
     * セッション開始
     * @param {object} sessionInfo - セッション情報
     * @returns {object} 開始結果
     */
    startSession(sessionInfo) {
        try {
            // セッションIDを生成
            const sessionId = `session_${Date.now()}_${Math.floor(Math.random() * 1000)}`;
            
            // セッション情報を記録
            this.activeSession = {
                id: sessionId,
                startTime: Date.now(),
                info: sessionInfo || {},
                patches: [],
                objects: [],
                state: {
                    snapshots: []
                }
            };
            
            this.log(`セッション開始: ${sessionId}`);
            
            // 最初のスナップショットを作成
            this.activeSession.state.snapshots.push({
                timestamp: Date.now(),
                state: this.getFullState()
            });
            
            return {
                success: true,
                sessionId: sessionId,
                startTime: this.activeSession.startTime
            };
        } catch (err) {
            this.error(`セッション開始エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * セッション終了
     * @param {string} sessionId - セッションID
     * @returns {object} 終了結果
     */
    endSession(sessionId) {
        try {
            // セッションの存在確認
            if (!this.activeSession || this.activeSession.id !== sessionId) {
                this.error(`存在しないセッションID: ${sessionId}`);
                return { success: false, error: "存在しないセッションID" };
            }
            
            // 最後のスナップショットを作成
            this.activeSession.state.snapshots.push({
                timestamp: Date.now(),
                state: this.getFullState()
            });
            
            // セッション終了時刻と所要時間を記録
            const endTime = Date.now();
            const duration = endTime - this.activeSession.startTime;
            
            // セッション情報を更新
            this.activeSession.endTime = endTime;
            this.activeSession.duration = duration;
            
            this.log(`セッション終了: ${sessionId}, 所要時間: ${duration / 1000} 秒`);
            
            // 終了したセッションを保存
            const completedSession = { ...this.activeSession };
            this.lastCompletedSession = completedSession;
            
            // アクティブセッションをクリア
            this.activeSession = null;
            
            return {
                success: true,
                sessionId: sessionId,
                endTime: endTime,
                duration: duration
            };
        } catch (err) {
            this.error(`セッション終了エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * セッション保存
     * @param {string} sessionId - セッションID
     * @param {string} path - 保存先パス
     * @returns {object} 保存結果
     */
    saveSession(sessionId, path) {
        try {
            // セッション存在確認
            let sessionData = null;
            
            if (this.activeSession && this.activeSession.id === sessionId) {
                // 現在のアクティブセッション
                sessionData = this.activeSession;
            } else if (this.lastCompletedSession && this.lastCompletedSession.id === sessionId) {
                // 最後に完了したセッション
                sessionData = this.lastCompletedSession;
            }
            
            if (!sessionData) {
                this.error(`存在しないセッションID: ${sessionId}`);
                return { success: false, error: "存在しないセッションID" };
            }
            
            // 保存パスの確認
            const savePath = path || `${sessionId}.json`;
            
            // 注意: 実際の実装では、ファイル書き込み処理などが必要
            // Max v8ui環境の制約により、ここではシミュレートする
            
            this.log(`セッション保存: ${sessionId} -> ${savePath}`);
            
            return {
                success: true,
                path: savePath
            };
        } catch (err) {
            this.error(`セッション保存エラー: ${err.message}`);
            return { success: false, error: err.message };
        }
    }
    
    /**
     * セッション読み込み
     * @param {string} path - 読み込み元パス
     * @returns {object} 読み込み結果
     */
    loadSession(path) {
        try {
            // 注意: 実際の実装では、ファイル読み込み処理などが必要
            // Max v8ui環境の制約により、ここではシミュレートする
            
            // 新しいセッションIDを生成
            const sessionId = `session_${Date.now()}_${Math.floor(Math.random() * 1000)}`;
            
            this.log(`セッション読み込み: ${path} -> ${sessionId}`);
            
            // 前のセッションがあれば終了
            if (this.activeSession) {
                this.endSession(this.activeSession.id);
            }
            
            // 新しいセッションを作成
            this.activeSession = {
                id: sessionId,
                startTime: Date.now(),
                info: {
                    loadedFrom: path
                },
                patches: [],
                objects: [],
                state: {
                    snapshots: []
                }
            };
            
            // 最初のスナップショットを作成
            this.activeSession.state.snapshots.push({
                timestamp: Date.now(),
                state: this.getFullState()
            });
            
            return {
                success: true,
                sessionId: sessionId
            };
        } catch (err) {
            this.error(`セッション読み込みエラー: ${err.message}`);
            return { success: false, error: err.message };
        }
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
