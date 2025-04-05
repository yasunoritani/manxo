# v8ui プロジェクト現状 - 詳細レポート

## プロジェクト概要

v8uiプロジェクトはMax/MSPとClaude Desktopを連携させるためのシステムを開発するプロジェクトです。Min-DevKitを活用してMax環境内からClaude Desktopの機能を利用できるようにすることを目的としています。プロジェクトはC++とMin-DevKit、WebSocket通信、MCPプロトコルを使用して実装されます。

## 最新の状況

### 1. GitHub Issue体系の再構築

プロジェクトのすべての古いIssueをクローズし、新しく整理された4つの主要Issueを作成しました。新しいIssueには「#001」から始まる番号を振り、タイトルにもその番号を明記しています（GitHubの内部番号とは別）。

**新しいIssue一覧**:
- **#001: Min-DevKitとClaude Desktop連携の実装** (#131) - 優先度:最高、カテゴリ:コア機能
- **#002: プロジェクト構造の再設計** (#132) - 優先度:高、カテゴリ:インフラストラクチャ
- **#003: WebSocketクライアントの簡略化** (#133) - 優先度:高、カテゴリ:コア機能
- **#004: MCPツールセットの実装** (#134) - 優先度:高、カテゴリ:コア機能

これらのIssueは以下のツールとスクリプトを使用してGitHubにアップロードされました:
- `scripts/upload_new_issues.py` - GitHubのトークン認証を使用してIssueをアップロード

### 2. プロジェクト構造

プロジェクトは以下の構造で整理されています:

```
v8ui/
├── CMakeLists.txt
├── README.md
├── .github/
│   └── workflows/
│       └── mcp_integration_tests.yml  # GitHub Actionsワークフロー
├── docs/
│   ├── issues/                        # Issue定義ファイル
│   │   ├── issue_001_min_devkit_claude_integration.md
│   │   ├── issue_002_project_restructuring.md
│   │   ├── issue_003_websocket_client_simplification.md
│   │   └── issue_004_mcp_tools_implementation.md
│   ├── requirements/
│   └── project_restructuring_plan.md
├── scripts/
│   └── upload_new_issues.py
└── src/
    ├── mcp-server/
    └── min-devkit/
        └── osc_bridge/
```

### 3. 外部ライブラリの依存関係

プロジェクトは以下の外部ライブラリに依存しています:
- **libwebsockets**: WebSocket通信用ライブラリ（現在はMacOS Universal Binaryビルドスクリプトを使用）
- **oscpack**: OSC通信用ライブラリ（Gitサブモジュールとして管理）

### 4. 今後の開発計画

1. **Issue #001の実装**:
   - Min-DevKitとMCPプロトコルの統合
   - 双方向フィードバックループの確立
   - Max環境情報の収集と提供機能の開発

2. **Issue #002の実装**:
   - 新しいディレクトリ構造の確立
   - CMake設定の改善
   - 外部ライブラリの管理方法の最適化

3. **Issue #003の実装**:
   - 簡略化されたWebSocketクライアントの設計と実装
   - エラー処理と接続安定性の強化
   - コードの最適化とドキュメント作成

4. **Issue #004の実装**:
   - ツールレジストリとディスパッチ機構の設計
   - 基本ツールセットの実装
   - 音楽制作ツールの開発

## 技術的詳細

### 1. MCPクライアント設計

クライアントは以下の機能を提供します:
- WebSocket接続管理
- メッセージの送受信
- エラー処理と回復メカニズム
- ツール呼び出し

```cpp
// 簡略化されたクライアント設計
namespace v8ui::mcp {
    class MCPClient {
    public:
        static MCPClient& getInstance();
        bool connect(const std::string& url, int port, const std::string& path = "/");
        void disconnect();
        bool sendMessage(const json& message);
        // その他の機能...
    };
}
```

### 2. Min-DevKit連携アーキテクチャ

Min-DevKitとMCPを統合するためのブリッジクラス:

```cpp
// ブリッジクラス概要
namespace mcp_mindevkit {
    class MCPMinBridge {
    public:
        static MCPMinBridge& getInstance();
        void registerMCPCall(const std::string& tool_name, std::function<json(const json&)> handler);
        json executeTool(const std::string& tool_name, const json& params);
        // Max操作メソッド...
    };
}
```

### 3. ツール実装構造

MCPツールは以下の構造で設計されています:

```cpp
namespace v8ui::tools {
    struct ToolResult {
        bool success;
        std::string message;
        json data;
    };

    class Tool {
    public:
        virtual std::string getName() const = 0;
        virtual std::string getDescription() const = 0;
        virtual json getParameterSchema() const = 0;
        virtual json getReturnSchema() const = 0;
        virtual ToolResult execute(const json& parameters) = 0;
    };

    class ToolRegistry {
    public:
        static ToolRegistry& getInstance();
        void registerTool(std::unique_ptr<Tool> tool);
        ToolResult callTool(const std::string& name, const json& parameters);
    };
}
```

## 重要な命名規則と用語

- **v8ui**: プロジェクト全体の名前空間
- **MCP**: Model Context Protocol、Claude Desktopとの通信プロトコル
- **Min-DevKit**: Max/MSPの拡張開発キット
- **MCPClient**: MCP通信を担当するクライアントクラス
- **MCPMinBridge**: Min-DevKitとMCPの橋渡しをするクラス
- **Tool**: MCPツールの基底クラス
- **ToolRegistry**: ツールの登録と呼び出しを管理するクラス

## 特記事項

1. プロジェクトのIssue番号は「001」から始まる連番で管理（GitHubの内部番号とは別）
2. ソースコードのディレクトリ構造は今後Issue #002に基づいて再構築予定
3. libwebsocketsとoscpackの管理は外部依存として最適化予定
4. すべての実装はC++17準拠で行う
5. コードスタイルと命名規則は一貫性を保つ

## 確認事項

- リポジトリはクローンされた状態で継続利用されるため、ローカルの作業環境と設定はそのまま引き継がれます
- 環境変数（GITHUB_TOKENなど）も引き継がれます
- 現在まで行った作業内容はすべてリポジトリにコミットされています

以上の情報を元に、次のチャットセッションでもスムーズに作業が継続できるようにしてください。
