#pragma once
#include "BwtTransform/BwtTransform.h"
#include "MtfTransform/MtfTransform.h"
#include "RleTransform/RleTransform.h"
#include <vector>
#include <cstdint>

class BwtEngine {
public:
    BwtEngine();

    // [수정] 반환 타입을 RleResult로 변경하여 허프만 코더와 직접 연결
    RleResult process_stream(const std::vector<uint16_t>& token_block);

    // 이 함수는 최종적으로 uint16_t 스트림을 반환하므로 그대로 둡니다.
    std::vector<uint16_t> inverse_process_stream(const std::vector<uint8_t>& compressed_data);

private:
    BwtTransform bwt_transformer_;
    MtfTransform mtf_transformer_;
    RleTransform rle_transformer_;
};