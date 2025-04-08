/**
 * Maxpatジェネレーター
 * Max/MSPパッチファイル（.maxpat）を生成するためのモジュール
 */

const fs = require('fs');
const path = require('path');

// テンプレートディレクトリのパス
const TEMPLATES_DIR = path.join(__dirname, 'templates');

/**
 * 利用可能なテンプレート一覧を取得
 * @returns {Array<string>} テンプレート名の配列
 */
function getAvailableTemplates () {
    try {
        return fs.readdirSync(TEMPLATES_DIR)
            .filter(file => file.endsWith('.maxpat'))
            .map(file => file.replace('.maxpat', ''));
    } catch (err) {
        console.error('テンプレート一覧取得エラー:', err.message);
        return [];
    }
}

/**
 * 基本的なMaxパッチを生成
 * @param {Object} options - 生成オプション
 * @param {string} [options.templateName='basic_template'] - 使用するテンプレート名
 * @param {string} [options.outputPath] - 出力ファイルパス（指定がない場合は結果を返すのみ）
 * @param {string} [options.patchName] - パッチの名前
 * @param {Object} [options.customValues={}] - テンプレート内で置換するカスタム値
 * @returns {Object|boolean} 生成されたパッチオブジェクトまたは成功/失敗を示すブール値
 */
function generateBasicPatch (options = {}) {
    const {
        templateName = 'basic_template',
        outputPath,
        patchName = 'Generated Patch',
        customValues = {}
    } = options;

    try {
        // テンプレートファイルのパス
        const templatePath = path.join(TEMPLATES_DIR, `${templateName}.maxpat`);

        // テンプレートが存在するか確認
        if (!fs.existsSync(templatePath)) {
            throw new Error(`テンプレート "${templateName}" が見つかりません`);
        }

        // テンプレートファイルを読み込む
        const templateContent = fs.readFileSync(templatePath, 'utf8');
        let patchObj = JSON.parse(templateContent);

        // パッチの基本情報を更新
        if (customValues.description) {
            patchObj.patcher.description = customValues.description;
        }

        // パッチ名をコメントとして追加
        if (patchName) {
            // 既存のboxesの配列を取得
            const boxes = patchObj.patcher.boxes || [];

            // 新しいコメントオブジェクトを作成
            const commentBox = {
                "box": {
                    "id": `obj-comment-title`,
                    "maxclass": "comment",
                    "numinlets": 1,
                    "numoutlets": 0,
                    "patching_rect": [50.0, 25.0, 300.0, 20.0],
                    "text": patchName
                }
            };

            // 既存のboxesにコメントを追加
            boxes.push(commentBox);
            patchObj.patcher.boxes = boxes;
        }

        // カスタムオブジェクトの追加やその他のカスタマイズはここで行う

        // 結果をJSONに変換
        const resultJson = JSON.stringify(patchObj, null, 2);

        // 出力パスが指定されている場合はファイルに保存
        if (outputPath) {
            fs.writeFileSync(outputPath, resultJson, 'utf8');
            console.log(`パッチを生成しました: ${outputPath}`);
            return true;
        }

        // 出力パスが指定されていない場合はオブジェクトを返す
        return patchObj;
    } catch (err) {
        console.error('パッチ生成エラー:', err.message);
        return false;
    }
}

/**
 * オブジェクトをパッチに追加
 * @param {Object} patchObj - 更新するパッチオブジェクト
 * @param {Object} objectOptions - 追加するオブジェクトの情報
 * @param {string} objectOptions.type - オブジェクトの種類（例: 'button', 'slider', 'message'）
 * @param {number} objectOptions.x - X座標
 * @param {number} objectOptions.y - Y座標
 * @param {string} [objectOptions.text] - オブジェクトのテキスト（messageオブジェクトなど）
 * @param {Object} [objectOptions.attributes={}] - その他の属性
 * @returns {string} 追加されたオブジェクトのID
 */
