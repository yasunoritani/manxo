# Issue #004: MCPツールセットの実装

## 概要

Max環境でのパッチング操作やオブジェクト操作を実現するためのMCPツールセットを実装する必要があります。このツールセットはClaude Desktopから呼び出し可能で、自然言語指示に基づいてMax環境でのパッチング操作を実行できる機能を提供します。

## 目的

- 効果的なMCPツール定義を確立し、Claude Desktopからのアクセスを可能にする
- Maxパッチ操作のための包括的なツールセットを実装する
- 音楽制作支援ツールを開発し、ユーザーの創作プロセスを強化する
- 自然言語指示をMax操作に変換する能力を提供する
- ツールの拡張性を確保し、将来的な機能追加を容易にする

## タスク

### 4.1 ツール基盤の設計と実装

- [ ] ツールレジストリとディスパッチ機構の設計
- [ ] ツール定義と登録メカニズムの実装
- [ ] パラメータ検証と型変換の仕組み開発
- [ ] エラー処理とレポーティング機構の実装

### 4.2 基本ツール実装

- [ ] パッチ管理ツール（作成、保存、読み込み）
- [ ] オブジェクト操作ツール（追加、削除、移動）
- [ ] 接続管理ツール（接続、切断、再配線）
- [ ] プロパティ編集ツール（パラメータ変更、属性設定）

### 4.3 音楽制作ツール実装

- [ ] シンセサイザー生成ツール（アディティブ、FM、サブトラクティブ）
- [ ] エフェクト作成ツール（リバーブ、ディレイ、フィルタ）
- [ ] ミキシングツール（レベル調整、パン、EQ）
- [ ] シーケンシングツール（パターン生成、ループ作成）

### 4.4 ユーティリティツール実装

- [ ] ヘルプとドキュメントツール
- [ ] デバッグと診断ツール
- [ ] 状態管理と履歴ツール
- [ ] テンプレートと再利用ツール

## 技術的詳細

### ツール構造設計

```cpp
namespace v8ui::tools {

// ツール結果構造体
struct ToolResult {
    bool success;
    std::string message;
    json data;

    static ToolResult success(const std::string& msg = "", const json& data = {});
    static ToolResult failure(const std::string& msg, const json& data = {});
};

// ツール基底クラス
class Tool {
public:
    virtual ~Tool() = default;

    // ツール情報
    virtual std::string getName() const = 0;
    virtual std::string getDescription() const = 0;
    virtual json getParameterSchema() const = 0;
    virtual json getReturnSchema() const = 0;

    // ツール実行
    virtual ToolResult execute(const json& parameters) = 0;

protected:
    // パラメータ検証ヘルパー
    bool validateParameters(const json& parameters) const;

    // エラーハンドリングヘルパー
    ToolResult handleError(const std::string& message);
};

// ツールレジストリ
class ToolRegistry {
public:
    static ToolRegistry& getInstance();

    // ツール登録
    void registerTool(std::unique_ptr<Tool> tool);

    // ツール呼び出し
    ToolResult callTool(const std::string& name, const json& parameters);

    // ツール情報
    std::vector<std::string> getRegisteredToolNames() const;
    json getToolDefinitions() const;

private:
    ToolRegistry();
    ~ToolRegistry();

    std::unordered_map<std::string, std::unique_ptr<Tool>> tools_;
};

} // namespace v8ui::tools
```

### ツール定義例

パッチ内にFMシンセサイザーを作成するツールの例:

