#include "SeparationEngine.h"
#include <map>

SeparatedStreams SeparationEngine::separate(const std::vector<uint8_t>& raw_data) {
    // --- 1단계: 사전 분석 (빈도수 계산) ---

    // 2비트 심볼(00, 01, 10, 11)의 빈도를 저장할 배열
    size_t freqs[4] = { 0 };

    // 1바이트씩 순회하며 2비트 심볼 4개씩 처리
    for (uint8_t byte : raw_data) {
        // 비트 연산을 사용하여 1바이트에서 2비트 심볼 4개 추출
        freqs[(byte >> 6) & 0x03]++; // 첫 2비트
        freqs[(byte >> 4) & 0x03]++; // 두 번째 2비트
        freqs[(byte >> 2) & 0x03]++; // 세 번째 2비트
        freqs[(byte >> 0) & 0x03]++; // 네 번째 2비트
    }

    // --- 2단계: 메타데이터 결정 ---
    SeparatedStreams result;
    // '11'의 빈도가 '00'보다 작거나 같으면, 보조 마스크의 '1'은 '11'을 의미
    result.aux_mask_1_represents_11 = (freqs[0b11] <= freqs[0b00]);

    // --- 3단계: 스트림 분리 ---

    // 벡터 메모리 사전 할당으로 성능 향상
    result.value_bitmap.reserve(freqs[0b10] + freqs[0b01]);
    result.reconstructed_stream.reserve(raw_data.size() * 4);
    result.auxiliary_mask.reserve(freqs[0b00] + freqs[0b11]);

    for (uint8_t byte : raw_data) {
        // 처리할 심볼들을 배열에 담아 반복 처리
        uint8_t symbols[4] = {
            static_cast<uint8_t>((byte >> 6) & 0x03),
            static_cast<uint8_t>((byte >> 4) & 0x03),
            static_cast<uint8_t>((byte >> 2) & 0x03),
            static_cast<uint8_t>((byte >> 0) & 0x03)
        };

        for (uint8_t sym : symbols) {
            switch (sym) {
            case 0b10: // 심볼 "10"
                result.value_bitmap.push_back(0);
                result.reconstructed_stream.push_back(0); // 지도 마커
                break;

            case 0b01: // 심볼 "01"
                result.value_bitmap.push_back(1);
                result.reconstructed_stream.push_back(0); // 지도 마커
                break;

            case 0b00: // 심볼 "00"
                result.reconstructed_stream.push_back(1); // 데이터 자리표시자
                result.auxiliary_mask.push_back(result.aux_mask_1_represents_11 ? 0 : 1);
                break;

            case 0b11: // 심볼 "11"
                result.reconstructed_stream.push_back(1); // 데이터 자리표시자
                result.auxiliary_mask.push_back(result.aux_mask_1_represents_11 ? 1 : 0);
                break;
            }
        }
    }

    return result;
}