function addObject (patchObj, objectOptions) {
    const {
        type,
        x,
        y,
        text,
        attributes = {}
    } = objectOptions;

    // 新しいオブジェクトID（簡易的な実装）
    const objectId = `obj-${Math.floor(Math.random() * 1000)}`;

    // 基本的なオブジェクト構造
    const newObject = {
        "box": {
            "id": objectId,
            "maxclass": type,
            "numinlets": getDefaultNuminlets(type),
            "numoutlets": getDefaultNumoutlets(type),
            "outlettype": getDefaultOutletTypes(type),
            "patching_rect": [x, y, getDefaultWidth(type), getDefaultHeight(type)],
        }
    };

    // テキストがある場合は追加
    if (text) {
        newObject.box.text = text;
    }

    // その他の属性を追加
    Object.keys(attributes).forEach(key => {
        newObject.box[key] = attributes[key];
    });

    // パッチのboxes配列にオブジェクトを追加
    if (!patchObj.patcher.boxes) {
        patchObj.patcher.boxes = [];
    }

    patchObj.patcher.boxes.push(newObject);

    return objectId;
}

/**
 * オブジェクト間の接続を追加
 * @param {Object} patchObj - 更新するパッチオブジェクト
 * @param {string} sourceId - 接続元オブジェクトのID
 * @param {number} sourceOutlet - 接続元のアウトレット番号（0から始まる）
 * @param {string} destId - 接続先オブジェクトのID
 * @param {number} destInlet - 接続先のインレット番号（0から始まる）
 * @returns {boolean} 成功/失敗を示すブール値
 */
function connect (patchObj, sourceId, sourceOutlet, destId, destInlet) {
    try {
        // 接続情報を作成
        const connection = {
            "patchline": {
                "destination": [destId, destInlet],
                "source": [sourceId, sourceOutlet]
            }
        };

        // パッチのlines配列に接続を追加
        if (!patchObj.patcher.lines) {
            patchObj.patcher.lines = [];
        }

        patchObj.patcher.lines.push(connection);

        return true;
    } catch (err) {
        console.error('接続追加エラー:', err.message);
        return false;
    }
}

/**
 * オブジェクトタイプに基づいてデフォルトのインレット数を取得
 * @param {string} type - オブジェクトタイプ
 * @returns {number} インレット数
 */
function getDefaultNuminlets (type) {
    const defaults = {
        'button': 1,
        'toggle': 1,
        'slider': 1,
        'message': 2,
        'comment': 1,
        'newobj': 1,
        'flonum': 1,
        'number': 1
    };

    return defaults[type] || 1;
}

/**
 * オブジェクトタイプに基づいてデフォルトのアウトレット数を取得
 * @param {string} type - オブジェクトタイプ
 * @returns {number} アウトレット数
 */
function getDefaultNumoutlets (type) {
    const defaults = {
        'button': 1,
        'toggle': 1,
        'slider': 1,
        'message': 1,
        'comment': 0,
        'newobj': 1,
        'flonum': 1,
        'number': 1
    };

    return defaults[type] || 0;
}

/**
 * オブジェクトタイプに基づいてデフォルトのアウトレットタイプを取得
 * @param {string} type - オブジェクトタイプ
 * @returns {Array} アウトレットタイプの配列
 */
function getDefaultOutletTypes (type) {
    const defaults = {
        'button': ["bang"],
        'toggle': ["int"],
        'slider': ["float"],
        'message': [""],
        'newobj': [""],
        'flonum': ["float"],
        'number': ["int"]
    };

    return defaults[type] || [];
}

/**
 * オブジェクトタイプに基づいてデフォルトの幅を取得
 * @param {string} type - オブジェクトタイプ
 * @returns {number} 幅
 */
function getDefaultWidth (type) {
    const defaults = {
        'button': 20.0,
        'toggle': 20.0,
        'slider': 20.0,
        'message': 50.0,
        'comment': 150.0,
        'newobj': 100.0,
        'flonum': 50.0,
        'number': 50.0
    };

    return defaults[type] || 100.0;
}

/**
 * オブジェクトタイプに基づいてデフォルトの高さを取得
 * @param {string} type - オブジェクトタイプ
 * @returns {number} 高さ
 */
function getDefaultHeight (type) {
    const defaults = {
        'button': 20.0,
        'toggle': 20.0,
        'slider': 100.0,
        'message': 22.0,
        'comment': 20.0,
        'newobj': 22.0,
        'flonum': 22.0,
        'number': 22.0
    };

    return defaults[type] || 22.0;
}

module.exports = {
    getAvailableTemplates,
    generateBasicPatch,
    addObject,
    connect
};
