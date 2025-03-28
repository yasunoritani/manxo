/**
 * OSC通信クライアント
 * Max 9とのOSC通信を担当するモジュール
 * エラー処理と接続回復機能強化版
 */

const { Client, Server } = require('node-osc');
const { EventEmitter } = require('events');

class OSCClient extends EventEmitter {
  constructor(host, port) {
    super();
    this.host = host;
    this.port = port;
    this.client = new Client(host, port);
    this.server = null;
    this.serverPort = port + 1; // 受信用ポートはMax送信用ポート+1
    this.connected = false;
    this.messageQueue = [];
    this.commandIdCounter = 0;
    this.pendingCommands = new Map();
    
    // 接続回復用の属性
    this.reconnecting = false;
    this.maxRetries = 5;
    this.retryCount = 0;
    this.retryDelay = 3000; // 3秒
    this.connectionErrors = [];
  }

  /**
   * OSCサーバーの開始と接続確認
   * @returns {Promise<boolean>} 接続成功の可否
   */
  async connect() {
    if (this.connected) return true;
    
    // 接続試行中は重複実行を防止
    if (this.reconnecting) {
      return new Promise(resolve => {
        const checkInterval = setInterval(() => {
          if (!this.reconnecting) {
            clearInterval(checkInterval);
            resolve(this.connected);
          }
        }, 500);
      });
    }
    
    this.reconnecting = true;
    this.emit('connecting');
    
    return new Promise((resolve, reject) => {
      try {
        console.log(`Max 9 (${this.host}:${this.port})への接続を開始します...`);
        
        // 既存のサーバーがあればお掃除
        if (this.server) {
          try {
            this.server.close();
          } catch (e) {
            console.warn(`既存サーバー終了エラー: ${e.message}`);
          }
        }
        
        // 受信用OSCサーバーのセットアップ
        this.server = new Server(this.serverPort, '0.0.0.0');
        
        this.server.on('message', (msg, rinfo) => {
          const [address, ...args] = msg;
          this.handleIncomingMessage(address, args);
        });
        
        this.server.on('error', (err) => {
          console.error(`OSCサーバーエラー: ${err.message}`);
          this.emit('server-error', err);
          
          // エラーを記録
          this.connectionErrors.push({
            type: 'server_error',
            message: err.message,
            timestamp: Date.now()
          });
        });
        
        // 接続確認リクエスト送信
        this.sendMessage('/mcp/system/ping', [Date.now()], (error, response) => {
          this.reconnecting = false;
          
          if (error) {
            console.error(`Max接続エラー: ${error.message}`);
            this.connected = false;
            
            // エラーを記録
            this.connectionErrors.push({
              type: 'connection_error',
              message: error.message,
              timestamp: Date.now()
            });
            
            this.emit('connect-error', error);
            reject(new Error(`Maxへの接続に失敗しました: ${error.message}`));
            return;
          }
          
          // 接続成功
          this.connected = true;
          this.retryCount = 0; // リトライカウンタをリセット
          
          console.log(`Max 9への接続に成功しました (${this.host}:${this.port})`);
          this.emit('connected');
          resolve(true);
        }, 5000); // 5秒タイムアウト
      } catch (err) {
        this.reconnecting = false;
        this.connected = false;
        
        // エラーを記録
        this.connectionErrors.push({
          type: 'connect_exception',
          message: err.message,
          timestamp: Date.now()
        });
        
        console.error(`接続例外: ${err.message}`);
        this.emit('connect-exception', err);
        reject(err);
      }
    });
  }

  /**
   * 切断処理
   * @returns {Promise<boolean>} 切断成功の可否
   */
  async disconnect() {
    try {
      if (this.server) {
        this.server.close();
        this.server = null;
      }
      
      if (this.client) {
        this.client.close();
      }
      
      this.connected = false;
      this.emit('disconnected');
      console.log(`Max 9との接続を終了しました (${this.host}:${this.port})`);
      return true;
    } catch (err) {
      console.error(`切断エラー: ${err.message}`);
      this.emit('disconnect-error', err);
      return false;
    }
  }
  
  /**
   * 接続の再確立を試みる
   * @returns {Promise<boolean>} 再接続成功の可否
   */
  async reconnect() {
    // 既に接続済みであれば何もしない
    if (this.connected) return true;
    
    // 再試行回数のチェック
    if (this.retryCount >= this.maxRetries) {
      console.error(`最大再接続試行回数(${this.maxRetries})に達しました`);
      this.emit('max-retries-exceeded');
      return false;
    }
    
    // 再試行回数をインクリメント
    this.retryCount++;
    console.log(`Max 9への再接続を試行します (${this.retryCount}/${this.maxRetries})...`);
    
    try {
      // 既存の接続を切断
      await this.disconnect();
      
      // 少し待機
      await new Promise(resolve => setTimeout(resolve, 1000));
      
      // 新しいクライアントを作成
      this.client = new Client(this.host, this.port);
      
      // 再接続
      const success = await this.connect();
      
      if (success) {
        console.log(`Max 9への再接続に成功しました (${this.host}:${this.port})`);
        this.emit('reconnected');
      }
      
      return success;
    } catch (err) {
      console.error(`再接続エラー: ${err.message}`);
      this.emit('reconnect-error', err);
      
      // エラーを記録
      this.connectionErrors.push({
        type: 'reconnect_error',
        message: err.message,
        timestamp: Date.now(),
        attempt: this.retryCount
      });
      
      return false;
    }
  }

