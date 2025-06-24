#include "BwtEngine.h"
#include <iostream>

BwtEngine::BwtEngine() {}

std::vector<uint16_t> BwtEngine::process_stream(const std::vector<uint16_t>& token_block) {
    std::cout << "--- BWT Engine Start ---" << std::endl;

    // 1단계: BWT 적용
    BwtResult bwt_result = bwt_transformer_.apply(token_block);

    // 2단계: MTF 적용
    std::vector<uint16_t> mtf_result = mtf_transformer_.apply(bwt_result);

    // 3단계: RLE 적용
    std::vector<uint16_t> rle_result = rle_transformer_.apply(mtf_result);

    std::cout << "--- BWT Engine End (RLE processed) ---" << std::endl;

    // RLE 처리 결과인 숫자 스트림을 그대로 반환
    return rle_result;
}