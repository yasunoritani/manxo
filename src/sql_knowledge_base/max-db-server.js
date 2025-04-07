/**
 * Max/MSP SQL知識ベースサーバー
 * OSCを通じてMaxパッチからのクエリを処理し、SQLiteデータベースから情報を提供します
 */

const path = require('path');
const sqlite3 = require('sqlite3').verbose();
const osc = require('node-osc');
// Max APIが必要な場合はコメントを外す
// const Max = require('max-api');

// Maxの代わりにコンソール出力
const MaxConsole = {
    post: function (message, type) {
        console.log(`[${type || 'info'}] ${message}`);
    }
};

// 実際のMax APIかコンソール出力か決定
const Max = global.Max || MaxConsole;

// 設定
const DB_FILE = path.join(__dirname, 'max_claude_kb.db');
const OSC_SERVER_PORT = 7400;
const OSC_CLIENT_PORT = 7500;
const OSC_HOST = '127.0.0.1';

// グローバル変数
let db = null;
let oscServer = null;
let oscClient = null;

/**
 * データベース初期化
 */
async function initDatabase () {
    return new Promise((resolve, reject) => {
        Max.post(`SQLiteデータベースを開いています: ${DB_FILE}`);

        db = new sqlite3.Database(DB_FILE, sqlite3.OPEN_READONLY, (err) => {
            if (err) {
                Max.post(`データベース接続エラー: ${err.message}`, 'error');
                reject(err);
                return;
            }

            Max.post('データベース接続に成功しました');
            resolve(db);
        });
    });
}

/**
 * OSCサーバーとクライアントの初期化
 */
function initOSC () {
    Max.post(`OSCサーバーを起動しています: ${OSC_HOST}:${OSC_SERVER_PORT}`);

    // OSCクライアント（応答送信用）
    oscClient = new osc.Client(OSC_HOST, OSC_CLIENT_PORT);

    // OSCサーバー（リクエスト受信用）
    oscServer = new osc.Server(OSC_SERVER_PORT, OSC_HOST);

    oscServer.on('message', handleOSCMessage);

    Max.post('OSCサーバーの起動に成功しました');
}

/**
 * OSCメッセージのハンドラー
 */
function handleOSCMessage (msg, rinfo) {
    const address = msg[0];
    const args = msg.slice(1);

    Max.post(`OSCメッセージを受信: ${address}`);

    // アドレスパターンに基づいて適切なハンドラーにディスパッチ
    if (address === '/claude/query/validate_code') {
        validateCode(args[0], sendResponse);
    } else if (address === '/claude/query/check_connection') {
        checkConnection(args[0], args[1], args[2], args[3], sendResponse);
    } else if (address === '/claude/query/get_api_mapping') {
        getApiMapping(args[0], sendResponse);
    } else if (address === '/claude/query/get_validation_rules') {
        getValidationRules(args[0], sendResponse);
    } else if (address === '/claude/query/get_max_object') {
        getMaxObject(args[0], sendResponse);
    } else if (address === '/claude/query/get_min_api') {
        getMinApi(args[0], sendResponse);
    } else if (address === '/claude/query/search_max_objects') {
        searchMaxObjects(args[0], args[1] || 10, sendResponse);
    } else if (address === '/claude/query/search_min_api') {
        searchMinApi(args[0], args[1] || 10, sendResponse);
    } else {
        sendResponse('/max/error', {
            status: 'error',
            message: `未知のクエリアドレス: ${address}`
        });
    }
}

/**
 * コード検証
 * @param {string} code 検証するコード
 * @param {function} callback 応答コールバック
 */
