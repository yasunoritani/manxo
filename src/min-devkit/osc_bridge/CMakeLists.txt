## MCP OSC Bridge CMake configuration
cmake_minimum_required(VERSION 3.10)

# プロジェクト情報
project(mcp.osc_bridge VERSION 1.0.0 DESCRIPTION "OSC Bridge for MCP-Max with M4L compatibility")

# C++17を有効化
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# macOS固有の設定
if(APPLE)
    # Max for Live環境は基本Intelプロセッサ、尤もにLive 11以前では特に注意が必要
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures for Mac OS X" FORCE)
    
    # Max 8.2に合わせたデプロイメントターゲット
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum OS X deployment version" FORCE)
    set(CMAKE_MACOSX_RPATH 1)
    
    # M4L環境用の最適化設定
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fvisibility=hidden -fvisibility-inlines-hidden")
    
    # 動的ライブラリのリンク設定
    set(CMAKE_INSTALL_RPATH "@loader_path/../support")
endif()

# ソースファイル
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp.osc_bridge.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp.osc_client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp.osc_server.cpp
)

# ヘッダーファイル
set(HEADER_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp.osc_types.hpp
)

# テストファイル
set(TEST_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/mcp.osc_bridge_test.cpp
)

# Max SDKのパスを環境変数から取得
# 未設定の場合はデフォルト値を使用
if(NOT DEFINED ENV{MAX_SDK_PATH})
    set(MAX_SDK_PATH "/Applications/Max.app/Contents/Resources/c74support" CACHE PATH "Path to Max SDK")
else()
    set(MAX_SDK_PATH $ENV{MAX_SDK_PATH} CACHE PATH "Path to Max SDK")
endif()
message(STATUS "Using Max SDK path: ${MAX_SDK_PATH}")

# Min-DevKitのinclude追加
set(MIN_DEVKIT_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../min-dev/min-devkit)
set(MIN_API_DIR ${MIN_DEVKIT_ROOT}/source/min-api)
set(MIN_LIB_DIR ${MIN_DEVKIT_ROOT}/source/min-lib)

include_directories(
    ${MIN_API_DIR}/include
    ${MIN_LIB_DIR}/include
    ${MAX_SDK_PATH}/max-includes
    ${MAX_SDK_PATH}/msp-includes
    ${MAX_SDK_PATH}/jit-includes
)

# oscpackのパスを柔軟に設定
# 環境変数が設定されていればそれを使用
# 指定がなければプロジェクト相対パスから探索
if(DEFINED ENV{OSCPACK_ROOT})
    set(OSCPACK_ROOT_DIR $ENV{OSCPACK_ROOT})
    message(STATUS "Using oscpack from environment variable: ${OSCPACK_ROOT_DIR}")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../../dependencies/oscpack")
    set(OSCPACK_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../dependencies/oscpack")
    message(STATUS "Using oscpack from relative path: ${OSCPACK_ROOT_DIR}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/external/oscpack")
    set(OSCPACK_ROOT_DIR "${CMAKE_SOURCE_DIR}/external/oscpack")
    message(STATUS "Using oscpack from external folder: ${OSCPACK_ROOT_DIR}")
else()
    message(FATAL_ERROR "oscpack not found. Please set OSCPACK_ROOT environment variable or ensure the library exists in a known location.")
endif()

# 拡張パスのパスフォーマットを正規化
file(TO_CMAKE_PATH "${OSCPACK_ROOT_DIR}" OSCPACK_ROOT_DIR)
set(OSCPACK_INCLUDE_DIR ${OSCPACK_ROOT_DIR})

# ライブラリパスの父ディレクトリをいくつかチェック
if(EXISTS "${OSCPACK_ROOT_DIR}/build/liboscpack.a" OR EXISTS "${OSCPACK_ROOT_DIR}/build/oscpack.lib")
    set(OSCPACK_LIBRARY_DIR "${OSCPACK_ROOT_DIR}/build")
elseif(EXISTS "${OSCPACK_ROOT_DIR}/lib/liboscpack.a" OR EXISTS "${OSCPACK_ROOT_DIR}/lib/oscpack.lib")
    set(OSCPACK_LIBRARY_DIR "${OSCPACK_ROOT_DIR}/lib")
else()
    # 見つからない場合は自動ビルドを試行
    message(STATUS "oscpack library not found in expected locations. Will attempt to build it.")
    # ビルドディレクトリを作成
    file(MAKE_DIRECTORY "${OSCPACK_ROOT_DIR}/build")
    set(OSCPACK_LIBRARY_DIR "${OSCPACK_ROOT_DIR}/build")
    
    # oscpackをビルド（実際のビルドコマンド）
    if(WIN32)
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E chdir ${OSCPACK_ROOT_DIR} cmake -E make_directory build
            COMMAND ${CMAKE_COMMAND} -E chdir ${OSCPACK_ROOT_DIR}/build cmake ..
            COMMAND ${CMAKE_COMMAND} -E chdir ${OSCPACK_ROOT_DIR}/build cmake --build . --config Release
            RESULT_VARIABLE OSCPACK_BUILD_RESULT
        )
    else()
        execute_process(
            COMMAND ${CMAKE_COMMAND} -E chdir ${OSCPACK_ROOT_DIR} mkdir -p build
            COMMAND ${CMAKE_COMMAND} -E chdir ${OSCPACK_ROOT_DIR}/build cmake ..
            COMMAND ${CMAKE_COMMAND} -E chdir ${OSCPACK_ROOT_DIR}/build make
            RESULT_VARIABLE OSCPACK_BUILD_RESULT
        )
    endif()
    
    # ビルド結果の確認
    if(NOT OSCPACK_BUILD_RESULT EQUAL 0)
        message(WARNING "Failed to build oscpack automatically. Please build manually in: ${OSCPACK_LIBRARY_DIR}")
    else()
        message(STATUS "Successfully built oscpack library")
    endif()
