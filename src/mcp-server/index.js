/**
 * Max 9 MCP Server
 * Claude DesktopとMax 9を連携するためのMCPサーバー実装
 */

require('dotenv').config();
const { createServer } = require('fastmcp');
const { Client, Server } = require('node-osc');

// 環境変数の読み込み
const MAX_OSC_IP = process.env.MAX_OSC_IP || '127.0.0.1';
const MAX_OSC_SEND_PORT = parseInt(process.env.MAX_OSC_SEND_PORT) || 7400;
const MAX_OSC_RECEIVE_PORT = parseInt(process.env.MAX_OSC_RECEIVE_PORT) || 7500;
const MAX_ACCESS_LEVEL = process.env.MAX_ACCESS_LEVEL || 'full'; // full, restricted, readonly
const DEBUG_MODE = process.env.DEBUG_MODE === 'true';

// OSC通信用のクライアントとサーバーを初期化
let oscClient = null;
let oscServer = null;

// Max 9の接続状態管理
let maxConnected = false;
let activePatchIds = {};
let activeObjects = {};

// デバッグ用ログ関数
const log = (message) => {
  if (DEBUG_MODE) {
    console.log(`[MCP-Server] ${message}`);
  }
};

// OSCサーバーの初期化
const initOSC = () => {
  try {
    // OSCクライアント（MCPサーバーからMax 9へのメッセージ送信用）
    oscClient = new Client(MAX_OSC_IP, MAX_OSC_SEND_PORT);
    
    // OSCサーバー（Max 9からのメッセージ受信用）
    oscServer = new Server(MAX_OSC_RECEIVE_PORT, '0.0.0.0');
    
    log(`OSC通信を初期化しました`);
    log(`Max 9への送信先: ${MAX_OSC_IP}:${MAX_OSC_SEND_PORT}`);
    log(`Max 9からの受信ポート: ${MAX_OSC_RECEIVE_PORT}`);
    
    // OSCメッセージ受信ハンドラーを設定
    oscServer.on('message', (msg, rinfo) => {
      const address = msg[0];
      const args = msg.slice(1);
      
      log(`受信 OSC: ${address} ${args.join(' ')}`);
      
      // Max 9からのメッセージを処理
      if (address === '/max/response/connected') {
        handleMaxConnected(args);
      }
      else if (address.startsWith('/max/response/')) {
        handleMaxResponse(address, args);
      }
      else if (address.startsWith('/max/params/')) {
        handleMaxParamUpdate(address, args);
      }
      else if (address.startsWith('/max/error/')) {
        handleMaxError(address, args);
      }
    });
    
    return true;
  } catch (error) {
    console.error(`OSC初期化エラー: ${error.message}`);
    return false;
  }
};

// Max 9への接続確立ハンドラー
const handleMaxConnected = (args) => {
  const status = args[0];
  
  if (status === 1) {
    maxConnected = true;
    log('Max 9との接続が確立されました');
  } else {
    maxConnected = false;
    log('Max 9との接続が切断されました');
  }
};

// Max 9からのレスポンスハンドラー
const handleMaxResponse = (address, args) => {
  const responseType = address.split('/')[3];
  
  switch (responseType) {
    case 'patchCreated':
      const patchId = args[0];
      const patchName = args[1];
      activePatchIds[patchId] = { name: patchName, objects: {} };
      log(`パッチが作成されました: ${patchName} (ID: ${patchId})`);
      break;
      
    case 'objectCreated':
      const objPatchId = args[0];
      const objectId = args[1];
      const objectType = args[2];
      const x = args[3];
      const y = args[4];
      
      if (activePatchIds[objPatchId]) {
        activePatchIds[objPatchId].objects[objectId] = { type: objectType, position: { x, y } };
        activeObjects[objectId] = { patchId: objPatchId, type: objectType };
        log(`オブジェクトが作成されました: ${objectType} (ID: ${objectId})`);
      }
      break;
      
    case 'status':
      const patchCount = args[0];
      const objectCount = args[1];
      log(`Max 9の状態: ${patchCount}パッチ, ${objectCount}オブジェクト`);
      break;
      
    default:
      log(`レスポンス受信: ${responseType} ${args.join(' ')}`);
  }
};

