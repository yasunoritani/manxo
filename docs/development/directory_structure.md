# v8uiプロジェクト ディレクトリ構造

## 現状と課題

現在のv8uiプロジェクトのディレクトリ構造は、初期開発段階での試行錯誤により若干の不整合が生じています。新しいIssue体系（#001〜#009）と機能的要件に合わせて、明確で一貫性のあるディレクトリ構造への再編が必要です。

## 理想的なディレクトリ構造

以下に、Issue #002（プロジェクト構造の再設計）に基づく理想的なディレクトリ構造を提案します：

```
v8ui/
├── src/                            # ソースコード
│   ├── core/                       # コア機能・共通ライブラリ
│   │   ├── platform/               # クロスプラットフォーム抽象化 (#005)
│   │   │   ├── common/             # 共通インターフェース
│   │   │   ├── mac/                # macOS固有実装
│   │   │   └── win/                # Windows固有実装
│   │   ├── utils/                  # ユーティリティ関数
│   │   ├── logging/                # ロギング機能
│   │   └── types/                  # 共通データ型定義
│   │
│   ├── communication/              # 通信関連 (#003)
│   │   ├── websocket/              # WebSocketクライアント
│   │   │   ├── client.hpp          # クライアント定義
│   │   │   └── client.cpp          # クライアント実装
│   │   ├── osc/                    # OSC関連
│   │   │   ├── parser.hpp/cpp      # OSCパーサー
│   │   │   └── bridge.hpp/cpp      # OSCブリッジ
│   │   └── mcp/                    # MCPプロトコル
│   │       ├── protocol.hpp/cpp    # プロトコル定義
│   │       └── messages.hpp/cpp    # メッセージ型
│   │
│   ├── max/                        # Max統合 (#001)
│   │   ├── min_bridge/             # Min-DevKit連携
│   │   │   ├── llm.mcp.cpp         # Claude連携オブジェクト
│   │   │   └── osc_bridge.cpp      # OSCブリッジオブジェクト
│   │   ├── objects/                # カスタムMaxオブジェクト
│   │   └── api/                    # Max API抽象化
│   │
│   ├── mcp/                        # MCP機能 (#004)
│   │   ├── tools/                  # MCPツールセット
│   │   │   ├── registry.hpp/cpp    # ツールレジストリ
│   │   │   ├── base_tool.hpp/cpp   # 基本ツール定義
│   │   │   └── max_tools/          # Max環境操作ツール
│   │   ├── claude/                 # Claude Desktop連携
│   │   └── services/               # MCP関連サービス
│   │
│   ├── security/                   # セキュリティ対策 (#007)
│   │   ├── auth/                   # 認証・承認
│   │   ├── crypto/                 # 暗号化機能
│   │   └── validation/             # 入力検証
│   │
│   └── performance/                # パフォーマンス (#009)
│       ├── metrics.hpp/cpp         # パフォーマンス測定
│       └── optimizations/          # 最適化コンポーネント
│
├── tests/                          # テスト
│   ├── unit/                       # ユニットテスト
│   ├── integration/                # 統合テスト
│   └── performance/                # パフォーマンステスト
│
├── docs/                           # ドキュメント
│   ├── issues/                     # Issue詳細
│   ├── design/                     # 設計ドキュメント
│   ├── api/                        # API仕様
│   └── user/                       # ユーザードキュメント
│
├── scripts/                        # ビルド・デプロイメントスクリプト
│   ├── build/                      # ビルドスクリプト
│   ├── deploy/                     # デプロイスクリプト
│   └── ci/                         # CI/CD設定
│
├── external/                       # 外部依存ライブラリ
│   ├── libwebsockets/              # WebSocketライブラリ
│   ├── oscpack/                    # OSCパッケージ
│   └── min-devkit/                 # Min-DevKit
│
└── build/                          # ビルド出力 (gitignore)
    ├── debug/                      # デバッグビルド
    └── release/                    # リリースビルド
```

## 主要コンポーネントの説明

### 1. コア (src/core/)

クロスプラットフォーム抽象化レイヤーや共通ユーティリティなど、プロジェクト全体で利用される基盤的なコードを配置します。Issue #005に関連する要素は主にここに実装されます。

### 2. 通信 (src/communication/)

WebSocketクライアント、OSC通信、MCPプロトコル関連のコード。Issue #003の主要部分がここに実装されます。

### 3. Max統合 (src/max/)

Max/MSPとの連携コード。Min-DevKitを使ったエクスターナルやMax API抽象化層を含みます。Issue #001の中心的な実装はここに配置されます。

### 4. MCP (src/mcp/)

MCPツールセットと関連サービス。Issue #004の実装がここに集約されます。

### 5. セキュリティ (src/security/)

認証や暗号化などのセキュリティ関連機能。Issue #007の実装はここに配置されます。

### 6. パフォーマンス (src/performance/)

パフォーマンス測定と最適化関連のコード。Issue #009の実装がここに集約されます。

## 移行計画

現在の構造から新しいディレクトリ構造への移行は、以下の手順で行います：

1. **基本的なディレクトリ構造の作成**
   - 新しいディレクトリツリーを作成
   - `.gitkeep`ファイルで空ディレクトリを維持

2. **ファイルの段階的移行**
   - 関連する機能ごとにファイルを移動
   - パスとインクルード参照の更新
   - 各移行後にビルドテスト

3. **古いディレクトリの整理**
   - 移行完了後、不要になったディレクトリの削除
   - 残存ファイルの確認と適切な配置

## ビルドシステムの調整

CMakeファイルも新しいディレクトリ構造に合わせて更新が必要です：

```cmake
# 例: 新しいディレクトリ構造に対応したCMake設定

# コアライブラリ
add_library(v8ui_core
  src/core/platform/common/platform.cpp
  src/core/platform/${PLATFORM_DIR}/platform_impl.cpp
  src/core/utils/string_utils.cpp
  # ...その他のファイル
)

# 通信ライブラリ
add_library(v8ui_communication
  src/communication/websocket/client.cpp
  src/communication/osc/parser.cpp
  src/communication/osc/bridge.cpp
  src/communication/mcp/protocol.cpp
  # ...その他のファイル
)

# Max統合
add_library(v8ui_max MODULE
  src/max/min_bridge/llm.mcp.cpp
  src/max/min_bridge/osc_bridge.cpp
  # ...その他のファイル
)

# 依存関係の設定
target_link_libraries(v8ui_max
  PRIVATE
    v8ui_core
    v8ui_communication
    # ...その他の依存関係
)
```

## 命名規則

プロジェクト全体での一貫した命名規則も確立します：

1. **ディレクトリ名**: 小文字とアンダースコアを使用 (例: `max_tools`)
2. **ファイル名**:
   - ヘッダ: 小文字とアンダースコア、`.hpp`拡張子 (例: `websocket_client.hpp`)
   - 実装: 小文字とアンダースコア、`.cpp`拡張子 (例: `websocket_client.cpp`)
3. **クラス名**: キャメルケース (例: `WebSocketClient`)
4. **名前空間**: 小文字 (例: `v8ui::communication::websocket`)

## まとめ

提案されたディレクトリ構造は、v8uiプロジェクトの論理的な組織化を実現し、Issue間の関係性を明確にします。クロスプラットフォーム対応やセキュリティなどの横断的な関心事も適切に配置され、将来の拡張性も考慮されています。
