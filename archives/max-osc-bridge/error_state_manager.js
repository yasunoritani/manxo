/**
 * error_state_manager.js
 * Max 9とClaude Desktop MCP間の階層化されたエラーハンドリングと状態管理システム
 * Issue 8の実装
 */

/**
 * エラータイプの定義
 * 階層化されたエラー分類システム
 */
const ErrorTypes = {
    // 通信関連エラー
    COMMUNICATION: {
        OSC_SEND_FAILED: "osc_send_failed",
        OSC_RECEIVE_FAILED: "osc_receive_failed",
        CONNECTION_LOST: "connection_lost",
        TIMEOUT: "timeout",
        PORT_BUSY: "port_busy"
    },
    
    // Max操作関連エラー
    MAX_OPERATION: {
        OBJECT_CREATION_FAILED: "object_creation_failed",
        PATCH_OPERATION_FAILED: "patch_operation_failed",
        PARAMETER_UPDATE_FAILED: "parameter_update_failed",
        RUNTIME_ERROR: "max_runtime_error",
        MISSING_DEPENDENCY: "missing_dependency"
    },
    
    // MCP関連エラー
    MCP: {
        AUTHENTICATION_FAILED: "authentication_failed",
        INVALID_COMMAND: "invalid_command",
        COMMAND_EXECUTION_FAILED: "command_execution_failed",
        SESSION_EXPIRED: "session_expired",
        PERMISSION_DENIED: "permission_denied"
    },
    
    // システム関連エラー
    SYSTEM: {
        MEMORY_LIMIT: "memory_limit_exceeded",
        RESOURCE_UNAVAILABLE: "resource_unavailable",
        FILE_OPERATION_FAILED: "file_operation_failed",
        INITIALIZATION_FAILED: "initialization_failed",
        UNEXPECTED_ERROR: "unexpected_error"
    }
};

/**
 * システム状態の定義
 */
const SystemStates = {
    // 全体的な接続状態
    CONNECTION: {
        DISCONNECTED: "disconnected",
        CONNECTING: "connecting",
        CONNECTED: "connected",
        RECONNECTING: "reconnecting",
        CONNECTION_ERROR: "connection_error"
    },
    
    // セッション状態
    SESSION: {
        INACTIVE: "inactive",
        INITIALIZING: "initializing",
        ACTIVE: "active",
        CLOSING: "closing",
        ERROR: "error"
    },
    
    // 操作状態
    OPERATION: {
        IDLE: "idle",
        PROCESSING: "processing",
        SUCCESS: "success",
        PARTIAL_SUCCESS: "partial_success",
        FAILED: "failed"
    },
    
    // 同期状態
    SYNC: {
        IN_SYNC: "in_sync",
        SYNCING: "syncing",
        SYNC_ERROR: "sync_error",
        OUTDATED: "outdated"
    }
};

/**
 * エラーの詳細情報を含むエラーオブジェクト
 */
class MCPError extends Error {
    /**
     * @param {string} message - エラーメッセージ
     * @param {string} type - エラータイプ（ErrorTypesから）
     * @param {Object} details - 追加詳細情報
     * @param {Error} originalError - 元のエラーオブジェクト（存在する場合）
     */
    constructor(message, type, details = {}, originalError = null) {
        super(message);
        this.name = "MCPError";
        this.type = type;
        this.details = details;
        this.originalError = originalError;
        this.timestamp = new Date();
        
        // エラー階層の自動判定
        this.category = this.determineCategory();
        
        // スタックトレースの保持
        if (Error.captureStackTrace) {
            Error.captureStackTrace(this, MCPError);
        }
    }
    
    /**
     * エラーカテゴリを決定
     * @returns {string} エラーカテゴリ
     */
    determineCategory() {
        for (const category in ErrorTypes) {
            for (const errorType in ErrorTypes[category]) {
                if (ErrorTypes[category][errorType] === this.type) {
                    return category;
                }
            }
        }
        return "UNKNOWN";
    }
    
    /**
     * エラーの文字列表現
     * @returns {string} フォーマットされたエラー文字列
     */
    toString() {
        return `[${this.category}:${this.type}] ${this.message}`;
    }
    
    /**
     * JSONシリアライズ可能なエラー表現
     * @returns {Object} JSONオブジェクト
     */
    toJSON() {
        return {
            name: this.name,
            message: this.message,
            category: this.category,
            type: this.type,
            details: this.details,
            timestamp: this.timestamp.toISOString(),
            stack: this.stack
        };
    }
}

