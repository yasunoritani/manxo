# Issue #1: プロジェクト構造の再設計

## 概要

現在のプロジェクト構造は複数のLLM対応や不要な機能を含む複雑な実装になっています。AbletonMCPプロジェクトを参考に、Claude Desktop専用のMCP連携に特化したシンプルな構造へ再設計する必要があります。

## 目的

- AbletonMCPの構造と実装手法を分析し、Max環境に適応する
- プロジェクトの新しいディレクトリ構造を設計・実装する
- Min-DevKit開発環境を適切に設定する
- 開発ワークフローを最適化する
- 外部ライブラリの依存関係を整理し、効率的な管理方法を確立する

## タスク

### 1.1 AbletonMCPの分析

- [x] AbletonMCPのコードベース分析（特にMCPサーバー部分）
- [ ] AbletonMCPのツール定義と実装方法の調査
- [ ] Claude Desktopとの連携設定の詳細調査
- [ ] ベストプラクティスと適用可能なパターンの抽出

### 1.2 新しいディレクトリ構造の作成

- [ ] プロジェクトルート構造の作成
- [ ] `src/`ディレクトリ内のコンポーネント分割
- [ ] `docs/`ディレクトリの整備
- [ ] テスト用ディレクトリとフレームワークの設定

### 1.3 開発環境のセットアップ

- [ ] CMakeビルドシステムの設定
- [ ] Min-DevKit依存関係の設定
- [ ] libwebsocketsなど必要ライブラリの追加
- [ ] ビルドとテストの自動化設定

### 1.4 コンポーネント間のインターフェース設計

- [ ] MCPレイヤーと操作レイヤーの間のインターフェース定義
- [ ] 操作レイヤーと実行レイヤーの間のインターフェース定義
- [ ] モジュール間の依存関係の最小化

### 1.5 外部ライブラリの依存関係管理

- [ ] libwebsocketsの依存関係管理とビルドプロセスの最適化
- [ ] oscpackのサブモジュール管理と更新手順の確立
- [ ] 外部ライブラリのバージョン管理とコンパチビリティ確保
- [ ] 依存関係のドキュメント作成

## 技術的詳細

### 推奨するディレクトリ構造

```
max-claude-mcp/
├── src/
│   ├── mcp_client/
│   │   ├── mcp_client.hpp    # MCPクライアントヘッダー
│   │   ├── mcp_client.cpp    # MCPクライアント実装
│   │   ├── websocket.hpp     # WebSocketラッパー
│   │   └── protocol.hpp      # MCPプロトコル定義
│   │
│   ├── max_operations/
│   │   ├── patch.hpp         # パッチ操作API
│   │   ├── object.hpp        # オブジェクト操作API
│   │   ├── connection.hpp    # 接続操作API
│   │   └── patterns/         # 再利用可能なパッチパターン
│   │
│   ├── tools/
│   │   ├── basic_tools.hpp   # 基本ツール定義
│   │   ├── synth_tools.hpp   # シンセ関連ツール
│   │   └── effect_tools.hpp  # エフェクト関連ツール
│   │
│   └── min-devkit/           # Min-DevKit実装（既存）
│
├── include/                   # 公開ヘッダー
├── docs/                      # ドキュメント
├── tests/                     # テスト
│   ├── unit/                  # 単体テスト
│   ├── integration/           # 統合テスト
│   └── e2e/                   # エンドツーエンドテスト
├── examples/                  # サンプルパッチ
├── external/                  # 外部ライブラリ
│   ├── libwebsockets/         # libwebsockets（サブモジュール）
│   └── oscpack/               # oscpack（サブモジュール）
├── scripts/                   # ビルド/依存関係スクリプト
│   ├── build_libwebsockets.sh # libwebsocketsビルドスクリプト
│   └── setup_dependencies.sh  # 依存関係セットアップスクリプト
└── CMakeLists.txt             # CMake設定
```

### CMake設定例（外部ライブラリ対応版）

