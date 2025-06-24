#pragma once
#include <vector>
#include <cstdint>

class rANS_Coder {
public:
    /**
     * @brief 심볼 스트림(0과 1로 구성)을 rANS 알고리즘으로 압축
     * @param symbol_stream '값 비트맵' 또는 '보조 마스크' 같은 입력 스트림. 각 요소는 0 또는 1
     * @return 압축된 데이터 바이트 벡터. 헤더(빈도수 정보) 포함
     */
    std::vector<uint8_t> encode(const std::vector<uint8_t>& symbol_stream);

    /**
     * @brief rANS로 압축된 데이터를 원본 심볼 스트림으로 복호화
     * @param compressed_data encode 함수로 생성된 압축 데이터
     * @return 복원된 원본 심볼 스트림
     */
    std::vector<uint8_t> decode(const std::vector<uint8_t>& compressed_data);
};