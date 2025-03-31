# OSC Bridge for Max/MSP
cmake_minimum_required(VERSION 3.10)

# プロジェクト名の設定
project(osc_bridge)

# Min-APIパスの設定
if (NOT DEFINED C74_MIN_API_DIR)
    # GitHubからクローンしたmin-devリポジトリを使用
    set(C74_MIN_API_DIR "/Users/mymac/v8ui/min-dev/min-devkit/source/min-api")
    
    if(NOT EXISTS "${C74_MIN_API_DIR}")
        message(FATAL_ERROR "Min-API not found at: ${C74_MIN_API_DIR}")
    else()
        message(STATUS "Using Min-API from cloned repository: ${C74_MIN_API_DIR}")
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

# Issue 21: M4L環境でのテストファイル
set(TEST_FILES
	error_recovery_test.cpp
	extended_types_test.cpp
	new_test_snippet.cpp
	m4l_lifecycle_test.cpp
	multi_instance_test.cpp
	performance_test.cpp
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
# M4L環境テスト（Issue 21）
#############################################################

# テストオプションの設定（デフォルトでは無効）
option(BUILD_OSC_BRIDGE_TESTS "Build OSC Bridge test suite" OFF)

if(BUILD_OSC_BRIDGE_TESTS)
    # 各テストファイルごとにテストターゲットを作成
    foreach(TEST_FILE ${TEST_FILES})
        # ファイル名からターゲット名を生成
        get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
        set(TEST_TARGET "test_${TEST_NAME}")
        
        # テストターゲットの追加
        add_executable(${TEST_TARGET} ${TEST_FILE})
        
        # テストに必要なライブラリをリンク
        target_link_libraries(${TEST_TARGET} PUBLIC ${OSCPACK_LIB})
        
        # テスト固有の設定
        target_compile_definitions(${TEST_TARGET} PRIVATE
            MCP_OSC_TEST
            _CRT_SECURE_NO_WARNINGS
            NOMINMAX
        )
        
        # テスト出力ディレクトリの設定
        set_target_properties(${TEST_TARGET} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
        )
        
        message(STATUS "Configured test target: ${TEST_TARGET}")
    endforeach()
    
    message(STATUS "OSC Bridge tests enabled. Use -DBUILD_OSC_BRIDGE_TESTS=OFF to disable.")
else()
    message(STATUS "OSC Bridge tests disabled. Use -DBUILD_OSC_BRIDGE_TESTS=ON to enable.")
endif()
