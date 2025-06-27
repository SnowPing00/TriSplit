#include "SeparationEngine.h"
#include <iostream>
#include <map>

SeparatedStreams SeparationEngine::separate(const std::vector<uint8_t>& raw_data) {
    // --- 1단계: 사전 분석 (빈도수 계산) ---
    size_t freqs[4] = { 0 };
    for (uint8_t byte : raw_data) {
        freqs[(byte >> 6) & 0x03]++;
        freqs[(byte >> 4) & 0x03]++;
        freqs[(byte >> 2) & 0x03]++;
        freqs[(byte >> 0) & 0x03]++;
    }

    // --- 2단계: 메타데이터 결정 ---
    SeparatedStreams result;
    result.aux_mask_1_represents_11 = (freqs[0b11] <= freqs[0b00]);

    // --- 3단계: 스트림 분리 ---
    result.value_bitmap.reserve(freqs[0b10] + freqs[0b01]);
    result.reconstructed_stream.reserve(raw_data.size() * 4);
    result.auxiliary_mask.reserve(freqs[0b00] + freqs[0b11]);

    for (uint8_t byte : raw_data) {
        uint8_t symbols[4] = {
            static_cast<uint8_t>((byte >> 6) & 0x03),
            static_cast<uint8_t>((byte >> 4) & 0x03),
            static_cast<uint8_t>((byte >> 2) & 0x03),
            static_cast<uint8_t>((byte >> 0) & 0x03)
        };

        for (uint8_t sym : symbols) {
            switch (sym) {
            case 0b10:
                result.value_bitmap.push_back(0);
                result.reconstructed_stream.push_back(0); // 지도 마커 (0)
                break;
            case 0b01:
                result.value_bitmap.push_back(1);
                result.reconstructed_stream.push_back(0); // 지도 마커 (0)
                break;
            case 0b00:
                result.reconstructed_stream.push_back(1); // 데이터 자리표시자 (1)
                result.auxiliary_mask.push_back(result.aux_mask_1_represents_11 ? 0 : 1);
                break;
            case 0b11:
                result.reconstructed_stream.push_back(1); // 데이터 자리표시자 (1)
                result.auxiliary_mask.push_back(result.aux_mask_1_represents_11 ? 1 : 0);
                break;
            }
        }
    }

    return result;
}

std::vector<uint8_t> SeparationEngine::reconstruct(
    const std::vector<uint8_t>& value_bitmap,
    const std::vector<uint8_t>& auxiliary_mask,
    const std::vector<uint8_t>& reconstructed_stream,
    bool aux_mask_1_represents_11,
    uint64_t original_size) // original_size는 이제 최종 크기 검증용으로만 사용
{
    std::vector<uint8_t> two_bit_chunks;
    // [수정] 루프 횟수를 reconstructed_stream의 실제 크기를 기반으로 하도록 변경
    two_bit_chunks.reserve(reconstructed_stream.size());

    size_t bitmap_idx = 0;
    size_t mask_idx = 0;

    uint8_t symbol_for_mask_0, symbol_for_mask_1;
    if (aux_mask_1_represents_11) {
        symbol_for_mask_0 = 0b00;
        symbol_for_mask_1 = 0b11;
    }
    else {
        symbol_for_mask_0 = 0b11;
        symbol_for_mask_1 = 0b00;
    }

    // [수정] 루프 조건을 original_size * 4 대신 reconstructed_stream.size()로 변경
    for (size_t i = 0; i < reconstructed_stream.size(); ++i) {
        uint8_t symbol_type = reconstructed_stream[i];
        if (symbol_type == 0) { // 지도 마커
            if (bitmap_idx < value_bitmap.size()) {
                uint8_t bit = value_bitmap[bitmap_idx++];
                two_bit_chunks.push_back(bit == 0 ? 0b10 : 0b01);
            }
            else {
                // 이 경우는 데이터 손상을 의미하므로 에러 처리나 로깅을 추가할 수 있습니다.
                std::cerr << "Warning: value_bitmap index out of bounds." << std::endl;
                break;
            }
        }
        else { // 데이터 자리표시자
            if (mask_idx < auxiliary_mask.size()) {
                uint8_t bit = auxiliary_mask[mask_idx++];
                two_bit_chunks.push_back(bit == 0 ? symbol_for_mask_0 : symbol_for_mask_1);
            }
            else {
                std::cerr << "Warning: auxiliary_mask index out of bounds." << std::endl;
                break;
            }
        }
    }

    // 복원된 2비트 청크들을 다시 1바이트로 합치는 로직
    std::vector<uint8_t> final_bytes;
    final_bytes.reserve(two_bit_chunks.size() / 4 + 1);
    for (size_t i = 0; i < two_bit_chunks.size(); i += 4) {
        uint8_t byte = 0;
        if (i + 0 < two_bit_chunks.size()) byte |= (two_bit_chunks[i + 0] << 6);
        if (i + 1 < two_bit_chunks.size()) byte |= (two_bit_chunks[i + 1] << 4);
        if (i + 2 < two_bit_chunks.size()) byte |= (two_bit_chunks[i + 2] << 2);
        if (i + 3 < two_bit_chunks.size()) byte |= (two_bit_chunks[i + 3] << 0);
        final_bytes.push_back(byte);
    }

    // 최종적으로 복원된 크기가 헤더의 original_size와 일치하는지 확인 (선택적)
    if (final_bytes.size() != original_size) {
        std::cerr << "Warning: Reconstructed size (" << final_bytes.size()
            << ") does not match original size (" << original_size << ")." << std::endl;
    }

    return final_bytes;
}