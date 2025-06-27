// Author: SnowPing00
// KO: 이 파일은 SeparationEngine 클래스의 멤버 함수들을 구현합니다.
//     SeparationEngine은 TriSplit의 핵심 철학인 "분리하고, 변환하고, 정복하라"에서
//     '분리'와 '정복(재조립)' 단계를 담당합니다.
// EN: This file implements the member functions of the SeparationEngine class.
//     The SeparationEngine is responsible for the 'Divide' and 'Conquer (Reconstruct)' phases
//     of TriSplit's core philosophy: "Divide, Transform, and Conquer."
#include "SeparationEngine.h"
#include <iostream>
#include <map>

// KO: 원본 데이터를 3개의 특화된 스트림으로 분리하는 함수입니다.
// EN: A function that separates the original data into three specialized streams.
SeparatedStreams SeparationEngine::separate(const std::vector<uint8_t>& raw_data) {
    // --- 단계 1: 사전 분석 (빈도수 계산) ---
    // --- Phase 1: Pre-analysis (Frequency Counting) ---
    // KO: 원본 데이터를 2비트 심볼(00, 01, 10, 11) 단위로 보고 각 심볼의 등장 빈도를 계산합니다.
    //     이 정보는 'auxiliary_mask'를 생성하는 데 사용됩니다.
    // EN: Treats the original data as 2-bit symbols (00, 01, 10, 11) and counts the occurrence frequency of each symbol.
    //     This information is used to generate the 'auxiliary_mask'.
    size_t freqs[4] = { 0 };
    for (uint8_t byte : raw_data) {
        freqs[(byte >> 6) & 0x03]++; // 1st 2-bit symbol
        freqs[(byte >> 4) & 0x03]++; // 2nd 2-bit symbol
        freqs[(byte >> 2) & 0x03]++; // 3rd 2-bit symbol
        freqs[(byte >> 0) & 0x03]++; // 4th 2-bit symbol
    }

    // --- 단계 2: 메타데이터 결정 ---
    // --- Phase 2: Metadata Decision ---
    SeparatedStreams result;
    // KO: 00과 11 심볼 중 더 드물게 나타나는 심볼을 결정합니다.
    //     이 정보는 auxiliary_mask에서 '1'이 무엇을 의미하는지를 나타내는 메타데이터가 됩니다.
    // EN: Determines which symbol is rarer between '00' and '11'.
    //     This information becomes the metadata indicating what a '1' in the auxiliary_mask represents.
    result.aux_mask_1_represents_11 = (freqs[0b11] <= freqs[0b00]);

    // --- 단계 3: 스트림 분리 ---
    // --- Phase 3: Stream Separation ---
    // KO: 메모리 재할당을 최소화하기 위해 각 스트림의 크기를 미리 예약합니다.
    // EN: Reserves the capacity for each stream in advance to minimize memory reallocations.
    result.value_bitmap.reserve(freqs[0b10] + freqs[0b01]);
    result.reconstructed_stream.reserve(raw_data.size() * 4);
    result.auxiliary_mask.reserve(freqs[0b00] + freqs[0b11]);

    // KO: 다시 원본 데이터를 순회하며 각 2비트 심볼을 3개의 스트림으로 분해합니다.
    // EN: Iterates through the original data again, deconstructing each 2-bit symbol into the three streams.
    for (uint8_t byte : raw_data) {
        uint8_t symbols[4] = {
            static_cast<uint8_t>((byte >> 6) & 0x03),
            static_cast<uint8_t>((byte >> 4) & 0x03),
            static_cast<uint8_t>((byte >> 2) & 0x03),
            static_cast<uint8_t>((byte >> 0) & 0x03)
        };

        for (uint8_t sym : symbols) {
            switch (sym) {
            case 0b10: // Symbol '10'
                // KO: 값 정보(0)를 value_bitmap에 저장합니다.
                // EN: Store the value information (0) in the value_bitmap.
                result.value_bitmap.push_back(0);
                // KO: '10'/'01' 심볼의 위치를 나타내는 마커(0)를 reconstructed_stream에 저장합니다.
                // EN: Store a marker (0) in the reconstructed_stream to indicate the position of a '10'/'01' symbol.
                result.reconstructed_stream.push_back(0);
                break;
            case 0b01: // Symbol '01'
                // KO: 값 정보(1)를 value_bitmap에 저장합니다.
                // EN: Store the value information (1) in the value_bitmap.
                result.value_bitmap.push_back(1);
                // KO: '10'/'01' 심볼의 위치를 나타내는 마커(0)를 reconstructed_stream에 저장합니다.
                // EN: Store a marker (0) in the reconstructed_stream to indicate the position of a '10'/'01' symbol.
                result.reconstructed_stream.push_back(0);
                break;
            case 0b00: // Symbol '00'
                // KO: '00'/'11' 심볼의 위치를 나타내는 자리표시자(1)를 reconstructed_stream에 저장합니다.
                // EN: Store a placeholder (1) in the reconstructed_stream to indicate the position of a '00'/'11' symbol.
                result.reconstructed_stream.push_back(1);
                // KO: '00'이 희소 심볼인지 아닌지에 따라 auxiliary_mask에 0 또는 1을 저장합니다.
                // EN: Store 0 or 1 in the auxiliary_mask depending on whether '00' is the rare symbol.
                result.auxiliary_mask.push_back(result.aux_mask_1_represents_11 ? 0 : 1);
                break;
            case 0b11: // Symbol '11'
                // KO: '00'/'11' 심볼의 위치를 나타내는 자리표시자(1)를 reconstructed_stream에 저장합니다.
                // EN: Store a placeholder (1) in the reconstructed_stream to indicate the position of a '00'/'11' symbol.
                result.reconstructed_stream.push_back(1);
                // KO: '11'이 희소 심볼인지 아닌지에 따라 auxiliary_mask에 0 또는 1을 저장합니다.
                // EN: Store 0 or 1 in the auxiliary_mask depending on whether '11' is the rare symbol.
                result.auxiliary_mask.push_back(result.aux_mask_1_represents_11 ? 1 : 0);
                break;
            }
        }
    }

    return result;
}


