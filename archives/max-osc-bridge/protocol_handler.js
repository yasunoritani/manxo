/**
 * protocol_handler.js
 * Max 9 - Claude Desktop MCP連携用プロトコルハンドラー
 * Issue 3: 通信プロトコルとメッセージング仕様の定義
 */

/**
 * プロトコルハンドラークラス
 * OSCメッセージの受信と処理、および応答の送信を担当
 */
class ProtocolHandler {
  constructor(bridge) {
    this.bridge = bridge;
    this.config = bridge.config;
    this.logger = bridge.logger;
    this.pendingRequests = new Map();
    this.debugMode = false;
    
    // OSCアドレスパターン定義
    this.OSC_PATTERNS = {
      // 受信パターン
      RECEIVE: {
        PATCH: '/mcp/patch',
        OBJECT: '/mcp/object',
        PARAM: '/mcp/param',
        SYSTEM: '/mcp/system',
        STATE: '/mcp/state',
        SESSION: '/mcp/session'
      },
      // 送信パターン
      SEND: {
        RESPONSE: '/max/response',
        ERROR: '/max/error',
        STATE: '/max/state',
        PARAMS: '/max/params'
      }
    };
    
    // エラーコード定義
    this.ERROR_CODES = {
      // 通信エラー (100-199)
      COMMUNICATION_ERROR: 100,
      INVALID_ADDRESS: 104,
      INVALID_ARGUMENTS: 105,
      
      // パッチエラー (200-299)
      PATCH_ERROR: 200,
      PATCH_NOT_FOUND: 201,
      PATCH_CREATION_FAILED: 203,
      PATCH_SAVE_FAILED: 204,
      PATCH_LOAD_FAILED: 205,
      PATCH_CLOSE_FAILED: 206,
      
      // オブジェクトエラー (300-399)
      OBJECT_ERROR: 300,
      OBJECT_NOT_FOUND: 301,
      OBJECT_CREATION_FAILED: 302,
      
      // パラメータエラー (400-499)
      PARAMETER_ERROR: 400,
      PARAMETER_NOT_FOUND: 401,
      
      // システムエラー (500-599)
      SYSTEM_ERROR: 500,
      NOT_IMPLEMENTED: 508
    };
  }

  /**
   * 初期化
   */
  init() {
    this.setupMessageHandlers();
    this.logger.info('Protocol handler initialized');
    return this;
  }

