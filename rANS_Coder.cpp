#include "rANS_Coder.h"
#include "rans_byte.h" // ryg_rans 라이브러리
#include <stdexcept>
#include <vector>
#include <cstring>   // for memcpy

// --- ENCODE (기존 바이트 단위 인코더) ---
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

    // 엣지 케이스: 한 종류의 심볼만 있는 경우 (거의 발생하지 않음)
    if (freqs[0] == 0 || freqs[1] == 0) {
        std::vector<uint8_t> output(8); // 헤더만 있는 데이터
        uint32_t freq0_val = (freqs[0] == 0) ? 0 : prob_scale;
        memcpy(output.data(), &total_symbols, 4);
        memcpy(output.data() + 4, &freq0_val, 4);
        return output; // 데이터 없이 헤더만 반환
    }

    // 빈도수 정규화
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

    // rANS 인코딩
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

    // 최종 결과물 조합
    size_t compressed_size = (compressed_buffer.data() + compressed_buffer.size()) - ptr;
    std::vector<uint8_t> final_output(8 + compressed_size);
    memcpy(final_output.data(), &total_symbols, 4);
    memcpy(final_output.data() + 4, &norm_freqs[0], 4);
    memcpy(final_output.data() + 8, ptr, compressed_size);

    return final_output;
}

// --- DECODE (기존 바이트 단위 디코더) ---
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
// +++ 신규 함수: 비트 스트림 처리용 (encode_bits / decode_bits)
// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// --- ENCODE_BITS ---
std::vector<uint8_t> rANS_Coder::encode_bits(const std::vector<bool>& bit_stream) {
    if (bit_stream.empty()) {
        return {};
    }

    const uint32_t total_bits = static_cast<uint32_t>(bit_stream.size());
    uint32_t freqs[2] = { 0, 0 }; // [0] = false의 빈도, [1] = true의 빈도
    for (bool bit : bit_stream) {
        freqs[bit ? 1 : 0]++;
    }

    const uint32_t scale_bits = 14;
    const uint32_t prob_scale = 1 << scale_bits;

    // 엣지 케이스: 모든 비트가 0이거나 모든 비트가 1인 경우
    if (freqs[0] == 0 || freqs[1] == 0) {
        std::vector<uint8_t> output(8); // 헤더만 있는 데이터
        uint32_t freq0_val = (freqs[0] == 0) ? 0 : prob_scale;
        memcpy(output.data(), &total_bits, 4);
        memcpy(output.data() + 4, &freq0_val, 4);
        return output;
    }

    // 빈도수 정규화 (기존 로직과 동일)
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

    // rANS 인코딩
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

    // 최종 결과물 조합
    size_t compressed_size = (compressed_buffer.data() + compressed_buffer.size()) - ptr;
    std::vector<uint8_t> final_output(8 + compressed_size);
    memcpy(final_output.data(), &total_bits, 4);
    memcpy(final_output.data() + 4, &norm_freqs[0], 4);
    memcpy(final_output.data() + 8, ptr, compressed_size);

    return final_output;
}

// --- DECODE_BITS ---
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
            // "00"을 인코딩 (LIFO: 0을 넣고, 그 위에 0을 또 넣음)
            RansEncPut(&rans, &ptr, 0, norm_freqs[0], scale_bits);
            RansEncPut(&rans, &ptr, 0, norm_freqs[0], scale_bits);
        }
        else {
            // "01"을 인코딩 (LIFO: 1을 넣고, 그 위에 0을 넣음)
            RansEncPut(&rans, &ptr, norm_freqs[0], norm_freqs[1], scale_bits); // 1
            RansEncPut(&rans, &ptr, 0, norm_freqs[0], scale_bits);            // 0
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

    // 디코딩 루프: 비트를 2개씩 순차적으로 읽어 심볼을 정확히 복원
    for (uint32_t i = 0; i < total_bits; i += 2) {
        // LIFO에 따라 가장 마지막에 들어간 비트를 먼저 꺼냄.
        // 이 비트는 항상 '0'이어야 함.
        uint32_t cf_prefix = RansDecGet(&rans, scale_bits);
        uint8_t bit_prefix = cum2sym[cf_prefix];
        RansDecAdvanceSymbol(&rans, &ptr, &dsyms[bit_prefix], scale_bits);

        // 그 다음 비트(payload)를 꺼내서 심볼을 결정함.
        uint32_t cf_payload = RansDecGet(&rans, scale_bits);
        uint8_t bit_payload = cum2sym[cf_payload];
        RansDecAdvanceSymbol(&rans, &ptr, &dsyms[bit_payload], scale_bits);

        if (bit_payload == 1) {
            // 페이로드 비트가 1이면, 원래 패턴은 "01" -> rare_symbol
            decoded_output.push_back(rare_symbol);
        }
        else {
            // 페이로드 비트가 0이면, 원래 패턴은 "00" -> common_symbol
            decoded_output.push_back(common_symbol);
        }
    }

    return decoded_output;
}