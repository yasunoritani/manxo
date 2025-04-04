#!/bin/bash

# OSC Bridge テスト実行スクリプト
# Issue 22: Catchテストフレームワークの環境構築とテスト自動化
# Issue 24: Min-DevKit依存分離とスタンドアロンテスト環境の追加
#
# 使用方法: ./run_tests.sh [オプション] [テストフィルター]
# オプション:
#   --standalone    Min-DevKit非依存のスタンドアロンテストを実行
#   --legacy        Min-DevKit依存の従来のテストを実行（デフォルト）
#   --all           両方のテスト環境でテストを実行
#
# 例:
#   ./run_tests.sh                     # 従来のテストを実行
#   ./run_tests.sh --standalone        # スタンドアロンテストを実行
#   ./run_tests.sh "M4L lifecycle"     # 従来のテストからM4Lライフサイクルテストのみ実行
#   ./run_tests.sh --all               # 両方のテスト環境でテストを実行

set -e

# 設定
# スクリプトの場所からプロジェクトルートを取得
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="${SCRIPT_DIR}"
BUILD_DIR="${PROJECT_ROOT}/src/min-devkit/osc_bridge/build"
TEST_RESULTS_DIR="${PROJECT_ROOT}/docs/test_results"
DATE_STAMP=$(date +"%Y%m%d_%H%M%S")
TEST_LOG_FILE="${TEST_RESULTS_DIR}/test_result_${DATE_STAMP}.txt"

# テストモード（デフォルトは従来のMin-DevKit依存テスト）
TEST_MODE="legacy"
TEST_FILTER=""

# パラメータの解析
for arg in "$@"; do
  case $arg in
    --standalone)
      TEST_MODE="standalone"
      shift
      ;;
    --legacy)
      TEST_MODE="legacy"
      shift
      ;;
    --all)
      TEST_MODE="all"
      shift
      ;;
    *)
      # フィルター引数と判断
      TEST_FILTER="$arg"
      ;;
  esac
done

# 結果ディレクトリの作成
mkdir -p "${TEST_RESULTS_DIR}"

# ビルド設定とテスト実行関数
build_and_run_legacy_tests() {
  echo "🔧 OSC Bridge従来テスト（Min-DevKit依存）をビルドしています..."
  cd "${PROJECT_ROOT}/src/min-devkit/osc_bridge"
  cmake -S . -B "${BUILD_DIR}" -DBUILD_OSC_BRIDGE_TESTS=ON
  cmake --build "${BUILD_DIR}" --target test_osc_bridge

  # テスト実行
  echo "🧪 従来のテスト環境でテストを実行しています..."
  cd "${BUILD_DIR}"

  # テストバイナリの確認
  TEST_BINARY="${BUILD_DIR}/tests/test_osc_bridge"
  if [ ! -f "${TEST_BINARY}" ]; then
    echo "❌ エラー: テストバイナリが見つかりませんでした: ${TEST_BINARY}"
    echo "ビルドディレクトリを確認: $(ls -la ${BUILD_DIR})"
    echo "testsディレクトリを確認: $(ls -la ${BUILD_DIR}/tests 2>/dev/null || echo 'testsディレクトリが存在しません')"
    return 1
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
  echo "📋 従来のテスト結果は ${TEST_LOG_FILE} に保存されます"
  {
    echo "================================="
    echo "OSC Bridge従来テスト実行レポート（Min-DevKit依存）"
    echo "実行日時: $(date)"
    echo "================================="
    echo ""
    echo "環境設定情報:"
    echo "- Max SDKパス: ${MAX_SDK_PATH}"
    echo "- テストバイナリ: $(basename "${TEST_BINARY}")"
    echo "- 実行ディレクトリ: $(pwd)"
    echo ""

    if [ -n "$TEST_FILTER" ]; then
      echo "🔍 フィルター '$TEST_FILTER' でテストを実行"
      # 環境変数を設定してテスト実行
      DYLD_FRAMEWORK_PATH="${MAX_FRAMEWORK_PATH}:${JIT_FRAMEWORK_PATH}:${MSP_FRAMEWORK_PATH}:$DYLD_FRAMEWORK_PATH" \
      DYLD_LIBRARY_PATH="${MAX_FRAMEWORK_PATH}:${JIT_FRAMEWORK_PATH}:${MSP_FRAMEWORK_PATH}:$DYLD_LIBRARY_PATH" \
          "./$(basename "${TEST_BINARY}")" "$TEST_FILTER" -s
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
    echo "================================="
    if [ ${TEST_RESULT} -eq 0 ]; then
      echo "✅ 従来のテスト結果: 成功"
    else
      echo "❌ 従来のテスト結果: 失敗"
    fi
    echo "================================="

    # 元のディレクトリに戻る
    cd "${CURRENT_DIR}"

    return $TEST_RESULT
  } | tee -a "${TEST_LOG_FILE}"

  return ${PIPESTATUS[0]}
}