/**
 * 状態管理とエラーハンドリングのための中央管理システム
 */
class ErrorStateManager {
    /**
     * @param {Object} bridge - MCPBridgeインスタンス
     * @param {Object} options - オプション設定
     */
    constructor(bridge, options = {}) {
        this.bridge = bridge;
        this.options = Object.assign({
            maxErrorHistory: 50,               // 保持するエラー履歴の最大数
            autoReconnect: true,               // 接続エラー時に自動再接続するか
            maxReconnectAttempts: 5,           // 最大再接続試行回数
            reconnectInterval: 2000,           // 再接続間隔（ミリ秒）
            exponentialBackoff: true,          // 指数関数的バックオフを使用するか
            persistStateChanges: true,         // 状態変更を永続化するか
            stateStoragePath: "mcp_state.json" // 状態保存先ファイルパス
        }, options);
        
        // 状態の初期化
        this.state = {
            connection: SystemStates.CONNECTION.DISCONNECTED,
            session: SystemStates.SESSION.INACTIVE,
            operation: SystemStates.OPERATION.IDLE,
            sync: SystemStates.SYNC.IN_SYNC,
            lastStateChange: new Date(),
            sessionId: null,
            reconnectAttempts: 0,
            lastReconnectTime: null,
            pendingOperations: {},
            operationCounter: 0
        };
        
        // エラー履歴
        this.errorHistory = [];
        
        // 状態変更リスナー
        this.stateChangeListeners = {};
        
        // エラーハンドラー
        this.errorHandlers = {
            [ErrorTypes.COMMUNICATION.CONNECTION_LOST]: this.handleConnectionLost.bind(this),
            [ErrorTypes.COMMUNICATION.TIMEOUT]: this.handleTimeout.bind(this),
            // 他のエラーハンドラーをここに追加
        };
        
        this.loadState();
        this.log("ErrorStateManager initialized");
    }
    
    /**
     * エラーを作成して記録
     * @param {string} message - エラーメッセージ
     * @param {string} type - ErrorTypesに定義されたエラータイプ
     * @param {Object} details - 詳細情報
     * @param {Error} originalError - 元のエラー（オプション）
     * @returns {MCPError} 作成されたエラーオブジェクト
     */
    createError(message, type, details = {}, originalError = null) {
        const error = new MCPError(message, type, details, originalError);
        
        // エラー履歴に追加
        this.errorHistory.unshift(error);
        
        // 履歴サイズの制限
        if (this.errorHistory.length > this.options.maxErrorHistory) {
            this.errorHistory = this.errorHistory.slice(0, this.options.maxErrorHistory);
        }
        
        // エラーハンドラーを呼び出す
        this.handleError(error);
        
        return error;
    }
    
    /**
     * エラーを処理
     * @param {MCPError} error - 処理するエラー
     */
    handleError(error) {
        // エラーをログに記録
        this.logError(error.toString(), error);
        
        // 特定タイプのエラー用ハンドラーがあれば実行
        if (this.errorHandlers[error.type]) {
            this.errorHandlers[error.type](error);
        }
        
        // エラーイベントを発火
        this.emitStateChange('error', error);
    }
    
    /**
     * 接続断エラーの処理
     * @param {MCPError} error - 接続断エラー
     */
    handleConnectionLost(error) {
        // 接続状態を更新
        this.updateState({
            connection: SystemStates.CONNECTION.DISCONNECTED
        });
        
        // 自動再接続が有効なら再接続を試みる
        if (this.options.autoReconnect && 
            this.state.reconnectAttempts < this.options.maxReconnectAttempts) {
            
            // 再接続試行回数を増やす
            this.updateState({
                reconnectAttempts: this.state.reconnectAttempts + 1,
                connection: SystemStates.CONNECTION.RECONNECTING
            });
            
            // 再接続間隔の計算（指数関数的バックオフ）
            let reconnectDelay = this.options.reconnectInterval;
            if (this.options.exponentialBackoff) {
                reconnectDelay *= Math.pow(2, this.state.reconnectAttempts - 1);
            }
            
            this.log(`Attempting to reconnect in ${reconnectDelay}ms (attempt ${this.state.reconnectAttempts}/${this.options.maxReconnectAttempts})`);
            
            // 再接続をスケジュール
            setTimeout(() => {
                this.reconnect();
            }, reconnectDelay);
        } else if (this.state.reconnectAttempts >= this.options.maxReconnectAttempts) {
            this.updateState({
                connection: SystemStates.CONNECTION.CONNECTION_ERROR
            });
            this.log("Max reconnect attempts reached, giving up");
        }
    }
    
