#!/bin/bash
# oscpackライブラリをx86_64とarm64の両方のアーキテクチャでビルドするスクリプト

# 現在のディレクトリを保存
CURRENT_DIR=$(pwd)

# x86_64ビルド用のディレクトリを作成してビルド
mkdir -p build_x86_64
cd build_x86_64
cmake .. -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
make

# arm64ビルド用のディレクトリを作成してビルド
cd ..
mkdir -p build_arm64
cd build_arm64
cmake .. -DCMAKE_OSX_ARCHITECTURES=arm64 -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
make

# ユニバーサルバイナリを作成するディレクトリ
cd ..
mkdir -p build_universal

# lipo を使って x86_64 と arm64 のライブラリを結合し、ユニバーサルバイナリを作成
lipo -create build_x86_64/liboscpack.a build_arm64/liboscpack.a -output build_universal/liboscpack.a

# ユニバーサルバイナリをメインのbuildディレクトリにコピー
mkdir -p build
cp build_universal/liboscpack.a build/

echo "Universal binary for oscpack created successfully at build/liboscpack.a"
echo "Deployment target: macOS 10.13 for both architectures (x86_64, arm64)"

# 元のディレクトリに戻る
cd $CURRENT_DIR
