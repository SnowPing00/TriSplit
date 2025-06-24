#pragma once
#include "../BwtTransform/BwtTransform.h"
#include <vector>
#include <cstdint>

class MtfTransform {
public:
    // BWT 결과를 받아 MTF 변환된 숫자 스트림을 반환
    std::vector<uint16_t> apply(const BwtResult& bwt_result);
};