// KO: 이 파일은 rANS_Coder 클래스의 멤버 함수들을 구현합니다.
//     rANS_Coder는 Fabian 'ryg' Giesen의 'rans_byte.h' 라이브러리를 사용하여,
//     TriSplit 프로젝트에 필요한 특정 종류의 데이터 스트림(바이너리, 비트, 재구성 스트림)을
//     압축 및 복호화하는 고수준 인터페이스를 제공합니다.
// EN: This file implements the member functions of the rANS_Coder class.
//     rANS_Coder uses Fabian 'ryg' Giesen's 'rans_byte.h' library to provide
//     a high-level interface for compressing and decompressing specific types of data streams
//     (binary, bit, reconstructed streams) required for the TriSplit project.
#include "rANS_Coder.h"
#include "../rans_byte.h"
#include <stdexcept>
#include <vector>
#include <cstring>

// --- ENCODE (Legacy byte-wise encoder) ---
// --- 인코딩 (기존 바이트 단위 인코더) ---
// KO: 0 또는 1의 값을 갖는 바이트(uint8_t) 스트림을 rANS 알고리즘으로 압축합니다.
// EN: Compresses a stream of bytes (uint8_t) containing values of 0 or 1 using the rANS algorithm.
std::vector<uint8_t> rANS_Coder::encode(const std::vector<uint8_t>& symbol_stream) {
    if (symbol_stream.empty()) {
        return {};
    }

    const uint32_t total_symbols = static_cast<uint32_t>(symbol_stream.size());
    uint32_t freqs[2] = { 0, 0 }; // [0] = '0'의 빈도, [1] = '1'의 빈도
    for (uint8_t sym : symbol_stream) {
        if (sym < 2) freqs[sym]++;
    }

    const uint32_t scale_bits = 14;
    const uint32_t prob_scale = 1 << scale_bits;

    if (freqs[0] == 0 || freqs[1] == 0) {
        std::vector<uint8_t> output(8);
        uint32_t freq0_val = (freqs[0] == 0) ? 0 : prob_scale;
        memcpy(output.data(), &total_symbols, 4);
        memcpy(output.data() + 4, &freq0_val, 4);
        return output;
    }

    uint32_t norm_freqs[2];
    uint32_t cum_freqs[3] = { 0, freqs[0], freqs[0] + freqs[1] };
    for (int i = 1; i <= 2; i++) {
        cum_freqs[i] = ((uint64_t)prob_scale * cum_freqs[i]) / total_symbols;
    }
    for (int i = 0; i < 2; i++) {
        if (freqs[i] > 0 && cum_freqs[i + 1] == cum_freqs[i]) {
            if (i == 0) { if (cum_freqs[2] > cum_freqs[1]) cum_freqs[2]--; }
            else { if (cum_freqs[1] > cum_freqs[0]) cum_freqs[1]--; }
        }
    }
    norm_freqs[0] = cum_freqs[1] - cum_freqs[0];
    norm_freqs[1] = cum_freqs[2] - cum_freqs[1];

    size_t original_size = symbol_stream.size();
    std::vector<uint8_t> compressed_buffer(original_size + (original_size / 5) + 16);
    uint8_t* ptr = compressed_buffer.data() + compressed_buffer.size();

    RansState rans;
    RansEncInit(&rans);
    for (size_t i = symbol_stream.size(); i > 0; --i) {
        uint8_t s = symbol_stream[i - 1];
        if (s == 0) RansEncPut(&rans, &ptr, 0, norm_freqs[0], scale_bits);
        else RansEncPut(&rans, &ptr, norm_freqs[0], norm_freqs[1], scale_bits);
    }
    RansEncFlush(&rans, &ptr);

    size_t compressed_size = (compressed_buffer.data() + compressed_buffer.size()) - ptr;
    std::vector<uint8_t> final_output(8 + compressed_size);
    memcpy(final_output.data(), &total_symbols, 4);
    memcpy(final_output.data() + 4, &norm_freqs[0], 4);
    memcpy(final_output.data() + 8, ptr, compressed_size);

    return final_output;
}