    /**
     * タイムアウトエラーの処理
     * @param {MCPError} error - タイムアウトエラー
     */
    handleTimeout(error) {
        // タイムアウトした操作があれば失敗として処理
        if (error.details.operationId && this.state.pendingOperations[error.details.operationId]) {
            const operation = this.state.pendingOperations[error.details.operationId];
            
            // 操作を失敗としてマーク
            this.markOperationComplete(error.details.operationId, false, {
                error: error.message,
                errorType: error.type
            });
            
            this.log(`Operation ${operation.name} timed out after ${operation.elapsed}ms`);
        }
    }
    
    /**
     * システム状態を更新
     * @param {Object} newState - 更新する状態プロパティ
     */
    updateState(newState) {
        const oldState = {...this.state};
        
        // 状態を更新
        this.state = {
            ...this.state,
            ...newState,
            lastStateChange: new Date()
        };
        
        // 変更内容をログ
        for (const key in newState) {
            if (oldState[key] !== this.state[key]) {
                this.log(`State change: ${key} = ${this.state[key]} (was: ${oldState[key]})`);
            }
        }
        
        // 状態変更を通知
        for (const key in newState) {
            if (oldState[key] !== this.state[key]) {
                this.emitStateChange(key, this.state[key], oldState[key]);
            }
        }
        
        // 状態を永続化
        if (this.options.persistStateChanges) {
            this.saveState();
        }
    }
    
    /**
     * 操作を開始して追跡
     * @param {string} operationName - 操作名
     * @param {Object} details - 操作の詳細
     * @returns {string} 操作ID
     */
    startOperation(operationName, details = {}) {
        const operationId = `op_${Date.now()}_${this.state.operationCounter++}`;
        
        // 操作情報
        const operation = {
            id: operationId,
            name: operationName,
            details: details,
            startTime: Date.now(),
            status: 'running',
            elapsed: 0
        };
        
        // 操作を追跡
        const pendingOperations = {...this.state.pendingOperations};
        pendingOperations[operationId] = operation;
        
        this.updateState({
            pendingOperations,
            operation: SystemStates.OPERATION.PROCESSING
        });
        
        this.log(`Started operation: ${operationName} (ID: ${operationId})`);
        
        return operationId;
    }
    
    /**
     * 操作の完了をマーク
     * @param {string} operationId - 操作ID
     * @param {boolean} success - 操作が成功したか
     * @param {Object} result - 操作結果
     * @returns {boolean} 操作が存在し、正常に完了マークされたか
     */
    markOperationComplete(operationId, success, result = {}) {
        // 操作が存在するか確認
        if (!this.state.pendingOperations[operationId]) {
            this.log(`Cannot complete unknown operation: ${operationId}`);
            return false;
        }
        
        // 操作情報を取得
        const operation = this.state.pendingOperations[operationId];
        
        // 経過時間を計算
        const elapsed = Date.now() - operation.startTime;
        
        // 操作情報を更新
        const updatedOperation = {
            ...operation,
            status: success ? 'success' : 'failed',
            endTime: Date.now(),
            elapsed,
            result
        };
        
        // 保留中操作リストを更新
        const pendingOperations = {...this.state.pendingOperations};
        pendingOperations[operationId] = updatedOperation;
        
        // アクティブな操作数をカウント
        const activeOperationCount = Object.values(pendingOperations)
            .filter(op => op.status === 'running')
            .length;
        
        // 全体的な操作状態を更新
        let operationState;
        if (activeOperationCount > 0) {
            operationState = SystemStates.OPERATION.PROCESSING;
        } else {
            // 全ての操作が完了
            const failedOperations = Object.values(pendingOperations)
                .filter(op => op.status === 'failed')
                .length;
                
            if (failedOperations === 0) {
                operationState = SystemStates.OPERATION.SUCCESS;
            } else if (failedOperations < Object.keys(pendingOperations).length) {
                operationState = SystemStates.OPERATION.PARTIAL_SUCCESS;
            } else {
                operationState = SystemStates.OPERATION.FAILED;
            }
        }
        
        this.updateState({
            pendingOperations,
            operation: operationState
        });
        
        this.log(`Operation ${operation.name} ${success ? 'succeeded' : 'failed'} after ${elapsed}ms`);
        
        // 完了した操作をクリーンアップ
        setTimeout(() => {
            this.cleanupCompletedOperations();
        }, 5000);
        
        return true;
    }
    
