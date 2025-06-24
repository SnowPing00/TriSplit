#pragma once
#include <vector>
#include <cstdint>
#include <map>
#include <memory>
#include <string> 

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
    // 이 함수 하나만 외부에서 호출
    std::vector<uint8_t> huffman_encode(const std::vector<uint16_t>& symbol_stream);

private:
    // huffman_encode 내부에서만 사용될 비공개 헬퍼 함수들
    void _generate_codes(const HuffmanNode* node, std::string current_code, std::map<uint16_t, std::string>& huffman_codes);
};