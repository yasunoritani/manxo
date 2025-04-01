#!/bin/bash

# OSC Bridge テスト実行スクリプト
# Issue 22: Catchテストフレームワークの環境構築とテスト自動化
# 
# 使用方法: ./run_tests.sh [テストフィルター]
# 例: ./run_tests.sh "M4L lifecycle"  # M4Lライフサイクルテストのみ実行

set -e

# 設定
PROJECT_ROOT="/Users/mymac/v8ui"
BUILD_DIR="${PROJECT_ROOT}/src/min-devkit/osc_bridge/build"
TEST_RESULTS_DIR="${PROJECT_ROOT}/docs/test_results"
DATE_STAMP=$(date +"%Y%m%d_%H%M%S")
TEST_LOG_FILE="${TEST_RESULTS_DIR}/test_result_${DATE_STAMP}.txt"

# 結果ディレクトリの作成
mkdir -p "${TEST_RESULTS_DIR}"

# ビルド実行
echo "🔧 OSC Bridgeテストをビルドしています..."
cd "${PROJECT_ROOT}/src/min-devkit/osc_bridge"
cmake -S . -B "${BUILD_DIR}" -DBUILD_OSC_BRIDGE_TESTS=ON
cmake --build "${BUILD_DIR}" --target test_osc_bridge

# テスト実行
echo "🧪 テストを実行しています..."
cd "${BUILD_DIR}"

# テストバイナリの確認
TEST_BINARY="${BUILD_DIR}/tests/test_osc_bridge"
if [ ! -f "${TEST_BINARY}" ]; then
    echo "❌ エラー: テストバイナリが見つかりませんでした: ${TEST_BINARY}"
    echo "ビルドディレクトリを確認: $(ls -la ${BUILD_DIR})"
    echo "testsディレクトリを確認: $(ls -la ${BUILD_DIR}/tests 2>/dev/null || echo 'testsディレクトリが存在しません')"
    exit 1
fi

# Max/MSPフレームワークのパスを設定
MAX_SDK_PATH="${PROJECT_ROOT}/min-dev/min-devkit/source/min-api/max-sdk-base/c74support"
MAX_FRAMEWORK_PATH="${MAX_SDK_PATH}/max-includes"
JIT_FRAMEWORK_PATH="${MAX_SDK_PATH}/jit-includes"
MSP_FRAMEWORK_PATH="${MAX_SDK_PATH}/msp-includes"

# 現在の作業ディレクトリを保存
CURRENT_DIR=$(pwd)

# テスト実行ディレクトリに移動
cd "$(dirname "${TEST_BINARY}")"

# テスト実行（フィルター付きまたはすべて）
echo "📋 テスト実行ログは ${TEST_LOG_FILE} に保存されます"
{
    echo "=================================="
    echo "OSC Bridge テスト実行レポート"
    echo "実行日時: $(date)"
    echo "=================================="
    echo ""
    echo "環境設定情報:"
    echo "- Max SDKパス: ${MAX_SDK_PATH}"
    echo "- テストバイナリ: $(basename "${TEST_BINARY}")"
    echo "- 実行ディレクトリ: $(pwd)"
    echo ""
    
    if [ -n "$1" ]; then
        echo "🔍 フィルター '$1' でテストを実行"
        # 環境変数を設定してテスト実行
        DYLD_FRAMEWORK_PATH="${MAX_FRAMEWORK_PATH}:${JIT_FRAMEWORK_PATH}:${MSP_FRAMEWORK_PATH}:$DYLD_FRAMEWORK_PATH" \
        DYLD_LIBRARY_PATH="${MAX_FRAMEWORK_PATH}:${JIT_FRAMEWORK_PATH}:${MSP_FRAMEWORK_PATH}:$DYLD_LIBRARY_PATH" \
            "./$(basename "${TEST_BINARY}")" "$1" -s
    else 
        echo "🔍 全テストを実行"
        # 環境変数を設定してテスト実行
        DYLD_FRAMEWORK_PATH="${MAX_FRAMEWORK_PATH}:${JIT_FRAMEWORK_PATH}:${MSP_FRAMEWORK_PATH}:$DYLD_FRAMEWORK_PATH" \
        DYLD_LIBRARY_PATH="${MAX_FRAMEWORK_PATH}:${JIT_FRAMEWORK_PATH}:${MSP_FRAMEWORK_PATH}:$DYLD_LIBRARY_PATH" \
            "./$(basename "${TEST_BINARY}")" -s
    fi
    
    # 終了コードを保存
    TEST_RESULT=$?
    
    echo ""
    echo "=================================="
    if [ ${TEST_RESULT} -eq 0 ]; then
        echo "✅ テスト結果: 成功"
    else
        echo "❌ テスト結果: 失敗"
    fi
    echo "=================================="
    
    # 元のディレクトリに戻る
    cd "${CURRENT_DIR}"
} | tee "${TEST_LOG_FILE}"

# 結果の評価
if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo "✅ すべてのテストが成功しました。"
    echo "📊 テスト結果は ${TEST_LOG_FILE} に保存されました。"
    exit 0
else
    echo "❌ テストが失敗しました。詳細は上記のログを確認してください。"
    echo "📊 テスト結果は ${TEST_LOG_FILE} に保存されました。"
    exit 1
fi
