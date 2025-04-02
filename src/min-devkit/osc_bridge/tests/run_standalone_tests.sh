#!/bin/bash

# OSC Bridge スタンドアロンテスト実行スクリプト
# Min-DevKit依存なしのテストを実行するための改良版スクリプト

# カラー設定
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 現在のディレクトリを保存
CURRENT_DIR=$(pwd)

# スクリプトのディレクトリを取得
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"

# oscpackのルートディレクトリ（相対パスで参照）
OSCPACK_ROOT="$SCRIPT_DIR/../../../../oscpack"

# テストビルドディレクトリ（プロジェクトルートからの相対パス）
BUILD_DIR="$SCRIPT_DIR/../build"

# 実行パラメータ
VERBOSE=0
FORCE_REBUILD=0

# 引数の解析
for arg in "$@"
do
    case $arg in
        -v|--verbose)
        VERBOSE=1
        shift
        ;;
        -f|--force-rebuild)
        FORCE_REBUILD=1
        shift
        ;;
        *)
        # 不明な引数
        echo -e "${YELLOW}警告: 不明な引数 - $arg${NC}"
        shift
        ;;
    esac
done

# ビルドディレクトリが存在しない場合、または--force-rebuildが指定された場合に作成/再構築
if [ ! -d "$BUILD_DIR" ] || [ $FORCE_REBUILD -eq 1 ]; then
    echo -e "${BLUE}スタンドアロンテストをビルドしています...${NC}"
    
    # ビルドディレクトリが存在する場合は削除
    if [ -d "$BUILD_DIR" ]; then
        rm -rf "$BUILD_DIR"
    fi
    
    # ビルドディレクトリの作成
    mkdir -p "$BUILD_DIR"
    
    # CMakeの設定とビルド
    cd "$BUILD_DIR" || exit 1
    cmake .. || { echo -e "${RED}CMake設定エラー${NC}"; exit 1; }
    make || { echo -e "${RED}ビルドエラー${NC}"; exit 1; }
fi

# テストバイナリへのパス
TEST_BINARY="$BUILD_DIR/tests/test_osc_bridge_standalone"
if [ ! -f "$TEST_BINARY" ]; then
    echo -e "${RED}❌ エラー: テストバイナリが見つかりませんでした: ${TEST_BINARY}${NC}"
    exit 1
fi

# テストの実行
echo -e "${BLUE}スタンドアロンテストを実行しています...${NC}"
cd "$SCRIPT_DIR" || exit 1

if [ $VERBOSE -eq 1 ]; then
    # 詳細出力
    $TEST_BINARY -v
else
    # コンパクト出力（セクションごとに分割）
    $TEST_BINARY -s
fi

# 実行結果の確認
TEST_RESULT=$?
if [ $TEST_RESULT -eq 0 ]; then
    echo -e "${GREEN}✅ すべてのテストが成功しました！${NC}"
else
    echo -e "${RED}❌ テストが失敗しました（終了コード: $TEST_RESULT）${NC}"
fi

# 元のディレクトリに戻る
cd "$CURRENT_DIR" || exit 1

exit $TEST_RESULT
