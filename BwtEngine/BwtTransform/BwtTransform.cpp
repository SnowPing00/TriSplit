#include "BwtTransform.h"
extern "C" {
#include "../libsais/libsais.h"
}

#include <iostream>
#include <numeric>

BwtResult BwtTransform::apply(const std::vector<uint16_t>& token_block) {
    int32_t n = static_cast<int32_t>(token_block.size());
    if (n == 0) return {};

    std::vector<int32_t> T(token_block.begin(), token_block.end());
    std::vector<int32_t> SA(n);
    int32_t k = 65536;

    // libsais를 사용하여 BWT 수행
    libsais_int(T.data(), SA.data(), n, k, 0);

    BwtResult result;
    result.l_stream.resize(n);
    for (int32_t i = 0; i < n; ++i) {
        if (SA[i] == 0) {
            result.primary_index = i;
            result.l_stream[i] = T[n - 1];
        }
        else {
            result.l_stream[i] = T[SA[i] - 1];
        }
    }

    std::cout << "BWT transform applied on 1D stream." << std::endl;
    return result;
}

// BWT Inverse
std::vector<uint16_t> BwtTransform::inverse_apply(const BwtResult& bwt_result) {
    const auto& l_stream = bwt_result.l_stream;
    const uint32_t primary_index = bwt_result.primary_index;
    const size_t n = l_stream.size();

    if (n == 0) return {};

    std::vector<uint16_t> original_stream(n);

    // 1. C-table (Cumulative Frequency Table) 생성
    std::vector<uint32_t> c_table(65537, 0);
    for (uint16_t symbol : l_stream) {
        c_table[symbol]++;
    }
    uint32_t sum = 0;
    for (int i = 0; i < 65537; ++i) {
        uint32_t temp = c_table[i];
        c_table[i] = sum;
        sum += temp;
    }

    // 2. P-array (LF-mapping에 사용될 카운터) 생성
    std::vector<uint32_t> p_array(n);
    std::vector<uint32_t> temp_counts(65536, 0);
    for (size_t i = 0; i < n; ++i) {
        uint16_t symbol = l_stream[i];
        p_array[i] = c_table[symbol] + temp_counts[symbol];
        temp_counts[symbol]++;
    }

    // 3. 역변환 수행
    uint32_t current_index = primary_index;
    for (size_t i = 0; i < n; ++i) {
        original_stream[n - 1 - i] = l_stream[current_index];
        current_index = p_array[current_index];
    }

    std::cout << "BWT inverse transform applied on 1D stream." << std::endl;
    return original_stream;
}