function validateCode (code, callback) {
    if (!code) {
        callback('/max/response/validate_code', {
            status: 'error',
            message: 'コードが指定されていません'
        });
        return;
    }

    // コードを解析して潜在的な問題を特定するための簡易パターンマッチング
    const issues = [];

    // データベースから検証ルールを取得
    db.all('SELECT * FROM validation_rules', [], (err, rules) => {
        if (err) {
            callback('/max/response/validate_code', {
                status: 'error',
                message: `データベースエラー: ${err.message}`
            });
            return;
        }

        // 各ルールに対してコードをチェック
        rules.forEach(rule => {
            try {
                const regex = new RegExp(rule.pattern, 'g');
                if (regex.test(code)) {
                    issues.push({
                        type: rule.rule_type,
                        severity: rule.severity,
                        description: rule.description,
                        suggestion: rule.suggestion,
                        example_fix: rule.example_fix
                    });
                }
            } catch (e) {
                Max.post(`正規表現エラー (${rule.pattern}): ${e.message}`, 'error');
            }
        });

        // 応答を送信
        callback('/max/response/validate_code', {
            status: 'success',
            validation: {
                is_valid: issues.length === 0,
                issues: issues
            }
        });
    });
}

/**
 * オブジェクト接続の検証
 */
function checkConnection (source, sourceOutlet, destination, destinationInlet, callback) {
    if (!source || !destination) {
        callback('/max/response/check_connection', {
            status: 'error',
            message: 'ソースとデスティネーションが必要です'
        });
        return;
    }

    // 数値に変換
    sourceOutlet = parseInt(sourceOutlet, 10) || 0;
    destinationInlet = parseInt(destinationInlet, 10) || 0;

    // データベースから接続パターンを検索
    db.get(
        'SELECT * FROM connection_patterns WHERE source_object = ? AND destination_object = ?',
        [source, destination],
        (err, pattern) => {
            if (err) {
                callback('/max/response/check_connection', {
                    status: 'error',
                    message: `データベースエラー: ${err.message}`
                });
                return;
            }

            if (!pattern) {
                // 具体的なパターンが見つからない場合、類似パターンを探す
                db.all(
                    'SELECT * FROM connection_patterns WHERE source_object LIKE ? OR destination_object LIKE ?',
                    [`%${source}%`, `%${destination}%`],
                    (err, similarPatterns) => {
                        if (err) {
                            callback('/max/response/check_connection', {
                                status: 'error',
                                message: `データベースエラー: ${err.message}`
                            });
                            return;
                        }

                        callback('/max/response/check_connection', {
                            status: 'warning',
                            message: '直接的な接続パターンは見つかりませんでした',
                            similar_patterns: similarPatterns || [],
                            suggestion: '類似したパターンを参考にしてください'
                        });
                    }
                );
                return;
            }

            // 接続パターンが見つかった場合
            const isOutletMatched = pattern.source_outlet == sourceOutlet;
            const isInletMatched = pattern.destination_inlet == destinationInlet;
            const isRecommended = pattern.is_recommended === 1;

            callback('/max/response/check_connection', {
                status: 'success',
                connection: {
                    pattern_found: true,
                    outlet_matched: isOutletMatched,
                    inlet_matched: isInletMatched,
                    is_recommended: isRecommended,
                    description: pattern.description,
                    audio_signal_flow: pattern.audio_signal_flow === 1,
                    performance_impact: pattern.performance_impact,
                    compatibility_issues: pattern.compatibility_issues
                }
            });
        }
    );
}

/**
 * API意図マッピングの取得
 */
function getApiMapping (intent, callback) {
    if (!intent) {
        callback('/max/response/get_api_mapping', {
            status: 'error',
            message: '意図の説明が必要です'
        });
        return;
    }

    // データベースから類似の意図を検索
    db.all(
        `SELECT api_mapping.*, min_devkit_api.function_name, min_devkit_api.signature, min_devkit_api.example_usage
     FROM api_mapping
     LEFT JOIN min_devkit_api ON api_mapping.min_devkit_function_id = min_devkit_api.id
     WHERE api_mapping.natural_language_intent LIKE ?
     ORDER BY LENGTH(api_mapping.natural_language_intent) ASC
     LIMIT 5`,
        [`%${intent}%`],
        (err, mappings) => {
            if (err) {
                callback('/max/response/get_api_mapping', {
                    status: 'error',
                    message: `データベースエラー: ${err.message}`
                });
                return;
            }

            if (!mappings || mappings.length === 0) {
                callback('/max/response/get_api_mapping', {
                    status: 'warning',
                    message: '意図に一致するAPIマッピングが見つかりませんでした',
                    api_mapping: null
                });
                return;
            }

            // 最も関連性の高いマッピングを選択
            const bestMatch = mappings[0];

            callback('/max/response/get_api_mapping', {
                status: 'success',
                api_mapping: {
                    function: bestMatch.function_name,
                    template: bestMatch.transformation_template,
                    signature: bestMatch.signature,
                    example: bestMatch.example_usage,
                    context: bestMatch.context_requirements
                },
                alternatives: mappings.slice(1)
            });
        }
    );
}

