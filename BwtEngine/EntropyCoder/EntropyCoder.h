#pragma once
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <string> 
#include "../RleTransform/RleTransform.h"

// 허프만 트리의 노드를 표현하는 구조체
struct HuffmanNode {
    uint16_t symbol;
    int frequency;
    std::unique_ptr<HuffmanNode> left;
    std::unique_ptr<HuffmanNode> right;

    // 우선순위 큐에서 빈도수를 기준으로 비교하기 위한 연산자
    bool operator>(const HuffmanNode& other) const {
        return frequency > other.frequency;
    }
};

class EntropyCoder {
public:
    std::vector<uint8_t> huffman_encode(const RleResult& rle_result);
    // 복호화 함수는 RleResult를 반환
    RleResult huffman_decode(const std::vector<uint8_t>& compressed_data);

private:
    void _generate_codes(const HuffmanNode* node, std::string current_code, std::map<uint16_t, std::string>& huffman_codes);
};