// --- DECODE (Legacy byte-wise decoder) ---
// --- 디코딩 (기존 바이트 단위 디코더) ---
// KO: `encode` 함수로 압축된 데이터를 원본 바이트 스트림으로 복호화합니다.
// EN: Decodes data compressed by the `encode` function back into the original byte stream.
std::vector<uint8_t> rANS_Coder::decode(const std::vector<uint8_t>& compressed_data) {
    if (compressed_data.size() < 8) {
        throw std::runtime_error("Invalid compressed data: header too small.");
    }

    uint32_t total_symbols;
    uint32_t norm_freqs[2];
    const uint32_t scale_bits = 14;
    const uint32_t prob_scale = 1 << scale_bits;

    memcpy(&total_symbols, compressed_data.data(), 4);
    memcpy(&norm_freqs[0], compressed_data.data() + 4, 4);

    if (total_symbols == 0) return {};
    if (norm_freqs[0] == 0 || norm_freqs[0] == prob_scale) {
        uint8_t symbol_to_repeat = (norm_freqs[0] == 0) ? 1 : 0;
        return std::vector<uint8_t>(total_symbols, symbol_to_repeat);
    }
    norm_freqs[1] = prob_scale - norm_freqs[0];

    RansDecSymbol dsyms[2];
    RansDecSymbolInit(&dsyms[0], 0, norm_freqs[0]);
    RansDecSymbolInit(&dsyms[1], norm_freqs[0], norm_freqs[1]);
    std::vector<uint8_t> cum2sym(prob_scale);
    for (uint32_t i = 0; i < norm_freqs[0]; i++) cum2sym[i] = 0;
    for (uint32_t i = norm_freqs[0]; i < prob_scale; i++) cum2sym[i] = 1;

    uint8_t* ptr = const_cast<uint8_t*>(compressed_data.data()) + 8;
    RansState rans;
    RansDecInit(&rans, &ptr);

    std::vector<uint8_t> decoded_output(total_symbols);
    for (size_t i = 0; i < total_symbols; i++) {
        uint32_t cf = RansDecGet(&rans, scale_bits);
        uint8_t s = cum2sym[cf];
        decoded_output[i] = s;
        RansDecAdvanceSymbol(&rans, &ptr, &dsyms[s], scale_bits);
    }

    return decoded_output;
}


// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// +++ Bit Stream Handlers (encode_bits / decode_bits)
// +++ 비트 스트림 처리용 함수 (encode_bits / decode_bits)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// --- ENCODE_BITS ---
// KO: `std::vector<bool>` 형태의 비트 스트림을 압축합니다. 로직은 바이트 버전과 거의 동일합니다.
// EN: Compresses a bit stream in the form of `std::vector<bool>`. The logic is almost identical to the byte version.
std::vector<uint8_t> rANS_Coder::encode_bits(const std::vector<bool>& bit_stream) {
    if (bit_stream.empty()) {
        return {};
    }

    const uint32_t total_bits = static_cast<uint32_t>(bit_stream.size());
    uint32_t freqs[2] = { 0, 0 }; // [0] = frequency of false, [1] = frequency of true
    for (bool bit : bit_stream) {
        freqs[bit ? 1 : 0]++;
    }

    const uint32_t scale_bits = 14;
    const uint32_t prob_scale = 1 << scale_bits;

    if (freqs[0] == 0 || freqs[1] == 0) {
        std::vector<uint8_t> output(8);
        uint32_t freq0_val = (freqs[0] == 0) ? 0 : prob_scale;
        memcpy(output.data(), &total_bits, 4);
        memcpy(output.data() + 4, &freq0_val, 4);
        return output;
    }

    uint32_t norm_freqs[2];
    uint32_t cum_freqs[3] = { 0, freqs[0], freqs[0] + freqs[1] };
    for (int i = 1; i <= 2; i++) {
        cum_freqs[i] = ((uint64_t)prob_scale * cum_freqs[i]) / total_bits;
    }
    for (int i = 0; i < 2; i++) {
        if (freqs[i] > 0 && cum_freqs[i + 1] == cum_freqs[i]) {
            if (i == 0) { if (cum_freqs[2] > cum_freqs[1]) cum_freqs[2]--; }
            else { if (cum_freqs[1] > cum_freqs[0]) cum_freqs[1]--; }
        }
    }
    norm_freqs[0] = cum_freqs[1] - cum_freqs[0];
    norm_freqs[1] = cum_freqs[2] - cum_freqs[1];

    size_t original_size_bytes = (bit_stream.size() + 7) / 8;
    std::vector<uint8_t> compressed_buffer(original_size_bytes + (original_size_bytes / 5) + 16);
    uint8_t* ptr = compressed_buffer.data() + compressed_buffer.size();

    RansState rans;
    RansEncInit(&rans);
    for (size_t i = bit_stream.size(); i > 0; --i) {
        bool bit = bit_stream[i - 1];
        if (!bit) { // bit == false (0)
            RansEncPut(&rans, &ptr, 0, norm_freqs[0], scale_bits);
        }
        else { // bit == true (1)
            RansEncPut(&rans, &ptr, norm_freqs[0], norm_freqs[1], scale_bits);
        }
    }
    RansEncFlush(&rans, &ptr);

    size_t compressed_size = (compressed_buffer.data() + compressed_buffer.size()) - ptr;
    std::vector<uint8_t> final_output(8 + compressed_size);
    memcpy(final_output.data(), &total_bits, 4);
    memcpy(final_output.data() + 4, &norm_freqs[0], 4);
    memcpy(final_output.data() + 8, ptr, compressed_size);

    return final_output;
}