  /**
   * メッセージハンドラーのセットアップ
   */
  setupMessageHandlers() {
    // パッチ操作
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.PATCH}/create`, this.handlePatchCreate.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.PATCH}/open`, this.handlePatchOpen.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.PATCH}/save`, this.handlePatchSave.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.PATCH}/close`, this.handlePatchClose.bind(this));
    
    // オブジェクト操作
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.OBJECT}/create`, this.handleObjectCreate.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.OBJECT}/delete`, this.handleObjectDelete.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.OBJECT}/connect`, this.handleObjectConnect.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.OBJECT}/disconnect`, this.handleObjectDisconnect.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.OBJECT}/move`, this.handleObjectMove.bind(this));
    
    // パラメータ操作
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.PARAM}/set`, this.handleParamSet.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.PARAM}/get`, this.handleParamGet.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.PARAM}/watch`, this.handleParamWatch.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.PARAM}/unwatch`, this.handleParamUnwatch.bind(this));
    
    // システム操作
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.SYSTEM}/init`, this.handleSystemInit.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.SYSTEM}/shutdown`, this.handleSystemShutdown.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.SYSTEM}/status`, this.handleSystemStatus.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.SYSTEM}/ping`, this.handleSystemPing.bind(this));
    
    // 状態同期
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.STATE}/sync`, this.handleStateSync.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.STATE}/diff/sync`, this.handleStateDiffSync.bind(this));
    
    // セッション管理
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.SESSION}/start`, this.handleSessionStart.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.SESSION}/end`, this.handleSessionEnd.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.SESSION}/save`, this.handleSessionSave.bind(this));
    this.bridge.registerOscCallback(`${this.OSC_PATTERNS.RECEIVE.SESSION}/load`, this.handleSessionLoad.bind(this));
  }
  /**
   * レスポンス送信
   * @param {string} category - カテゴリ
   * @param {string} action - アクション
   * @param {string} requestId - リクエストID
   * @param {*} data - レスポンスデータ
   */
  sendResponse(category, action, requestId, data) {
    const address = `${this.OSC_PATTERNS.SEND.RESPONSE}/${category}/${action}`;
    const args = [requestId];
    
    if (data !== undefined) {
      if (typeof data === 'object') {
        args.push(JSON.stringify(data));
      } else {
        args.push(data);
      }
    }
    
    this.bridge.sendOscMessage(address, args);
    
    if (this.debugMode) {
      this.logger.debug(`Response sent: ${address} ${JSON.stringify(args)}`);
    }
  }

  /**
   * エラー送信
   * @param {string} category - カテゴリ
   * @param {string} action - アクション
   * @param {string} requestId - リクエストID
   * @param {number} errorCode - エラーコード
   * @param {string} errorMessage - エラーメッセージ
   */
  sendError(category, action, requestId, errorCode, errorMessage) {
    const address = `${this.OSC_PATTERNS.SEND.ERROR}/${category}/${action}`;
    const args = [requestId, errorCode, errorMessage];
    
    this.bridge.sendOscMessage(address, args);
    this.logger.error(`Error sent: [${errorCode}] ${errorMessage}`);
  }

  /**
   * パッチ作成処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handlePatchCreate(address, args) {
    const requestId = args[0];
    const name = args[1];
    
    if (!name) {
      this.sendError('patch', 'create', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 'Missing patch name');
      return;
    }
    
    try {
      // パッチ作成処理
      const patcher = this.bridge.createPatch(name);
      
      if (patcher) {
        // 成功時のレスポンス
        this.sendResponse('patch', 'create', requestId, {
          id: patcher.id,
          name: patcher.name,
          path: patcher.filepath || ''
        });
        
        // 状態変更通知
        this.sendStateUpdate('patch', 'created', patcher.id, {
          name: patcher.name,
          path: patcher.filepath || ''
        });
      } else {
        this.sendError('patch', 'create', requestId, this.ERROR_CODES.PATCH_CREATION_FAILED, 'Failed to create patch');
      }
    } catch (err) {
      this.logger.error(`Create patch error: ${err.message}`);
      this.sendError('patch', 'create', requestId, this.ERROR_CODES.PATCH_CREATION_FAILED, err.message);
    }
  }
  
  /**
   * パッチ開く処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handlePatchOpen(address, args) {
    const requestId = args[0];
    const path = args[1];
    
    if (!path) {
      this.sendError('patch', 'open', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 'Missing patch path');
      return;
    }
    
    try {
      // パッチを開く処理
      const patcher = this.bridge.openPatch(path);
      
      if (patcher) {
        // 成功時のレスポンス
        this.sendResponse('patch', 'open', requestId, {
          id: patcher.id,
          name: patcher.name,
          path: path
        });
        
        // 状態変更通知
        this.sendStateUpdate('patch', 'opened', patcher.id, {
          name: patcher.name,
          path: path
        });
      } else {
        this.sendError('patch', 'open', requestId, this.ERROR_CODES.PATCH_LOAD_FAILED, 'Failed to open patch');
      }
    } catch (err) {
      this.logger.error(`Open patch error: ${err.message}`);
      this.sendError('patch', 'open', requestId, this.ERROR_CODES.PATCH_LOAD_FAILED, err.message);
    }
  }
  
  /**
   * パッチ保存処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handlePatchSave(address, args) {
    const requestId = args[0];
    const patchId = args[1];
    const path = args[2]; // オプション
    
    if (!patchId) {
      this.sendError('patch', 'save', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 'Missing patch ID');
      return;
    }
    
    try {
      // パッチの保存処理
      const result = this.bridge.savePatch(patchId, path);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('patch', 'save', requestId, {
          id: patchId,
          path: result.path
        });
        
        // 状態変更通知
        this.sendStateUpdate('patch', 'saved', patchId, {
          path: result.path
        });
      } else {
        this.sendError('patch', 'save', requestId, this.ERROR_CODES.PATCH_SAVE_FAILED, result.error || 'Failed to save patch');
      }
    } catch (err) {
      this.logger.error(`Save patch error: ${err.message}`);
      this.sendError('patch', 'save', requestId, this.ERROR_CODES.PATCH_SAVE_FAILED, err.message);
    }
  }
  
  /**
   * パッチ閉じる処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handlePatchClose(address, args) {
    const requestId = args[0];
    const patchId = args[1];
    
    if (!patchId) {
      this.sendError('patch', 'close', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 'Missing patch ID');
      return;
    }
    
    try {
      // パッチを閉じる処理
      const result = this.bridge.closePatch(patchId);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('patch', 'close', requestId, {
          id: patchId
        });
        
        // 状態変更通知
        this.sendStateUpdate('patch', 'closed', patchId, {});
      } else {
        this.sendError('patch', 'close', requestId, this.ERROR_CODES.PATCH_CLOSE_FAILED, result.error || 'Failed to close patch');
      }
    } catch (err) {
      this.logger.error(`Close patch error: ${err.message}`);
      this.sendError('patch', 'close', requestId, this.ERROR_CODES.PATCH_CLOSE_FAILED, err.message);
    }
  }
  
  /**
   * 状態更新通知の送信
   * @param {string} category - カテゴリ
   * @param {string} eventType - イベントタイプ
   * @param {string} objectId - オブジェクトID
   * @param {*} data - 状態データ
   */
  sendStateUpdate(category, eventType, objectId, data) {
    const address = `${this.OSC_PATTERNS.SEND.STATE}/${category}/${eventType}`;
    const dataStr = typeof data === 'object' ? JSON.stringify(data) : data;
    const args = [objectId, dataStr];
    
    this.bridge.sendOscMessage(address, args);
    
    if (this.debugMode) {
      this.logger.debug(`State update sent: ${address} ${objectId}`);
    }
  }

  /**
   * パラメータ更新通知の送信
   * @param {string} objectId - オブジェクトID
   * @param {string} paramName - パラメータ名
   * @param {*} value - パラメータ値
   */
  sendParamUpdate(objectId, paramName, value) {
    const address = this.OSC_PATTERNS.SEND.PARAMS;
    const args = [objectId, paramName, value];
    
    this.bridge.sendOscMessage(address, args);
    
    if (this.debugMode) {
      this.logger.debug(`Parameter update sent: ${objectId}.${paramName} = ${value}`);
    }
  }

  /**
   * オブジェクト作成処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleObjectCreate(address, args) {
    const requestId = args[0];
    const patchId = args[1];
    const objType = args[2];
    const x = args[3] ? parseFloat(args[3]) : 0;
    const y = args[4] ? parseFloat(args[4]) : 0;
    
    if (!patchId || !objType) {
      this.sendError('object', 'create', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 
                    'Missing required arguments: patchId and objType');
      return;
    }
    
    try {
      // オブジェクト作成処理
      const result = this.bridge.createObject(patchId, objType, x, y);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('object', 'create', requestId, {
          patchId: patchId,
          objectId: result.objectId,
          type: objType,
          position: { x, y }
        });
        
        // 状態変更通知
        this.sendStateUpdate('object', 'created', result.objectId, {
          patchId: patchId,
          type: objType,
          position: { x, y }
        });
      } else {
        this.sendError('object', 'create', requestId, this.ERROR_CODES.OBJECT_CREATION_FAILED, 
                      result.error || 'Failed to create object');
      }
    } catch (err) {
      this.logger.error(`Create object error: ${err.message}`);
      this.sendError('object', 'create', requestId, this.ERROR_CODES.OBJECT_CREATION_FAILED, err.message);
    }
  }
  
  /**
   * オブジェクト削除処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleObjectDelete(address, args) {
    const requestId = args[0];
    const patchId = args[1];
    const objectId = args[2];
    
    if (!patchId || !objectId) {
      this.sendError('object', 'delete', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 
                    'Missing required arguments: patchId and objectId');
      return;
    }
    
    try {
      // オブジェクト削除処理
      const result = this.bridge.deleteObject(patchId, objectId);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('object', 'delete', requestId, {
          patchId: patchId,
          objectId: objectId
        });
        
        // 状態変更通知
        this.sendStateUpdate('object', 'deleted', objectId, {
          patchId: patchId
        });
      } else {
        this.sendError('object', 'delete', requestId, this.ERROR_CODES.OBJECT_NOT_FOUND, 
                      result.error || 'Failed to delete object');
      }
    } catch (err) {
      this.logger.error(`Delete object error: ${err.message}`);
      this.sendError('object', 'delete', requestId, this.ERROR_CODES.OBJECT_ERROR, err.message);
    }
  }
  
  /**
   * オブジェクト接続処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleObjectConnect(address, args) {
    const requestId = args[0];
    const patchId = args[1];
    const sourceId = args[2];
    const sourceOutlet = args[3] ? parseInt(args[3], 10) : 0;
    const destId = args[4];
    const destInlet = args[5] ? parseInt(args[5], 10) : 0;
    
    if (!patchId || !sourceId || !destId) {
      this.sendError('object', 'connect', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 
                    'Missing required arguments: patchId, sourceId, and destId');
      return;
    }
    
    try {
      // オブジェクト接続処理
      const result = this.bridge.connectObjects(patchId, sourceId, sourceOutlet, destId, destInlet);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('object', 'connect', requestId, {
          patchId: patchId,
          sourceId: sourceId,
          sourceOutlet: sourceOutlet,
          destId: destId,
          destInlet: destInlet,
          connectionId: result.connectionId
        });
        
        // 状態変更通知
        this.sendStateUpdate('connection', 'created', result.connectionId, {
          patchId: patchId,
          sourceId: sourceId,
          sourceOutlet: sourceOutlet,
          destId: destId,
          destInlet: destInlet
        });
      } else {
        this.sendError('object', 'connect', requestId, this.ERROR_CODES.OBJECT_ERROR, 
                      result.error || 'Failed to connect objects');
      }
    } catch (err) {
      this.logger.error(`Connect objects error: ${err.message}`);
      this.sendError('object', 'connect', requestId, this.ERROR_CODES.OBJECT_ERROR, err.message);
    }
  }
  
  /**
   * オブジェクト切断処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleObjectDisconnect(address, args) {
    const requestId = args[0];
    const patchId = args[1];
    const connectionId = args[2];
    
    // 代替引数: 接続IDの代わりにソースとデスティネーション情報で切断する場合
    const sourceId = connectionId ? null : args[2];
    const sourceOutlet = connectionId ? null : (args[3] ? parseInt(args[3], 10) : 0);
    const destId = connectionId ? null : args[4];
    const destInlet = connectionId ? null : (args[5] ? parseInt(args[5], 10) : 0);
    
    if (!patchId || (!connectionId && (!sourceId || !destId))) {
      this.sendError('object', 'disconnect', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 
                    'Missing required arguments: either connectionId or (sourceId and destId)');
      return;
    }
    
    try {
      // オブジェクト切断処理
      let result;
      if (connectionId) {
        result = this.bridge.disconnectByConnectionId(patchId, connectionId);
      } else {
        result = this.bridge.disconnectObjects(patchId, sourceId, sourceOutlet, destId, destInlet);
      }
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('object', 'disconnect', requestId, {
          patchId: patchId,
          connectionId: connectionId || result.connectionId
        });
        
        // 状態変更通知
        this.sendStateUpdate('connection', 'deleted', connectionId || result.connectionId, {
          patchId: patchId
        });
      } else {
        this.sendError('object', 'disconnect', requestId, this.ERROR_CODES.OBJECT_ERROR, 
                      result.error || 'Failed to disconnect objects');
      }
    } catch (err) {
      this.logger.error(`Disconnect objects error: ${err.message}`);
      this.sendError('object', 'disconnect', requestId, this.ERROR_CODES.OBJECT_ERROR, err.message);
    }
  }
  
  /**
   * オブジェクト移動処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleObjectMove(address, args) {
    const requestId = args[0];
    const patchId = args[1];
    const objectId = args[2];
    const x = args[3] ? parseFloat(args[3]) : null;
    const y = args[4] ? parseFloat(args[4]) : null;
    
    if (!patchId || !objectId || (x === null && y === null)) {
      this.sendError('object', 'move', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 
                    'Missing required arguments: patchId, objectId, and position');
      return;
    }
    
    try {
      // オブジェクト移動処理
      const result = this.bridge.moveObject(patchId, objectId, x, y);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('object', 'move', requestId, {
          patchId: patchId,
          objectId: objectId,
          position: { x, y }
        });
        
        // 状態変更通知
        this.sendStateUpdate('object', 'moved', objectId, {
          patchId: patchId,
          position: { x, y }
        });
      } else {
        this.sendError('object', 'move', requestId, this.ERROR_CODES.OBJECT_NOT_FOUND, 
                      result.error || 'Failed to move object');
      }
    } catch (err) {
      this.logger.error(`Move object error: ${err.message}`);
      this.sendError('object', 'move', requestId, this.ERROR_CODES.OBJECT_ERROR, err.message);
    }
  }
  
  /**
   * パラメータ設定処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleParamSet(address, args) {
    const requestId = args[0];
    const objectId = args[1];
    const paramName = args[2];
    const paramValue = args[3];
    
    if (!objectId || !paramName) {
      this.sendError('param', 'set', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 
                    'Missing required arguments: objectId and paramName');
      return;
    }
    
    try {
      // パラメータ設定処理
      const result = this.bridge.setObjectParam(objectId, paramName, paramValue);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('param', 'set', requestId, {
          objectId: objectId,
          paramName: paramName,
          value: paramValue
        });
        
        // 状態変更通知
        this.sendParamUpdate(objectId, paramName, paramValue);
      } else {
        this.sendError('param', 'set', requestId, this.ERROR_CODES.PARAMETER_ERROR, 
                      result.error || 'Failed to set parameter');
      }
    } catch (err) {
      this.logger.error(`Set parameter error: ${err.message}`);
      this.sendError('param', 'set', requestId, this.ERROR_CODES.PARAMETER_ERROR, err.message);
    }
  }
  
  /**
   * パラメータ取得処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleParamGet(address, args) {
    const requestId = args[0];
    const objectId = args[1];
    const paramName = args[2];
    
    if (!objectId || !paramName) {
      this.sendError('param', 'get', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 
                    'Missing required arguments: objectId and paramName');
      return;
    }
    
    try {
      // パラメータ取得処理
      const result = this.bridge.getObjectParam(objectId, paramName);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('param', 'get', requestId, {
          objectId: objectId,
          paramName: paramName,
          value: result.value
        });
      } else {
        this.sendError('param', 'get', requestId, this.ERROR_CODES.PARAMETER_NOT_FOUND, 
                      result.error || 'Failed to get parameter');
      }
    } catch (err) {
      this.logger.error(`Get parameter error: ${err.message}`);
      this.sendError('param', 'get', requestId, this.ERROR_CODES.PARAMETER_ERROR, err.message);
    }
  }
  
  /**
   * パラメータウォッチ処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleParamWatch(address, args) {
    const requestId = args[0];
    const objectId = args[1];
    const paramName = args[2];
    
    if (!objectId || !paramName) {
      this.sendError('param', 'watch', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 
                    'Missing required arguments: objectId and paramName');
      return;
    }
    
    try {
      // パラメータウォッチ処理
      const result = this.bridge.watchParam(objectId, paramName);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('param', 'watch', requestId, {
          objectId: objectId,
          paramName: paramName,
          watchId: result.watchId
        });
      } else {
        this.sendError('param', 'watch', requestId, this.ERROR_CODES.PARAMETER_ERROR, 
                      result.error || 'Failed to watch parameter');
      }
    } catch (err) {
      this.logger.error(`Watch parameter error: ${err.message}`);
      this.sendError('param', 'watch', requestId, this.ERROR_CODES.PARAMETER_ERROR, err.message);
    }
  }
  
  /**
   * パラメータウォッチ解除処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleParamUnwatch(address, args) {
    const requestId = args[0];
    const watchId = args[1];
    const objectId = watchId ? null : args[1];
    const paramName = watchId ? null : args[2];
    
    if (!watchId && (!objectId || !paramName)) {
      this.sendError('param', 'unwatch', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 
                    'Missing required arguments: either watchId or (objectId and paramName)');
      return;
    }
    
    try {
      // パラメータウォッチ解除処理
      let result;
      if (watchId) {
        result = this.bridge.unwatchParamById(watchId);
      } else {
        result = this.bridge.unwatchParam(objectId, paramName);
      }
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('param', 'unwatch', requestId, {
          watchId: watchId || result.watchId
        });
      } else {
        this.sendError('param', 'unwatch', requestId, this.ERROR_CODES.PARAMETER_ERROR, 
                      result.error || 'Failed to unwatch parameter');
      }
    } catch (err) {
      this.logger.error(`Unwatch parameter error: ${err.message}`);
      this.sendError('param', 'unwatch', requestId, this.ERROR_CODES.PARAMETER_ERROR, err.message);
    }
  }
  
  /**
   * システム初期化処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleSystemInit(address, args) {
    const requestId = args[0];
    const clientInfo = args[1] ? JSON.parse(args[1]) : {};
    
    try {
      // システム初期化処理
      const result = this.bridge.initializeSystem(clientInfo);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('system', 'init', requestId, {
          version: this.bridge.version,
          status: 'initialized',
          capabilities: this.bridge.capabilities
        });
      } else {
        this.sendError('system', 'init', requestId, this.ERROR_CODES.SYSTEM_ERROR, 
                      result.error || 'Failed to initialize system');
      }
    } catch (err) {
      this.logger.error(`System init error: ${err.message}`);
      this.sendError('system', 'init', requestId, this.ERROR_CODES.SYSTEM_ERROR, err.message);
    }
  }
  
  /**
   * システムシャットダウン処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleSystemShutdown(address, args) {
    const requestId = args[0];
    
    try {
      // システムシャットダウン処理
      const result = this.bridge.shutdownSystem();
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('system', 'shutdown', requestId, {
          status: 'shutdown'
        });
      } else {
        this.sendError('system', 'shutdown', requestId, this.ERROR_CODES.SYSTEM_ERROR, 
                      result.error || 'Failed to shutdown system');
      }
    } catch (err) {
      this.logger.error(`System shutdown error: ${err.message}`);
      this.sendError('system', 'shutdown', requestId, this.ERROR_CODES.SYSTEM_ERROR, err.message);
    }
  }
  
  /**
   * システムステータス取得処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleSystemStatus(address, args) {
    const requestId = args[0];
    
    try {
      // システムステータス取得処理
      const status = this.bridge.getSystemStatus();
      
      // レスポンス送信
      this.sendResponse('system', 'status', requestId, status);
    } catch (err) {
      this.logger.error(`System status error: ${err.message}`);
      this.sendError('system', 'status', requestId, this.ERROR_CODES.SYSTEM_ERROR, err.message);
    }
  }
  
  /**
   * システムピング処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleSystemPing(address, args) {
    const requestId = args[0];
    const timestamp = args[1] || Date.now();
    
    // ピングに対して即座にポングを返す
    this.sendResponse('system', 'ping', requestId, {
      timestamp: timestamp,
      response_time: Date.now() - parseInt(timestamp, 10)
    });
  }
  
  /**
   * 状態同期処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleStateSync(address, args) {
    const requestId = args[0];
    const category = args[1]; // オプション: patch, object, param
    
    try {
      // 状態同期処理
      const state = this.bridge.getFullState(category);
      
      // レスポンス送信
      this.sendResponse('state', 'sync', requestId, state);
    } catch (err) {
      this.logger.error(`State sync error: ${err.message}`);
      this.sendError('state', 'sync', requestId, this.ERROR_CODES.SYSTEM_ERROR, err.message);
    }
  }
  
  /**
   * 差分状態同期処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleStateDiffSync(address, args) {
    const requestId = args[0];
    const lastSyncId = args[1];
    
    if (!lastSyncId) {
      this.sendError('state', 'diff/sync', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 'Missing lastSyncId');
      return;
    }
    
    try {
      // 差分状態同期処理
      const diff = this.bridge.getStateDiff(lastSyncId);
      
      // レスポンス送信
      this.sendResponse('state', 'diff/sync', requestId, {
        syncId: diff.syncId,
        changes: diff.changes
      });
    } catch (err) {
      this.logger.error(`State diff sync error: ${err.message}`);
      this.sendError('state', 'diff/sync', requestId, this.ERROR_CODES.SYSTEM_ERROR, err.message);
    }
  }
  
  /**
   * セッション開始処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleSessionStart(address, args) {
    const requestId = args[0];
    const sessionInfo = args[1] ? JSON.parse(args[1]) : {};
    
    try {
      // セッション開始処理
      const result = this.bridge.startSession(sessionInfo);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('session', 'start', requestId, {
          sessionId: result.sessionId,
          startTime: result.startTime
        });
        
        // 状態変更通知
        this.sendStateUpdate('session', 'started', result.sessionId, {
          startTime: result.startTime
        });
      } else {
        this.sendError('session', 'start', requestId, this.ERROR_CODES.SYSTEM_ERROR, 
                      result.error || 'Failed to start session');
      }
    } catch (err) {
      this.logger.error(`Session start error: ${err.message}`);
      this.sendError('session', 'start', requestId, this.ERROR_CODES.SYSTEM_ERROR, err.message);
    }
  }
  
  /**
   * セッション終了処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleSessionEnd(address, args) {
    const requestId = args[0];
    const sessionId = args[1];
    
    if (!sessionId) {
      this.sendError('session', 'end', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 'Missing sessionId');
      return;
    }
    
    try {
      // セッション終了処理
      const result = this.bridge.endSession(sessionId);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('session', 'end', requestId, {
          sessionId: sessionId,
          endTime: result.endTime,
          duration: result.duration
        });
        
        // 状態変更通知
        this.sendStateUpdate('session', 'ended', sessionId, {
          endTime: result.endTime,
          duration: result.duration
        });
      } else {
        this.sendError('session', 'end', requestId, this.ERROR_CODES.SYSTEM_ERROR, 
                      result.error || 'Failed to end session');
      }
    } catch (err) {
      this.logger.error(`Session end error: ${err.message}`);
      this.sendError('session', 'end', requestId, this.ERROR_CODES.SYSTEM_ERROR, err.message);
    }
  }
  
  /**
   * セッション保存処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleSessionSave(address, args) {
    const requestId = args[0];
    const sessionId = args[1];
    const path = args[2];
    
    if (!sessionId) {
      this.sendError('session', 'save', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 'Missing sessionId');
      return;
    }
    
    try {
      // セッション保存処理
      const result = this.bridge.saveSession(sessionId, path);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('session', 'save', requestId, {
          sessionId: sessionId,
          path: result.path
        });
        
        // 状態変更通知
        this.sendStateUpdate('session', 'saved', sessionId, {
          path: result.path
        });
      } else {
        this.sendError('session', 'save', requestId, this.ERROR_CODES.SYSTEM_ERROR, 
                      result.error || 'Failed to save session');
      }
    } catch (err) {
      this.logger.error(`Session save error: ${err.message}`);
      this.sendError('session', 'save', requestId, this.ERROR_CODES.SYSTEM_ERROR, err.message);
    }
  }
  
  /**
   * セッション読み込み処理
   * @param {string} address - OSCアドレス
   * @param {Array} args - 引数配列
   */
  handleSessionLoad(address, args) {
    const requestId = args[0];
    const path = args[1];
    
    if (!path) {
      this.sendError('session', 'load', requestId, this.ERROR_CODES.INVALID_ARGUMENTS, 'Missing path');
      return;
    }
    
    try {
      // セッション読み込み処理
      const result = this.bridge.loadSession(path);
      
      if (result.success) {
        // 成功時のレスポンス
        this.sendResponse('session', 'load', requestId, {
          sessionId: result.sessionId,
          path: path
        });
        
        // 状態変更通知
        this.sendStateUpdate('session', 'loaded', result.sessionId, {
          path: path
        });
      } else {
        this.sendError('session', 'load', requestId, this.ERROR_CODES.SYSTEM_ERROR, 
                      result.error || 'Failed to load session');
      }
    } catch (err) {
      this.logger.error(`Session load error: ${err.message}`);
      this.sendError('session', 'load', requestId, this.ERROR_CODES.SYSTEM_ERROR, err.message);
    }
  }
}

// ProtocolHandlerをエクスポート
module.exports = ProtocolHandler;