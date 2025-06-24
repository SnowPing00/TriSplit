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