/**
 * AI-driven Max Patch Generator
 * Implementation inspired by Text-to-SQL approaches
 */

const path = require('path');
const fs = require('fs/promises');
const Database = require('better-sqlite3');
const { v4: uuidv4 } = require('uuid');
const vectorDB = require('../sql_knowledge_base/vector-db');

// データベース接続
const dbPath = path.join(__dirname, '../sql_knowledge_base/max_knowledge.db');
let db = null;
let vectorSearchEnabled = false;

/**
 * 初期化関数
 */
async function initialize() {
    try {
        db = new Database(dbPath, { readonly: true });
        console.log('SQLデータベースに接続しました');

        // ベクトル検索機能の初期化を試みる
        try {
            vectorSearchEnabled = await vectorDB.initVectorDB();
            if (vectorSearchEnabled) {
                console.log('ベクトル検索機能が有効化されました');
            } else {
                console.log('ベクトル検索機能は利用できません。キーワード検索を使用します。');
            }
        } catch (error) {
            console.warn('ベクトル検索初期化エラー:', error);
            console.log('キーワード検索にフォールバックします');
            vectorSearchEnabled = false;
        }

        return true;
    } catch (error) {
        console.error('データベース接続エラー:', error);
        return false;
    }
}

/**
 * 自然言語をタスクに分解
 * @param {string} naturalLanguage - ユーザーからの自然言語入力
 * @returns {Array} - 分解されたタスク配列
 */
async function decomposeTask(naturalLanguage) {
    // 1. 入力テキストを解析して主要な要素に分解
    const tasks = [];

    // 基本的な分解ロジック（将来的にはNLPを使用して高度化）
    if (naturalLanguage.includes('オーディオ') || naturalLanguage.includes('音声') ||
        naturalLanguage.includes('音')) {
        tasks.push({ type: 'audio', purpose: 'generation', description: naturalLanguage });
    }

    if (naturalLanguage.includes('ビデオ') || naturalLanguage.includes('映像') ||
        naturalLanguage.includes('jitter')) {
        tasks.push({ type: 'video', purpose: 'processing', description: naturalLanguage });
    }

    if (naturalLanguage.includes('シンセ') || naturalLanguage.includes('シンセサイザー')) {
        tasks.push({ type: 'synthesizer', purpose: 'instrument', description: naturalLanguage });
    }

    if (naturalLanguage.includes('エフェクト') || naturalLanguage.includes('フィルター')) {
        tasks.push({ type: 'effect', purpose: 'processing', description: naturalLanguage });
    }

    if (naturalLanguage.includes('UI') || naturalLanguage.includes('インターフェース') ||
        naturalLanguage.includes('コントロール')) {
        tasks.push({ type: 'ui', purpose: 'control', description: naturalLanguage });
    }

    // デフォルトタスク（特定のキーワードがない場合）
    if (tasks.length === 0) {
        tasks.push({ type: 'generic', purpose: 'processing', description: naturalLanguage });
    }

    return tasks;
}

/**
 * 関連するMaxオブジェクトを検索
 * @param {Object} task - 分解されたタスク
 * @returns {Array} - 関連するMaxオブジェクトの配列
 */
async function findRelevantObjects(task) {
    if (!db) return [];

    try {
        // ベクトル検索が有効な場合はセマンティック検索を使用
        if (vectorSearchEnabled) {
            console.log(`タスク「${task.type}」に関連するオブジェクトをセマンティック検索中...`);
            return await vectorDB.semanticSearch(task.description, 20);
        }

        // ベクトル検索が無効な場合は従来のキーワード検索を使用
        let query;
        let params;

        // タスクの種類に基づいてクエリを構築
        switch (task.type) {
            case 'audio':
                query = `SELECT * FROM max_objects WHERE category LIKE ? OR description LIKE ? LIMIT 20`;
                params = ['%audio%', '%audio%'];
                break;
            case 'video':
                query = `SELECT * FROM max_objects WHERE category LIKE ? OR description LIKE ? LIMIT 20`;
                params = ['%jitter%', '%video%'];
                break;
            case 'synthesizer':
                query = `SELECT * FROM max_objects WHERE category LIKE ? OR description LIKE ? OR description LIKE ? LIMIT 20`;
                params = ['%audio%', '%synth%', '%oscillator%'];
                break;
            case 'effect':
                query = `SELECT * FROM max_objects WHERE category LIKE ? OR description LIKE ? LIMIT 20`;
                params = ['%audio%', '%effect%'];
                break;
            case 'ui':
                query = `SELECT * FROM max_objects WHERE is_ui_object = 1 LIMIT 20`;
                params = [];
                break;
            default:
                query = `SELECT * FROM max_objects LIMIT 20`;
                params = [];
        }

        // クエリ実行
        const stmt = db.prepare(query);
        let objects;

        if (params.length > 0) {
            objects = stmt.all(...params);
        } else {
            objects = stmt.all();
        }

        return objects;
    } catch (error) {
        console.error('オブジェクト検索エラー:', error);
        return [];
    }
}

