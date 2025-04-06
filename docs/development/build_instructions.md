# v8uiプロジェクト ビルド手順

v8uiプロジェクトは、macOSとWindows両プラットフォームでの開発とビルドをサポートしています。このドキュメントでは、各プラットフォームでのビルド環境の設定と、プロジェクトのビルド手順を詳細に説明します。

## 必要条件

### 共通の要件
- CMake 3.17以上
- C++17対応コンパイラ
- Git
- Python 3.6以上（ビルドスクリプト用）

### macOS特有の要件
- macOS 10.15 (Catalina) 以上
- Xcode 12.0以上とコマンドラインツール
- Homebrew（推奨、依存関係のインストール用）

### Windows特有の要件
- Windows 10以上
- Visual Studio 2019以上（C++ワークロード付き）
- MSVC v142以上のビルドツール

## 依存ライブラリ

v8uiプロジェクトは以下の外部ライブラリに依存しています：

1. **libwebsockets**: WebSocket通信用（バージョン4.2.0以上推奨）
2. **oscpack**: OSCプロトコル実装用
3. **Min-DevKit**: Max/MSP外部オブジェクト開発用

## ビルド環境のセットアップ

### macOS

1. **Xcodeとコマンドラインツールのインストール**:
   ```bash
   xcode-select --install
   ```

2. **Homebrewのインストール**:
   ```bash
   /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
   ```

3. **必要なパッケージのインストール**:
   ```bash
   brew install cmake ninja python
   ```

4. **依存ライブラリのインストール**:
   ```bash
   brew install libwebsockets

   # oscpackとMin-DevKitはプロジェクトに含まれています
   ```

### Windows

1. **Visual Studioのインストール**:
   - [Visual Studioのダウンロードページ](https://visualstudio.microsoft.com/downloads/)から
   - 「C++によるデスクトップ開発」ワークロードを選択

2. **CMakeのインストール**:
   - [CMakeのダウンロードページ](https://cmake.org/download/)から
   - インストール時に「PATH変数に追加する」オプションを選択

3. **Python 3のインストール**:
   - [Python 3のダウンロードページ](https://www.python.org/downloads/windows/)から
   - インストール時に「PATH変数に追加する」オプションを選択

4. **依存ライブラリのインストール**:
   - vcpkgを使用する方法（推奨）：
     ```batch
     git clone https://github.com/Microsoft/vcpkg.git
     cd vcpkg
     bootstrap-vcpkg.bat
     vcpkg install libwebsockets:x64-windows
     ```

## プロジェクトのクローンとビルド

### リポジトリのクローン

```bash
git clone https://github.com/yourusername/v8ui.git
cd v8ui
git submodule update --init --recursive
```

### macOSでのビルド

#### Intel Mac

```bash
mkdir -p build/release
cd build/release
cmake -DCMAKE_BUILD_TYPE=Release -GNinja ../..
ninja
```

#### Apple Silicon (M1/M2)

```bash
mkdir -p build/release
cd build/release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -GNinja ../..
ninja
```

#### Universal Binary (Intel + ARM)

```bash
mkdir -p build/release
cd build/release
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64" -GNinja ../..
ninja
```

### Windowsでのビルド

#### Visual Studio

```batch
mkdir build\release
cd build\release
cmake -DCMAKE_BUILD_TYPE=Release -G "Visual Studio 16 2019" -A x64 ..\..
cmake --build . --config Release
```

#### Ninja (コマンドライン)

```batch
mkdir build\release
cd build\release
cmake -DCMAKE_BUILD_TYPE=Release -G Ninja ..\..
ninja
```

## ビルドオプション

CMakeでのビルド時に以下のオプションを指定できます：

| オプション | 説明 | デフォルト値 |
|------------|------|--------------|
| `-DENABLE_WEBSOCKET=ON/OFF` | WebSocketサポートの有効化/無効化 | `ON` |
| `-DENABLE_TESTS=ON/OFF` | テストのビルド | `OFF` |
| `-DENABLE_PERFORMANCE_METRICS=ON/OFF` | パフォーマンス計測の有効化 | `OFF` |
| `-DUSE_SYSTEM_LIBWEBSOCKETS=ON/OFF` | システムのlibwebsocketsを使用 | `ON` |
| `-DUSE_SYSTEM_OSCPACK=ON/OFF` | システムのoscpackを使用 | `OFF` |

## ビルド成果物

ビルドが成功すると、以下のファイルが生成されます：

### macOS
- `build/release/src/max/min_bridge/llm.mcp.mxo`: Claude Desktop連携用Maxエクスターナル
- `build/release/src/max/min_bridge/osc_bridge.mxo`: OSCブリッジ用Maxエクスターナル

### Windows
- `build\release\src\max\min_bridge\llm.mcp.mxe`: Claude Desktop連携用Maxエクスターナル
- `build\release\src\max\min_bridge\osc_bridge.mxe`: OSCブリッジ用Maxエクスターナル

## Max/MSPへのインストール

### macOS
```bash
cp -R build/release/src/max/min_bridge/*.mxo ~/Documents/Max\ 8/Packages/v8ui/externals/
```

### Windows
```batch
xcopy /E /I /Y build\release\src\max\min_bridge\*.mxe "%USERPROFILE%\Documents\Max 8\Packages\v8ui\externals\"
```

## トラブルシューティング

### 一般的な問題

1. **CMakeが依存ライブラリを見つけられない**:
   - `-DCMAKE_PREFIX_PATH=パス` オプションでライブラリのパスを指定

2. **ビルドエラー: ヘッダファイルが見つからない**:
   - 依存ライブラリが正しくインストールされているか確認
   - `-DCMAKE_INCLUDE_PATH=パス` で追加のインクルードパスを指定

### macOS特有の問題

1. **Undefined symbol エラー**:
   - otool -L でダイナミックライブラリの依存関係を確認
   - install_name_tool でパスを修正

### Windows特有の問題

1. **libwebsocketsのリンクエラー**:
   - vcpkgのtripletが正しいか確認（x64-windowsなど）
   - `-DCMAKE_TOOLCHAIN_FILE=パス/vcpkg.cmake` を指定

## 継続的インテグレーション

GitHub Actionsを使用して、Pull Requestごとに自動ビルドとテストが行われます。設定ファイルは `.github/workflows/` ディレクトリにあります。

## リリースビルド

公式リリース用のビルドは、以下の手順で行います：

```bash
# リリース用ビルドスクリプトの実行
python scripts/build/release_build.py --version X.Y.Z
```

このスクリプトは、macOSとWindowsの両方のビルドを行い、インストール可能なパッケージを生成します。