endif()

# パス設定
include_directories(${OSCPACK_INCLUDE_DIR})
link_directories(${OSCPACK_LIBRARY_DIR})

# 依存関係のバージョン情報
set(OSCPACK_VERSION "1.1.0" CACHE STRING "oscpack version")

# ターゲット定義
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${HEADER_FILES})

# コンパイルフラグの設定
target_compile_definitions(${PROJECT_NAME} PRIVATE
    MCP_OSC_VERSION="${PROJECT_VERSION}"
    _CRT_SECURE_NO_WARNINGS             # Windows環境での警告抜き
    NOMINMAX                           # Windows min/maxマクロの課題回避
)

# コンパイルオプションの共通設定
if(MSVC)
    # Visual Studio環境の場合
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /MP /EHsc)
elseif(APPLE)
    # macOS環境の場合
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
else()
    # Linux等他の環境の場合
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

# macOS固有のリンクオプション
if(APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        "-framework CoreFoundation"
        "-framework CoreAudio"
    )
    # Max向けオブジェクトスフィックスを設定
    set_target_properties(${PROJECT_NAME} PROPERTIES
        PREFIX ""
        SUFFIX ".mxo"
        BUNDLE TRUE
        BUNDLE_EXTENSION "mxo"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

# ライブラリファイル名の設定
if(WIN32)
    set(OSCPACK_LIB_NAME "oscpack.lib")
else()
    set(OSCPACK_LIB_NAME "liboscpack.a")
endif()

# フルパスを指定してリンクする
if(EXISTS "${OSCPACK_LIBRARY_DIR}/${OSCPACK_LIB_NAME}")
    target_link_libraries(${PROJECT_NAME} PRIVATE "${OSCPACK_LIBRARY_DIR}/${OSCPACK_LIB_NAME}")
else()
    message(WARNING "oscpack library not found at ${OSCPACK_LIBRARY_DIR}/${OSCPACK_LIB_NAME}, using generic linking")
    # 具体的なパスが見つからない場合は汎用的なリンクを試みる
    target_link_libraries(${PROJECT_NAME} PRIVATE "oscpack")
endif()

# テストターゲット
add_executable(${PROJECT_NAME}_test ${TEST_FILES})
target_link_libraries(${PROJECT_NAME}_test PRIVATE
    ${PROJECT_NAME}
    ${OSCPACK_LIBRARY_DIR}/liboscpack.a
)

# インストール先の設定
if(APPLE)
    # Max 8標準パッケージディレクトリ
    set(MAX_PACKAGE_DIR "$ENV{HOME}/Documents/Max 8/Packages/mcp-bridge")
    
    # Max for Live環境向けの追加デプロイ先
    set(M4L_PACKAGE_DIR "$ENV{HOME}/Music/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect")
else()
    # Windows用パス
    set(MAX_PACKAGE_DIR "$ENV{USERPROFILE}/Documents/Max 8/Packages/mcp-bridge")
    set(M4L_PACKAGE_DIR "$ENV{USERPROFILE}/Documents/Ableton/User Library/Presets/MIDI Effects/Max MIDI Effect")
endif()

# インストールターゲット
# 通常のMaxパッケージディレクトリへのインストール
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION "${MAX_PACKAGE_DIR}/externals"
    BUNDLE DESTINATION "${MAX_PACKAGE_DIR}/externals"
)

# Max for Live環境向けの追加デプロイ
# サポートファイル用ディレクトリを作成
install(FILES ${OSCPACK_LIBRARY_DIR}/liboscpack.a
    DESTINATION "${M4L_PACKAGE_DIR}/mcp-bridge/support"
)

# M4L用にエクステンションをコピー
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION "${M4L_PACKAGE_DIR}/mcp-bridge/externals"
    BUNDLE DESTINATION "${M4L_PACKAGE_DIR}/mcp-bridge/externals"
)

# M4Lデモパッチファイルがあれば追加
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/m4l-patches")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/m4l-patches/"
        DESTINATION "${M4L_PACKAGE_DIR}/mcp-bridge"
    )
endif()

# 標準インストールサポート
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