/**
 * オブジェクト間の接続パターンを検索
 * @param {Array} objects - Maxオブジェクトの配列
 * @param {string} description - タスクの自然言語記述
 * @returns {Array} - 接続パターンの配列
 */
async function findConnectionPatterns(objects, description = '') {
    if (!db || objects.length === 0) return [];

    try {
        // ベクトル検索が有効で、説明が提供されている場合はセマンティック検索を使用
        if (vectorSearchEnabled && description) {
            console.log('接続パターンをセマンティック検索中...');
            return await vectorDB.semanticSearchConnectionPatterns(description, 50);
        }

        // 従来のキーワード検索
        // オブジェクト名のリストを作成
        const objectNames = objects.map(obj => obj.name);

        // プレースホルダー作成
        const placeholders = objectNames.map(() => '?').join(',');

        // 接続パターンを検索するクエリ
        const query = `
      SELECT * FROM connection_patterns
      WHERE source_object IN (${placeholders})
         OR destination_object IN (${placeholders})
      LIMIT 50
    `;

        // クエリ実行
        const stmt = db.prepare(query);
        const patterns = stmt.all([...objectNames, ...objectNames]);

        return patterns;
    } catch (error) {
        console.error('接続パターン検索エラー:', error);
        return [];
    }
}

/**
 * 検証ルールに基づいて生成されたパッチを検証
 * @param {Object} patch - 生成されたパッチ
 * @returns {Object} - 検証結果と修正提案
 */
async function validatePatch(patch) {
    if (!db) return { valid: true, suggestions: [] };

    try {
        // 基本的な検証ルールを適用
        const validationResults = {
            valid: true,
            suggestions: []
        };

        // パッチ内のオブジェクトを取得
        const patchObjects = patch.patcher.boxes.map(box => {
            const objectClass = box.box.maxclass || '';
            return { id: box.box.id, class: objectClass };
        });

        // 接続を取得
        const connections = patch.patcher.lines || [];

        // 検証1: 終端されていない出力がないか確認
        const connectedOutlets = new Set();
        connections.forEach(conn => {
            connectedOutlets.add(`${conn.patchline.source[0]}-${conn.patchline.source[1]}`);
        });

        // 検証2: dac~がない場合は警告
        const hasDac = patchObjects.some(obj => obj.class === 'dac~');
        if (!hasDac && patchObjects.some(obj => obj.class.includes('~'))) {
            validationResults.suggestions.push('シグナルオブジェクトがありますが、dac~がありません。音声出力が必要な場合は追加してください。');
        }

        // 検証3: オブジェクト間の互換性をチェック
        for (const connection of connections) {
            const sourceId = connection.patchline.source[0];
            const destId = connection.patchline.destination[0];

            const sourceObj = patchObjects.find(obj => obj.id === sourceId);
            const destObj = patchObjects.find(obj => obj.id === destId);

            if (sourceObj && destObj) {
                // シグナルとメッセージの互換性チェック
                if (sourceObj.class.includes('~') && !destObj.class.includes('~')) {
                    validationResults.valid = false;
                    validationResults.suggestions.push(`シグナルオブジェクト(${sourceObj.class})からメッセージオブジェクト(${destObj.class})への直接接続は避けてください。`);
                }
            }
        }

        return validationResults;
    } catch (error) {
        console.error('パッチ検証エラー:', error);
        return { valid: true, suggestions: [] };
    }
}

/**
 * オブジェクトの最適なレイアウトを生成
 * @param {Array} objects - 配置するオブジェクト
 * @param {Array} connections - オブジェクト間の接続
 * @returns {Object} - 各オブジェクトの座標情報
 */
