#include "EntropyCoder.h"
#include <iostream>
#include <queue>
#include <functional>
#include <algorithm>

struct NodeComparator {
    bool operator()(const std::unique_ptr<HuffmanNode>& a, const std::unique_ptr<HuffmanNode>& b) const {
        return a->frequency > b->frequency;
    }
};

std::vector<uint8_t> EntropyCoder::huffman_encode(const std::vector<uint16_t>& symbol_stream) {
    if (symbol_stream.empty()) {
        return {};
    }

    std::map<uint16_t, int> frequencies;
    for (uint16_t symbol : symbol_stream) {
        frequencies[symbol]++;
    }

    std::vector<std::unique_ptr<HuffmanNode>> node_heap;
    for (const auto& pair : frequencies) {
        node_heap.push_back(std::make_unique<HuffmanNode>(HuffmanNode{ pair.first, pair.second, nullptr, nullptr }));
    }

    std::make_heap(node_heap.begin(), node_heap.end(), NodeComparator{});

    // 노드가 하나뿐인 엣지 케이스 처리
    if (node_heap.size() == 1) {
        auto single_node = std::move(node_heap.back());
        node_heap.pop_back();
        auto parent = std::make_unique<HuffmanNode>(HuffmanNode{ 0, single_node->frequency, std::move(single_node), nullptr });
        node_heap.push_back(std::move(parent));
        std::push_heap(node_heap.begin(), node_heap.end(), NodeComparator{});
    }

    while (node_heap.size() > 1) {
        std::pop_heap(node_heap.begin(), node_heap.end(), NodeComparator{});
        auto left = std::move(node_heap.back());
        node_heap.pop_back();

        std::pop_heap(node_heap.begin(), node_heap.end(), NodeComparator{});
        auto right = std::move(node_heap.back());
        node_heap.pop_back();

        int sum_freq = left->frequency + right->frequency;
        auto parent = std::make_unique<HuffmanNode>(HuffmanNode{ 0, sum_freq, std::move(left), std::move(right) });

        node_heap.push_back(std::move(parent));
        std::push_heap(node_heap.begin(), node_heap.end(), NodeComparator{});
    }

    const auto& root = node_heap.front();

    std::map<uint16_t, std::string> huffman_codes;
    _generate_codes(root.get(), "", huffman_codes);

    std::vector<uint8_t> compressed_data;

    // 헤더: 코드북 정보 기록
    uint16_t code_count = static_cast<uint16_t>(huffman_codes.size());
    compressed_data.push_back(static_cast<uint8_t>(code_count >> 8));
    compressed_data.push_back(static_cast<uint8_t>(code_count & 0xFF));

    // 디코딩을 위해 전체 비트 수를 헤더에 임시로 0으로 채움
    for (int i = 0; i < 8; ++i) {
        compressed_data.push_back(0);
    }

    for (const auto& pair : huffman_codes) {
        uint16_t symbol = pair.first;
        uint8_t code_length = static_cast<uint8_t>(pair.second.length());
        const std::string& code = pair.second;

        compressed_data.push_back(static_cast<uint8_t>(symbol >> 8));
        compressed_data.push_back(static_cast<uint8_t>(symbol & 0xFF));
        compressed_data.push_back(code_length);

        if (code_length > 0) {
            uint8_t byte = 0;
            for (int i = 0; i < code_length; ++i) {
                if (code[i] == '1') {
                    byte |= (1 << (7 - (i % 8)));
                }
                if ((i + 1) % 8 == 0) {
                    compressed_data.push_back(byte);
                    byte = 0;
                }
            }
            if (code_length % 8 != 0) {
                compressed_data.push_back(byte);
            }
        }
    }

    // 데이터 인코딩
    uint64_t total_bits = 0;
    std::vector<uint8_t> data_bytes;
    uint8_t current_byte = 0;
    int bit_position = 0;
    for (uint16_t symbol : symbol_stream) {
        const std::string& code = huffman_codes[symbol];
        total_bits += code.length();
        for (char bit : code) {
            if (bit == '1') {
                current_byte |= (1 << (7 - bit_position));
            }
            bit_position++;
            if (bit_position == 8) {
                data_bytes.push_back(current_byte);
                current_byte = 0;
                bit_position = 0;
            }
        }
    }

    if (bit_position > 0) {
        data_bytes.push_back(current_byte);
    }

    // 실제 계산된 비트 수를 헤더에 덮어쓰기
    for (int i = 0; i < 8; ++i) {
        compressed_data[2 + i] = static_cast<uint8_t>((total_bits >> (8 * i)) & 0xFF);
    }

    // 최종 데이터 병합
    compressed_data.insert(compressed_data.end(), data_bytes.begin(), data_bytes.end());

    std::cout << "Huffman encoding finished. Compressed size: " << compressed_data.size() << " bytes" << std::endl;
    return compressed_data;
}

void EntropyCoder::_generate_codes(const HuffmanNode* node, std::string current_code, std::map<uint16_t, std::string>& huffman_codes) {
    if (!node) {
        return;
    }
    if (!node->left && !node->right) {
        huffman_codes[node->symbol] = current_code.empty() ? "0" : current_code;
        return;
    }
    _generate_codes(node->left.get(), current_code + "0", huffman_codes);
    _generate_codes(node->right.get(), current_code + "1", huffman_codes);
}