/**
 * 検証ルールの取得
 */
function getValidationRules (context, callback) {
    // コンテキストが指定されていない場合はすべてのルールを返す
    const query = context
        ? 'SELECT * FROM validation_rules WHERE rule_type = ? OR context_requirements LIKE ?'
        : 'SELECT * FROM validation_rules';

    const params = context
        ? [context, `%${context}%`]
        : [];

    db.all(query, params, (err, rules) => {
        if (err) {
            callback('/max/response/get_validation_rules', {
                status: 'error',
                message: `データベースエラー: ${err.message}`
            });
            return;
        }

        if (!rules || rules.length === 0) {
            callback('/max/response/get_validation_rules', {
                status: 'warning',
                message: '指定されたコンテキストに関連するルールが見つかりませんでした',
                rules: []
            });
            return;
        }

        callback('/max/response/get_validation_rules', {
            status: 'success',
            rules: rules
        });
    });
}

/**
 * Maxオブジェクト情報の取得
 */
function getMaxObject (name, callback) {
    if (!name) {
        callback('/max/response/get_max_object', {
            status: 'error',
            message: 'オブジェクト名が必要です'
        });
        return;
    }

    db.get('SELECT * FROM max_objects WHERE name = ?', [name], (err, object) => {
        if (err) {
            callback('/max/response/get_max_object', {
                status: 'error',
                message: `データベースエラー: ${err.message}`
            });
            return;
        }

        if (!object) {
            // 類似名称のオブジェクトを探す
            db.all(
                'SELECT name FROM max_objects WHERE name LIKE ? LIMIT 5',
                [`%${name}%`],
                (err, similar) => {
                    if (err) {
                        callback('/max/response/get_max_object', {
                            status: 'error',
                            message: `データベースエラー: ${err.message}`
                        });
                        return;
                    }

                    callback('/max/response/get_max_object', {
                        status: 'warning',
                        message: `オブジェクト "${name}" は見つかりませんでした`,
                        similar_objects: similar ? similar.map(o => o.name) : []
                    });
                }
            );
            return;
        }

        // Booleanに変換
        object.is_ui_object = object.is_ui_object === 1;
        object.is_deprecated = object.is_deprecated === 1;

        callback('/max/response/get_max_object', {
            status: 'success',
            object: object
        });
    });
}

/**
 * Min-DevKit API情報の取得
 */
function getMinApi (functionName, callback) {
    if (!functionName) {
        callback('/max/response/get_min_api', {
            status: 'error',
            message: '関数名が必要です'
        });
        return;
    }

    db.get('SELECT * FROM min_devkit_api WHERE function_name = ?', [functionName], (err, api) => {
        if (err) {
            callback('/max/response/get_min_api', {
                status: 'error',
                message: `データベースエラー: ${err.message}`
            });
            return;
        }

        if (!api) {
            // 類似名称のAPI関数を探す
            db.all(
                'SELECT function_name FROM min_devkit_api WHERE function_name LIKE ? LIMIT 5',
                [`%${functionName}%`],
                (err, similar) => {
                    if (err) {
                        callback('/max/response/get_min_api', {
                            status: 'error',
                            message: `データベースエラー: ${err.message}`
                        });
                        return;
                    }

                    callback('/max/response/get_min_api', {
                        status: 'warning',
                        message: `API関数 "${functionName}" は見つかりませんでした`,
                        similar_functions: similar ? similar.map(a => a.function_name) : []
                    });
                }
            );
            return;
        }

        callback('/max/response/get_min_api', {
            status: 'success',
            api: api
        });
    });
}

