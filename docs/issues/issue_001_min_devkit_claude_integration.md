# Issue #001: Min-DevKitとClaude Desktop連携の実装

## 概要

Max/MSPの拡張開発キットであるMin-DevKitとClaude Desktopを効果的に連携させるための基盤設計と実装が必要です。この連携により、Claude DesktopのLLM能力をMax環境に統合し、AIによる音楽制作やパッチング支援を実現します。

## 目的

- Min-DevKitのC++インターフェースとMCPプロトコルを統合する
- Maxオブジェクトモデルとパッチ操作のための抽象化レイヤーを設計・実装する
- Claude Desktopと双方向フィードバックループを確立する
- パッチ生成や音楽制作支援のための自然言語処理パイプラインを構築する

## タスク

### 1.1 Min-DevKit APIとMCPの統合

- [ ] Min-DevKitのC++インターフェースとMCPプロトコルの連携仕様策定
- [ ] Min-DevKitオブジェクトモデルとMCPツール定義のマッピング設計
- [ ] Maxオブジェクトとパッチ操作のための抽象化レイヤーの実装
- [ ] カスタムパッチパターンライブラリの設計と実装

### 1.2 Max環境情報の収集と提供

- [ ] 利用可能なMaxオブジェクトとパラメータのインベントリ機能の実装
- [ ] 現在のパッチ状態と構造の分析と送信メカニズムの開発
- [ ] コンテキスト情報を効率的に伝達するためのプロトコルの設計
- [ ] Max環境からClaude Desktopへの情報フロー最適化

### 1.3 Claude指示のMin-DevKit実行変換

- [ ] 自然言語指示からMin-DevKit API呼び出しへの変換ロジックの開発
- [ ] Claudeの出力をMaxパッチ操作に変換するパーサーの実装
- [ ] コード検証と安全な実行メカニズムの構築
- [ ] パッチングパターンのテンプレート化と再利用の仕組み

### 1.4 双方向フィードバックループ

- [ ] 操作結果をClaude Desktopへ伝達するフィードバック機構の実装
- [ ] エラー状態と成功状態を明確に通知するシステムの開発
- [ ] インクリメンタルな操作と訂正のための状態保持メカニズムの実装
- [ ] 進行中の操作の視覚的フィードバックの設計

## 技術的詳細

### Min-DevKitとMCPの対応付け

Min-DevKitのC++インターフェースとMCPプロトコルを統合するための基本アーキテクチャ：

```cpp
// MCPとMin-DevKit連携の核となる構造体
namespace mcp_mindevkit {

// MCP呼び出しをMin-DevKit操作に変換するブリッジ
class MCPMinBridge {
public:
    // インスタンス取得
    static MCPMinBridge& getInstance();

    // MCP呼び出しの登録
    void registerMCPCall(const std::string& tool_name,
                         std::function<json(const json&)> handler);

    // MCPツール実行
    json executeTool(const std::string& tool_name, const json& params);

    // Max/MSPパッチ操作ヘルパー
    bool createObject(const std::string& patch_id,
                      const std::string& object_type,
                      float x, float y,
                      const std::string& args = "");

    bool connectObjects(const std::string& patch_id,
                       const std::string& source_id, int source_outlet,
                       const std::string& dest_id, int dest_inlet);

    // 様々なMax/MSP操作...

    // パッチ状態取得
    json getPatchState(const std::string& patch_id);

    // 結果のフィードバック
    void sendFeedback(const std::string& message, bool success,
                      const json& details = json::object());

private:
    MCPMinBridge();
    ~MCPMinBridge();

    // Min-DevKitオブジェクトへの参照
    std::unordered_map<std::string, void*> max_objects_;

    // 登録されたMCP呼び出し
    std::unordered_map<std::string, std::function<json(const json&)>> mcp_calls_;

    // パッチIDとMin-DevKitパッチオブジェクトのマッピング
    std::unordered_map<std::string, void*> patch_map_;

    // オブジェクトIDとMin-DevKitオブジェクトのマッピング
    std::unordered_map<std::string, void*> object_map_;

    // 最後の操作結果
    json last_result_;
};

} // namespace mcp_mindevkit
```

### MCPツール定義の例

Claude DesktopがMin-DevKit連携で使用できるMCPツールの定義例：

