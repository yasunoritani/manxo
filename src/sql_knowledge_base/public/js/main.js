/**
 * Max/MSP知識ベースUI用JavaScriptファイル
 */

// APIエンドポイントベースURL
const API_BASE_URL = 'http://localhost:3000/api';

// DOM要素
const searchInput = document.getElementById('search-input');
const searchType = document.getElementById('search-type');
const searchButton = document.getElementById('search-button');
const tabButtons = document.querySelectorAll('.tab-button');
const tabContents = document.querySelectorAll('.tab-content');
const modal = document.getElementById('detail-modal');
const modalContent = document.getElementById('modal-content');
const closeButton = document.querySelector('.close-button');
const categoriesList = document.getElementById('categories-list');

// 検索結果を表示する要素
const resultElements = {
    objects: document.getElementById('objects-results'),
    functions: document.getElementById('functions-results'),
    patterns: document.getElementById('patterns-results'),
    rules: document.getElementById('rules-results'),
    mappings: document.getElementById('mappings-results')
};

// カウント要素
const countElements = {
    objects: document.getElementById('objects-count'),
    functions: document.getElementById('functions-count'),
    patterns: document.getElementById('patterns-count'),
    rules: document.getElementById('rules-count'),
    mappings: document.getElementById('mappings-count')
};

// ページ読み込み時の初期化
document.addEventListener('DOMContentLoaded', () => {
    // カテゴリを読み込む
    loadCategories();

    // ヘルスチェック
    checkApiHealth();
});

// 検索ボタンクリック時のイベント
searchButton.addEventListener('click', () => {
    performSearch();
});

// Enterキー押下時の検索実行
searchInput.addEventListener('keypress', (e) => {
    if (e.key === 'Enter') {
        performSearch();
    }
});

// タブ切り替え
tabButtons.forEach(button => {
    button.addEventListener('click', () => {
        const tabName = button.dataset.tab;

        // アクティブなタブを更新
        tabButtons.forEach(btn => btn.classList.remove('active'));
        tabContents.forEach(content => content.classList.remove('active'));

        button.classList.add('active');
        document.getElementById(`${tabName}-tab`).classList.add('active');
    });
});

// モーダルを閉じる
closeButton.addEventListener('click', () => {
    modal.style.display = 'none';
});

// モーダル外クリックで閉じる
window.addEventListener('click', (e) => {
    if (e.target === modal) {
        modal.style.display = 'none';
    }
});

/**
 * APIのヘルスチェック
 */
async function checkApiHealth () {
    try {
        const response = await fetch(`${API_BASE_URL}/health`);
        const data = await response.json();

        if (data.status === 'ok') {
            console.log('API接続OK:', data.message);
        } else {
            console.error('APIステータスエラー:', data);
        }
    } catch (error) {
        console.error('API接続エラー:', error);
    }
}

/**
 * カテゴリを読み込む
 */
async function loadCategories () {
    try {
        const response = await fetch(`${API_BASE_URL}/max-objects/categories`);
        const categories = await response.json();

        // カテゴリリストを作成
        if (categories && categories.length > 0) {
            categoriesList.innerHTML = '';

            categories.forEach(category => {
                if (category) {
                    const categoryItem = document.createElement('div');
                    categoryItem.classList.add('category-item');
                    categoryItem.textContent = category;
                    categoryItem.addEventListener('click', () => {
                        searchInput.value = '';
                        searchType.value = 'objects';
                        loadObjectsByCategory(category);
                    });

                    categoriesList.appendChild(categoryItem);
                }
            });
        } else {
            categoriesList.innerHTML = '<div class="no-results">カテゴリがありません</div>';
        }
    } catch (error) {
        console.error('カテゴリ読み込みエラー:', error);
        categoriesList.innerHTML = '<div class="error">カテゴリの読み込みに失敗しました</div>';
    }
}

/**
 * カテゴリ別のオブジェクトを読み込む
 */
