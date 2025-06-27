#include "RleTransform.h"
#include <iostream>
#include <vector>
#include <cstdint>

// [수정] MtfResult를 인자로 받도록 변경
RleResult RleTransform::apply(const MtfResult& mtf_result) {
    const auto& mtf_stream = mtf_result.mtf_stream;
    if (mtf_stream.empty()) {
        return { {}, mtf_result.primary_index }; // primary_index를 그대로 전달
    }

    const uint16_t ZERO_RUN_CODE = 65535;
    std::vector<uint16_t> rle_stream;
    rle_stream.reserve(mtf_stream.size());

    size_t i = 0;
    while (i < mtf_stream.size()) {
        uint16_t current_symbol = mtf_stream[i];

        if (current_symbol != 0) {
            rle_stream.push_back(current_symbol);
            i++;
            continue;
        }

        size_t run_length = 0;
        size_t j = i;
        while (j < mtf_stream.size() && mtf_stream[j] == 0) {
            run_length++;
            j++;
        }

        if (run_length >= 3) {
            rle_stream.push_back(ZERO_RUN_CODE);
            rle_stream.push_back(static_cast<uint16_t>(run_length));
            i += run_length;
        }
        else {
            for (size_t k = 0; k < run_length; ++k) {
                rle_stream.push_back(0);
            }
            i += run_length;
        }
    }

    std::cout << "RLE transform applied. Stream size: " << mtf_stream.size() << " -> " << rle_stream.size() << std::endl;
    // RleResult 구조체에 결과를 담아 반환
    return { rle_stream, mtf_result.primary_index };
}

// [최종본] RLE 역변환 함수 (사용자님이 작성하신 올바른 버전)
MtfResult RleTransform::inverse_apply(const RleResult& rle_result) {
    const auto& rle_stream = rle_result.rle_stream;
    std::vector<uint16_t> mtf_stream;
    mtf_stream.reserve(rle_stream.size());

    const uint16_t ZERO_RUN_CODE = 65535;

    for (size_t i = 0; i < rle_stream.size(); ++i) {
        if (rle_stream[i] == ZERO_RUN_CODE) {
            if (i + 1 < rle_stream.size()) {
                size_t run_length = rle_stream[i + 1];
                mtf_stream.insert(mtf_stream.end(), run_length, 0);
                i++;
            }
        }
        else {
            mtf_stream.push_back(rle_stream[i]);
        }
    }

    std::cout << "RLE inverse transform applied. Stream size: " << rle_stream.size() << " -> " << mtf_stream.size() << std::endl;
    // MtfResult 구조체에 결과를 담아 반환
    return { mtf_stream, rle_result.primary_index };
}