```cmake
cmake_minimum_required(VERSION 3.19)
project(max_claude_mcp VERSION 0.1.0)

# C++17を使用
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# オプション（libwebsocketsをシステムから探す vs. 同梱版をビルド）
option(USE_SYSTEM_LIBWEBSOCKETS "Use system installed libwebsockets" OFF)
option(USE_SYSTEM_OSCPACK "Use system installed oscpack" OFF)

# Min-DevKit依存関係
find_package(MinDevKit REQUIRED)

# libwebsockets依存関係
if(USE_SYSTEM_LIBWEBSOCKETS)
    find_package(libwebsockets REQUIRED)
else()
    # 同梱されたlibwebsocketsをビルド
    add_subdirectory(external/libwebsockets)
    set(LIBWEBSOCKETS_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/libwebsockets/include")
    set(LIBWEBSOCKETS_LIBRARIES websockets)
endif()

# oscpack依存関係
if(USE_SYSTEM_OSCPACK)
    find_package(oscpack REQUIRED)
else()
    # 同梱されたoscpackをビルド
    add_subdirectory(external/oscpack)
    set(OSCPACK_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/external/oscpack")
    set(OSCPACK_LIBRARIES oscpack)
endif()

# MCPクライアントライブラリ
add_library(mcp_client
    src/mcp_client/mcp_client.cpp
    src/mcp_client/websocket.cpp
)

target_include_directories(mcp_client PUBLIC
    include
    ${LIBWEBSOCKETS_INCLUDE_DIRS}
)
target_link_libraries(mcp_client PRIVATE ${LIBWEBSOCKETS_LIBRARIES})

# Max操作ライブラリ
add_library(max_operations
    src/max_operations/patch.cpp
    src/max_operations/object.cpp
    src/max_operations/connection.cpp
)

target_link_libraries(max_operations PRIVATE MinDevKit)

# MCPツールライブラリ
add_library(mcp_tools
    src/tools/basic_tools.cpp
    src/tools/synth_tools.cpp
    src/tools/effect_tools.cpp
)

target_link_libraries(mcp_tools PRIVATE max_operations mcp_client)

# OSC Bridgeライブラリ
add_library(osc_bridge
    src/osc_bridge/osc_server.cpp
    src/osc_bridge/osc_client.cpp
)

target_include_directories(osc_bridge PUBLIC ${OSCPACK_INCLUDE_DIRS})
target_link_libraries(osc_bridge PRIVATE ${OSCPACK_LIBRARIES})

# メインMax外部オブジェクト
add_max_external(max_claude_mcp
    src/max_claude_mcp.cpp
)

target_link_libraries(max_claude_mcp PRIVATE
    mcp_client
    max_operations
    mcp_tools
    osc_bridge
)

# テスト
enable_testing()
add_subdirectory(tests)
```

### 外部ライブラリの管理方法

#### libwebsockets

1. **ビルドオプション**:
   - ユニバーサルバイナリ（Intel/Apple Silicon）サポート
   - 最小限の機能セットでビルド（WebSocketクライアントのみ）
   - スタティックリンクによる依存関係の簡素化

2. **提供機能**:
   - WebSocketクライアント接続（Claude Desktop MCP）
   - TLS/SSL対応（セキュア接続）
   - 非同期/イベント駆動モデル

3. **バージョン管理**:
   - 推奨バージョン: 4.3.x以上
   - git submodule経由で特定のコミットにピン止め
   - `scripts/update_libwebsockets.sh`で更新プロセスを自動化

#### oscpack

1. **用途**:
   - OSCメッセージの送受信
   - Max/MSPとの通信インターフェース

2. **変更点**:
   - ユニバーサルバイナリ（Intel/Apple Silicon）対応パッチ
   - CMake構成の最適化
   - スレッドセーフな実装への拡張

3. **バージョン管理**:
   - oscpack 1.1.0ベースに独自修正を適用
   - git submodule経由で管理
   - パッチセットとして変更を分離管理

## 外部ライブラリのバージョン要件

| ライブラリ | 最低バージョン | 推奨バージョン | 注意事項 |
|-----------|--------------|--------------|---------|
| libwebsockets | 4.0.0 | 4.3.2 | TLS/SSL対応必須、v4.0.0未満はサポート外 |
| oscpack | 1.1.0 | 1.1.0+ | 独自パッチを適用したバージョンを使用 |
| Min-DevKit | 8.1.0 | 8.2.0+ | Max 8.x系列と互換性のあるバージョン |

## AbletonMCPからの移植ガイド

AbletonMCPからMax環境へ移植する際の主要な考慮点：

1. **MCPサーバー構造**: AbletonMCPのサーバー部分は参考にしつつ、Min-DevKit用に再設計
2. **ツール定義**: 音楽制作ツールはMaxのオブジェクトモデルに合わせて再定義
3. **操作抽象化**: Ableton APIとMaxのAPIの違いを抽象化層で吸収
4. **エラーハンドリング**: Max特有のエラー状態やフィードバック方法を考慮
5. **外部ライブラリ依存**: 依存関係の最小化と互換性の確保

## 関連リソース

- [AbletonMCP GitHub](https://github.com/ahujasid/ableton-mcp)
- [Min-DevKit ドキュメント](https://github.com/Cycling74/min-devkit)
- [MCP仕様](https://modelcontextprotocol.io/)
- [CMake公式ドキュメント](https://cmake.org/documentation/)
- [libwebsockets ドキュメント](https://libwebsockets.org/lws-api-doc-main/html/index.html)
- [oscpack GitHub](https://github.com/RossBencina/oscpack)

## 完了条件

- プロジェクトの新しいディレクトリ構造が整備されている
- CMakeビルドシステムが正常に動作する
- AbletonMCPの主要コンポーネントの分析が完了している
- コンポーネント間のインターフェースが明確に定義されている
- 外部ライブラリ（libwebsockets, oscpack）の依存関係が適切に管理されている
- ビルドプロセスが複数プラットフォーム（macOS Intel/Apple Silicon）で動作する