// --- DECODE_BITS ---
// KO: `encode_bits`로 압축된 데이터를 원본 비트 스트림으로 복호화합니다.
// EN: Decodes data compressed with `encode_bits` back to the original bit stream.
std::vector<bool> rANS_Coder::decode_bits(const std::vector<uint8_t>& compressed_data) {
    if (compressed_data.size() < 8) {
        throw std::runtime_error("Invalid compressed data: header too small.");
    }

    uint32_t total_bits;
    uint32_t norm_freqs[2];
    const uint32_t scale_bits = 14;
    const uint32_t prob_scale = 1 << scale_bits;

    memcpy(&total_bits, compressed_data.data(), 4);
    memcpy(&norm_freqs[0], compressed_data.data() + 4, 4);

    if (total_bits == 0) return {};
    if (norm_freqs[0] == 0 || norm_freqs[0] == prob_scale) {
        bool bit_to_repeat = (norm_freqs[0] == 0); // freq0이 0이면 반복할 비트는 true(1)
        return std::vector<bool>(total_bits, bit_to_repeat);
    }
    norm_freqs[1] = prob_scale - norm_freqs[0];

    RansDecSymbol dsyms[2];
    RansDecSymbolInit(&dsyms[0], 0, norm_freqs[0]);
    RansDecSymbolInit(&dsyms[1], norm_freqs[0], norm_freqs[1]);
    std::vector<uint8_t> cum2sym(prob_scale);
    for (uint32_t i = 0; i < norm_freqs[0]; i++) cum2sym[i] = 0; // false
    for (uint32_t i = norm_freqs[0]; i < prob_scale; i++) cum2sym[i] = 1; // true

    uint8_t* ptr = const_cast<uint8_t*>(compressed_data.data()) + 8;
    RansState rans;
    RansDecInit(&rans, &ptr);

    std::vector<bool> decoded_output(total_bits);
    for (size_t i = 0; i < total_bits; i++) {
        uint32_t cf = RansDecGet(&rans, scale_bits);
        uint8_t s = cum2sym[cf];
        decoded_output[i] = (s != 0);
        RansDecAdvanceSymbol(&rans, &ptr, &dsyms[s], scale_bits);
    }

    return decoded_output;
}


// --- ENCODE_RECONSTRUCTED_STREAM ---
// KO: 'reconstructed_stream'을 위한 특수 인코더입니다. 이 스트림의 심볼(마커/자리표시자)을
//     더 압축이 잘되는 비트 패턴("00", "01")으로 변환한 뒤 rANS로 압축합니다.
// EN: A special encoder for the 'reconstructed_stream'. It converts the symbols (markers/placeholders)
//     of this stream into more compressible bit patterns ("00", "01") and then compresses them with rANS.
std::vector<uint8_t> rANS_Coder::encode_reconstructed_stream(const std::vector<uint8_t>& recon_stream, bool is_placeholder_common) {
    if (recon_stream.empty()) {
        return {};
    }

    uint8_t common_symbol_val = is_placeholder_common ? 1 : 0;
    size_t n_common = 0;
    for (uint8_t s : recon_stream) {
        if (s == common_symbol_val) n_common++;
    }
    size_t n_rare = recon_stream.size() - n_common;

    const uint32_t total_bits = static_cast<uint32_t>(recon_stream.size() * 2);
    if (total_bits == 0) return {};

    uint32_t freqs[2];
    freqs[0] = static_cast<uint32_t>(n_common * 2 + n_rare * 1);
    freqs[1] = static_cast<uint32_t>(n_rare * 1);

    const uint32_t scale_bits = 14;
    const uint32_t prob_scale = 1 << scale_bits;

    if (freqs[1] == 0) {
        std::vector<uint8_t> output(8);
        memcpy(output.data(), &total_bits, 4);
        uint32_t freq0_val = prob_scale;
        memcpy(output.data() + 4, &freq0_val, 4);
        return output;
    }

    uint32_t norm_freqs[2];
    uint32_t cum_freqs[3] = { 0, freqs[0], freqs[0] + freqs[1] };
    for (int i = 1; i <= 2; i++) {
        cum_freqs[i] = ((uint64_t)prob_scale * cum_freqs[i]) / total_bits;
    }
    for (int i = 0; i < 2; i++) {
        if (freqs[i] > 0 && cum_freqs[i + 1] == cum_freqs[i]) {
            if (i == 0) { if (cum_freqs[2] > cum_freqs[1]) cum_freqs[2]--; }
            else { if (cum_freqs[1] > cum_freqs[0]) cum_freqs[1]--; }
        }
    }
    norm_freqs[0] = cum_freqs[1] - cum_freqs[0];
    norm_freqs[1] = cum_freqs[2] - cum_freqs[1];

    size_t approx_size = (total_bits / 8) + (total_bits / 40) + 16;
    std::vector<uint8_t> compressed_buffer(approx_size);
    uint8_t* ptr = compressed_buffer.data() + compressed_buffer.size();
    RansState rans;
    RansEncInit(&rans);

    for (size_t i = recon_stream.size(); i > 0; --i) {
        uint8_t s = recon_stream[i - 1];
        if (s == common_symbol_val) {
            RansEncPut(&rans, &ptr, 0, norm_freqs[0], scale_bits);
            RansEncPut(&rans, &ptr, 0, norm_freqs[0], scale_bits);
        }
        else {
            RansEncPut(&rans, &ptr, norm_freqs[0], norm_freqs[1], scale_bits);
            RansEncPut(&rans, &ptr, 0, norm_freqs[0], scale_bits);
        }
    }
    RansEncFlush(&rans, &ptr);

    size_t compressed_size = (compressed_buffer.data() + compressed_buffer.size()) - ptr;
    std::vector<uint8_t> final_output(8 + compressed_size);
    memcpy(final_output.data(), &total_bits, 4);
    memcpy(final_output.data() + 4, &norm_freqs[0], 4);
    memcpy(final_output.data() + 8, ptr, compressed_size);

    return final_output;
}

