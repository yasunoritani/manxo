/**
 * 設定管理モジュール
 * 環境変数とデフォルト設定の読み込みを担当
 */

const path = require('path');
const fs = require('fs');

// デフォルト設定
const defaultConfig = {
  environment: 'development',
  max: {
    host: '127.0.0.1',
    port: 7400,
    accessLevel: 'full' // full, restricted, readonly
  },
  logging: {
    level: 'info',
    file: null
  }
};

/**
 * 設定を読み込む
 * 環境変数 > 設定ファイル > デフォルト設定 の優先順位
 */
function loadConfig(configPath) {
  let config = { ...defaultConfig };
  
  // 設定ファイルの読み込み（存在する場合）
  if (configPath) {
    try {
      const configFile = path.resolve(configPath);
      if (fs.existsSync(configFile)) {
        const fileConfig = JSON.parse(fs.readFileSync(configFile, 'utf8'));
        config = { ...config, ...fileConfig };
      }
    } catch (err) {
      console.warn(`Warning: Failed to load config file: ${err.message}`);
    }
  }
  
  // 環境変数からの読み込み
  if (process.env.MAX_HOST) {
    config.max.host = process.env.MAX_HOST;
  }
  
  if (process.env.MAX_OSC_PORT) {
    config.max.port = parseInt(process.env.MAX_OSC_PORT, 10);
  }
  
  if (process.env.MAX_ACCESS_LEVEL) {
    config.max.accessLevel = process.env.MAX_ACCESS_LEVEL;
  }
  
  if (process.env.NODE_ENV) {
    config.environment = process.env.NODE_ENV;
  }
  
  if (process.env.LOG_LEVEL) {
    config.logging.level = process.env.LOG_LEVEL;
  }
  
  if (process.env.LOG_FILE) {
    config.logging.file = process.env.LOG_FILE;
  }
  
  validateConfig(config);
  
  return config;
}

/**
 * 設定のバリデーション
 */
function validateConfig(config) {
  // アクセスレベルの検証
  const validAccessLevels = ['full', 'restricted', 'readonly'];
  if (!validAccessLevels.includes(config.max.accessLevel)) {
    throw new Error(`Invalid access level: ${config.max.accessLevel}. Valid values are: ${validAccessLevels.join(', ')}`);
  }
  
  // ポート番号の検証
  if (isNaN(config.max.port) || config.max.port < 1 || config.max.port > 65535) {
    throw new Error(`Invalid port number: ${config.max.port}`);
  }
}

module.exports = { loadConfig, defaultConfig };
