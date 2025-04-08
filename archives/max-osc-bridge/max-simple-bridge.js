/**
 * max-simple-bridge.js
 *
 * Max 7.x/8.x向け簡易OSCブリッジ
 * osc_bridge.mxo (Min-DevKit実装) を使用したOSC通信の実装
 *
 * 作成者: Claude Desktop MCP Team
 * 最終更新: 2023-04-06
 *
 * 使用方法:
 * 1. Maxパッチに「js max-simple-bridge.js [receivePort] [sendPort] [dynamicPorts]」を追加
 * 2. パッチにosc_bridge.mxoオブジェクトが存在することを確認（デフォルトID: obj-1）
 * 3. connect() / disconnect() 関数を呼び出して接続管理
 * 4. sendMessage() 関数でOSCメッセージを送信
 * 5. handleMessage() 関数でOSCメッセージを受信処理
 */

// グローバル変数
var g_debug = true;               // デバッグ出力の有効化
var g_mcpServerIP = "localhost";  // サーバーホスト名
var g_mcpReceivePort = 7500;      // 受信ポート
var g_mcpSendPort = 7400;         // 送信ポート
var g_dynamicPorts = true;        // 動的ポート割り当て
var g_oscBridge = null;           // osc_bridgeオブジェクトの参照
var g_patcher = null;             // パッチャーオブジェクトの参照
var g_connected = false;          // 接続状態
var g_objectId = "obj-1";         // osc_bridgeオブジェクトのID
var g_reconnectAttempts = 0;      // 再接続試行回数
var g_maxReconnectAttempts = 5;   // 最大再接続試行回数
var g_autoReconnect = true;       // 自動再接続の有効化

/**
 * 初期化関数
 * ブリッジの初期設定を行います
 */
function init () {
    post("Max OSCブリッジを初期化しています...\n");

    // バージョン情報
    post("max-simple-bridge.js バージョン 1.1.0\n");
    post("対応: Max 7.x/8.x, osc_bridge.mxo (Min-DevKit)\n");

    // パッチャー参照を取得
    g_patcher = this.patcher;

    if (!g_patcher) {
        error("パッチャーにアクセスできません。\n");
        return;
    } else {
        post("パッチャーへのアクセスが取得できました\n");
    }

    // 環境変数の読み込み
    try {
        if (jsarguments.length > 1) {
            // 第1引数: オブジェクトID (省略可)
            if (jsarguments[1].indexOf("obj-") === 0) {
                g_objectId = jsarguments[1];
                post("osc_bridgeオブジェクトID: " + g_objectId + "\n");

                // ポート情報は引数シフト
                if (jsarguments.length > 2) {
                    g_mcpReceivePort = parseInt(jsarguments[2]) || g_mcpReceivePort;

                    if (jsarguments.length > 3) {
                        g_mcpSendPort = parseInt(jsarguments[3]) || g_mcpSendPort;

                        if (jsarguments.length > 4) {
                            g_dynamicPorts = (jsarguments[4] === "1" || jsarguments[4] === "true");
                        }
                    }
                }
            } else {
                // 従来形式: 第1引数が受信ポート
                g_mcpReceivePort = parseInt(jsarguments[1]) || g_mcpReceivePort;

                if (jsarguments.length > 2) {
                    g_mcpSendPort = parseInt(jsarguments[2]) || g_mcpSendPort;

                    if (jsarguments.length > 3) {
                        g_dynamicPorts = (jsarguments[3] === "1" || jsarguments[3] === "true");
                    }
                }
            }
        }

        post("設定: 受信ポート=" + g_mcpReceivePort + ", 送信ポート=" + g_mcpSendPort + ", 動的ポート=" + g_dynamicPorts + "\n");
    } catch (e) {
        post("jsarguments解析エラー: " + e.message + "\n");
    }

    // OSCブリッジを取得
    setupOSCBridge();

    post("Max OSCブリッジの初期化が完了しました\n");
    return "初期化完了";
}

/**
 * OSCブリッジ設定
 * osc_bridge.mxoオブジェクトを検索し、参照を取得します
 */
function setupOSCBridge () {
    if (!g_patcher) {
        error("setupOSCBridge: パッチャーが利用できません\n");
        return;
    }

    try {
        // osc_bridgeオブジェクトを取得する
        var oscBridgeObj = g_patcher.getnamed(g_objectId);

        if (oscBridgeObj) {
            g_oscBridge = oscBridgeObj;
            post("osc_bridgeオブジェクト (" + g_objectId + ") を取得しました\n");

            // 初期状態でdisconnectを確認
            g_connected = false;
        } else {
            error("osc_bridgeオブジェクト (" + g_objectId + ") が見つかりません\n");
            error("パッチ内でosc_bridge " + g_mcpReceivePort + " " + g_mcpSendPort + "が正しく設定されているか確認してください\n");

            // 自動的に他のIDを検索
            findOSCBridgeInPatch();
            return;
        }

        post("OSCブリッジが設定されました\n");
    } catch (e) {
        error("OSCブリッジ設定エラー: " + e.message + "\n");
    }
}

