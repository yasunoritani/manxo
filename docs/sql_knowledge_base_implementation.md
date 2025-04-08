# LLM-SQL-MinDevKit 連携システム実装詳細

## 1. システム概要

このシステムは、以下の3つの主要コンポーネントの連携により、C++の詳細な知識がなくてもMaxプロジェクトを開発できる環境を実現します：

1. **大規模言語モデル (LLM)**: Claude Desktop
2. **SQL知識ベース**: MinDevKitとMax/MSPの情報リポジトリ
3. **MinDevKit**: C++ベースのMax/MSP開発キット

![システム概念図](../assets/system_concept.png)

### 1.1 システムの主要目的

* **知識の橋渡し**: C++やMinDevKitの詳細な知識がなくても開発可能に
* **適切なAPI選択**: SQL知識ベースを参照してLLMが適切なMinDevKit APIを選択
* **検証と修正**: コード、接続、パッチの妥当性検証と修正提案
* **効率的な開発**: 繰り返し作業の自動化と知識の活用

## 2. SQL知識ベースの役割と構造

### 2.1 知識ベースの役割

SQLデータベースは**LLMの意思決定を支援する知識源**として機能し、以下を可能にします：

* MinDevKit APIの適切な選択と使用方法の提示
* オブジェクト間の有効な接続パターンの識別
* 一般的なエラーパターンと修正方法の提供
* 自然言語の意図からAPIへの変換支援

### 2.2 データベース構造

SQLite3データベース (`max_objects.db`) には以下の情報が構造化されています：

| テーブル名 | レコード数 | 内容と役割 |
|------------|----------|------|
| min_devkit_api | 169 | **MinDevKit API関数情報**<br>・関数名、パラメータ、返り値<br>・使用例とコンテキスト<br>・一般的なエラーパターン |
| max_objects | 561 | **Max/MSPオブジェクト情報**<br>・名前、タイプ、インレット/アウトレット情報<br>・シグナルタイプと互換性<br>・使用上の注意点 |
| connection_patterns | 201 | **接続パターン情報**<br>・有効な接続組み合わせ<br>・最適な接続順序<br>・パフォーマンス情報 |
| validation_rules | 105 | **検証ルール情報**<br>・コード品質基準<br>・パターンマッチングルール<br>・セキュリティガイドライン |
| api_mapping | 160 | **自然言語からAPI変換**<br>・意図とAPIの対応関係<br>・コンテキスト固有の用語マッピング |

### 2.3 知識ベースへのアクセス例

```javascript
// MinDevKit API情報の取得例
db.all(
  'SELECT name, parameters, return_type, usage_example FROM min_devkit_api WHERE context = ?',
  ['audio_processing'],
  (err, rows) => {
    // LLMに情報提供するためのフォーマット処理
  }
);
```

## 3. LLMとSQL知識ベースの連携

### 3.1 連携フロー

1. **ユーザーの意図理解**: LLMがユーザーの自然言語指示を理解
2. **知識ベース検索**: 意図に基づき適切なSQL検索を実行
3. **コンテキスト活用**: 検索結果をLLMのプロンプトコンテキストに追加
4. **コード生成・検証**: MinDevKit APIを活用したコード生成と検証
5. **フィードバック**: 結果をユーザーに提示し、必要に応じて反復

### 3.2 MCPプロトコルを活用した実装

MCP（Model Context Protocol）を用いて、LLMとSQL知識ベース間の連携を実現：

```javascript
// ツール登録例
this.mcpServer.registerTool({
  name: 'queryMinDevKitAPI',
  description: '指定した機能やコンテキストに関連するMinDevKit API情報を取得',
  inputJsonSchema: {
    type: 'object',
    required: ['functionality'],
    properties: {
      functionality: {
        type: 'string',
        description: '実現したい機能（例: "オーディオ処理", "UIコントロール"）'
      },
      context: {
        type: 'string',
        description: '使用コンテキスト（例: "max_object", "audio_processing"）'
      }
    }
  },
  execute: async ({ functionality, context }) => {
    // SQL知識ベースから関連APIを検索
    const apis = await this.getRelevantAPIs(functionality, context);
    return { content: JSON.stringify(apis, null, 2) };
  }
});
```

