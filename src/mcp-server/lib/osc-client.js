/**
 * OSC通信クライアント
 * Max 9とのOSC通信を担当するモジュール
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
  }

  /**
   * OSCサーバーの開始と接続確認
   */
  async connect() {
    if (this.connected) return true;
    
    return new Promise((resolve, reject) => {
      try {
        // 受信用OSCサーバーのセットアップ
        this.server = new Server(this.serverPort, '0.0.0.0');
        
        this.server.on('message', (msg, rinfo) => {
          const [address, ...args] = msg;
          this.handleIncomingMessage(address, args);
        });
        
        // 接続確認リクエスト送信
        this.sendMessage('/mcp/system/ping', [Date.now()], (error, response) => {
          if (error) {
            reject(new Error(`Failed to connect to Max: ${error.message}`));
            return;
          }
          
          this.connected = true;
          resolve(true);
        }, 3000); // 3秒タイムアウト
      } catch (err) {
        reject(err);
      }
    });
  }

  /**
   * 切断処理
   */
  async disconnect() {
    if (this.server) {
      this.server.close();
      this.server = null;
    }
    
    if (this.client) {
      this.client.close();
    }
    
    this.connected = false;
    return true;
  }

  /**
   * OSCメッセージの送信
   * @param {string} address - OSCアドレスパターン
   * @param {Array} args - 引数
   * @param {Function} callback - コールバック関数
   * @param {number} timeout - タイムアウト時間(ms)
   * @returns {string} コマンドID
   */
  sendMessage(address, args = [], callback = null, timeout = 5000) {
    const commandId = this.generateCommandId();
    
    // コマンドIDを最初の引数として追加
    const argsWithId = [commandId, ...args];
    
    this.client.send(address, argsWithId);
    
    if (callback) {
      const timeoutId = setTimeout(() => {
        if (this.pendingCommands.has(commandId)) {
          const error = new Error(`Command timed out: ${address}`);
          callback(error);
          this.pendingCommands.delete(commandId);
        }
      }, timeout);
      
      this.pendingCommands.set(commandId, { callback, timeoutId });
    }
    
    return commandId;
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
}

module.exports = { OSCClient };