/**
 * パッチ内のosc_bridgeオブジェクトを自動検索
 */
function findOSCBridgeInPatch () {
    if (!g_patcher) return;

    post("パッチ内のosc_bridgeオブジェクトを検索しています...\n");

    // すべてのオブジェクトを走査
    var maxObj = g_patcher.firstobject;
    while (maxObj) {
        var className = maxObj.maxclass;
        if (className === "newobj") {
            var text = maxObj.text || "";
            if (text.indexOf("osc_bridge") === 0) {
                post("osc_bridgeオブジェクトを発見: " + maxObj.varname + "\n");
                g_oscBridge = maxObj;
                g_objectId = maxObj.varname || "unknown";
                return true;
            }
        }
        maxObj = maxObj.nextobject;
    }

    error("パッチ内にosc_bridgeオブジェクトが見つかりませんでした。\n");
    error("パッチにosc_bridge " + g_mcpReceivePort + " " + g_mcpSendPort + "オブジェクトを追加してください。\n");
    return false;
}

/**
 * OSC接続を確立
 * osc_bridgeオブジェクトに接続メッセージを送信します
 */
function connect () {
    if (!g_oscBridge) {
        error("OSC接続できません: osc_bridgeが見つかりません\n");
        return false;
    }

    try {
        // connectメッセージを送信（引数なし - osc_bridgeの仕様に合わせる）
        g_oscBridge.message("connect");
        g_connected = true;
        g_reconnectAttempts = 0;  // 再接続カウンタをリセット
        post("OSC接続要求を送信しました\n");

        // 少し時間をおいて接続通知を送る
        delay(function () {
            // 初期接続通知
            if (g_connected) {
                sendMessage("/max/connected", 1);
                post("接続通知を送信しました\n");
            }
        }, 500); // 500ms待機

        return true;
    } catch (e) {
        error("OSC接続エラー: " + e.message + "\n");
        return false;
    }
}

/**
 * OSC接続を切断
 * osc_bridgeオブジェクトに切断メッセージを送信します
 */
function disconnect () {
    if (!g_oscBridge) {
        error("OSC切断できません: osc_bridgeが見つかりません\n");
        return false;
    }

    try {
        // 切断前に通知
        if (g_connected) {
            try {
                sendMessage("/max/connected", 0);
            } catch (e) {
                // 送信エラーは無視
            }
        }

        // disconnectメッセージを送信
        g_oscBridge.message("disconnect");
        g_connected = false;
        post("OSC切断要求を送信しました\n");
        return true;
    } catch (e) {
        error("OSC切断エラー: " + e.message + "\n");
        return false;
    }
}

/**
 * 再接続を試みる
 * 接続が失われた場合に再接続を試みます
 */
function reconnect () {
    if (g_reconnectAttempts >= g_maxReconnectAttempts) {
        error("最大再接続試行回数に達しました。手動で再接続してください。\n");
        return false;
    }

    g_reconnectAttempts++;
    post("再接続を試みています... (試行 " + g_reconnectAttempts + "/" + g_maxReconnectAttempts + ")\n");

    // 切断してから再接続
    try {
        disconnect();

        // 少し待ってから再接続
        delay(function () {
            connect();
        }, 1000);

        return true;
    } catch (e) {
        error("再接続エラー: " + e.message + "\n");
        return false;
    }
}

/**
 * ディレイ関数
 * 指定した時間後に関数を実行します
 */
function delay (callback, time) {
    var task = new Task(callback);
    task.schedule(time);
    return task;
}

/**
 * OSCメッセージ送信
 * osc_bridgeオブジェクトを通じてOSCメッセージを送信します
 *
 * @param {string} address - OSCアドレスパターン
 * @param {any} value - 送信する値
 * @return {boolean} 送信成功の場合はtrue
 */
function sendMessage (address, value) {
    if (!g_oscBridge) {
        error("OSC送信できません: osc_bridgeが見つかりません\n");
        return false;
    }

    if (!g_connected) {
        post("警告: 接続されていないためメッセージは送信されません: " + address + " " + value + "\n");

        // 自動再接続が有効な場合は再接続を試みる
        if (g_autoReconnect) {
            post("自動再接続を試みます...\n");
            reconnect();
        }

        return false;
    }

    try {
        // アドレスが/で始まっているか確認
        if (address.charAt(0) !== '/') {
            address = '/' + address;
        }

        // osc_bridgeオブジェクトにメッセージを送信
        // アドレスとデータを別々に渡す
        g_oscBridge.message(address, value);

        if (g_debug) {
            post("OSC送信: " + address + " " + value + "\n");
        }

        return true;
    } catch (e) {
        error("OSC送信エラー: " + e.message + "\n");
        return false;
    }
}