  /**
   * OSCメッセージの送信
   * エラー処理と自動再接続機能を追加
   * @param {string} address - OSCアドレスパターン
   * @param {Array} args - 引数
   * @param {Function} callback - コールバック関数
   * @param {number} timeout - タイムアウト時間(ms)
   * @returns {string} コマンドID
   */
  sendMessage(address, args = [], callback = null, timeout = 5000) {
    // 接続状態の確認
    if (!this.connected && !this.reconnecting && address !== '/mcp/system/ping') {
      console.warn(`送信失敗 (非接続状態): ${address}`);
      
      // 自動再接続を試みる
      this.reconnect().catch(err => {
        console.error(`自動再接続エラー: ${err.message}`);
      });
      
      // エラーをコールバックに通知
      if (callback) {
        callback(new Error('Not connected to Max 9'));
      }
      
      return null;
    }
    
    const commandId = this.generateCommandId();
    
    try {
      // コマンドIDを最初の引数として追加
      const argsWithId = [commandId, ...args];
      
      // メッセージ送信
      this.client.send(address, argsWithId);
      
      if (callback) {
        const timeoutId = setTimeout(() => {
          if (this.pendingCommands.has(commandId)) {
            const error = new Error(`コマンドタイムアウト: ${address}`);
            callback(error);
            this.pendingCommands.delete(commandId);
            
            // タイムアウト情報を記録
            this.connectionErrors.push({
              type: 'command_timeout',
              address,
              commandId,
              timestamp: Date.now()
            });
          }
        }, timeout);
        
        this.pendingCommands.set(commandId, { callback, timeoutId });
      }
      
      return commandId;
    } catch (err) {
      console.error(`送信エラー (${address}): ${err.message}`);
      
      // エラーを記録
      this.connectionErrors.push({
        type: 'send_error',
        address,
        message: err.message,
        timestamp: Date.now()
      });
      
      // 接続エラーの場合は自動再接続
      if (err.message.includes('ECONNREFUSED') || err.message.includes('not running')) {
        this.connected = false;
        this.reconnect().catch(e => {
          console.error(`再接続エラー: ${e.message}`);
        });
      }
      
      if (callback) {
        callback(err);
      }
      
      this.emit('send-error', address, err);
      return null;
    }
  }

  /**
   * 受信メッセージの処理
   * @param {string} address - 受信したOSCアドレス
   * @param {Array} args - 受信した引数
   */
  handleIncomingMessage(address, args) {
    // レスポンスの処理
    if (address.startsWith('/max/response/')) {
      const commandId = args[0];
      const data = args.slice(1);
      
      if (this.pendingCommands.has(commandId)) {
        const { callback, timeoutId } = this.pendingCommands.get(commandId);
        clearTimeout(timeoutId);
        callback(null, data);
        this.pendingCommands.delete(commandId);
      }
    }
    // エラーの処理
    else if (address.startsWith('/max/error/')) {
      const commandId = args[0];
      const errorMessage = args[1] || 'Unknown error';
      
      if (this.pendingCommands.has(commandId)) {
        const { callback, timeoutId } = this.pendingCommands.get(commandId);
        clearTimeout(timeoutId);
        callback(new Error(errorMessage));
        this.pendingCommands.delete(commandId);
      }
    }
    // その他のメッセージは全体に放出
    else {
      this.emit('message', address, args);
    }
  }

  /**
   * コマンドIDの生成
   * @returns {string} 生成されたコマンドID
   */
  generateCommandId() {
    this.commandIdCounter++;
    return `cmd_${Date.now()}_${this.commandIdCounter}`;
  }
  
  /**
   * 接続ステータスの取得
   * @returns {Object} 接続ステータス情報
   */
  getStatus() {
    return {
      connected: this.connected,
      host: this.host,
      port: this.port,
      serverPort: this.serverPort,
      retryCount: this.retryCount,
      reconnecting: this.reconnecting,
      pendingCommandsCount: this.pendingCommands.size,
      lastErrors: this.connectionErrors.slice(-5) // 直近の5つのエラーを返す
    };
  }
  
  /**
   * 受信ログとエラー記録の正規化
   * メモリ使用量を最適化
   */
  cleanup() {
    // メッセージキューが大きすぎる場合は古いものを消去
    if (this.messageQueue.length > 1000) {
      this.messageQueue = this.messageQueue.slice(-500);
    }
    
    // エラー記録も古いものを除去
    if (this.connectionErrors.length > 100) {
      this.connectionErrors = this.connectionErrors.slice(-50);
    }
    
    // 古いタイムアウトコマンドをクリーンアップ
    const now = Date.now();
    for (const [commandId, data] of this.pendingCommands.entries()) {
      // 10分以上経過したコマンドはクリーンアップ
      if (now - parseInt(commandId.split('_')[1]) > 10 * 60 * 1000) {
        clearTimeout(data.timeoutId);
        this.pendingCommands.delete(commandId);
      }
    }
  }
}

module.exports = { OSCClient };
