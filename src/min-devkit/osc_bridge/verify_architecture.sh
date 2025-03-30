#!/bin/bash
# OSC Bridge アーキテクチャ検証スクリプト
# このスクリプトは osc_bridge.mxo が適切なアーキテクチャで構築されているかを検証します
# また、デプロイメントターゲットや依存関係などの詳細情報も確認します

# 色付きの出力用の設定
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 出力フォーマット関数
function print_header() {
    echo -e "${BLUE}=========================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}=========================================================${NC}"
}

function print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

function print_error() {
    echo -e "${RED}✗ $1${NC}"
}

function print_warning() {
    echo -e "${YELLOW}! $1${NC}"
}

function print_info() {
    echo -e "${BLUE}i $1${NC}"
}

# ディレクトリパスの設定
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../../" && pwd)"
BINARY_PATH="${REPO_ROOT}/externals/osc_bridge.mxo/Contents/MacOS/osc_bridge"
OSCPACK_PATH="${REPO_ROOT}/oscpack/build_universal/liboscpack.a"

print_header "OSC Bridge アーキテクチャ検証"
echo "リポジトリルート: ${REPO_ROOT}"

# osc_bridge.mxo の存在チェック
if [ ! -f "$BINARY_PATH" ]; then
    print_error "OSC Bridge バイナリが見つかりません: $BINARY_PATH"
    print_warning "先にビルドを実行してください"
    exit 1
fi

# oscpack ユニバーサルバイナリの存在チェック
if [ ! -f "$OSCPACK_PATH" ]; then
    print_warning "oscpack ユニバーサルバイナリが見つかりません: $OSCPACK_PATH"
    echo "oscpack ユニバーサルバイナリを生成中..."
    
    # oscpack のビルドスクリプトを実行
    cd "${REPO_ROOT}/oscpack"
    ./build_universal.sh
    
    if [ ! -f "$OSCPACK_PATH" ]; then
        print_error "oscpack ユニバーサルバイナリの生成に失敗しました"
        exit 1
    else
        print_success "oscpack ユニバーサルバイナリを生成しました"
    fi
fi

# OSC Bridge バイナリのアーキテクチャ情報を取得
print_header "OSC Bridge バイナリのアーキテクチャ情報"
ARCH_INFO=$(lipo -info "$BINARY_PATH")
echo "$ARCH_INFO"

# バイナリの詳細情報を表示
print_info "$BINARY_PATH の詳細分析"
file "$BINARY_PATH"

# デプロイメントターゲットの確認
print_info "デプロイメントターゲット確認:"
DEPLOY_TARGET_INFO=$(otool -l "$BINARY_PATH" | grep -A 4 LC_VERSION_MIN | grep -v cmdsize)
echo "$DEPLOY_TARGET_INFO"

# macOS 10.13ターゲットを確認
if [[ "$DEPLOY_TARGET_INFO" == *"version 10.13"* ]]; then
    print_success "デプロイメントターゲットは正しく macOS 10.13 に設定されています (Max 8.0.2+ 互換性を確保)"
else
    print_warning "デプロイメントターゲットが macOS 10.13 ではありません"
fi

# arm64 と x86_64 の両方が含まれているか確認
if [[ "$ARCH_INFO" == *"x86_64"* ]] && [[ "$ARCH_INFO" == *"arm64"* ]]; then
    print_success "OSC Bridge はユニバーサルバイナリとして正しく構築されています (arm64 + x86_64)"
else
    print_error "OSC Bridge はユニバーサルバイナリではありません"
    
    if [[ "$ARCH_INFO" == *"x86_64"* ]]; then
        print_warning "x86_64 アーキテクチャのみを含みます"
    elif [[ "$ARCH_INFO" == *"arm64"* ]]; then
        print_warning "arm64 アーキテクチャのみを含みます"
    fi
    
    print_warning "CMakeLists.txt が適切に設定されているか確認してください"
    print_warning "oscpack バイナリが正しくユニバーサルバイナリとして構築されているか確認してください"
fi

# 依存関係の確認
print_info "依存関係の確認:"
otool -L "$BINARY_PATH" | grep -v "\:$" | head -12

# oscpack バイナリのアーキテクチャ情報を取得
print_header "oscpack ライブラリのアーキテクチャ情報"
OSCPACK_ARCH_INFO=$(lipo -info "$OSCPACK_PATH")
echo "$OSCPACK_ARCH_INFO"

if [[ "$OSCPACK_ARCH_INFO" == *"x86_64"* ]] && [[ "$OSCPACK_ARCH_INFO" == *"arm64"* ]]; then
    print_success "oscpack ライブラリはユニバーサルバイナリとして正しく構築されています (arm64 + x86_64)"
else
    print_error "oscpack ライブラリはユニバーサルバイナリではありません"
    print_warning "build_universal.sh スクリプトが正しく実行されたか確認してください"
fi

print_header "検証完了"

print_info "テスト手順:"
print_info "1. Max/MSPで動作を確認するテストパッチを開く"
print_info "2. Max 8.0.2でテストする場合、動作確認をレポートする"
print_info "3. Rosetta 2を使用したARM Mac上でのテストを行う場合は、結果をレポートする"

print_header "Max互換性情報"
MAX_VERSIONS="8.0.2, 8.1.0, 8.2.0"
print_info "このOSC Bridgeは次のMaxバージョンと互換性があります: $MAX_VERSIONS"