async function loadObjectsByCategory (category) {
    try {
        const response = await fetch(`${API_BASE_URL}/max-objects?category=${encodeURIComponent(category)}`);
        const objects = await response.json();

        // 結果をクリア
        clearAllResults();

        // オブジェクト結果を表示
        displayResults('objects', objects);

        // タブを切り替え
        tabButtons.forEach(btn => btn.classList.remove('active'));
        tabContents.forEach(content => content.classList.remove('active'));

        document.querySelector('[data-tab="objects"]').classList.add('active');
        document.getElementById('objects-tab').classList.add('active');

    } catch (error) {
        console.error('オブジェクト読み込みエラー:', error);
    }
}

/**
 * 検索を実行する
 */
async function performSearch () {
    const query = searchInput.value.trim();
    const type = searchType.value;

    if (!query) {
        alert('検索キーワードを入力してください');
        return;
    }

    // 検索中の表示
    Object.values(resultElements).forEach(element => {
        element.innerHTML = '<div class="loading">検索中...</div>';
    });

    try {
        const response = await fetch(`${API_BASE_URL}/search`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json'
            },
            body: JSON.stringify({ query, type })
        });

        const results = await response.json();

        // 結果をクリア
        clearAllResults();

        // 結果を表示
        if (type === 'all') {
            // 全タイプの結果を表示
            displayResults('objects', results.objects);
            displayResults('functions', results.functions);
            displayResults('patterns', results.patterns);
            displayResults('rules', results.rules);
            displayResults('mappings', results.mappings);

            // 結果総数を確認
            const totalResults =
                results.objects.length +
                results.functions.length +
                results.patterns.length +
                results.rules.length +
                results.mappings.length;

            if (totalResults === 0) {
                Object.values(resultElements).forEach(element => {
                    element.innerHTML = '<div class="no-results">検索結果がありません</div>';
                });
            }
        } else {
            // 特定タイプの結果のみを表示
            const typeResults = results[type] || [];
            displayResults(type, typeResults);

            if (typeResults.length === 0) {
                resultElements[type].innerHTML = '<div class="no-results">検索結果がありません</div>';
            }

            // そのタブを表示
            tabButtons.forEach(btn => btn.classList.remove('active'));
            tabContents.forEach(content => content.classList.remove('active'));

            document.querySelector(`[data-tab="${type}"]`).classList.add('active');
            document.getElementById(`${type}-tab`).classList.add('active');
        }
    } catch (error) {
        console.error('検索エラー:', error);
        Object.values(resultElements).forEach(element => {
            element.innerHTML = '<div class="error">検索中にエラーが発生しました</div>';
        });
    }
}

/**
 * 検索結果を表示する
 */
function displayResults (type, results) {
    const container = resultElements[type];
    const countElement = countElements[type];

    // 結果数を更新
    countElement.textContent = results.length;

    // 結果がない場合
    if (!results || results.length === 0) {
        container.innerHTML = '';
        return;
    }

    // 結果カードを生成
    container.innerHTML = '';

    results.forEach(item => {
        const card = document.createElement('div');
        card.classList.add('result-card');

        // タイプに応じたカード内容を生成
        switch (type) {
            case 'objects':
                card.innerHTML = `
          <h3>${escapeHtml(item.name)}</h3>
          <p>${escapeHtml(item.description || '説明なし')}</p>
          <div class="result-meta">
            <span class="tag">${escapeHtml(item.category || 'その他')}</span>
          </div>
        `;
                card.addEventListener('click', () => showObjectDetail(item));
                break;

            case 'functions':
                card.innerHTML = `
          <h3>${escapeHtml(item.function_name)}</h3>
          <p>${escapeHtml(item.description || '説明なし')}</p>
          <div class="result-meta">
            <span class="tag">${escapeHtml(item.return_type || 'void')}</span>
          </div>
        `;
                card.addEventListener('click', () => showFunctionDetail(item));
                break;

            case 'patterns':
                card.innerHTML = `
          <h3>${escapeHtml(item.source_object)} → ${escapeHtml(item.destination_object)}</h3>
          <p>${escapeHtml(item.description || '説明なし')}</p>
          <div class="result-meta">
            ${item.is_recommended ? '<span class="tag">推奨</span>' : ''}
            ${item.audio_signal_flow ? '<span class="tag">オーディオ</span>' : ''}
          </div>
        `;
                card.addEventListener('click', () => showPatternDetail(item));
                break;

            case 'rules':
                card.innerHTML = `
          <h3>${escapeHtml(item.rule_type)}</h3>
          <p>${escapeHtml(item.description || '説明なし')}</p>
          <div class="result-meta">
            <span class="tag">${escapeHtml(item.severity || 'info')}</span>
          </div>
        `;
                card.addEventListener('click', () => showRuleDetail(item));
                break;

            case 'mappings':
                card.innerHTML = `
          <h3>${escapeHtml(item.natural_language_intent)}</h3>
          <p>${escapeHtml(item.transformation_template || '変換なし')}</p>
          <div class="result-meta">
            <span class="tag">${escapeHtml(item.function_name || 'マッピングなし')}</span>
          </div>
        `;
                card.addEventListener('click', () => showMappingDetail(item));
                break;
        }

        container.appendChild(card);
    });
}

