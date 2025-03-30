# OSC Bridge for Max/MSP
cmake_minimum_required(VERSION 3.10)

# プロジェクト名の設定
project(osc_bridge)

# Min-APIパスの設定
if (NOT DEFINED C74_MIN_API_DIR)
    set(C74_MIN_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../min-dev/min-devkit/source/min-api")
endif()

# Min-DevKitの構成スクリプトをインクルード
include(${C74_MIN_API_DIR}/script/min-pretarget.cmake)

#############################################################
# MAX EXTERNAL
#############################################################

# includeディレクトリ設定
include_directories( 
	"${C74_INCLUDES}"
)

# ソースファイル設定 - Min-DevKitでは同じヘッダーを複数のファイルで使うと重複シンボルが発生するため、
# 統合した単一ファイルのみを使用する
set(SOURCE_FILES
	mcp.osc_bridge.cpp
)

# ライブラリの定義
add_library( 
	${PROJECT_NAME} 
	MODULE
	${SOURCE_FILES}
)

# C++17を有効化
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS固有の設定
if(APPLE)
    # ユニバーサルバイナリ（arm64とx86_64の両方）をサポート
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for Mac OS X" FORCE)
    
    # Max 8.2+に対応したデプロイメントターゲット
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
    
    # M4L環境用の最適化設定
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
endif()

# OSCPackライブラリの設定
# 環境変数またはリポジトリ内の場所から探索
if(DEFINED ENV{OSCPACK_ROOT})
    set(OSCPACK_ROOT_DIR $ENV{OSCPACK_ROOT})
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../oscpack")
    set(OSCPACK_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../oscpack")
else()
    message(FATAL_ERROR "oscpack library not found. Please set OSCPACK_ROOT environment variable.")
endif()

# oscpackライブラリの設定
include_directories("${OSCPACK_ROOT_DIR}")

# oscpackライブラリパスの候補
set(OSCPACK_LIB_CANDIDATES
    "${OSCPACK_ROOT_DIR}/build/liboscpack.a"
    "${OSCPACK_ROOT_DIR}/lib/liboscpack.a"
    "${OSCPACK_ROOT_DIR}/build/oscpack.lib"
    "${OSCPACK_ROOT_DIR}/lib/oscpack.lib"
)

# 実際のライブラリパスを探索
foreach(CANDIDATE ${OSCPACK_LIB_CANDIDATES})
    if(EXISTS "${CANDIDATE}")
        set(OSCPACK_LIB "${CANDIDATE}")
        break()
    endif()
endforeach()

# OSCpackをプリリンクするためにターゲットに追加
if(DEFINED OSCPACK_LIB)
    message(STATUS "Using oscpack library: ${OSCPACK_LIB}")
    target_link_libraries(${PROJECT_NAME} PUBLIC "${OSCPACK_LIB}")
else()
    message(STATUS "Using generic oscpack library")
    target_link_libraries(${PROJECT_NAME} PUBLIC "oscpack")
endif()

# macOS固有のリンクオプション
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PUBLIC
        "-framework CoreFoundation"
        "-framework CoreAudio"
    )
endif()

# コンパイル定義
target_compile_definitions(${PROJECT_NAME} PRIVATE
    MCP_OSC_VERSION="1.0.0"
    _CRT_SECURE_NO_WARNINGS
    NOMINMAX
)

# Min-DevKitの後処理スクリプトを実行
include(${C74_MIN_API_DIR}/script/min-posttarget.cmake)
