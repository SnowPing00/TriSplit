#include "SeparationEngine.h"
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
    uint64_t original_size)
{
    std::vector<uint8_t> two_bit_chunks;
    two_bit_chunks.reserve(original_size * 4);

    size_t bitmap_idx = 0;
    size_t mask_idx = 0;

    // ======================= [오류 수정 지점] =======================
    // aux_mask_1_represents_11 플래그에 따라 심볼을 올바르게 매핑합니다.
    uint8_t symbol_for_mask_0, symbol_for_mask_1;
    if (aux_mask_1_represents_11) {
        // '1'이 '11'을 의미하는 경우
        symbol_for_mask_0 = 0b00;
        symbol_for_mask_1 = 0b11;
    }
    else {
        // '1'이 '00'을 의미하는 경우
        symbol_for_mask_0 = 0b11;
        symbol_for_mask_1 = 0b00;
    }
    // ===============================================================

    for (size_t i = 0; i < original_size * 4; ++i) {
        if (i >= reconstructed_stream.size()) break; // 안전장치

        uint8_t symbol_type = reconstructed_stream[i];
        if (symbol_type == 0) {
            if (bitmap_idx < value_bitmap.size()) {
                uint8_t bit = value_bitmap[bitmap_idx++];
                two_bit_chunks.push_back(bit == 0 ? 0b10 : 0b01);
            }
        }
        else {
            if (mask_idx < auxiliary_mask.size()) {
                uint8_t bit = auxiliary_mask[mask_idx++];
                two_bit_chunks.push_back(bit == 0 ? symbol_for_mask_0 : symbol_for_mask_1);
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

    return final_bytes;
}