/**
 * すべての結果をクリアする
 */
function clearAllResults () {
    Object.values(resultElements).forEach(element => {
        element.innerHTML = '';
    });

    Object.values(countElements).forEach(element => {
        element.textContent = '0';
    });
}

/**
 * Maxオブジェクトの詳細を表示
 */
function showObjectDetail (object) {
    let html = `
    <div class="detail-view">
      <h2>${escapeHtml(object.name)}</h2>

      <div class="section">
        <h3>基本情報</h3>
        <p>${escapeHtml(object.description || '説明なし')}</p>
        <p><strong>カテゴリ:</strong> ${escapeHtml(object.category || '未分類')}</p>
        <p><strong>互換性:</strong> ${escapeHtml(object.version_compatibility || '不明')}</p>
      </div>

      <div class="section">
        <h3>入出力情報</h3>
        <p><strong>インレット:</strong> ${formatJsonOrText(object.inlets)}</p>
        <p><strong>アウトレット:</strong> ${formatJsonOrText(object.outlets)}</p>
      </div>

      <div class="section">
        <h3>その他</h3>
        <p><strong>UIオブジェクト:</strong> ${object.is_ui_object ? 'はい' : 'いいえ'}</p>
        <p><strong>非推奨:</strong> ${object.is_deprecated ? 'はい' : 'いいえ'}</p>
        ${object.alternative ? `<p><strong>代替オブジェクト:</strong> ${escapeHtml(object.alternative)}</p>` : ''}
      </div>
    </div>
  `;

    showModal(html);
}

/**
 * Min-DevKit API関数の詳細を表示
 */
function showFunctionDetail (func) {
    let html = `
    <div class="detail-view">
      <h2>${escapeHtml(func.function_name)}</h2>

      <div class="section">
        <h3>シグネチャ</h3>
        <pre>${escapeHtml(func.signature || '不明')}</pre>
      </div>

      <div class="section">
        <h3>基本情報</h3>
        <p>${escapeHtml(func.description || '説明なし')}</p>
        <p><strong>戻り値の型:</strong> ${escapeHtml(func.return_type || 'void')}</p>
        <p><strong>互換性:</strong> ${escapeHtml(func.version_compatibility || '不明')}</p>
      </div>

      <div class="section">
        <h3>パラメータ</h3>
        <p>${formatJsonOrText(func.parameters)}</p>
      </div>

      <div class="section">
        <h3>使用例</h3>
        <pre>${escapeHtml(func.example_usage || '例なし')}</pre>
      </div>
    </div>
  `;

    showModal(html);
}

/**
 * 接続パターンの詳細を表示
 */