// パラメータ更新ハンドラー
const handleMaxParamUpdate = (address, args) => {
  const paramName = address.split('/')[3];
  const paramValue = args[0];
  
  log(`パラメータ更新: ${paramName} = ${paramValue}`);
};

// エラーハンドラー
const handleMaxError = (address, args) => {
  const errorType = address.split('/')[3];
  const errorMessage = args[0];
  
  console.error(`Max 9エラー (${errorType}): ${errorMessage}`);
};

// OSCメッセージを送信
const sendOSCMessage = (address, ...args) => {
  if (!oscClient || !maxConnected) {
    log('OSC送信できません: Max 9と接続されていません');
    return false;
  }
  
  try {
    const messageArray = [address, ...args];
    oscClient.send(...messageArray);
    
    log(`送信 OSC: ${address} ${args.join(' ')}`);
    return true;
  } catch (error) {
    console.error(`OSCメッセージ送信エラー: ${error.message}`);
    return false;
  }
};

// Max 9に接続要求を送信
const connectToMax = () => {
  return sendOSCMessage('/mcp/system/connection', 1);
};

// Max 9の状態を取得
const getMaxStatus = () => {
  return sendOSCMessage('/mcp/system/status');
};

// 新規パッチを作成
const createMaxPatch = (patchId, patchName) => {
  return sendOSCMessage('/mcp/patch/create', patchId, patchName || 'Untitled');
};

// パッチにオブジェクトを追加
const addObjectToPatch = (patchId, objectId, objectType, x, y) => {
  return sendOSCMessage('/mcp/object/create', patchId, objectId, objectType, x || 100, y || 100);
};

// オブジェクト間に接続を作成
const connectObjects = (sourceId, sourceOutlet, targetId, targetInlet) => {
  return sendOSCMessage('/mcp/object/connect', sourceId, sourceOutlet || 0, targetId, targetInlet || 0);
};

// オブジェクトのパラメータを設定
const setObjectParameter = (objectId, paramName, paramValue) => {
  return sendOSCMessage('/mcp/object/param', objectId, paramName, paramValue);
};

// パッチをファイルに保存
const savePatch = (patchId, filePath) => {
  return sendOSCMessage('/mcp/patch/save', patchId, filePath);
};

// シンプルなシンセサイザーパッチを自動生成
const generateSimpleSynth = async () => {
  const patchId = 'synth_' + Date.now();
  const patchName = 'Simple Synthesizer';
  
  // パッチを作成
  await createMaxPatch(patchId, patchName);
  
  // オブジェクトを追加
  await addObjectToPatch(patchId, 'cycle1', 'cycle~', 100, 100);
  await addObjectToPatch(patchId, 'gain1', 'gain~', 100, 150);
  await addObjectToPatch(patchId, 'dac1', 'ezdac~', 100, 200);
  
  // オブジェクトを接続
  await connectObjects('cycle1', 0, 'gain1', 0);
  await connectObjects('gain1', 0, 'dac1', 0);
  await connectObjects('gain1', 0, 'dac1', 1);
  
  // パラメータを設定
  await setObjectParameter('cycle1', 'freq', 440);
  await setObjectParameter('gain1', 'level', 0.5);
  
  return patchId;
};

