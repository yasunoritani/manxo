# v8ui プロジェクト現状 - 詳細レポート

## プロジェクト概要

v8uiプロジェクトはMax/MSPとClaude Desktopを連携させるためのシステムを開発するプロジェクトです。Min-DevKitを活用してMax環境内からClaude Desktopの機能を利用できるようにすることを目的としています。プロジェクトはC++とMin-DevKit、WebSocket通信、MCPプロトコルを使用して実装されます。

## 最新の状況と再設計について

プロジェクトは途中で再設計が行われ、Issue体系が変更されました。当初は#1〜#10までの10個のIssueが計画されていましたが、現在は#001〜#004の4つの主要Issueに再構成されています。ただし、古いIssue体系（#27〜#31）の一部も残っており、混在した状態になっています。

### 1. GitHub Issue体系の再構築

プロジェクトのすべての古いIssueをクローズし、新しく整理された4つの主要Issueを作成しました。新しいIssueには「#001」から始まる番号を振り、タイトルにもその番号を明記しています（GitHubの内部番号とは別）。

**新しいIssue一覧**:
- **#001: Min-DevKitとClaude Desktop連携の実装** (#131) - 優先度:最高、カテゴリ:コア機能
- **#002: プロジェクト構造の再設計** (#132) - 優先度:高、カテゴリ:インフラストラクチャ
- **#003: WebSocketクライアントの簡略化** (#133) - 優先度:高、カテゴリ:コア機能
- **#004: MCPツールセットの実装** (#134) - 優先度:高、カテゴリ:コア機能

**古いIssue（一部残存）**:
- **#27: WebSocketサポートの実装** - 実装済み
- **#28: Claude/LLM MCPコネクタの実装** - 進行中
- **#29: WebSocketセキュリティ** - 未着手
- **#30: パフォーマンス最適化** - 未着手
- **#31: 高度なルーティング** - 未着手

### 2. 現在のプロジェクト構造

プロジェクトは以下の構造で整理されています:

```
v8ui/
├── .github/                      # GitHub関連ファイル
│   └── workflows/                # GitHub Actions
├── docs/                         # ドキュメント
│   ├── issues/                   # 新Issue定義ファイル (#001〜#004)
│   ├── requirements/             # 要件定義
│   └── project_current_state.md  # 現状レポート（このファイル）
├── issues/                       # 古いIssueファイル (#27〜#31)
├── libwebsockets/                # WebSocket通信ライブラリ
├── oscpack/                      # OSC通信ライブラリ
├── scripts/                      # ユーティリティスクリプト
├── src/                          # ソースコード
│   ├── mcp-server/               # Node.js MCPサーバー
│   ├── min-devkit/               # Min-DevKit関連実装
│   │   └── osc_bridge/           # OSC通信とWebSocket連携
│   ├── protocol/                 # プロトコル定義
│   ├── max-osc-bridge/           # 旧OSCブリッジ実装
│   └── max-bridge/               # 旧Max連携実装
└── tests/                        # テストコード
```

### 3. 外部ライブラリの依存関係

プロジェクトは以下の外部ライブラリに依存しています:
- **libwebsockets**: WebSocket通信用ライブラリ（MacOS Universal Binaryビルドスクリプトを使用）
- **oscpack**: OSC通信用ライブラリ（Gitサブモジュールとして管理）
- **nlohmann/json**: C++用JSONライブラリ
- **Min-DevKit**: Max/MSP拡張開発キット

### 4. 各Issueの実装状況

#### #001: Min-DevKitとClaude Desktop連携の実装
- **進捗状況**: 30%
- **実装済み**:
  - Min-DevKit基本セットアップ
  - 簡単なMax操作のための基盤実装
- **未実装**:
  - Max環境情報の収集と提供機能
  - Claude指示のMin-DevKit実行変換
  - 双方向フィードバックループの完成

#### #002: プロジェクト構造の再設計
- **進捗状況**: 40%
- **実装済み**:
  - 基本ディレクトリ構造の整理
  - 新しいIssue体系の確立
- **未実装**:
  - CMake設定の完全な最適化
  - 外部ライブラリ管理の改善
  - コーディング規約の確立

#### #003: WebSocketクライアントの簡略化
- **進捗状況**: 70%
- **実装済み**:
  - 基本的なWebSocketクライアント機能
  - Claude Desktop専用の通信機能