/**
 * Maxオブジェクトの検索
 */
function searchMaxObjects (keyword, limit, callback) {
    if (!keyword) {
        callback('/max/response/search_max_objects', {
            status: 'error',
            message: '検索キーワードが必要です'
        });
        return;
    }

    limit = Math.min(Math.max(parseInt(limit, 10) || 10, 1), 50);

    db.all(
        `SELECT * FROM max_objects
     WHERE name LIKE ? OR description LIKE ? OR category LIKE ?
     LIMIT ?`,
        [`%${keyword}%`, `%${keyword}%`, `%${keyword}%`, limit],
        (err, objects) => {
            if (err) {
                callback('/max/response/search_max_objects', {
                    status: 'error',
                    message: `データベースエラー: ${err.message}`
                });
                return;
            }

            if (!objects || objects.length === 0) {
                callback('/max/response/search_max_objects', {
                    status: 'warning',
                    message: `キーワード "${keyword}" に一致するオブジェクトが見つかりませんでした`,
                    objects: []
                });
                return;
            }

            // Booleanに変換
            objects.forEach(obj => {
                obj.is_ui_object = obj.is_ui_object === 1;
                obj.is_deprecated = obj.is_deprecated === 1;
            });

            callback('/max/response/search_max_objects', {
                status: 'success',
                count: objects.length,
                objects: objects
            });
        }
    );
}

/**
 * Min-DevKit API関数の検索
 */
function searchMinApi (keyword, limit, callback) {
    if (!keyword) {
        callback('/max/response/search_min_api', {
            status: 'error',
            message: '検索キーワードが必要です'
        });
        return;
    }

    limit = Math.min(Math.max(parseInt(limit, 10) || 10, 1), 50);

    db.all(
        `SELECT * FROM min_devkit_api
     WHERE function_name LIKE ? OR description LIKE ? OR parameters LIKE ?
     LIMIT ?`,
        [`%${keyword}%`, `%${keyword}%`, `%${keyword}%`, limit],
        (err, apis) => {
            if (err) {
                callback('/max/response/search_min_api', {
                    status: 'error',
                    message: `データベースエラー: ${err.message}`
                });
                return;
            }

            if (!apis || apis.length === 0) {
                callback('/max/response/search_min_api', {
                    status: 'warning',
                    message: `キーワード "${keyword}" に一致するAPI関数が見つかりませんでした`,
                    apis: []
                });
                return;
            }

            callback('/max/response/search_min_api', {
                status: 'success',
                count: apis.length,
                apis: apis
            });
        }
    );
}

/**
 * OSC応答の送信
 */
function sendResponse (address, data) {
    // JSONに変換
    const jsonData = JSON.stringify(data);

    // OSCメッセージとして送信
    oscClient.send(address, jsonData, () => {
        Max.post(`OSC応答を送信: ${address}`);
    });

    // Max APIを通じてもメッセージを出力
    Max.outlet(jsonData);
}

/**
 * シャットダウン処理
 */
function shutdown () {
    if (oscServer) {
        oscServer.close();
        Max.post('OSCサーバーを停止しました');
    }

    if (db) {
        db.close((err) => {
            if (err) {
                Max.post(`データベース接続クローズエラー: ${err.message}`, 'error');
            } else {
                Max.post('データベース接続を閉じました');
            }
        });
    }
}

/**
 * 起動処理
 */
async function startup () {
    try {
        Max.post('Max SQL知識ベースサーバーを起動中...');

        // データベース初期化
        await initDatabase();

        // OSC初期化
        initOSC();

        Max.post('サーバーの起動が完了しました。リクエスト待機中...');
    } catch (err) {
        Max.post(`起動エラー: ${err.message}`, 'error');
    }
}

// Max APIイベントハンドラ
Max.addHandler('bang', () => {
    Max.post('SQL知識ベースサーバーが稼働中です');
});

Max.addHandler('start', startup);
Max.addHandler('stop', shutdown);

// 起動
startup();
