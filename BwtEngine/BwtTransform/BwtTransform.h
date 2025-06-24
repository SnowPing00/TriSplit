#pragma once
#include <vector>
#include <cstdint>

// BWT 변환 결과를 담을 구조체
struct BwtResult {
    std::vector<uint16_t> l_stream;
    uint32_t primary_index = 0;
};

class BwtTransform {
public:
    BwtResult apply(const std::vector<uint16_t>& token_block);
};