function generateLayout(objects, connections) {
    // シンプルなグリッドベースのレイアウト
    const layout = {};
    const GRID_SIZE = 80;
    const MARGIN = 30;

    // 階層レベルを計算
    const levels = {};
    const visited = new Set();

    // 接続グラフを構築
    const graph = {};
    objects.forEach(obj => {
        graph[obj.id] = [];
    });

    connections.forEach(conn => {
        const sourceId = conn.source_id;
        const destId = conn.dest_id;
        if (graph[sourceId]) {
            graph[sourceId].push(destId);
        }
    });

    // DFSでレベルを割り当て
    function assignLevel(id, level) {
        if (visited.has(id)) return;
        visited.add(id);

        levels[id] = Math.max(level, levels[id] || 0);

        if (graph[id]) {
            graph[id].forEach(neighbor => {
                assignLevel(neighbor, level + 1);
            });
        }
    }

    // ルートノードからレベル割り当て開始
    const rootNodes = objects.filter(obj => {
        return !connections.some(conn => conn.dest_id === obj.id);
    });

    rootNodes.forEach(root => {
        assignLevel(root.id, 0);
    });

    // レベルごとにオブジェクトをグループ化
    const levelGroups = {};
    objects.forEach(obj => {
        const level = levels[obj.id] || 0;
        if (!levelGroups[level]) {
            levelGroups[level] = [];
        }
        levelGroups[level].push(obj);
    });

    // レベルごとにレイアウト
    const maxLevel = Math.max(...Object.keys(levelGroups).map(k => parseInt(k)));

    for (let level = 0; level <= maxLevel; level++) {
        const objectsInLevel = levelGroups[level] || [];
        const levelWidth = objectsInLevel.length * GRID_SIZE;
        const startX = MARGIN;

        objectsInLevel.forEach((obj, index) => {
            layout[obj.id] = {
                x: startX + index * GRID_SIZE,
                y: MARGIN + level * GRID_SIZE
            };
        });
    }

    return layout;
}

/**
 * テンプレートからパッチを生成
 * @param {string} templateName - テンプレート名
 * @param {Array} objects - 挿入するオブジェクト
 * @param {Array} connections - オブジェクト間の接続
 * @returns {Object} - 生成されたパッチオブジェクト
 */
async function generateFromTemplate(templateName, objects, connections) {
    try {
        // テンプレートファイルの読み込み
        const templatePath = path.join(__dirname, 'templates', `${templateName}.maxpat`);
        const templateContent = await fs.readFile(templatePath, 'utf8');
        const template = JSON.parse(templateContent);

        // オブジェクトレイアウトを生成
        const layout = generateLayout(objects, connections);

        // テンプレートにオブジェクトと接続を追加
        const patchObjects = objects.map((obj, index) => {
            const objId = `obj-${index + 10}`;
            const position = layout[obj.id] || { x: 100 + index * 80, y: 200 };

            return {
                box: {
                    id: objId,
                    maxclass: obj.name,
                    numinlets: obj.inlets || 1,
                    numoutlets: obj.outlets || 1,
                    outlettype: Array(obj.outlets || 1).fill(""),
                    patching_rect: [position.x, position.y, 100, 22],
                    text: obj.name
                }
            };
        });

        // 接続を生成
        const patchConnections = connections.map((conn, index) => {
            const sourceObj = objects.findIndex(o => o.id === conn.source_id);
            const destObj = objects.findIndex(o => o.id === conn.dest_id);

            if (sourceObj === -1 || destObj === -1) return null;

            return {
                patchline: {
                    destination: [`obj-${destObj + 10}`, conn.dest_inlet || 0],
                    source: [`obj-${sourceObj + 10}`, conn.source_outlet || 0]
                }
            };
        }).filter(Boolean);

        // 既存のオブジェクトと接続を残しつつ、新しいものを追加
        template.patcher.boxes = [...template.patcher.boxes, ...patchObjects];

        if (!template.patcher.lines) {
            template.patcher.lines = [];
        }
        template.patcher.lines = [...template.patcher.lines, ...patchConnections];

        return template;
    } catch (error) {
        console.error('テンプレートからのパッチ生成エラー:', error);

        // エラー時はシンプルなパッチを返す
        return {
            patcher: {
                fileversion: 1,
                appversion: {
                    major: 8,
                    minor: 5,
                    revision: 0,
                    architecture: "x64",
                    modernui: 1
                },
                classnamespace: "box",
                rect: [100, 100, 640, 480],
                bglocked: 0,
                openinpresentation: 0,
                default_fontsize: 12.0,
                default_fontface: 0,
                default_fontname: "Arial",
                gridonopen: 1,
                gridsize: [15.0, 15.0],
                gridsnaponopen: 1,
                objectsnaponopen: 1,
                statusbarvisible: 2,
                toolbarvisible: 1,
                lefttoolbarpinned: 0,
                toptoolbarpinned: 0,
                righttoolbarpinned: 0,
                bottomtoolbarpinned: 0,
                toolbars_unpinned_last_save: 0,
                tallnewobj: 0,
                boxanimatetime: 200,
                enablehscroll: 1,
                enablevscroll: 1,
                devicewidth: 0.0,
                description: "",
                digest: "",
                tags: "",
                style: "",
                subpatcher_template: "",
                assistshowspatchername: 0,
                boxes: [{
                    box: {
                        id: "obj-1",
                        maxclass: "comment",
                        numinlets: 1,
                        numoutlets: 0,
                        patching_rect: [50.0, 50.0, 200.0, 20.0],
                        text: "Error: Could not generate patch from template"
                    }
                }],
                lines: []
            }
        };
    }
}

