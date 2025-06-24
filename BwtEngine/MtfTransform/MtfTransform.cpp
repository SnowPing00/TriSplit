#include "MtfTransform.h"
#include <list>
#include <algorithm>
#include <set>
#include <iostream>

std::vector<uint16_t> MtfTransform::apply(const BwtResult& bwt_result) {
    const auto& l_stream = bwt_result.l_stream;
    if (l_stream.empty()) {
        return {};
    }

    // 1. 알파벳 리스트 초기화
    // 입력 스트림에 나타나는 모든 '고유한' 토큰들로 리스트를 구성
    std::set<uint16_t> alphabet_set(l_stream.begin(), l_stream.end());
    std::list<uint16_t> alphabet_list(alphabet_set.begin(), alphabet_set.end());

    std::vector<uint16_t> mtf_stream;
    mtf_stream.reserve(l_stream.size()); // 미리 메모리를 할당하여 성능 향상

    // 2. MTF 변환 수행
    for (uint16_t symbol : l_stream) {
        // 2a. 리스트에서 현재 심볼의 위치(반복자) 탐색
        auto it = std::find(alphabet_list.begin(), alphabet_list.end(), symbol);

        // 2b. 시작점부터의 거리(인덱스)를 계산하여 결과 스트림에 추가
        size_t index = std::distance(alphabet_list.begin(), it);
        mtf_stream.push_back(static_cast<uint16_t>(index));

        // 2c. 찾은 심볼을 리스트의 맨 앞으로 이동
		if (index != 0) { // 이미 맨 앞에 있다면 건드리지 않음
            // 현재 위치 심볼을 제거
            alphabet_list.erase(it);
            // 맨 앞에 다시 추가
            alphabet_list.push_front(symbol);
        }
    }

    std::cout << "MTF transform applied." << std::endl;
    return mtf_stream;
}