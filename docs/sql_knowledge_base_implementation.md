# SQL知識ベースと修正提案機能の実装詳細

## 1. 概要

このドキュメントでは、Max/MSP SQL知識ベース検証エンジンの現在の実装状態と、特に修正提案機能の仕組みについて詳細に説明します。

## 2. SQL知識ベースの構造

### 2.1 データベース概要

SQLite3データベース (`max_objects.db`) は以下のテーブルで構成されています：

| テーブル名 | レコード数 | 内容 |
|------------|----------|------|
| max_objects | 561 | Max/MSPオブジェクト情報（名前、タイプ、インレット数、アウトレット数、説明） |
| min_devkit_api | 169 | Min-DevKit API関数情報 |
| connection_patterns | 201 | 接続パターン情報 |
| validation_rules | 105 | 検証ルール情報 |
| api_mapping | 160 | 自然言語意図からAPI関数へのマッピング |

### 2.2 主要テーブルのスキーマ

```sql
-- max_objectsテーブル
CREATE TABLE max_objects (
  id INTEGER PRIMARY KEY,
  name TEXT NOT NULL,
  type TEXT,
  inlets TEXT,  -- JSON形式の配列として格納
  outlets TEXT, -- JSON形式の配列として格納
  description TEXT
);

-- connection_compatibilityテーブル
CREATE TABLE connection_compatibility (
  id INTEGER PRIMARY KEY,
  source_type TEXT NOT NULL,
  target_type TEXT NOT NULL,
  compatibility_level INTEGER,
  notes TEXT
);
```

## 3. 検証エンジンの実装

### 3.1 ValidationEngine クラス

`ValidationEngine`クラスはJavaScript/Min-DevKitコードの検証を担当します。主な機能：

- データベース接続の管理
- オブジェクト情報のキャッシング
- 互換性ルールの読み込み

```javascript
class ValidationEngine {
    constructor () {
        this.db = null;
        this.objectCache = new Map();
        this.compatibility = new Map();
        this.initialized = false;
        this.initPromise = this.initialize();
    }

    // ...その他のメソッド
}
```

### 3.2 コード検証機能

現在の実装では、**主にJavaScriptコードの検証**に焦点が当てられています：

```javascript
validateCode (code, context = 'js') {
    const result = {
        valid: true,
        syntaxErrors: [],
        logicWarnings: [],
        suggestions: []
    };

    // 構文検証 - acornパーサーを使用
    try {
        const ast = acorn.parse(code, {
            ecmaVersion: 2020,
            locations: true,
            sourceType: 'script'
        });

        // 論理チェック（簡易版）
        this.performLogicChecks(ast, code, result);
    } catch (error) {
        // エラー処理
    }

    // コンテキスト固有の検証
    if (context === 'jsui') {
        this.validateJsuiCode(code, result);
    } else if (context === 'js') {
        this.validateJsCode(code, result);
    }

    return result;
}
```

**重要な注意点**: 現在の実装では、C++(Min-DevKit)の構文解析や検証に特化したコードは含まれていません。`min_object`コンテキストの処理も実装されていません。

### 3.3 接続検証機能

オブジェクト間の接続検証は、データベースからの互換性情報を使用して実装されています：

```javascript
async validateConnection (sourceObj, sourceOutlet, destObj, destInlet) {
    await this.ensureInitialized();

    // データベースからオブジェクト情報を取得
    const sourceInfo = this.objectCache.get(sourceObj);
    const destInfo = this.objectCache.get(destObj);

    // 互換性チェック
    // ...
}
```

## 4. 修正提案機能の実装

### 4.1 MCP経由のツール登録

ClaudeAI統合モジュールは、以下のようにMCPツールとして修正提案機能を登録しています：

```javascript
this.mcpServer.registerTool({
  name: 'suggestFixes',
  description: 'Suggest fixes for code issues',
  inputJsonSchema: {
    type: 'object',
    required: ['code', 'issues'],
    properties: {
      code: {
        type: 'string',
        description: 'Code with issues'
      },
      issues: {
        type: 'array',
        description: 'List of issues found during validation',
        items: {
          type: 'object'
        }
      }
    }
  },
  execute: async ({ code, issues }) => {
    // 実行ロジック
    const fix = await this.getFixSuggestion(code, issues);
    return { content: JSON.stringify(fix, null, 2) };
  }
});
```