function showPatternDetail (pattern) {
    let html = `
    <div class="detail-view">
      <h2>${escapeHtml(pattern.source_object)} → ${escapeHtml(pattern.destination_object)}</h2>

      <div class="section">
        <h3>接続詳細</h3>
        <p>${escapeHtml(pattern.description || '説明なし')}</p>
        <p><strong>ソースアウトレット:</strong> ${pattern.source_outlet}</p>
        <p><strong>デスティネーションインレット:</strong> ${pattern.destination_inlet}</p>
      </div>

      <div class="section">
        <h3>その他の情報</h3>
        <p><strong>推奨パターン:</strong> ${pattern.is_recommended ? 'はい' : 'いいえ'}</p>
        <p><strong>オーディオシグナルフロー:</strong> ${pattern.audio_signal_flow ? 'はい' : 'いいえ'}</p>
        ${pattern.performance_impact ? `<p><strong>パフォーマンスへの影響:</strong> ${escapeHtml(pattern.performance_impact)}</p>` : ''}
        ${pattern.compatibility_issues ? `<p><strong>互換性の問題:</strong> ${escapeHtml(pattern.compatibility_issues)}</p>` : ''}
      </div>
    </div>
  `;

    showModal(html);
}

/**
 * 検証ルールの詳細を表示
 */
function showRuleDetail (rule) {
    let html = `
    <div class="detail-view">
      <h2>${escapeHtml(rule.rule_type)}</h2>

      <div class="section">
        <h3>ルール情報</h3>
        <p>${escapeHtml(rule.description || '説明なし')}</p>
        <p><strong>パターン:</strong> ${escapeHtml(rule.pattern)}</p>
        <p><strong>重要度:</strong> ${escapeHtml(rule.severity || 'info')}</p>
      </div>

      <div class="section">
        <h3>推奨対応</h3>
        <p>${escapeHtml(rule.suggestion || '特になし')}</p>
        ${rule.example_fix ? `<pre>${escapeHtml(rule.example_fix)}</pre>` : ''}
      </div>

      ${rule.context_requirements ? `
      <div class="section">
        <h3>コンテキスト要件</h3>
        <p>${escapeHtml(rule.context_requirements)}</p>
      </div>
      ` : ''}
    </div>
  `;

    showModal(html);
}

/**
 * API意図マッピングの詳細を表示
 */
function showMappingDetail (mapping) {
    let html = `
    <div class="detail-view">
      <h2>${escapeHtml(mapping.natural_language_intent)}</h2>

      <div class="section">
        <h3>マッピング情報</h3>
        <p><strong>関連API関数:</strong> ${escapeHtml(mapping.function_name || '未マッピング')}</p>
      </div>

      <div class="section">
        <h3>変換テンプレート</h3>
        <pre>${escapeHtml(mapping.transformation_template || '変換テンプレートなし')}</pre>
      </div>

      ${mapping.context_requirements ? `
      <div class="section">
        <h3>コンテキスト要件</h3>
        <p>${escapeHtml(mapping.context_requirements)}</p>
      </div>
      ` : ''}
    </div>
  `;

    showModal(html);
}

/**
 * モーダル表示
 */
function showModal (content) {
    modalContent.innerHTML = content;
    modal.style.display = 'block';
}

/**
 * JSONをフォーマットして表示する（文字列の場合はそのまま）
 */
function formatJsonOrText (json) {
    try {
        if (!json) return '情報なし';

        // 既にオブジェクトの場合
        if (typeof json === 'object') {
            return `<pre>${escapeHtml(JSON.stringify(json, null, 2))}</pre>`;
        }

        // 文字列の場合はJSONとしてパースを試みる
        const parsed = JSON.parse(json);
        return `<pre>${escapeHtml(JSON.stringify(parsed, null, 2))}</pre>`;
    } catch (e) {
        // パースに失敗した場合は文字列として表示
        return escapeHtml(json);
    }
}

/**
 * HTMLエスケープ
 */
function escapeHtml (str) {
    if (!str) return '';
    return String(str)
        .replace(/&/g, '&amp;')
        .replace(/</g, '&lt;')
        .replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;')
        .replace(/'/g, '&#039;');
}