    /**
     * 完了した操作をクリーンアップ
     */
    cleanupCompletedOperations() {
        const pendingOperations = {...this.state.pendingOperations};
        let changed = false;
        
        // 完了してから一定時間経過した操作を削除
        for (const opId in pendingOperations) {
            const operation = pendingOperations[opId];
            if (operation.status !== 'running' && operation.endTime) {
                const age = Date.now() - operation.endTime;
                if (age > 5000) { // 5秒以上経過した操作を削除
                    delete pendingOperations[opId];
                    changed = true;
                }
            }
        }
        
        // 変更があった場合のみ状態を更新
        if (changed) {
            this.updateState({pendingOperations});
        }
    }
    
    /**
     * 状態変更リスナーを追加
     * @param {string} stateKey - 監視する状態キー
     * @param {Function} listener - 呼び出されるリスナー関数
     */
    addStateChangeListener(stateKey, listener) {
        if (!this.stateChangeListeners[stateKey]) {
            this.stateChangeListeners[stateKey] = [];
        }
        
        this.stateChangeListeners[stateKey].push(listener);
    }
    
    /**
     * 状態変更リスナーを削除
     * @param {string} stateKey - 監視している状態キー
     * @param {Function} listener - 削除するリスナー関数
     */
    removeStateChangeListener(stateKey, listener) {
        if (!this.stateChangeListeners[stateKey]) return;
        
        const index = this.stateChangeListeners[stateKey].indexOf(listener);
        if (index !== -1) {
            this.stateChangeListeners[stateKey].splice(index, 1);
        }
    }
    
    /**
     * 状態変更イベントを発火
     * @param {string} stateKey - 変更された状態キー
     * @param {*} newValue - 新しい値
     * @param {*} oldValue - 古い値
     */
    emitStateChange(stateKey, newValue, oldValue) {
        // 特定の状態キーのリスナーを呼び出す
        if (this.stateChangeListeners[stateKey]) {
            for (const listener of this.stateChangeListeners[stateKey]) {
                try {
                    listener(newValue, oldValue, stateKey);
                } catch (error) {
                    this.logError(`Error in state change listener for ${stateKey}`, error);
                }
            }
        }
        
        // 'all'リスナーを呼び出す
        if (this.stateChangeListeners['all']) {
            for (const listener of this.stateChangeListeners['all']) {
                try {
                    listener(stateKey, newValue, oldValue);
                } catch (error) {
                    this.logError(`Error in 'all' state change listener`, error);
                }
            }
        }
    }
    
    /**
     * 再接続を試みる
     */
    reconnect() {
        this.log(`Attempting to reconnect (attempt ${this.state.reconnectAttempts}/${this.options.maxReconnectAttempts})`);
        
        this.updateState({
            lastReconnectTime: new Date()
        });
        
        // 実際の再接続処理（bridgeのconnect関数を呼び出す想定）
        if (this.bridge && typeof this.bridge.connect === 'function') {
            try {
                this.bridge.connect();
            } catch (error) {
                // 再接続失敗
                this.createError(
                    "Failed to reconnect",
                    ErrorTypes.COMMUNICATION.CONNECTION_LOST,
                    { reconnectAttempt: this.state.reconnectAttempts },
                    error
                );
            }
        }
    }
    
    /**
     * 接続成功を通知
     */
    notifyConnectionSuccess() {
        this.updateState({
            connection: SystemStates.CONNECTION.CONNECTED,
            reconnectAttempts: 0,
            lastReconnectTime: null
        });
    }
    
    /**
     * 状態を永続化
     */
    saveState() {
        if (!this.bridge || !this.bridge.maxObj) return;
        
        try {
            // 保存すべき状態のみ抽出
            const stateToSave = {
                connection: this.state.connection,
                session: this.state.session,
                sessionId: this.state.sessionId,
                lastStateChange: this.state.lastStateChange.toISOString()
            };
            
            // JSONに変換
            const stateJson = JSON.stringify(stateToSave);
            
            // Maxのdictオブジェクトに保存
            this.bridge.maxObj.message("script", "send", "stateDict", "clear");
            
            for (const key in stateToSave) {
                let value = stateToSave[key];
                if (typeof value === 'object') {
                    value = JSON.stringify(value);
                }
                this.bridge.maxObj.message("script", "send", "stateDict", "replace", key, value);
            }
            
            // ファイルに保存
            this.bridge.maxObj.message("script", "send", "stateDict", "write", this.options.stateStoragePath);
            
        } catch (error) {
            this.logError("Failed to save state", error);
        }
    }
    