build_and_run_standalone_tests() {
  echo "🔧 OSC Bridgeスタンドアロンテスト（Min-DevKit非依存）をビルドしています..."
  cd "${PROJECT_ROOT}/src/min-devkit/osc_bridge"
  cmake -S . -B "${BUILD_DIR}" -DBUILD_OSC_BRIDGE_STANDALONE_TESTS=ON
  cmake --build "${BUILD_DIR}" --target test_osc_bridge_standalone

  # テスト実行
  echo "🧪 スタンドアロンテスト環境でテストを実行しています..."
  cd "${BUILD_DIR}"

  # テストバイナリの確認
  TEST_BINARY="${BUILD_DIR}/tests/test_osc_bridge_standalone"
  if [ ! -f "${TEST_BINARY}" ]; then
    echo "❌ エラー: スタンドアロンテストバイナリが見つかりませんでした: ${TEST_BINARY}"
    echo "ビルドディレクトリを確認: $(ls -la ${BUILD_DIR})"
    echo "testsディレクトリを確認: $(ls -la ${BUILD_DIR}/tests 2>/dev/null || echo 'testsディレクトリが存在しません')"
    return 1
  fi

  # 現在の作業ディレクトリを保存
  CURRENT_DIR=$(pwd)

  # テスト実行ディレクトリに移動
  cd "$(dirname "${TEST_BINARY}")"

  # テスト実行（フィルター付きまたはすべて）
  echo "📋 スタンドアロンテスト結果は ${TEST_LOG_FILE} に保存されます"
  {
    echo "================================="
    echo "OSC Bridgeスタンドアロンテスト実行レポート（Min-DevKit非依存）"
    echo "実行日時: $(date)"
    echo "================================="
    echo ""
    echo "環境設定情報:"
    echo "- テストバイナリ: $(basename "${TEST_BINARY}")"
    echo "- 実行ディレクトリ: $(pwd)"
    echo ""

    if [ -n "$TEST_FILTER" ]; then
      echo "🔍 フィルター '$TEST_FILTER' でスタンドアロンテストを実行"
      "./$(basename "${TEST_BINARY}")" "$TEST_FILTER" -s
    else
      echo "🔍 全スタンドアロンテストを実行"
      "./$(basename "${TEST_BINARY}")" -s
    fi

    # 終了コードを保存
    TEST_RESULT=$?

    echo ""
    echo "================================="
    if [ ${TEST_RESULT} -eq 0 ]; then
      echo "✅ スタンドアロンテスト結果: 成功"
    else
      echo "❌ スタンドアロンテスト結果: 失敗"
    fi
    echo "================================="

    # 元のディレクトリに戻る
    cd "${CURRENT_DIR}"

    return $TEST_RESULT
  } | tee -a "${TEST_LOG_FILE}"

  return ${PIPESTATUS[0]}
}

# ヘッダー情報を出力
echo "================================="
echo "OSC Bridge テスト実行"
echo "実行日時: $(date)"
echo "テストモード: ${TEST_MODE}"
if [ -n "$TEST_FILTER" ]; then
  echo "フィルター: $TEST_FILTER"
fi
echo "================================="
echo "" | tee -a "${TEST_LOG_FILE}"

# テストモードに応じたテスト実行
LEGACY_RESULT=0
STANDALONE_RESULT=0

case "${TEST_MODE}" in
  "legacy")
    build_and_run_legacy_tests
    LEGACY_RESULT=$?
    FINAL_RESULT=$LEGACY_RESULT
    ;;
  "standalone")
    build_and_run_standalone_tests
    STANDALONE_RESULT=$?
    FINAL_RESULT=$STANDALONE_RESULT
    ;;
  "all")
    build_and_run_legacy_tests
    LEGACY_RESULT=$?

    echo ""
    echo "================================="
    echo "両方のテスト環境を実行中..."
    echo "================================="
    echo "" | tee -a "${TEST_LOG_FILE}"

    build_and_run_standalone_tests
    STANDALONE_RESULT=$?

    # 両方のテストが成功した場合のみ成功
    if [ $LEGACY_RESULT -eq 0 ] && [ $STANDALONE_RESULT -eq 0 ]; then
      FINAL_RESULT=0
    else
      FINAL_RESULT=1
    fi
    ;;
  *)
    echo "❌ エラー: 不明なテストモード '${TEST_MODE}'"
    exit 1
    ;;
esac

# 最終結果の評価
echo ""
echo "================================="
echo "テスト実行サマリー:"

if [ "${TEST_MODE}" = "legacy" ] || [ "${TEST_MODE}" = "all" ]; then
  if [ $LEGACY_RESULT -eq 0 ]; then
    echo "✅ 従来のテスト: 成功"
  else
    echo "❌ 従来のテスト: 失敗"
  fi
fi

if [ "${TEST_MODE}" = "standalone" ] || [ "${TEST_MODE}" = "all" ]; then
  if [ $STANDALONE_RESULT -eq 0 ]; then
    echo "✅ スタンドアロンテスト: 成功"
  else
    echo "❌ スタンドアロンテスト: 失敗"
  fi
fi

echo "================================="

if [ $FINAL_RESULT -eq 0 ]; then
  echo "✅ すべてのテストが成功しました。"
  echo "📊 テスト結果は ${TEST_LOG_FILE} に保存されました。"
  exit 0
else
  echo "❌ テストが失敗しました。詳細は上記のログを確認してください。"
  echo "📊 テスト結果は ${TEST_LOG_FILE} に保存されました。"
  exit 1
fi
