#pragma once

#include <random>

namespace mcp {
namespace osc {
namespace test {

// ランダムポート生成用ヘルパー関数
inline int random_port(int min, int max) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(min, max);
    return dist(gen);
}

} // namespace test
} // namespace osc
} // namespace mcp
