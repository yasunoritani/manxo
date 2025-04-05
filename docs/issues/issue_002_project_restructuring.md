# Issue #002: プロジェクト構造の再設計

## 概要

現在のプロジェクト構造は拡張性と保守性に改善の余地があります。新機能の追加や将来的な変更に対応しやすくするために、プロジェクト構造の再設計が必要です。この改善により、開発効率の向上とコードの品質向上を目指します。

## 目的

- 機能ごとに明確に分けられたモジュール構造を確立する
- 依存関係を明確にし、疎結合なコンポーネント設計を実現する
- 外部ライブラリの管理方法を最適化する
- テスト容易性を向上させるために適切なディレクトリ構造を構築する
- モダンなC++の構成手法を取り入れた設計を実現する

## タスク

### 2.1 プロジェクト構造分析と再設計

- [ ] 現在のプロジェクト構造の詳細分析と問題点特定
- [ ] モジュール間の依存関係をマッピングし、改善案の策定
- [ ] 新しいディレクトリ構造と名前空間の設計
- [ ] 外部ライブラリ（libwebsockets、oscpack）の管理方法の確立

### 2.2 基本インフラストラクチャの整備

- [ ] ビルドシステム（CMake）の改善と最適化
- [ ] 共通ユーティリティと基本クラスの整理
- [ ] エラー処理と例外システムの統一
- [ ] ロギングと診断機能の実装

### 2.3 コードリファクタリング計画

- [ ] リファクタリングのための優先順位と段階的計画の策定
- [ ] コンポーネント間のインターフェース定義
- [ ] レガシーコードの段階的移行戦略の確立
- [ ] コードスタイルとドキュメント規約の策定

### 2.4 テスト環境の整備

- [ ] 各モジュールの単体テスト構造の設計
- [ ] 統合テスト環境の構築
- [ ] モックとスタブの実装計画
- [ ] 継続的インテグレーション設定の最適化

## 新しいプロジェクト構造

```
v8ui/
├── CMakeLists.txt                # プロジェクトのルートCMake設定
├── README.md                     # プロジェクト概要
├── docs/                         # ドキュメント
│   ├── api/                      # API仕様書
│   ├── issues/                   # 課題管理
│   └── requirements/             # 要件定義
├── src/                          # ソースコード
│   ├── mcp_client/               # MCP通信クライアント
│   │   ├── CMakeLists.txt
│   │   ├── include/              # 公開ヘッダー
│   │   └── src/                  # 実装
│   ├── min_bridge/               # Min-DevKit連携コンポーネント
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   └── src/
│   ├── osc_bridge/               # OSC連携コンポーネント
│   │   ├── CMakeLists.txt
│   │   ├── include/
│   │   └── src/
│   ├── tools/                    # MCP Tool実装
│   │   ├── CMakeLists.txt
│   │   ├── basic_tools/          # 基本ツール
│   │   ├── audio_tools/          # 音声処理ツール
│   │   └── patch_tools/          # パッチ操作ツール
│   └── utils/                    # 共通ユーティリティ
│       ├── CMakeLists.txt
│       ├── include/
│       └── src/
├── extern/                       # 外部ライブラリ
│   ├── libwebsockets/            # WebSocketライブラリ
│   └── oscpack/                  # OSCライブラリ
├── scripts/                      # ビルドスクリプトなど
│   ├── build_libwebsockets_universal.sh
│   └── upload_issues.py
├── tests/                        # テスト
│   ├── CMakeLists.txt
│   ├── unit/                     # 単体テスト
│   ├── integration/              # 統合テスト
│   └── mocks/                    # モック実装
└── examples/                     # サンプルプロジェクト
    ├── basic_client/             # 基本的なクライアント例
    └── max_patches/              # サンプルMaxパッチ
```

## 技術的詳細

### 依存関係管理の改善

外部ライブラリの管理を改善するために、以下のアプローチを採用します：

1. **libwebsockets**:
   - CMakeでの柔軟な連携（システム版/ローカルビルド選択可能）
   - ユニバーサルバイナリ（Intel/AppleSilicon）ビルドプロセスの最適化
   - バージョン管理の明確化と更新戦略の確立

2. **oscpack**:
   - Gitサブモジュールとしての管理の最適化
   - 必要最小限の依存関係としての統合
   - APIラッパーによる直接依存の最小化

