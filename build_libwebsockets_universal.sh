#!/bin/bash
# libwebsocketsのユニバーサルバイナリを作成するスクリプト

# 色の設定
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}=====================================${NC}"
echo -e "${BLUE}libwebsocketsユニバーサルビルドツール${NC}"
echo -e "${BLUE}=====================================${NC}"

# Homebrewがインストールされているか確認
if ! command -v brew &> /dev/null; then
    echo -e "${RED}エラー: Homebrewがインストールされていません${NC}"
    echo "詳細: https://brew.sh/"
    exit 1
fi

# libwebsocketsがインストールされているか確認
if ! brew list libwebsockets &> /dev/null; then
    echo -e "${YELLOW}libwebsocketsをインストールします...${NC}"
    brew install libwebsockets
fi

# 必要なディレクトリを作成
WORK_DIR=$(mktemp -d)
OUTPUT_DIR="$PWD/libwebsockets_universal"
mkdir -p "$OUTPUT_DIR"

echo -e "${GREEN}作業ディレクトリ: $WORK_DIR${NC}"
echo -e "${GREEN}出力ディレクトリ: $OUTPUT_DIR${NC}"

cd "$WORK_DIR" || exit 1

# libwebsocketsのソースを取得
echo -e "${YELLOW}libwebsocketsのソースコードをクローンしています...${NC}"
git clone https://github.com/warmcat/libwebsockets.git
cd libwebsockets || exit 1

# ARM64ビルド
echo -e "${YELLOW}ARM64アーキテクチャ向けにビルドしています...${NC}"
mkdir -p build_arm64
cd build_arm64 || exit 1
cmake .. -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_INSTALL_PREFIX="$WORK_DIR/install_arm64"
make -j4
make install
cd ..

# x86_64ビルド
echo -e "${YELLOW}x86_64アーキテクチャ向けにビルドしています...${NC}"
mkdir -p build_x86_64
cd build_x86_64 || exit 1
cmake .. -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_INSTALL_PREFIX="$WORK_DIR/install_x86_64"
make -j4
make install
cd ..

# ユニバーサルバイナリを作成
echo -e "${YELLOW}ユニバーサルバイナリを作成しています...${NC}"
mkdir -p "$OUTPUT_DIR/lib"
lipo -create \
    "$WORK_DIR/install_arm64/lib/libwebsockets.dylib" \
    "$WORK_DIR/install_x86_64/lib/libwebsockets.dylib" \
    -output "$OUTPUT_DIR/lib/libwebsockets.dylib"

# ヘッダーファイルをコピー
echo -e "${YELLOW}ヘッダーファイルをコピーしています...${NC}"
mkdir -p "$OUTPUT_DIR/include"
cp -r "$WORK_DIR/install_arm64/include/"* "$OUTPUT_DIR/include/"

# 必要に応じてシンボリックリンクを作成
cd "$OUTPUT_DIR/lib" || exit 1
ln -sf libwebsockets.dylib libwebsockets.19.dylib

echo -e "${GREEN}ビルド完了！ユニバーサルバイナリは $OUTPUT_DIR に作成されました${NC}"
echo -e "${YELLOW}CMakeListsでこのパスをlibwebsocketsのパスとして設定してください${NC}"

# 作業ディレクトリを削除
rm -rf "$WORK_DIR"