```cpp
class CreateFMSynthTool : public Tool {
public:
    std::string getName() const override {
        return "createFMSynth";
    }

    std::string getDescription() const override {
        return "FMシンセサイザーパッチを作成します";
    }

    json getParameterSchema() const override {
        return R"({
            "type": "object",
            "required": ["patchId", "position"],
            "properties": {
                "patchId": {
                    "type": "string",
                    "description": "対象のパッチID"
                },
                "position": {
                    "type": "object",
                    "properties": {
                        "x": {"type": "number"},
                        "y": {"type": "number"}
                    }
                },
                "numOperators": {
                    "type": "integer",
                    "minimum": 2,
                    "maximum": 4,
                    "default": 2,
                    "description": "オペレータ数"
                },
                "algorithm": {
                    "type": "integer",
                    "minimum": 1,
                    "maximum": 8,
                    "default": 1,
                    "description": "FMアルゴリズム番号"
                }
            }
        })"_json;
    }

    json getReturnSchema() const override {
        return R"({
            "type": "object",
            "properties": {
                "synthId": {
                    "type": "string",
                    "description": "作成されたシンセサイザーのID"
                },
                "objects": {
                    "type": "array",
                    "items": {
                        "type": "string",
                        "description": "作成されたオブジェクトのID"
                    }
                }
            }
        })"_json;
    }

    ToolResult execute(const json& parameters) override {
        // パラメータ検証
        if (!validateParameters(parameters)) {
            return handleError("無効なパラメータ");
        }

        try {
            // パッチIDの取得
            std::string patchId = parameters["patchId"];

            // 位置情報の取得
            float x = parameters["position"]["x"];
            float y = parameters["position"]["y"];

            // オプションパラメータの取得
            int numOperators = parameters.value("numOperators", 2);
            int algorithm = parameters.value("algorithm", 1);

            // FMシンセサイザーの構築ロジック...
            std::string synthId = "fm_synth_" + generateUniqueId();
            std::vector<std::string> objectIds;

            // 結果の返却
            json resultData = {
                {"synthId", synthId},
                {"objects", objectIds}
            };

            return ToolResult::success("FMシンセサイザーを作成しました", resultData);
        }
        catch (const std::exception& e) {
            return handleError(std::string("FMシンセサイザー作成エラー: ") + e.what());
        }
    }
};
```

### MCP呼び出し例

```json
{
  "type": "tool_call",
  "data": {
    "tool": "createFMSynth",
    "parameters": {
      "patchId": "patch_12345",
      "position": {"x": 100, "y": 200},
      "numOperators": 4,
      "algorithm": 3
    }
  }
}
```

### MCP応答例

```json
{
  "type": "tool_result",
  "data": {
    "success": true,
    "message": "FMシンセサイザーを作成しました",
    "result": {
      "synthId": "fm_synth_78901",
      "objects": ["obj_12345", "obj_12346", "obj_12347", "obj_12348", "obj_12349"]
    }
  }
}
```

## 実装ガイドライン

1. **モジュール性**:
   - 各ツールは独立してテスト可能な設計にする
   - 共通機能を抽出し、ユーティリティとして再利用する
   - ツール間の依存関係を最小限に抑える

2. **一貫したエラー処理**:
   - すべてのツールで統一されたエラー報告メカニズムを使用する
   - 明確でユーザーフレンドリーなエラーメッセージを提供する
   - 内部エラーと外部エラーを適切に区別する

3. **パラメータ検証**:
   - JSONスキーマによる厳格なパラメータ検証を実装する
   - デフォルト値と型変換を適切に処理する
   - 無効な入力に対して明確なエラーメッセージを提供する

4. **ドキュメント**:
   - 各ツールに詳細な説明と使用例を提供する
   - パラメータと戻り値を明確に文書化する
   - エッジケースと制限事項を記載する

## テスト戦略

### ユニットテスト
- 各ツールの独立したテスト
- パラメータ検証とエラー処理のテスト
- エッジケースの検証

### 統合テスト
- モック環境での全ツールセットのテスト
- Claude Desktopとの連携テスト
- エンドツーエンドの操作シナリオテスト

### 互換性テスト
- Max 8.xおよび9.x互換性テスト
- macOS（Intel/AppleSilicon）での動作検証
- パッチバージョン間の互換性テスト

## 完了条件

- 基本ツールセットが実装され、テスト済みであること
- 音楽制作ツールが実装され、サンプルパッチで動作確認できること
- ユーティリティツールが実装され、デバッグとヘルプ機能が動作すること
- すべてのツールがClaude Desktopから呼び出し可能であること
- ツール定義とドキュメントが完成していること
- 80%以上のテストカバレッジを達成していること
- Max 8.xおよび9.xとの互換性が確認されていること

## 優先度とカテゴリ

**優先度**: 高
**カテゴリ**: コア機能
