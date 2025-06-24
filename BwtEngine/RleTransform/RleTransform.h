#pragma once
#include <vector>
#include <cstdint>

class RleTransform {
public:
    // MTF 결과를 받아, 0-반복을 압축한 결과 스트림을 반환
    std::vector<uint16_t> apply(const std::vector<uint16_t>& mtf_stream);
};