### 4.2 修正提案の生成プロセス

修正提案は`getFixSuggestion`メソッドで以下のように生成されます：

```javascript
async getFixSuggestion (code, issues) {
  await this._ensureInitialized();

  // 問題がない場合は空の提案
  if (!issues || issues.length === 0) {
    return { has_proposals: false, proposals: [] };
  }

  try {
    // 基本的な修正提案を生成
    let basicProposals = { has_proposals: false, proposals: [] };

    if (this.validationEngine.generateFixProposals) {
      try {
        basicProposals = this.validationEngine.generateFixProposals(code, issues);
      } catch (error) {
        // エラー処理
      }
    }

    // テンプレートを使った修正提案用のプロンプトを準備
    const promptData = {
      code: code,
      issues: issues.map(issue =>
        `- ${issue.severity || 'issue'}: ${issue.description || issue.message || JSON.stringify(issue)}`
      ).join('\n')
    };

    // プロンプトテンプレートを埋める
    const prompt = this.fillTemplate('code_fix', promptData);

    // 結果を返す
    return {
      has_proposals: basicProposals.has_proposals,
      proposals: basicProposals.proposals || [],
      prompt_template: prompt.substring(0, 200) + '...' // プロンプトの一部を返す
    };
  } catch (error) {
    console.error('修正提案生成エラー:', error);
    throw new Error(`修正提案の生成に失敗しました: ${error.message}`);
  }
}
```

### 4.3 テンプレートシステム

修正提案は事前に定義されたテンプレートを使用しています：

```javascript
const DEFAULT_TEMPLATES = {
  'code_fix.txt': `あなたはMax/MSPプログラミングの専門家です。以下のJavaScriptコードに問題があります。コードを詳しく分析し、問題を修正した完全なコードを提供してください。

問題のコード:
{{code}}

検出された問題:
{{issues}}

修正したコード全体を提供してください。変更箇所には簡潔なコメントを入れてください。`,

  'connection_fix.txt': `あなたはMax/MSPプログラミングの専門家です。以下のオブジェクト接続に問題があります。この接続を最適化する方法を提案してください。

  // ...
  `,

  'patch_fix.txt': `あなたはMax/MSPプログラミングの専門家です。以下のパッチに問題があります。パッチの改善方法を提案してください。

  // ...
  `
};
```

## 5. 現在の実装の制限と課題

### 5.1 C++コード検証の制限

現在の実装には以下の制限があります：

1. **C++構文解析の欠如**: JavaScriptの構文解析（acorn）はあるが、C++コードパーサーがない
2. **Min-DevKit固有のチェック不足**: データベースにAPIリストはあるが、それを使ったコード検証ロジックがない
3. **単純なパターンマッチング**: 高度なコード理解ではなく、単純なパターンマッチングに依存

### 5.2 修正提案機能の限界

1. **基本的な提案のみ**: ValidationEngineの`generateFixProposals`の実装詳細が不明確
2. **テンプレートベース**: 固定テンプレートを使用したプロンプト生成に依存
3. **JavaScriptに特化**: C++コードの修正提案に特化したテンプレートが不十分

### 5.3 データベースと検証ロジックの乖離

1. **情報と機能の分離**: データベースに詳細な情報があるにもかかわらず、それを活用する検証ロジックが不足
2. **特にMin-DevKit API**: 169のAPI関数情報があるが、それらの正しい使用を検証するロジックがない

## 6. 今後の改善方向

### 6.1 短期的な改善

1. **パターンベースのC++チェック**: 最も一般的なMin-DevKit APIパターンの簡易チェック実装
2. **C++固有の修正提案テンプレート**: より具体的なC++関連の問題に対応したテンプレート追加

### 6.2 中長期的な改善

1. **C++パーサーの導入**: Clang/LibToolベースのC++コード解析機能の追加
2. **Min-DevKit APIの詳細検証**: データベース情報を活用したAPI使用の詳細検証
3. **コンテキスト認識型修正提案**: 問題のタイプと原因に基づく適応的な修正提案
