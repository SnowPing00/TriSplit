#include "BwtEngine.h"
#include "EntropyCoder/EntropyCoder.h"
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

// BWT 엔진의 역변환 파이프라인
std::vector<uint16_t> BwtEngine::inverse_process_stream(const std::vector<uint8_t>& compressed_data) {
    std::cout << "--- BWT Engine Inverse Start ---" << std::endl;

    // 1단계: Huffman 복호화
    EntropyCoder final_coder;
    std::vector<uint16_t> huffman_decoded = final_coder.huffman_decode(compressed_data);

    // 2단계: RLE 역변환
    std::vector<uint16_t> rle_decoded = rle_transformer_.inverse_apply(huffman_decoded);

    // 3단계: MTF 역변환
    BwtResult mtf_decoded_bwt_result = mtf_transformer_.inverse_apply(rle_decoded);

    // 4단계: BWT 역변환
    std::vector<uint16_t> original_stream = bwt_transformer_.inverse_apply(mtf_decoded_bwt_result);

    std::cout << "--- BWT Engine Inverse End ---" << std::endl;
    return original_stream;
}