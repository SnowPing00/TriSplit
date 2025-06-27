#pragma once
#include "../BwtTransform/BwtTransform.h"
#include <vector>
#include <cstdint>
#include <list> // std::list 사용을 위해 추가

// MTF 변환 결과를 담을 구조체
struct MtfResult {
    std::vector<uint16_t> mtf_stream;
    std::vector<uint16_t> initial_alphabet; // 초기 사전을 저장할 벡터
    uint32_t primary_index = 0;
};

class MtfTransform {
public:
    // 이제 MtfResult를 반환합니다.
    MtfResult apply(const BwtResult& bwt_result);

    // MtfResult를 인자로 받아 BwtResult를 반환합니다.
    BwtResult inverse_apply(const MtfResult& mtf_result);
};