// KO: 분리된 3개의 스트림을 원본 데이터로 재조립(복원)하는 함수입니다.
// EN: A function that reassembles (reconstructs) the original data from the three separated streams.
std::vector<uint8_t> SeparationEngine::reconstruct(
    const std::vector<uint8_t>& value_bitmap,
    const std::vector<uint8_t>& auxiliary_mask,
    const std::vector<uint8_t>& reconstructed_stream,
    bool aux_mask_1_represents_11,
    uint64_t original_size)
{
    // KO: 복원된 2비트 심볼들을 임시로 저장할 벡터입니다.
    // EN: A vector to temporarily store the reconstructed 2-bit symbols.
    std::vector<uint8_t> two_bit_chunks;
    two_bit_chunks.reserve(reconstructed_stream.size());

    // KO: 각 스트림을 순회하기 위한 인덱스 변수들입니다.
    // EN: Index variables for iterating through each stream.
    size_t bitmap_idx = 0;
    size_t mask_idx = 0;

    // KO: 메타데이터 플래그를 기반으로 auxiliary_mask의 0과 1이 어떤 심볼을 나타내는지 결정합니다.
    // EN: Determines which symbol a 0 or 1 in the auxiliary_mask represents, based on the metadata flag.
    uint8_t symbol_for_mask_0, symbol_for_mask_1;
    if (aux_mask_1_represents_11) {
        symbol_for_mask_0 = 0b00;
        symbol_for_mask_1 = 0b11;
    }
    else {
        symbol_for_mask_0 = 0b11;
        symbol_for_mask_1 = 0b00;
    }

    // KO: 재구성 스트림(reconstructed_stream)을 순회하며 원본 2비트 심볼을 복원합니다.
    // EN: Iterates through the reconstructed_stream to restore the original 2-bit symbols.
    for (size_t i = 0; i < reconstructed_stream.size(); ++i) {
        uint8_t symbol_type = reconstructed_stream[i];
        if (symbol_type == 0) { // KO: 마커(Marker)인 경우, '01' 또는 '10' 심볼을 의미합니다.
            // EN: If it's a marker, it signifies a '01' or '10' symbol.
            if (bitmap_idx < value_bitmap.size()) {
                uint8_t bit = value_bitmap[bitmap_idx++];
                two_bit_chunks.push_back(bit == 0 ? 0b10 : 0b01);
            }
            else {
                // KO: 데이터 손상을 의미. 경고를 출력하고 중단합니다.
                // EN: Indicates data corruption. Print a warning and break.
                std::cerr << "Warning: value_bitmap index out of bounds." << std::endl;
                break;
            }
        }
        else { // KO: 자리표시자(Placeholder)인 경우, '00' 또는 '11' 심볼을 의미합니다.
            // EN: If it's a placeholder, it signifies a '00' or '11' symbol.
            if (mask_idx < auxiliary_mask.size()) {
                uint8_t bit = auxiliary_mask[mask_idx++];
                two_bit_chunks.push_back(bit == 0 ? symbol_for_mask_0 : symbol_for_mask_1);
            }
            else {
                // KO: 데이터 손상을 의미. 경고를 출력하고 중단합니다.
                // EN: Indicates data corruption. Print a warning and break.
                std::cerr << "Warning: auxiliary_mask index out of bounds." << std::endl;
                break;
            }
        }
    }

    // --- 최종 조립 ---
    // --- Final Assembly ---
    // KO: 복원된 2비트 청크(심볼)들을 4개씩 묶어 다시 1바이트로 합칩니다.
    // EN: Groups the reconstructed 2-bit chunks (symbols) by four and combines them back into 1-byte values.
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

    // KO: (선택적) 최종 복원된 크기가 헤더에 기록된 원본 크기와 일치하는지 확인합니다.
    // EN: (Optional) Verifies if the final reconstructed size matches the original size recorded in the header.
    if (final_bytes.size() != original_size) {
        std::cerr << "Warning: Reconstructed size (" << final_bytes.size()
            << ") does not match original size (" << original_size << ")." << std::endl;
    }

    return final_bytes;
}