```json
{
  "tools": [
    {
      "name": "createMaxPatch",
      "description": "新しいMaxパッチを作成します",
      "parameters": {
        "type": "object",
        "required": ["name"],
        "properties": {
          "name": {
            "type": "string",
            "description": "パッチの名前"
          },
          "width": {
            "type": "integer",
            "description": "パッチウィンドウの幅（ピクセル）",
            "default": 600
          },
          "height": {
            "type": "integer",
            "description": "パッチウィンドウの高さ（ピクセル）",
            "default": 400
          }
        }
      },
      "returns": {
        "type": "object",
        "properties": {
          "patchId": {
            "type": "string",
            "description": "作成されたパッチの識別子"
          },
          "success": {
            "type": "boolean",
            "description": "操作が成功したかどうか"
          }
        }
      }
    },
    {
      "name": "addMaxObject",
      "description": "パッチにMaxオブジェクトを追加します",
      "parameters": {
        "type": "object",
        "required": ["patchId", "objectType", "x", "y"],
        "properties": {
          "patchId": {
            "type": "string",
            "description": "対象パッチの識別子"
          },
          "objectType": {
            "type": "string",
            "description": "オブジェクトの種類（例: 'cycle~', 'gain~'）"
          },
          "x": {
            "type": "number",
            "description": "X座標"
          },
          "y": {
            "type": "number",
            "description": "Y座標"
          },
          "args": {
            "type": "string",
            "description": "オブジェクトの引数（オプション）"
          }
        }
      },
      "returns": {
        "type": "object",
        "properties": {
          "objectId": {
            "type": "string",
            "description": "作成されたオブジェクトの識別子"
          },
          "success": {
            "type": "boolean",
            "description": "操作が成功したかどうか"
          }
        }
      }
    },
    {
      "name": "connectMaxObjects",
      "description": "2つのMaxオブジェクトを接続します",
      "parameters": {
        "type": "object",
        "required": ["patchId", "sourceId", "sourceOutlet", "destId", "destInlet"],
        "properties": {
          "patchId": {
            "type": "string",
            "description": "対象パッチの識別子"
          },
          "sourceId": {
            "type": "string",
            "description": "出力元オブジェクトの識別子"
          },
          "sourceOutlet": {
            "type": "integer",
            "description": "出力元オブジェクトのアウトレット番号"
          },
          "destId": {
            "type": "string",
            "description": "出力先オブジェクトの識別子"
          },
          "destInlet": {
            "type": "integer",
            "description": "出力先オブジェクトのインレット番号"
          }
        }
      },
      "returns": {
        "type": "object",
        "properties": {
          "connectionId": {
            "type": "string",
            "description": "作成された接続の識別子"
          },
          "success": {
            "type": "boolean",
            "description": "操作が成功したかどうか"
          }
        }
      }
    }
  ]
}
```

### 実装ガイドライン

1. **Min-DevKit拡張アプローチ**:
   - Min-DevKitのクラス継承を活用してMCP連携機能を拡張
   - `min::object`や`min::pd_class`などの適切な基底クラスを活用
   - C++17の機能（std::optional, std::variant）を積極的に活用

2. **スレッドセーフな実装**:
   - Max/MSPとの通信はメインスレッドで行う
   - MCPプロトコル処理は別スレッドで非同期に実行
   - スレッド間通信には適切な同期プリミティブを使用（std::mutex, std::condition_variable）

3. **エラー処理と回復**:
   - 失敗しやすい操作には適切な例外処理と回復メカニズム
   - 無効な入力やエッジケースに対する堅牢なバリデーション
   - エラー詳細のログ記録と分析機能

4. **パフォーマンス最適化**:
   - 処理負荷の高い操作の非同期実行
   - メモリプーリングと効率的なリソース管理
   - 複雑なパッチ操作のバッチ処理

## テスト計画

### 単体テスト
- Min-DevKit APIラッパーの各機能テスト
- MCPツール呼び出し変換機能のテスト
- パラメータバリデーションとエラー処理のテスト

### 統合テスト
- Claude Desktopとの実際の連携テスト
- 複雑なパッチ生成シナリオのエンドツーエンドテスト
- エラー状態と回復メカニズムの検証

### パフォーマンステスト
- 大規模パッチ操作時のレイテンシ測定
- メモリ使用量の評価
- 長時間使用時の安定性テスト

## 関連リソース

- [Min-DevKit GitHub](https://github.com/Cycling74/min-devkit)
- [Max/MSP オブジェクトリファレンス](https://docs.cycling74.com/max8/refpages/index.html)
- [MCP仕様](https://modelcontextprotocol.io/)
- [libwebsocketsドキュメント](https://libwebsockets.org/lws-api-doc-main/html/index.html)
- [v8uiリポジトリ](https://github.com/yasunoritani/v8ui)

## 完了条件

- Min-DevKit APIとMCPプロトコルの統合が実装されている
- Maxパッチの操作と制御を行うMCPツールが実装されている
- Claude Desktopへのフィードバック機構が機能している
- 自然言語指示からMin-DevKit API呼び出しへの変換が実現されている
- カバレッジ80%以上の自動テストが実装されている
- ドキュメントが整備されている

## 優先度とカテゴリ

**優先度**: 最高
**カテゴリ**: コア機能