// --- DECODE_RECONSTRUCTED_STREAM (오류 수정된 최종 버전) ---
// KO: `encode_reconstructed_stream`으로 압축된 데이터를 복호화합니다.
// EN: Decodes data compressed by `encode_reconstructed_stream`.
std::vector<uint8_t> rANS_Coder::decode_reconstructed_stream(const std::vector<uint8_t>& compressed_data, bool is_placeholder_common) {
    if (compressed_data.size() < 8) {
        throw std::runtime_error("Invalid compressed data: header too small.");
    }

    uint32_t total_bits;
    uint32_t norm_freqs[2];
    const uint32_t scale_bits = 14;
    const uint32_t prob_scale = 1 << scale_bits;

    memcpy(&total_bits, compressed_data.data(), 4);
    memcpy(&norm_freqs[0], compressed_data.data() + 4, 4);

    if (total_bits == 0) return {};

    if (norm_freqs[0] >= prob_scale) {
        uint8_t common_symbol = is_placeholder_common ? 1 : 0;
        if (total_bits % 2 != 0) {
            throw std::runtime_error("Total bits should be even for common-only stream.");
        }
        return std::vector<uint8_t>(total_bits / 2, common_symbol);
    }
    norm_freqs[1] = prob_scale - norm_freqs[0];

    RansDecSymbol dsyms[2];
    RansDecSymbolInit(&dsyms[0], 0, norm_freqs[0]);
    RansDecSymbolInit(&dsyms[1], norm_freqs[0], norm_freqs[1]);
    std::vector<uint8_t> cum2sym(prob_scale);
    for (uint32_t i = 0; i < norm_freqs[0]; i++) cum2sym[i] = 0;
    for (uint32_t i = norm_freqs[0]; i < prob_scale; i++) cum2sym[i] = 1;

    uint8_t* ptr = const_cast<uint8_t*>(compressed_data.data()) + 8;
    RansState rans;
    RansDecInit(&rans, &ptr);

    std::vector<uint8_t> decoded_output;
    decoded_output.reserve(total_bits / 2);

    uint8_t common_symbol = is_placeholder_common ? 1 : 0;
    uint8_t rare_symbol = is_placeholder_common ? 0 : 1;

    for (uint32_t i = 0; i < total_bits; i += 2) {
        uint32_t cf_prefix = RansDecGet(&rans, scale_bits);
        uint8_t bit_prefix = cum2sym[cf_prefix];
        RansDecAdvanceSymbol(&rans, &ptr, &dsyms[bit_prefix], scale_bits);

        uint32_t cf_payload = RansDecGet(&rans, scale_bits);
        uint8_t bit_payload = cum2sym[cf_payload];
        RansDecAdvanceSymbol(&rans, &ptr, &dsyms[bit_payload], scale_bits);

        if (bit_payload == 1) {
            decoded_output.push_back(rare_symbol);
        }
        else {
            decoded_output.push_back(common_symbol);
        }
    }

    return decoded_output;
}