#include "MtfTransform.h"
#include <algorithm>
#include <set>
#include <iostream>

// 반환 타입을 MtfResult로 변경하고, 초기 알파벳을 결과에 포함시킵니다.
MtfResult MtfTransform::apply(const BwtResult& bwt_result) {
    const auto& l_stream = bwt_result.l_stream;
    if (l_stream.empty()) {
        // 비어있더라도 primary_index는 전달해야 합니다.
        return { {}, {}, bwt_result.primary_index };
    }

    // 1. 알파벳 리스트 및 초기 알파벳 벡터 생성
    std::set<uint16_t> alphabet_set(l_stream.begin(), l_stream.end());
    std::vector<uint16_t> initial_alphabet_vector(alphabet_set.begin(), alphabet_set.end());
    std::list<uint16_t> alphabet_list(initial_alphabet_vector.begin(), initial_alphabet_vector.end());

    std::vector<uint16_t> mtf_stream;
    mtf_stream.reserve(l_stream.size());

    // 2. MTF 변환 수행 (기존 로직과 동일)
    for (uint16_t symbol : l_stream) {
        auto it = std::find(alphabet_list.begin(), alphabet_list.end(), symbol);
        size_t index = std::distance(alphabet_list.begin(), it);
        mtf_stream.push_back(static_cast<uint16_t>(index));

        if (index != 0) {
            alphabet_list.erase(it);
            alphabet_list.push_front(symbol);
        }
    }

    std::cout << "MTF transform applied. Alphabet size: " << initial_alphabet_vector.size() << std::endl;

    // 3. 결과 구조체에 모든 정보를 담아 반환
    return { mtf_stream, initial_alphabet_vector, bwt_result.primary_index };
}


// --- 논리 오류를 수정한 최종 역변환 함수 ---
BwtResult MtfTransform::inverse_apply(const MtfResult& mtf_result) {
    const auto& mtf_stream = mtf_result.mtf_stream;
    if (mtf_stream.empty()) {
        return { {}, mtf_result.primary_index };
    }

    // 1. 알파벳 리스트를 전달받은 'initial_alphabet'으로 정확하게 복원합니다.
    std::list<uint16_t> alphabet_list(mtf_result.initial_alphabet.begin(), mtf_result.initial_alphabet.end());

    // 만약 초기 알파벳이 비어있다면 오류입니다.
    if (alphabet_list.empty()) {
        throw std::runtime_error("Cannot decode MTF stream: initial alphabet is missing.");
    }

    std::vector<uint16_t> l_stream;
    l_stream.reserve(mtf_stream.size());

    // 2. MTF 역변환 수행
    for (uint16_t index : mtf_stream) {
        if (index >= alphabet_list.size()) {
            throw std::runtime_error("Invalid MTF index found during decoding.");
        }

        auto it = alphabet_list.begin();
        std::advance(it, index); // index만큼 이동

        uint16_t symbol = *it;
        l_stream.push_back(symbol);

        // 찾은 심볼을 맨 앞으로 이동
        if (index != 0) {
            alphabet_list.erase(it);
            alphabet_list.push_front(symbol);
        }
    }

    std::cout << "MTF inverse transform applied." << std::endl;
    return { l_stream, mtf_result.primary_index };
}