    /**
     * 永続化された状態をロード
     */
    loadState() {
        if (!this.bridge || !this.bridge.maxObj) return;
        
        try {
            // Maxのdictオブジェクトから読み込み
            this.bridge.maxObj.message("script", "send", "stateDict", "read", this.options.stateStoragePath);
            
            // 読み込んだ状態をJSオブジェクトに変換
            // 注: これは実際には非同期に行われるため、
            // 状態が読み込まれた後にイベントハンドラーでstateを更新する必要がある
            
            this.log("Loaded saved state");
        } catch (error) {
            this.logError("Failed to load state", error);
        }
    }
    
    /**
     * クラッシュ後の状態復元
     */
    recoverFromCrash() {
        this.log("Attempting to recover from crash");
        
        // 保存された状態をロード
        this.loadState();
        
        // 接続状態をチェック
        if (this.state.connection === SystemStates.CONNECTION.CONNECTED) {
            // セッションが有効か確認
            const operationId = this.startOperation("verify_session");
            
            // セッション検証コマンドを送信
            try {
                if (this.bridge && typeof this.bridge.verifySession === 'function') {
                    this.bridge.verifySession(
                        this.state.sessionId,
                        (success, response) => {
                            if (success) {
                                // セッションが有効
                                this.log("Successfully recovered session");
                                this.markOperationComplete(operationId, true, response);
                            } else {
                                // セッションが無効、再接続が必要
                                this.log("Session is invalid, reconnecting");
                                this.markOperationComplete(operationId, false, response);
                                
                                // 状態をリセットして再接続
                                this.updateState({
                                    connection: SystemStates.CONNECTION.DISCONNECTED,
                                    session: SystemStates.SESSION.INACTIVE,
                                    sessionId: null
                                });
                                
                                if (this.options.autoReconnect) {
                                    this.reconnect();
                                }
                            }
                        }
                    );
                }
            } catch (error) {
                // エラーを記録して再接続を試みる
                this.createError(
                    "Failed to verify session during recovery",
                    ErrorTypes.MCP.SESSION_EXPIRED,
                    { sessionId: this.state.sessionId },
                    error
                );
                
                this.markOperationComplete(operationId, false, { error: error.message });
                
                // 状態をリセット
                this.updateState({
                    connection: SystemStates.CONNECTION.DISCONNECTED,
                    session: SystemStates.SESSION.INACTIVE,
                    sessionId: null
                });
                
                if (this.options.autoReconnect) {
                    this.reconnect();
                }
            }
        }
    }
    
    /**
     * 完全な現在の状態を取得
     * @returns {Object} 現在の状態オブジェクト
     */
    getFullState() {
        return {...this.state};
    }
    
    /**
     * エラー履歴を取得
     * @param {number} limit - 取得するエラーの最大数
     * @returns {Array} エラー履歴の配列
     */
    getErrorHistory(limit = this.options.maxErrorHistory) {
        return this.errorHistory.slice(0, limit);
    }
    
    /**
     * ステータスサマリーを取得
     * @returns {Object} システムステータスのサマリー
     */
    getStatusSummary() {
        // アクティブな操作の数
        const activeOperations = Object.values(this.state.pendingOperations)
            .filter(op => op.status === 'running')
            .length;
            
        // 最新のエラー
        const latestError = this.errorHistory.length > 0 ? this.errorHistory[0] : null;
        
        return {
            connection: this.state.connection,
            session: this.state.session,
            operation: this.state.operation,
            sync: this.state.sync,
            activeOperations,
            reconnectAttempts: this.state.reconnectAttempts,
            sessionId: this.state.sessionId,
            lastStateChange: this.state.lastStateChange,
            latestError: latestError ? {
                message: latestError.message,
                type: latestError.type,
                category: latestError.category,
                timestamp: latestError.timestamp
            } : null
        };
    }
    
    /**
     * ログメッセージ
     * @param {string} message - ログメッセージ
     */
    log(message) {
        if (this.bridge && this.bridge.log) {
            this.bridge.log(`[ErrorStateManager] ${message}`);
        } else {
            console.log(`[ErrorStateManager] ${message}`);
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
            console.error(`[ErrorStateManager] ERROR: ${errorMsg}`);
            if (error && error.stack) {
                console.error(error.stack);
            }
        }
    }
}

// エクスポート
if (typeof module !== 'undefined' && module.exports) {
    module.exports = {
        ErrorStateManager,
        ErrorTypes,
        SystemStates,
        MCPError
    };
}