## 4. MinDevKit連携の実装

### 4.1 API選択と適用

LLMは以下の手順でMinDevKit APIを選択・適用します：

1. **ユーザー要件の分析**: 実現したい機能を理解
2. **SQL知識ベース検索**: 適切なAPI候補を特定
3. **最適化**: コンテキストに最適なAPI選択
4. **コード生成**: MinDevKit APIを使用したC++コード生成
5. **検証**: 生成コードの整合性チェック

### 4.2 実装例

```javascript
async function suggestMinDevKitImplementation(requirement) {
  // 1. 要件からキーワード抽出
  const keywords = extractKeywords(requirement);

  // 2. 関連するAPIを検索
  const apis = await db.all(
    'SELECT * FROM min_devkit_api WHERE context LIKE ? OR keywords LIKE ?',
    [`%${keywords[0]}%`, `%${keywords[1]}%`]
  );

  // 3. LLMにコンテキスト提供してコード生成
  const prompt = createPromptWithAPIs(requirement, apis);
  const generatedCode = await generateCodeWithLLM(prompt);

  // 4. 生成コードの検証
  const validationResult = validateMinDevKitCode(generatedCode);

  return {
    code: generatedCode,
    used_apis: apis.map(a => a.name),
    validation: validationResult
  };
}
```

## 5. 修正提案機能の実装

### 5.1 LLMベースの修正提案プロセス

修正提案は以下のプロセスで生成されます：

1. **問題検出**: コードパターン分析と知識ベース参照による問題検出
2. **コンテキスト構築**: 問題、コード、関連APIのコンテキスト作成
3. **LLM活用**: コンテキストに基づく修正案の生成
4. **修正適用**: 修正内容の適用と再検証

### 5.2 実装コード

```javascript
async function getFixProposal(code, issues) {
  // 1. 関連するAPIと修正パターンの検索
  const relevantAPIs = await findRelevantAPIs(code);
  const fixPatterns = await findFixPatterns(issues);

  // 2. LLMへのプロンプト作成
  const promptData = {
    code: code,
    issues: issues.map(issue => formatIssue(issue)).join('\n'),
    relevant_apis: formatAPIs(relevantAPIs),
    fix_patterns: formatFixPatterns(fixPatterns)
  };

  // 3. テンプレートにデータを適用
  const prompt = fillTemplate('code_fix', promptData);

  // 4. LLMによる修正提案生成
  return {
    has_proposals: true,
    proposals: await generateProposalsWithLLM(prompt),
    context: {
      apis_referenced: relevantAPIs.map(api => api.name),
      patterns_applied: fixPatterns.map(pattern => pattern.id)
    }
  };
}
```

## 6. システムの強みと制限

### 6.1 強み

1. **知識の集約と再利用**: SQL知識ベースによりMinDevKitとMax/MSPの専門知識を集約
2. **LLMの文脈理解活用**: 自然言語からAPIへの変換を効率化
3. **繰り返し作業の自動化**: 一般的なパターンの自動適用
4. **C++の知識なしで開発可能**: 直接MinDevKit APIを活用

### 6.2 現在の制限と課題

1. **完全なC++構文解析の欠如**: パターンマッチベースの検証に限定
2. **限定的なAPI適用範囲**: 特定のパターンに対応したAPI選択のみ
3. **変更検出の限界**: コード変更の影響範囲の分析能力の制限

## 7. 今後の発展方向

1. **API利用パターンの拡充**: より多様なユースケースへの対応
2. **SQL知識ベースの継続的更新**: 新APIや最適パターンの追加
3. **ユーザーフィードバックの取り込み**: 実際の利用パターンを基にした改善
4. **コンテキスト認識の向上**: より細粒度なコンテキスト解析と適用
