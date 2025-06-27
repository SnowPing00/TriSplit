#pragma once
// Author: SnowPing00
// KO: 헤더 파일이 중복으로 포함되는 것을 방지합니다.
// EN: Prevents the header file from being included multiple times.
#include <vector>
#include <cstdint>

// KO: rANS(range Asymmetric Numeral Systems) 인코딩 및 디코딩 기능을 제공하는 클래스입니다.
//     다양한 유형의 데이터 스트림(바이트, 비트, 특수 스트림)을 처리하기 위한 인터페이스를 포함합니다.
// EN: A class that provides rANS (range Asymmetric Numeral Systems) encoding and decoding functionalities.
//     It includes interfaces for handling various types of data streams (byte, bit, specialized streams).
class rANS_Coder {
public:
    // --- Byte/Bit Stream Processing Functions ---
    // --- 바이트/비트 스트림 처리 함수 ---

    // KO: 0 또는 1의 값을 갖는 바이트(uint8_t) 스트림을 압축합니다.
    // EN: Compresses a stream of bytes (uint8_t) containing values of 0 or 1.
    std::vector<uint8_t> encode(const std::vector<uint8_t>& symbol_stream);

    // KO: 'encode' 함수로 압축된 데이터를 원본 바이트 스트림으로 복호화합니다.
    // EN: Decodes data compressed by the 'encode' function back into the original byte stream.
    std::vector<uint8_t> decode(const std::vector<uint8_t>& compressed_data);

    // KO: bool 값의 벡터(true/false)로 표현된 비트 스트림을 압축합니다.
    // EN: Compresses a bit stream represented as a vector of bools (true/false).
    std::vector<uint8_t> encode_bits(const std::vector<bool>& bit_stream);

    // KO: 'encode_bits' 함수로 압축된 데이터를 원본 비트 스트림으로 복호화합니다.
    // EN: Decodes data compressed by the 'encode_bits' function back into the original bit stream.
    std::vector<bool> decode_bits(const std::vector<uint8_t>& compressed_data);

    // --- Special Stream Processing for Reconstructed Stream ---
    // --- 재구성 스트림(Reconstructed Stream)을 위한 특수 처리 함수 ---

    // KO: 'reconstructed_stream'을 위한 특수 인코딩 함수입니다.
    //     이 스트림은 '데이터 자리표시자'와 '마커' 두 종류의 심볼로 구성됩니다.
    //     이 함수는 더 흔한 심볼을 "00"으로, 드문 심볼을 "01"과 같은 비트 패턴으로 변환하여 rANS로 압축 효율을 높입니다.
    // @param recon_stream - 인코딩할 재구성 스트림.
    // @param is_placeholder_common - '데이터 자리표시자'가 스트림에서 더 흔한 심볼인지 여부를 나타내는 플래그.
    // EN: A specialized encoding function for the 'reconstructed_stream'.
    //     This stream consists of two types of symbols: 'data placeholders' and 'markers'.
    //     This function improves compression efficiency by converting the more common symbol to a bit pattern like "00" 
    //     and the rarer symbol to "01" before rANS encoding.
    // @param recon_stream - The reconstructed stream to be encoded.
    // @param is_placeholder_common - A flag indicating whether the 'data placeholder' is the more common symbol in the stream.
    std::vector<uint8_t> encode_reconstructed_stream(const std::vector<uint8_t>& recon_stream, bool is_placeholder_common);

    // KO: 'encode_reconstructed_stream'으로 압축된 데이터를 원본 재구성 스트림으로 복호화합니다.
    // @param compressed_data - 복호화할 압축된 데이터.
    // @param is_placeholder_common - 인코딩 시 '데이터 자리표시자'가 흔한 심볼로 처리되었는지 여부를 알려주는 플래그.
    // EN: Decodes data compressed with 'encode_reconstructed_stream' back to the original reconstructed stream.
    // @param compressed_data - The compressed data to be decoded.
    // @param is_placeholder_common - A flag indicating if the 'data placeholder' was treated as the common symbol during encoding.
    std::vector<uint8_t> decode_reconstructed_stream(const std::vector<uint8_t>& compressed_data, bool is_placeholder_common);
};