// MCP サーバーの初期化
const initMCPServer = async () => {
  // OSC通信を初期化
  if (!initOSC()) {
    console.error('OSC通信の初期化に失敗しました');
    return null;
  }
  
  // MCPツール定義
  const mcpTools = {
    connect_to_max: {
      description: 'Max 9にOSC接続する',
      parameters: {},
      handler: async () => {
        const result = connectToMax();
        return { connected: result };
      }
    },
    get_max_status: {
      description: 'Max 9の状態情報を取得する',
      parameters: {},
      handler: async () => {
        const result = getMaxStatus();
        return { 
          requested: result,
          patches: Object.keys(activePatchIds).length,
          objects: Object.keys(activeObjects).length
        };
      }
    },
    create_max_patch: {
      description: '新しいMax 9パッチを作成する',
      parameters: {
        patchName: { type: 'string', description: 'パッチの名前' }
      },
      handler: async ({ patchName }) => {
        const patchId = 'patch_' + Date.now();
        const result = await createMaxPatch(patchId, patchName);
        return { patchId, created: result };
      }
    },
    add_object_to_patch: {
      description: 'パッチにMaxオブジェクトを追加する',
      parameters: {
        patchId: { type: 'string', description: 'パッチID' },
        objectType: { type: 'string', description: 'オブジェクトタイプ' },
        x: { type: 'number', description: 'X座標', optional: true },
        y: { type: 'number', description: 'Y座標', optional: true }
      },
      handler: async ({ patchId, objectType, x, y }) => {
        const objectId = objectType + '_' + Date.now();
        const result = await addObjectToPatch(patchId, objectId, objectType, x, y);
        return { objectId, created: result };
      }
    },
    connect_objects: {
      description: 'Maxオブジェクト間に接続を作成する',
      parameters: {
        sourceId: { type: 'string', description: '送信元オブジェクトID' },
        targetId: { type: 'string', description: '送信先オブジェクトID' },
        sourceOutlet: { type: 'number', description: '送信元のアウトレットインデックス', optional: true },
        targetInlet: { type: 'number', description: '送信先のインレットインデックス', optional: true }
      },
      handler: async ({ sourceId, targetId, sourceOutlet, targetInlet }) => {
        const result = await connectObjects(sourceId, sourceOutlet, targetId, targetInlet);
        return { connected: result };
      }
    },
    set_object_parameter: {
      description: 'オブジェクトのパラメータを設定する',
      parameters: {
        objectId: { type: 'string', description: 'オブジェクトID' },
        paramName: { type: 'string', description: 'パラメータ名' },
        paramValue: { type: 'any', description: 'パラメータ値' }
      },
      handler: async ({ objectId, paramName, paramValue }) => {
        const result = await setObjectParameter(objectId, paramName, paramValue);
        return { set: result };
      }
    },
    save_patch: {
      description: 'パッチをファイルに保存する',
      parameters: {
        patchId: { type: 'string', description: 'パッチID' },
        filePath: { type: 'string', description: 'ファイルパス' }
      },
      handler: async ({ patchId, filePath }) => {
        const result = await savePatch(patchId, filePath);
        return { saved: result };
      }
    },
    generate_simple_synth: {
      description: 'シンプルなシンセサイザーパッチを自動生成する',
      parameters: {},
      handler: async () => {
        const patchId = await generateSimpleSynth();
        return { patchId, generated: true };
      }
    }
  };

  // MCPリソース定義
  const mcpResources = {
    max_documentation: {
      description: 'Max 9のドキュメンテーション情報',
      parameters: {
        objectType: { type: 'string', description: 'オブジェクトタイプ' }
      },
      handler: async ({ objectType }) => {
        // 実際の実装ではドキュメンテーションを取得する
        return { objectType, documentation: 'ドキュメント情報...' };
      }
    },
    patch_info: {
      description: '特定のパッチの詳細情報',
      parameters: {
        patchId: { type: 'string', description: 'パッチID' }
      },
      handler: async ({ patchId }) => {
        if (!activePatchIds[patchId]) {
          throw new Error(`パッチが見つかりません: ${patchId}`);
        }
        
        return {
          patchId,
          name: activePatchIds[patchId].name,
          objectCount: Object.keys(activePatchIds[patchId].objects).length,
          objects: activePatchIds[patchId].objects
        };
      }
    }
  };

  // MCPサーバーを作成
  const server = createServer({
    tools: mcpTools,
    resources: mcpResources
  });
  
  // サーバーを起動
  await server.start();
  
  log('MCP サーバーを起動しました');
  
  return server;
};

// メイン関数
const main = async () => {
  try {
    const server = await initMCPServer();
    
    if (!server) {
      console.error('MCPサーバーの初期化に失敗しました');
      process.exit(1);
    }
    
    log('Max 9 MCP サーバーが稼働中です');
    
    // プロセス終了時の処理
    process.on('SIGINT', async () => {
      log('MCPサーバーをシャットダウンしています...');
      
      if (oscServer) {
        oscServer.close();
      }
      
      if (oscClient) {
        oscClient.close();
      }
      
      if (server) {
        await server.stop();
      }
      
      process.exit(0);
    });
  } catch (error) {
    console.error(`メインプロセスエラー: ${error.message}`);
    process.exit(1);
  }
};

// サーバー起動
main();
