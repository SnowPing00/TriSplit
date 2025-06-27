#pragma once
#include "../MtfTransform/MtfTransform.h" // MtfResult를 사용하기 위해
#include <vector>
#include <cstdint>

// RLE 변환 결과를 담을 구조체
struct RleResult {
    std::vector<uint16_t> rle_stream;
    uint32_t primary_index = 0;
};

class RleTransform {
public:
    // [수정] MtfResult를 인자로 받아 RLE 변환을 수행
    RleResult apply(const MtfResult& mtf_result);

    // RLE 스트림을 받아 MTF 결과로 역변환
    MtfResult inverse_apply(const RleResult& rle_result);
};