### CMake構成の最適化

```cmake
# プロジェクト全体の基本設定
cmake_minimum_required(VERSION 3.15)
project(v8ui VERSION 1.0.0 LANGUAGES CXX)

# 共通設定とオプション
option(V8UI_USE_SYSTEM_LIBWEBSOCKETS "システムのlibwebsocketsを使用する" OFF)
option(V8UI_BUILD_TESTS "テストをビルドする" ON)
option(V8UI_BUILD_EXAMPLES "サンプルをビルドする" ON)

# C++17を使用
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 依存関係の設定
if(V8UI_USE_SYSTEM_LIBWEBSOCKETS)
    find_package(Libwebsockets REQUIRED)
else()
    add_subdirectory(extern/libwebsockets)
endif()

# OSCPack (サブモジュール)
add_subdirectory(extern/oscpack)

# コンポーネントの追加
add_subdirectory(src/mcp_client)
add_subdirectory(src/min_bridge)
add_subdirectory(src/osc_bridge)
add_subdirectory(src/tools)
add_subdirectory(src/utils)

# テストの追加（オプション）
if(V8UI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# サンプルの追加（オプション）
if(V8UI_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
```

### 名前空間構造

```cpp
// 全体の名前空間
namespace v8ui {

// MCPクライアント
namespace mcp {
    class Client;
    class Message;
    // ...
}

// Min-DevKit連携
namespace min_bridge {
    class MinInterface;
    // ...
}

// OSC連携
namespace osc {
    class OSCBridge;
    class MessageConverter;
    // ...
}

// ツール実装
namespace tools {
    class ToolRegistry;
    class ToolResult;

    // 基本ツール
    namespace basic {
        // ...
    }

    // 音声処理ツール
    namespace audio {
        // ...
    }

    // パッチ操作ツール
    namespace patch {
        // ...
    }
}

// ユーティリティ
namespace utils {
    class Logger;
    class Config;
    // ...
}

} // namespace v8ui
```

## 実装ガイドライン

1. **モジュール間の通信**:
   - 明確に定義されたインターフェースを通じてのみ通信
   - 過度な依存関係を避けるためにイベントベースの通信を優先
   - インターフェース抽象化によるコンポーネント置換容易性の確保

2. **ビルドシステム**:
   - CMakeモジュールの適切な分割と再利用
   - プラットフォーム依存のコードの明確な分離
   - ビルドオプションによる機能の柔軟な有効/無効化

3. **コーディング規約**:
   - 一貫したコーディングスタイルの適用（.clang-formatの整備）
   - ドキュメント規約の確立（Doxygen互換）
   - モダンC++イディオムの積極的採用（RAII, PIMPL, etc.）

4. **テスト戦略**:
   - 各モジュールの機能単位での単体テスト設計
   - モックを活用した依存関係のない単体テスト
   - 継続的インテグレーションでの自動テスト実行

## 移行計画

1. **段階的移行**:
   - 基本インフラストラクチャの整備（2週間）
   - コアコンポーネントの移行（3週間）
   - 周辺機能の段階的リファクタリング（4週間）

2. **既存機能の確保**:
   - 各段階での機能テストによる回帰防止
   - 互換性レイヤーによる段階的移行のサポート
   - 詳細な移行ドキュメントの整備

## 関連リソース

- [現行プロジェクト構造分析ドキュメント](../analysis/current_structure.md)
- [モダンC++プロジェクト構造のベストプラクティス](https://github.com/lefticus/cpp_starter_project)
- [CMakeの効率的な使用方法](https://cliutils.gitlab.io/modern-cmake/)
- [libwebsockets ドキュメント](https://libwebsockets.org/)
- [oscpack GitHub](https://github.com/MariadeAnton/oscpack)

## 完了条件

- 新しいプロジェクト構造が確立され、ディレクトリが整理されている
- すべてのコンポーネントが新構造に移行されている
- 外部ライブラリの管理方法が最適化されている
- CMake設定が改善され、柔軟なビルドオプションが提供されている
- コーディング規約とドキュメント規約が確立されている
- テスト環境が整備され、単体テストと統合テストが実行可能になっている
- すべての既存機能が新構造でも正常に動作することが確認されている

## 優先度とカテゴリ

**優先度**: 高
**カテゴリ**: インフラストラクチャ
