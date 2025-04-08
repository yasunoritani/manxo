/**
 * parameter_sync.js
 * Max 9とClaude Desktop MCP間のパラメータ同期メカニズムの実装
 * Issue 7の実装
 */

class ParameterSyncManager {
    /**
     * パラメータ同期管理クラス
     * param.oscオブジェクトを活用してMax 9とMCP間のパラメータを双方向同期する
     */
    constructor(bridge, options = {}) {
        this.bridge = bridge;
        this.maxObj = bridge.maxObj;
        this.options = Object.assign({
            syncInterval: 50,      // ミリ秒単位の同期間隔
            batchSize: 10,         // 一度に送信するパラメータの最大数
            retryAttempts: 3,      // 同期失敗時の再試行回数
            retryDelay: 100,       // 再試行の遅延（ミリ秒）
            paramPrefix: "/params" // パラメータOSCアドレスのプレフィックス
        }, options);

        // 監視中のパラメータ
        this.watchedParams = {};
        
        // 変更されたパラメータのキュー
        this.changedParamQueue = [];
        
        // sync用タイマーID
        this.syncTimerId = null;
        
        // 次の同期バッチのスケジュール
        this.scheduleSyncBatch();
        
        this.log("ParameterSyncManager initialized");
    }
    
    /**
     * param.oscオブジェクトの作成とセットアップ
     * @param {string} paramName - パラメータ名
     * @param {number} initialValue - 初期値
     * @param {object} options - パラメータのオプション（min, max, defaultなど）
     * @returns {string} 作成されたparam.oscオブジェクトのID
     */
    createParamOsc(paramName, initialValue, options = {}) {
        const paramId = `param_${paramName.replace(/[^a-zA-Z0-9]/g, '_')}`;
        const defaults = {
            min: 0,
            max: 1,
            default: initialValue || 0,
            type: "float" // float, int, enumの選択肢
        };
        
        const paramOptions = Object.assign({}, defaults, options);
        
        // Max側でparam.oscオブジェクトを作成
        try {
            // オブジェクトがすでに存在するか確認
            if (this.watchedParams[paramId]) {
                this.log(`Parameter ${paramName} already exists, updating instead`);
                this.updateParamValue(paramName, initialValue);
                return paramId;
            }
            
            // param.oscオブジェクトの作成
            const x = 20; // 仮の位置
            const y = 20 + Object.keys(this.watchedParams).length * 40; // 縦に並べる
            
            // オブジェクト作成
            this.maxObj.createObject("param.osc", [x, y, paramName], 
                paramOptions.min, paramOptions.max, paramOptions.default);
            
            // スクリプト変数として設定
            this.maxObj.message("script", "send", "setvar", paramId, 
                Object.keys(this.watchedParams).length);
            
            // OSCアドレスの設定
            const oscAddress = `${this.options.paramPrefix}/${paramName}`;
            this.maxObj.message("script", "send", paramId, "address", oscAddress);
            
            // 型の設定
            if (paramOptions.type === "int") {
                this.maxObj.message("script", "send", paramId, "type", "int");
            } else if (paramOptions.type === "enum") {
                this.maxObj.message("script", "send", paramId, "type", "enum");
                if (paramOptions.enumValues) {
                    // 列挙型の値を設定
                    this.maxObj.message("script", "send", paramId, "enumvals", ...paramOptions.enumValues);
                }
            }
            
            // 監視対象に追加
            this.watchedParams[paramId] = {
                name: paramName,
                oscAddress: oscAddress,
                value: initialValue,
                options: paramOptions,
                lastSync: Date.now(),
                syncAttempts: 0
            };
            
            // パラメータ変更時のコールバックを設定
            this.setupParamChangeCallback(paramId);
            
            this.log(`Created param.osc for ${paramName}`);
            return paramId;
        } catch (error) {
            this.logError(`Failed to create param.osc for ${paramName}`, error);
            return null;
        }
    }
    
    /**
     * パラメータ変更時のコールバックをセットアップ
     * @param {string} paramId - パラメータID
     */
    setupParamChangeCallback(paramId) {
        const param = this.watchedParams[paramId];
        if (!param) return;
        
        // param.oscオブジェクトからの変更通知を受け取るための接続
        this.maxObj.message("script", "connect", paramId, 0, "paramValueChanged", 0);
        
        // 値が変わったときのハンドラを定義
        this.maxObj.message("script", "function", "paramValueChanged", (value, oscAddress) => {
            if (oscAddress === param.oscAddress) {
                // パラメータ値の更新
                const oldValue = param.value;
                param.value = value;
                
                // 変更キューに追加（重複を防ぐ）
                if (!this.changedParamQueue.includes(paramId)) {
                    this.changedParamQueue.push(paramId);
                }
                
                this.log(`Parameter ${param.name} changed: ${oldValue} -> ${value}`);
                
                // MCPサーバーに通知（リアルタイム性が重要な場合）
                if (this.options.immediateSync) {
                    this.syncParamToMCP(paramId);
                }
            }
        });
    }
    
