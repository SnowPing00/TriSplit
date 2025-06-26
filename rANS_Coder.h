#pragma once
#include <vector>
#include <cstdint>

class rANS_Coder {
public:
    // 바이트/비트 스트림 처리 함수
    std::vector<uint8_t> encode(const std::vector<uint8_t>& symbol_stream);
    std::vector<uint8_t> decode(const std::vector<uint8_t>& compressed_data);
    std::vector<uint8_t> encode_bits(const std::vector<bool>& bit_stream);
    std::vector<bool> decode_bits(const std::vector<uint8_t>& compressed_data);

    // Reconstructed Stream 비트 변환 인코딩
    std::vector<uint8_t> encode_reconstructed_stream(const std::vector<uint8_t>& recon_stream, bool is_placeholder_common);

    // Reconstructed Stream 디코딩
    std::vector<uint8_t> decode_reconstructed_stream(const std::vector<uint8_t>& compressed_data, bool is_placeholder_common);
};
