#!/bin/bash
# libwebsockets ユニバーサルバイナリビルドスクリプト
# OSC Bridgeプロジェクト用

set -e  # エラー発生時に停止

BASE_DIR=$(pwd)
SOURCE_DIR="$BASE_DIR/source"

# 必要なディレクトリを作成
mkdir -p build_x86_64
mkdir -p build_arm64
mkdir -p build_universal

echo "===== libwebsockets ユニバーサルバイナリビルド開始 ====="
echo "実行日時: $(date)"

# ソースコードの取得
if [ ! -d "$SOURCE_DIR" ]; then
  echo "💾 libwebsocketsのソースコードを取得しています..."
  mkdir -p "$SOURCE_DIR"
  curl -L https://github.com/warmcat/libwebsockets/archive/refs/tags/v4.3.2.tar.gz -o libwebsockets.tar.gz
  tar xzf libwebsockets.tar.gz -C "$SOURCE_DIR" --strip-components=1
  rm libwebsockets.tar.gz
fi

# x86_64アーキテクチャ用のビルド
echo "🔧 x86_64アーキテクチャ向けのlibwebsocketsをビルドしています..."
cd build_x86_64
cmake -DCMAKE_OSX_ARCHITECTURES=x86_64 \
      -DLWS_WITH_SSL=ON \
      -DLWS_WITH_HTTP2=OFF \
      -DLWS_WITH_MINIMAL_EXAMPLES=OFF \
      -DLWS_WITH_LIBUV=OFF \
      -DCMAKE_INSTALL_PREFIX=./install \
      "$SOURCE_DIR"
make -j4
make install
cd "$BASE_DIR"

# arm64アーキテクチャ用のビルド
echo "🔧 arm64アーキテクチャ向けのlibwebsocketsをビルドしています..."
cd build_arm64
cmake -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DLWS_WITH_SSL=ON \
      -DLWS_WITH_HTTP2=OFF \
      -DLWS_WITH_MINIMAL_EXAMPLES=OFF \
      -DLWS_WITH_LIBUV=OFF \
      -DCMAKE_INSTALL_PREFIX=./install \
      "$SOURCE_DIR"
make -j4
make install
cd "$BASE_DIR"

# ユニバーサルバイナリの作成
echo "🔄 ユニバーサルバイナリを作成しています..."
lipo -create build_x86_64/install/lib/libwebsockets.dylib build_arm64/install/lib/libwebsockets.dylib -output build_universal/libwebsockets.dylib

# シンボリックリンクの作成
cd build_universal
ln -sf libwebsockets.dylib libwebsockets.19.dylib
cd ..

# ヘッダーファイルをコピー
echo "📂 ヘッダーファイルをコピーしています..."
mkdir -p build_universal/include
cp -r build_arm64/install/include/* build_universal/include/

echo "✅ libwebsockets ユニバーサルバイナリのビルドが完了しました"
echo "ライブラリパス: $(pwd)/build_universal/libwebsockets.dylib"
echo "ヘッダーパス: $(pwd)/build_universal/include"
echo "===== ビルド終了 ====="