/**
 * 自己修正によるパッチの改善
 * @param {Object} patch - 検証に失敗したパッチ
 * @param {Object} validationResults - 検証結果と修正提案
 * @returns {Object} - 修正されたパッチ
 */
async function selfCorrectPatch(patch, validationResults) {
    // 検証結果に基づいてパッチを修正
    let correctedPatch = JSON.parse(JSON.stringify(patch)); // ディープコピー

    try {
        // 提案に基づいて修正を適用
        for (const suggestion of validationResults.suggestions) {
            if (suggestion.includes('dac~がありません')) {
                // dac~オブジェクトを追加
                const lastY = Math.max(...correctedPatch.patcher.boxes.map(box =>
                    box.box.patching_rect[1] + box.box.patching_rect[3]));

                // 音声関連オブジェクトを見つける
                const audioObjects = correctedPatch.patcher.boxes.filter(box =>
                    box.box.maxclass.includes('~'));

                // dac~オブジェクトを追加
                if (audioObjects.length > 0) {
                    const dacObj = {
                        box: {
                            id: `obj-dac`,
                            maxclass: "dac~",
                            numinlets: 2,
                            numoutlets: 0,
                            patching_rect: [200, lastY + 50, 40, 22],
                            text: "dac~"
                        }
                    };

                    correctedPatch.patcher.boxes.push(dacObj);

                    // 最後の音声オブジェクトからdac~への接続を追加
                    const lastAudioObj = audioObjects[audioObjects.length - 1];

                    const connection = {
                        patchline: {
                            destination: [dacObj.box.id, 0],
                            source: [lastAudioObj.box.id, 0]
                        }
                    };

                    if (!correctedPatch.patcher.lines) {
                        correctedPatch.patcher.lines = [];
                    }

                    correctedPatch.patcher.lines.push(connection);

                    // ステレオの場合は右チャンネルも接続
                    if (lastAudioObj.box.numoutlets > 1) {
                        const connection2 = {
                            patchline: {
                                destination: [dacObj.box.id, 1],
                                source: [lastAudioObj.box.id, 1]
                            }
                        };
                        correctedPatch.patcher.lines.push(connection2);
                    }
                }
            }

            // シグナル->メッセージの不適切な接続を修正
            else if (suggestion.includes('シグナルオブジェクト') && suggestion.includes('メッセージオブジェクト')) {
                // 問題のある接続を特定して削除
                const problematicConnections = correctedPatch.patcher.lines.filter(line => {
                    const sourceId = line.patchline.source[0];
                    const destId = line.patchline.destination[0];

                    const sourceObj = correctedPatch.patcher.boxes.find(box => box.box.id === sourceId);
                    const destObj = correctedPatch.patcher.boxes.find(box => box.box.id === destId);

                    return sourceObj && destObj &&
                           sourceObj.box.maxclass.includes('~') &&
                           !destObj.box.maxclass.includes('~');
                });

                // 不適切な接続を削除
                correctedPatch.patcher.lines = correctedPatch.patcher.lines.filter(line =>
                    !problematicConnections.includes(line)
                );

                // 代わりに snapshot~ オブジェクトを追加して接続
                problematicConnections.forEach((conn, index) => {
                    const sourceId = conn.patchline.source[0];
                    const sourceOutlet = conn.patchline.source[1];
                    const destId = conn.patchline.destination[0];
                    const destInlet = conn.patchline.destination[1];

                    const sourceObj = correctedPatch.patcher.boxes.find(box => box.box.id === sourceId);
                    const destObj = correctedPatch.patcher.boxes.find(box => box.box.id === destId);

                    if (sourceObj && destObj) {
                        // sourceとdestの中間位置を計算
                        const sourceRect = sourceObj.box.patching_rect;
                        const destRect = destObj.box.patching_rect;

                        const snapshotX = (sourceRect[0] + destRect[0]) / 2;
                        const snapshotY = (sourceRect[1] + destRect[1]) / 2;

                        // snapshot~ オブジェクトを追加
                        const snapshotId = `obj-snapshot-${index}`;
                        const snapshotObj = {
                            box: {
                                id: snapshotId,
                                maxclass: "snapshot~",
                                numinlets: 1,
                                numoutlets: 1,
                                outlettype: ["float"],
                                patching_rect: [snapshotX, snapshotY, 60, 22],
                                text: "snapshot~ 100"
                            }
                        };

                        correctedPatch.patcher.boxes.push(snapshotObj);

                        // 新しい接続: source -> snapshot~
                        const conn1 = {
                            patchline: {
                                destination: [snapshotId, 0],
                                source: [sourceId, sourceOutlet]
                            }
                        };

                        // 新しい接続: snapshot~ -> destination
                        const conn2 = {
                            patchline: {
                                destination: [destId, destInlet],
                                source: [snapshotId, 0]
                            }
                        };

                        correctedPatch.patcher.lines.push(conn1, conn2);
                    }
                });
            }
        }

        return correctedPatch;
    } catch (error) {
        console.error('パッチ自己修正エラー:', error);
        return patch; // エラー時は元のパッチを返す
    }
}

