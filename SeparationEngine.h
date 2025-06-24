#pragma once

#include <vector>
#include <cstdint>

// 분리된 스트림들을 담을 구조체 (0 또는 1 값만 가짐)
struct SeparatedStreams {
    std::vector<uint8_t> value_bitmap;          // 값 비트맵
    std::vector<uint8_t> reconstructed_stream;  // 재구성된 스트림
    std::vector<uint8_t> auxiliary_mask;        // 보조 마스크
    bool aux_mask_1_represents_11 = false;      // 보조 마스크의 '1'이 '11'을 의미하는지에 대한 메타데이터 플래그
};

class SeparationEngine {
public:
    // 분리된 스트림 반환 함수
    SeparatedStreams separate(const std::vector<uint8_t>& raw_data);
};