- **未実装**:
  - エラー処理の完全な改善
  - 安定性テストと最適化
  - 全ドキュメントの作成

#### #004: MCPツールセットの実装
- **進捗状況**: 20%
- **実装済み**:
  - ツールレジストリの基本設計
  - 一部の基本ツール実装
- **未実装**:
  - 包括的なツールセットの実装
  - 音楽制作固有ツールの開発
  - ツールのドキュメントと例

### 5. 主要コンポーネントの実装状況

#### Min-DevKit OSC Bridge
- **ファイル**: src/min-devkit/osc_bridge/
- **状況**: 活発に開発中
- **機能**: OSC通信とWebSocketを統合し、Claude DesktopとMaxの橋渡しを行う
- **主要ファイル**:
  - llm.mcp.cpp: Claude DesktopとのWebSocket通信
  - osc_bridge.cpp: OSC通信の基本実装
  - mcp.websocket_client.hpp: WebSocketクライアント実装

#### MCP Server
- **ファイル**: src/mcp-server/
- **状況**: 基本実装完了
- **機能**: Node.jsベースのMCPサーバー、Claude DesktopとOSC通信の仲介
- **主要ファイル**:
  - index.js: MCPサーバーのエントリーポイント
  - lib/osc-client.js: OSCクライアント機能
  - lib/tools.js: MCPツール定義

### 6. 今後の開発計画

1. **Issue #001の優先実装**:
   - Min-DevKitとMCPプロトコルの統合を完成させる
   - Max環境情報の収集と提供機能を開発する
   - 双方向フィードバックループを確立する

2. **Issue #003の完成**:
   - WebSocketクライアントのエラー処理を強化する
   - 安定性テストを実施する
   - ドキュメントを完成させる

3. **Issue #002の継続**:
   - CMake設定を最適化する
   - 外部ライブラリ管理を改善する
   - コードのリファクタリングを進める

4. **Issue #004の本格化**:
   - ツールセットの実装を拡張する
   - 音楽制作固有ツールを開発する
   - テストとドキュメントを充実させる

## 技術的詳細

### 1. WebSocketクライアント設計

現在の実装では、libwebsocketsを使用してWebSocketクライアントを実装しています。これは`llm.mcp.cpp`と`mcp.websocket_client.hpp`に実装されています。

```cpp
// 簡略化されたクライアント設計（現在の実装から）
class llm_client {
public:
    // メッセージ受信コールバック
    std::function<void(const std::string&)> on_message;

    // 接続管理
    bool connect(const std::string& url);
    void disconnect();

    // メッセージ送信
    bool send_message(const json& message);
    // ...
};
```

### 2. Min-DevKit連携アーキテクチャ

Min-DevKitとの連携は現在開発中ですが、基本的なアーキテクチャは以下のようになっています:

```cpp
// Min-DevKit連携の基本設計案
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

### 3. MCPツール実装構造

MCPツールは以下の構造で設計されていますが、実装はまだ初期段階です:

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

## 特記事項と課題

1. **再設計の影響**:
   - プロジェクトの再設計によりIssue体系が混在している
   - 古いIssue（#27〜#31）と新しいIssue（#001〜#004）の整合性をとる必要がある
   - 一部のコードは再設計前の設計に基づいており、リファクタリングが必要

2. **優先度と依存関係**:
   - Issue #001（Min-DevKit連携）が最優先だが、#003（WebSocketクライアント）の安定化も重要
   - ツール実装（#004）はMin-DevKit連携（#001）に依存している
   - プロジェクト構造の再設計（#002）は他の作業と並行して進める必要がある

3. **技術的課題**:
   - libwebsocketsのユニバーサルバイナリ対応（Intel/AppleSilicon）
   - Max内部APIへのMin-DevKitを通じたアクセス方法の最適化
   - マルチスレッド処理とMax/MSPの同期モデルの整合性確保

4. **不足している部分**:
   - 包括的なテスト計画と実行
   - ユーザー向けドキュメントの作成
   - パッケージ化とリリースプロセスの確立

## 次のステップ

1. Issue体系を完全に統一し、古いIssueを適切に整理する
2. #001と#003の実装を優先的に進める
3. プロジェクト構造（#002）の最適化を継続する
4. テスト計画とドキュメント作成のためのIssueを新たに作成することを検討する
