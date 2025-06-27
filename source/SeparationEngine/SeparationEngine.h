#pragma once
// KO: 헤더 파일이 중복으로 포함되는 것을 방지합니다.
// EN: Prevents the header file from being included multiple times.
#include <vector>
#include <cstdint>

// KO: SeparationEngine이 원본 데이터를 분리한 후 3개의 스트림을 담는 구조체입니다.
//     각 스트림은 0 또는 1의 값만 가지는 단순한 형태로 변환됩니다.
// EN: A struct that holds the three streams after the SeparationEngine separates the original data.
//     Each stream is converted into a simple form containing only values of 0 or 1.
struct SeparatedStreams {
    // KO: '01', '10' 심볼의 값 정보(각각 1, 0)를 저장합니다. 순수한 정보 스트림입니다.
    // EN: Stores the value information of '01' and '10' symbols (1 and 0, respectively). This is a pure information stream.
    std::vector<uint8_t> value_bitmap;

    // KO: 원본 심볼의 구조적 정보를 담습니다. '00'/'11'의 위치는 한 종류의 값으로, '01'/'10'의 위치는 다른 종류의 값으로 표시됩니다.
    // EN: Contains the structural information of the original symbols. The positions of '00'/'11' are marked with one value, 
    //     and the positions of '01'/'10' are marked with another.
    std::vector<uint8_t> reconstructed_stream;

    // KO: '00'과 '11' 중 더 드물게 나타나는 심볼의 위치만 1로 표시하는 희소 비트 마스크입니다. 예외적 정보 스트림입니다.
    // EN: A sparse bitmask that marks the positions of the rarer symbol between '00' and '11' with a 1. This is an exceptional information stream.
    std::vector<uint8_t> auxiliary_mask;

    // KO: 보조 마스크(auxiliary_mask)의 '1'이 '11' 심볼을 의미하는지 여부를 저장하는 메타데이터 플래그입니다.
    //     false일 경우 '1'은 '00'을 의미합니다.
    // EN: A metadata flag that stores whether a '1' in the auxiliary_mask represents the '11' symbol.
    //     If false, a '1' represents '00'.
    bool aux_mask_1_represents_11 = false;
};

// KO: TriSplit 압축기의 핵심 로직 중 하나로, 원본 데이터를 통계적 특성이 다른 3개의 스트림으로 분리하고,
//     다시 원본 데이터로 재조립하는 역할을 담당합니다.
// EN: One of the core logics of the TriSplit compressor, responsible for separating the original data into
//     three streams with different statistical properties, and reassembling them back into the original data.
class SeparationEngine {
public:
    // KO: 원본 바이트 스트림을 입력받아 3개의 특화된 스트림으로 분리합니다.
    // @param data - 분리할 원본 데이터.
    // @return 분리된 스트림들을 담고 있는 SeparatedStreams 구조체.
    // EN: Takes the original byte stream as input and separates it into three specialized streams.
    // @param data - The original data to be separated.
    // @return A SeparatedStreams struct containing the separated streams.
    SeparatedStreams separate(const std::vector<uint8_t>& data);

    // KO: 분리된 3개의 스트림과 메타데이터를 이용해 원본 데이터를 재조립(복원)합니다.
    // @param value_bitmap - 값 비트맵 스트림.
    // @param auxiliary_mask - 보조 마스크 스트림.
    // @param reconstructed_stream - 재구성된 스트림.
    // @param aux_mask_1_represents_11 - 보조 마스크의 '1'이 '11'을 의미하는지에 대한 플래그.
    // @param original_size - 원본 데이터의 크기 (바이트 단위). 복원 후 데이터 검증에 사용됩니다.
    // @return 재조립된 원본 데이터.
    // EN: Reassembles (reconstructs) the original data from the three separated streams and metadata.
    // @param value_bitmap - The value bitmap stream.
    // @param auxiliary_mask - The auxiliary mask stream.
    // @param reconstructed_stream - The reconstructed stream.
    // @param aux_mask_1_represents_11 - Flag indicating whether '1' in the aux mask represents '11'.
    // @param original_size - The size of the original data in bytes. Used for data verification after reconstruction.
    // @return The reassembled original data.
    std::vector<uint8_t> reconstruct(
        const std::vector<uint8_t>& value_bitmap,
        const std::vector<uint8_t>& auxiliary_mask,
        const std::vector<uint8_t>& reconstructed_stream,
        bool aux_mask_1_represents_11,
        uint64_t original_size
    );
};