    /**
     * パラメータ値をMax側で更新
     * @param {string} paramName - パラメータ名
     * @param {number|string} value - 新しい値
     * @returns {boolean} 更新が成功したかどうか
     */
    updateParamValue(paramName, value) {
        // パラメータIDを探す
        const paramId = Object.keys(this.watchedParams).find(
            id => this.watchedParams[id].name === paramName
        );
        
        if (!paramId) {
            this.logError(`Parameter ${paramName} not found`);
            return false;
        }
        
        try {
            // パラメータ値を更新
            this.maxObj.message("script", "send", paramId, "set", value);
            this.watchedParams[paramId].value = value;
            this.watchedParams[paramId].lastSync = Date.now();
            
            this.log(`Updated parameter ${paramName} to ${value}`);
            return true;
        } catch (error) {
            this.logError(`Failed to update parameter ${paramName}`, error);
            return false;
        }
    }
    
    /**
     * 単一パラメータをMCPサーバーに同期
     * @param {string} paramId - パラメータID
     * @returns {boolean} 同期が成功したかどうか
     */
    syncParamToMCP(paramId) {
        const param = this.watchedParams[paramId];
        if (!param) return false;
        
        try {
            // OSCを使ってMCPサーバーにパラメータ変更を通知
            const oscAddress = `/mcp/param/update`;
            const success = this.bridge.sendOSC(oscAddress, param.name, param.value);
            
            if (success) {
                // 同期成功をマーク
                param.lastSync = Date.now();
                param.syncAttempts = 0;
                
                // 変更キューから削除
                const index = this.changedParamQueue.indexOf(paramId);
                if (index !== -1) {
                    this.changedParamQueue.splice(index, 1);
                }
                
                return true;
            } else {
                // 同期失敗、再試行カウントを増やす
                param.syncAttempts++;
                
                // 再試行上限を超えたらエラーをログ
                if (param.syncAttempts > this.options.retryAttempts) {
                    this.logError(`Failed to sync parameter ${param.name} after ${param.syncAttempts} attempts`);
                }
                
                return false;
            }
        } catch (error) {
            this.logError(`Error syncing parameter ${param.name}`, error);
            return false;
        }
    }
    
    /**
     * MCPサーバーからのパラメータ更新を処理
     * @param {string} paramName - パラメータ名
     * @param {number|string} value - 新しい値
     */
    handleMCPParamUpdate(paramName, value) {
        this.updateParamValue(paramName, value);
    }
    
    /**
     * 次の同期バッチをスケジュール
     */
    scheduleSyncBatch() {
        if (this.syncTimerId) {
            clearTimeout(this.syncTimerId);
        }
        
        this.syncTimerId = setTimeout(() => {
            this.syncBatch();
            this.scheduleSyncBatch(); // 再スケジュール
        }, this.options.syncInterval);
    }
    
    /**
     * 変更されたパラメータのバッチ同期
     */
    syncBatch() {
        if (this.changedParamQueue.length === 0) return;
        
        // バッチサイズ分のパラメータを同期
        const batchSize = Math.min(this.options.batchSize, this.changedParamQueue.length);
        const batch = this.changedParamQueue.slice(0, batchSize);
        
        // バッチ内の各パラメータを同期
        let syncedCount = 0;
        for (const paramId of batch) {
            if (this.syncParamToMCP(paramId)) {
                syncedCount++;
            }
        }
        
        this.log(`Batch sync: ${syncedCount}/${batchSize} parameters synced`);
    }
    
    /**
     * すべてのパラメータを同期
     */
    syncAllParams() {
        // すべてのパラメータを変更キューに追加
        this.changedParamQueue = Object.keys(this.watchedParams);
        
        // 即時同期実行
        this.syncBatch();
        
        this.log(`Initiated sync for all ${this.changedParamQueue.length} parameters`);
    }
    
    /**
     * 監視中のすべてのパラメータをクリア
     */
    clearAllParams() {
        // 各param.oscオブジェクトを削除
        for (const paramId in this.watchedParams) {
            try {
                this.maxObj.message("script", "delete", paramId);
            } catch (error) {
                this.logError(`Error deleting param.osc ${paramId}`, error);
            }
        }
        
        // 状態をリセット
        this.watchedParams = {};
        this.changedParamQueue = [];
        
        this.log("Cleared all parameters");
    }
    
    /**
     * ログメッセージ
     * @param {string} message - ログメッセージ
     */
    log(message) {
        if (this.bridge && this.bridge.log) {
            this.bridge.log(`[ParamSync] ${message}`);
        } else {
            console.log(`[ParamSync] ${message}`);
        }
    }
    
    /**
     * エラーログ
     * @param {string} message - エラーメッセージ
     * @param {Error} error - エラーオブジェクト（オプション）
     */
    logError(message, error) {
        const errorMsg = error ? `${message}: ${error.message}` : message;
        if (this.bridge && this.bridge.logError) {
            this.bridge.logError(errorMsg, error);
        } else {
            console.error(`[ParamSync] ERROR: ${errorMsg}`);
            if (error && error.stack) {
                console.error(error.stack);
            }
        }
    }
}

// クラスをエクスポート
if (typeof module !== 'undefined' && module.exports) {
    module.exports = ParameterSyncManager;
}
