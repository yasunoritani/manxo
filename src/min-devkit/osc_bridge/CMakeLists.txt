## MCP OSC Bridge CMake configuration
cmake_minimum_required(VERSION 3.10)

# プロジェクト情報
project(mcp.osc_bridge)

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

# Min-DevKitのinclude追加
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../min-dev/min-devkit/source/min-api/include)

# OSCパッケージ
find_package(oscpack REQUIRED)
if(NOT oscpack_FOUND)
    message(FATAL_ERROR "oscpack library not found. Please install oscpack.")
endif()

# ターゲット定義
add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${HEADER_FILES})

# リンクライブラリ
target_link_libraries(${PROJECT_NAME} PRIVATE
    min-api
    min-lib
    oscpack
)

# テストターゲット
add_executable(${PROJECT_NAME}_test ${TEST_FILES})
target_link_libraries(${PROJECT_NAME}_test PRIVATE
    ${PROJECT_NAME}
    min-api
    min-lib
    oscpack
)

# インストール設定
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)
