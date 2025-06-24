#include "RleTransform.h"
#include <iostream>
#include <vector>
#include <cstdint>

std::vector<uint16_t> RleTransform::apply(const std::vector<uint16_t>& mtf_stream) {
    if (mtf_stream.empty()) {
        return {};
    }

    // 0의 반복을 표시하기 위한 특수 마커 ID. 
    // 65536개의 토큰 ID 중 가장 마지막 번호를 사용
    const uint16_t ZERO_RUN_CODE = 65535;

    std::vector<uint16_t> rle_stream;
    rle_stream.reserve(mtf_stream.size());

    size_t i = 0;
    while (i < mtf_stream.size()) {
        uint16_t current_symbol = mtf_stream[i];

        // 현재 심볼이 0이 아니면 결과 스트림에 추가
        if (current_symbol != 0) {
            rle_stream.push_back(current_symbol);
            i++;
            continue;
        }

        //현재 심볼이 0인 경우 연속된 0의 갯수 확인
        size_t run_length = 0;
        size_t j = i;
        while (j < mtf_stream.size() && mtf_stream[j] == 0) {
            run_length++;
            j++;
        }

        // 2. 압축 여부 결정
        if (run_length >= 3) {
            rle_stream.push_back(ZERO_RUN_CODE);
            rle_stream.push_back(static_cast<uint16_t>(run_length));
            i += run_length; // 처리한 만큼 인덱스 점프
        }
        else {
            // 반복이 2번 이하로 짧으면, 있는 그대로 추가
            for (size_t k = 0; k < run_length; ++k) {
                rle_stream.push_back(0);
            }
            i += run_length;
        }
    }

    std::cout << "RLE transform applied. Stream size: " << mtf_stream.size() << " -> " << rle_stream.size() << std::endl;
    return rle_stream;
}