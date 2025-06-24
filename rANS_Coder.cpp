#include "rANS_Coder.h"
#include "rans_byte.h" // ryg_rans 라이브러리
#include <stdexcept>
#include <vector>

// --- ENCODE ---
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

    // 엣지 케이스: 한 종류의 심볼만 있는 경우 압축하지 않고 원본 데이터를 저장합니다.
    if (freqs[0] == 0 || freqs[1] == 0) {
        std::vector<uint8_t> output;
        output.resize(8 + total_symbols); // 헤더 8바이트 + 데이터 크기

        // 헤더에 빈도수 정보를 기록 (0 또는 prob_scale)
        uint32_t freq0_val = (freqs[0] == 0) ? 0 : prob_scale;
        memcpy(output.data(), &total_symbols, 4);
        memcpy(output.data() + 4, &freq0_val, 4);

        if (total_symbols > 0) {
            memcpy(output.data() + 8, symbol_stream.data(), total_symbols);
        }
        return output;
    }

    // 빈도수 정규화: rANS가 요구하는 형태로 빈도수 스케일을 조정합니다. (총합 prob_scale)
    uint32_t norm_freqs[2];
    uint32_t cum_freqs[3] = { 0, freqs[0], freqs[0] + freqs[1] };

    // 1. 전체 심볼 수 대비 비율로 누적 빈도수를 정규화합니다.
    for (int i = 1; i <= 2; i++) {
        cum_freqs[i] = ((uint64_t)prob_scale * cum_freqs[i]) / total_symbols;
    }

    // 2. 정규화 과정에서 빈도가 0이 되어버린 심볼이 없도록 최소 빈도를 보장합니다.
    for (int i = 0; i < 2; i++) {
        if (freqs[i] > 0 && cum_freqs[i + 1] == cum_freqs[i]) {
            // 다른 심볼의 빈도에서 1을 가져옵니다.
            if (i == 0) {
                if (cum_freqs[2] > cum_freqs[1]) cum_freqs[2]--;
            }
            else { // i == 1
                if (cum_freqs[1] > cum_freqs[0]) cum_freqs[1]--;
            }
        }
    }
    // 3. 최종 정규화된 개별 빈도수를 계산합니다.
    norm_freqs[0] = cum_freqs[1] - cum_freqs[0];
    norm_freqs[1] = cum_freqs[2] - cum_freqs[1];

    // 압축 버퍼를 넉넉하게 할당합니다 (원본의 1.2배 + 16바이트).
    size_t original_size = symbol_stream.size();
    std::vector<uint8_t> compressed_buffer(original_size + (original_size / 5) + 16);
    // rANS는 역순으로 데이터를 쓰므로 포인터를 버퍼의 끝으로 설정합니다.
    uint8_t* ptr = compressed_buffer.data() + compressed_buffer.size();

    RansState rans;
    RansEncInit(&rans);

    // rANS 인코딩 루프 (데이터의 끝에서부터 시작)
    for (size_t i = symbol_stream.size(); i > 0; --i) {
        uint8_t s = symbol_stream[i - 1];
        if (s == 0) {
            RansEncPut(&rans, &ptr, 0, norm_freqs[0], scale_bits);
        }
        else {
            RansEncPut(&rans, &ptr, norm_freqs[0], norm_freqs[1], scale_bits);
        }
    }
    RansEncFlush(&rans, &ptr);

    // 최종 결과물: 헤더와 압축된 데이터를 하나의 벡터로 조합합니다.
    size_t compressed_size = (compressed_buffer.data() + compressed_buffer.size()) - ptr;
    std::vector<uint8_t> final_output(8 + compressed_size);

    // 헤더 쓰기: [총 심볼 수 (4바이트)] [심볼 0의 빈도수 (4바이트)]
    memcpy(final_output.data(), &total_symbols, 4);
    memcpy(final_output.data() + 4, &norm_freqs[0], 4);
    // 압축된 데이터 쓰기
    memcpy(final_output.data() + 8, ptr, compressed_size);

    return final_output;
}


// --- DECODE ---
std::vector<uint8_t> rANS_Coder::decode(const std::vector<uint8_t>& compressed_data) {
    if (compressed_data.size() < 8) {
        throw std::runtime_error("Invalid compressed data: header too small.");
    }

    // 1. 헤더 파싱
    uint32_t total_symbols;
    uint32_t norm_freqs[2];
    const uint32_t scale_bits = 14;
    const uint32_t prob_scale = 1 << scale_bits;

    memcpy(&total_symbols, compressed_data.data(), 4);
    memcpy(&norm_freqs[0], compressed_data.data() + 4, 4);

    if (total_symbols == 0) {
        return {};
    }

    // 엣지 케이스: 한 종류의 심볼로만 구성된 비압축 데이터 처리
    if (norm_freqs[0] == 0 || norm_freqs[0] == prob_scale) {
        uint8_t symbol_to_repeat = (norm_freqs[0] == 0) ? 1 : 0;
        return std::vector<uint8_t>(total_symbols, symbol_to_repeat);
    }
    // 심볼 1의 빈도수는 헤더 정보로부터 계산합니다.
    norm_freqs[1] = prob_scale - norm_freqs[0];

    // 2. 디코딩 준비
    // 디코딩 속도 향상을 위한 심볼 정보 및 조회 테이블(cum2sym) 준비
    RansDecSymbol dsyms[2];
    RansDecSymbolInit(&dsyms[0], 0, norm_freqs[0]);
    RansDecSymbolInit(&dsyms[1], norm_freqs[0], norm_freqs[1]);

    std::vector<uint8_t> cum2sym(prob_scale);
    for (uint32_t i = 0; i < norm_freqs[0]; i++) cum2sym[i] = 0;
    for (uint32_t i = norm_freqs[0]; i < prob_scale; i++) cum2sym[i] = 1;

    // 3. rANS 디코딩
    uint8_t* ptr = const_cast<uint8_t*>(compressed_data.data()) + 8;
    RansState rans;
    RansDecInit(&rans, &ptr);

    std::vector<uint8_t> decoded_output;
    decoded_output.reserve(total_symbols);

    for (size_t i = 0; i < total_symbols; i++) {
        uint32_t cf = RansDecGet(&rans, scale_bits);
        uint8_t s = cum2sym[cf];
        decoded_output.push_back(s);
        RansDecAdvanceSymbol(&rans, &ptr, &dsyms[s], scale_bits);
    }

    return decoded_output;
}