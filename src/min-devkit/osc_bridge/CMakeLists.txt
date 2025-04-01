# OSC Bridge for Max/MSP
cmake_minimum_required(VERSION 3.10)

# プロジェクト名の設定
project(osc_bridge)

# Min-APIパスの設定
if (NOT DEFINED C74_MIN_API_DIR)
    # GitHubからクローンしたmin-devリポジトリを使用するが、相対パスで参照
    set(C74_MIN_API_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../min-dev/min-devkit/source/min-api")
    
    if(NOT EXISTS "${C74_MIN_API_DIR}")
        message(WARNING "Min-API not found at relative path: ${C74_MIN_API_DIR}")
        message(STATUS "Trying alternative relative paths...")
        
        # 代替パスを試す
        set(ALT_MIN_API_DIR "${CMAKE_SOURCE_DIR}/min-dev/min-devkit/source/min-api")
        if(EXISTS "${ALT_MIN_API_DIR}")
            set(C74_MIN_API_DIR "${ALT_MIN_API_DIR}")
            message(STATUS "Found Min-API at: ${C74_MIN_API_DIR}")
        else()
            message(FATAL_ERROR "Min-API not found in any expected location. Please set C74_MIN_API_DIR manually.")
        endif()
    else()
        message(STATUS "Using Min-API from relative path: ${C74_MIN_API_DIR}")
    endif()
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

# ソースファイル設定 - Issue 19: Min-DevKitヘッダーの重複インクルード対応
# Min-DevKitの設計思想に従い、単一ファイル方式を采用する
# Min-DevKitヘッダー(c74_min.h)は各ソースファイルで一度だけインクルードされる必要がある
set(SOURCE_FILES
	osc_bridge.cpp
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
    
    # アーキテクチャの確認メッセージ
    message(STATUS "Building for architectures: ${CMAKE_OSX_ARCHITECTURES}")
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

# oscpackライブラリパスの候補 - build_universalを優先的に検索
set(OSCPACK_LIB_CANDIDATES
    "${OSCPACK_ROOT_DIR}/build_universal/liboscpack.a"  # 優先度最高（ユニバーサルビルド）
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
    
    # ユニバーサルビルドのチェック（macOSのみ）
    if(APPLE AND OSCPACK_LIB MATCHES "build_universal/liboscpack.a")
        message(STATUS "Using universal build of oscpack library")
    elseif(APPLE)
        message(WARNING "Not using the universal build of oscpack library. This may cause architecture compatibility issues.")
        message(WARNING "Run 'oscpack/build_universal.sh' to create a universal binary.")
    endif()
    
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

#############################################################
# テスト環境（Issue 24: Min-DevKit依存分離）
#############################################################

# テストタイプのオプション設定
option(BUILD_OSC_BRIDGE_TESTS "Build OSC Bridge test suite with Min-DevKit integration" OFF)
option(BUILD_OSC_BRIDGE_STANDALONE_TESTS "Build standalone OSC Bridge tests without Min-DevKit dependency" OFF)

# Issue 24: Min-DevKit依存のテスト環境（従来）
if(BUILD_OSC_BRIDGE_TESTS)
    message(STATUS "Building OSC Bridge tests with Min-DevKit integration")
    
    # Min-DevKit依存のテストファイル
    set(MIN_DEVKIT_TEST_FILES
        tests/standalone_test.cpp
        # Issue 24: これらのテストはMin-DevKit依存なしバージョンに移行中
        # tests/error_recovery_test.cpp
        # tests/simple_test.cpp
        # tests/extended_types_test.cpp
        # tests/m4l_lifecycle_test.cpp
        # tests/multi_instance_test.cpp
        # tests/performance_test.cpp
    )
    
    # Catch2のインストール
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v2.13.8 # M4L環境との互換性確認済み
    )
    FetchContent_MakeAvailable(Catch2)
    
    # Catch2の検出とインクルード
    set(Catch2_SOURCE_DIR "${catch2_SOURCE_DIR}")
    set(Catch2_INCLUDE_DIR "${catch2_SOURCE_DIR}/single_include")
    
    # テスト実行ファイルの設定
    add_executable(test_osc_bridge ${MIN_DEVKIT_TEST_FILES})
    
    # インクルードディレクトリを明示的に追加
    target_include_directories(test_osc_bridge PRIVATE
        "${Catch2_INCLUDE_DIR}"
        "${Catch2_INCLUDE_DIR}/catch2"
        "${catch2_SOURCE_DIR}/include"
    )
    
    # OSCpackとリンク
    if(DEFINED OSCPACK_LIB)
        target_link_libraries(test_osc_bridge PRIVATE "${OSCPACK_LIB}")
    else()
        target_link_libraries(test_osc_bridge PRIVATE "oscpack")
    endif()
    
    # テストバイナリの出力先を設定
    set_target_properties(test_osc_bridge PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

# Issue 24: Min-DevKit非依存のスタンドアロンテスト環境（新規）
if(BUILD_OSC_BRIDGE_STANDALONE_TESTS)
    message(STATUS "Building standalone OSC Bridge tests without Min-DevKit dependency")
    
    # tests/ディレクトリのCMakeListsを使用してスタンドアロンテストをビルド
    add_subdirectory(tests)
endif()

# どちらのテストも有効でない場合の情報表示
if(NOT BUILD_OSC_BRIDGE_TESTS AND NOT BUILD_OSC_BRIDGE_STANDALONE_TESTS)
    message(STATUS "OSC Bridge tests are disabled. To enable tests, use one of the following options:")
    message(STATUS "  -DBUILD_OSC_BRIDGE_TESTS=ON             # Min-DevKit統合テスト")
    message(STATUS "  -DBUILD_OSC_BRIDGE_STANDALONE_TESTS=ON  # Min-DevKit非依存のスタンドアロンテスト")
endif()