/**
 * メインの生成関数
 * @param {string} naturalLanguage - ユーザーの自然言語入力
 * @returns {Object} - 生成されたMaxパッチ
 */
async function generatePatch(naturalLanguage) {
    try {
        // 1. タスクの分解
        const tasks = await decomposeTask(naturalLanguage);

        // 2. タスクごとに関連オブジェクトを検索
        const allObjects = [];
        const allConnections = [];

        for (const task of tasks) {
            const objects = await findRelevantObjects(task);

            if (objects.length > 0) {
                // オブジェクトにIDを割り当て
                const objectsWithIds = objects.map(obj => ({
                    ...obj,
                    id: uuidv4()
                }));

                allObjects.push(...objectsWithIds);

                // オブジェクト間の接続パターンを検索
                const connections = await findConnectionPatterns(objects, task.description);

                // 接続をオブジェクトIDに変換
                const mappedConnections = connections.map(conn => {
                    const sourceObj = objectsWithIds.find(o => o.name === conn.source_object);
                    const destObj = objectsWithIds.find(o => o.name === conn.destination_object);

                    if (sourceObj && destObj) {
                        return {
                            ...conn,
                            source_id: sourceObj.id,
                            dest_id: destObj.id
                        };
                    }
                    return null;
                }).filter(Boolean);

                allConnections.push(...mappedConnections);
            }
        }

        // 3. 適切なテンプレートを選択
        let templateName = 'empty_template';

        if (tasks.some(t => t.type === 'audio')) {
            templateName = 'audio_flow_template';
        } else if (tasks.some(t => t.type === 'video')) {
            templateName = 'video_template';
        }

        // 4. テンプレートからパッチを生成
        let patch = await generateFromTemplate(templateName, allObjects, allConnections);

        // 5. 生成されたパッチを検証
        const validationResults = await validatePatch(patch);

        // 6. 必要に応じてパッチを自己修正
        if (!validationResults.valid) {
            patch = await selfCorrectPatch(patch, validationResults);
        }

        return patch;
    } catch (error) {
        console.error('パッチ生成エラー:', error);
        throw error;
    }
}

/**
 * リソースをクリーンアップ
 */
function cleanup() {
    if (db) {
        db.close();
        console.log('データベース接続を閉じました');
    }

    // ベクトルデータベースをクリーンアップ
    if (vectorSearchEnabled) {
        vectorDB.closeVectorDB();
    }
}

module.exports = {
    initialize,
    generatePatch,
    cleanup
};