/**
 * 複数の値を持つOSCメッセージ送信
 *
 * @param {string} address - OSCアドレスパターン
 * @param {Array} values - 送信する値の配列
 * @return {boolean} 送信成功の場合はtrue
 */
function sendMultipleValues (address, values) {
    if (!g_oscBridge || !g_connected || !Array.isArray(values)) {
        return false;
    }

    try {
        // アドレスが/で始まっているか確認
        if (address.charAt(0) !== '/') {
            address = '/' + address;
        }

        // valuesの内容をスプレッド演算子のように展開
        var args = [address].concat(values);
        g_oscBridge.message.apply(g_oscBridge, args);

        if (g_debug) {
            post("OSC送信 (複数値): " + address + " " + values.join(" ") + "\n");
        }

        return true;
    } catch (e) {
        error("OSC送信エラー (複数値): " + e.message + "\n");
        return false;
    }
}

/**
 * メッセージ受信ハンドラ
 * osc_bridgeからのアウトレット出力を処理します
 *
 * @param {string} address - OSCアドレスパターン
 * @param {any} value - 受信した値
 */
function handleMessage (address, value) {
    if (!address) {
        post("警告: 無効なOSCメッセージ受信\n");
        return;
    }

    if (g_debug) {
        post("OSC受信: " + address + " " + (value !== undefined ? value : "(値なし)") + "\n");
    }

    // 基本的なメッセージ応答
    if (address === "/mcp/ping") {
        sendMessage("/max/pong", Date.now());
    }
    else if (address === "/mcp/test") {
        sendMessage("/max/response/test", "接続テスト成功");
    }
    else if (address === "/mcp/status") {
        sendMessage("/max/status", g_connected ? "connected" : "disconnected");
    }
    else if (address === "/mcp/reconnect") {
        reconnect();
    }

    // 追加のメッセージハンドラはここに実装
}

/**
 * 複数引数を持つメッセージ受信ハンドラ
 * osc_bridgeからの複数値を含むメッセージを処理します
 *
 * @param {string} address - OSCアドレスパターン
 * @param {...any} args - 受信した値（可変長引数）
 */
function handleMultipleArgs (address) {
    if (!address) return;

    // 引数を配列に変換
    var args = Array.prototype.slice.call(arguments, 1);

    if (g_debug) {
        post("OSC受信 (複数引数): " + address + " " + args.join(" ") + "\n");
    }

    // 基本ハンドラに処理を委譲
    handleMessage(address, args.length > 0 ? args[0] : undefined);
}

/**
 * エラー情報の送信
 * エラーメッセージをログに出力し、OSCで送信します
 *
 * @param {string} message - エラーメッセージ
 * @param {boolean} sendOsc - OSC経由でエラーを送信するかどうか
 */
function sendError (message, sendOsc = true) {
    error("エラー: " + message + "\n");
    if (sendOsc && g_connected && g_oscBridge) {
        sendMessage("/max/error", message);
    }
}

/**
 * 接続状態の取得
 * 現在の接続状態を返します
 *
 * @return {boolean} 接続されている場合はtrue
 */
function isConnected () {
    return g_connected;
}

/**
 * デバッグモードの設定
 *
 * @param {boolean} enable - デバッグ出力を有効にするかどうか
 */
function setDebug (enable) {
    g_debug = !!enable;
    post("デバッグモード: " + (g_debug ? "有効" : "無効") + "\n");
}

/**
 * 自動再接続の設定
 *
 * @param {boolean} enable - 自動再接続を有効にするかどうか
 */
function setAutoReconnect (enable) {
    g_autoReconnect = !!enable;
    post("自動再接続: " + (g_autoReconnect ? "有効" : "無効") + "\n");
}

// ヘルプメッセージを表示
function help () {
    post("\nmax-simple-bridge.js ヘルプ:\n");
    post("connect() - OSC接続を確立\n");
    post("disconnect() - OSC接続を切断\n");
    post("reconnect() - OSC接続を再確立\n");
    post("sendMessage(address, value) - OSCメッセージを送信\n");
    post("sendMultipleValues(address, [values]) - 複数値を持つOSCメッセージを送信\n");
    post("isConnected() - 接続状態を取得\n");
    post("setDebug(enable) - デバッグモードを設定\n");
    post("setAutoReconnect(enable) - 自動再接続を設定\n");
    post("help() - このヘルプメッセージを表示\n");
}

// 初期化の実行
init();
