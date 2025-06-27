#include "EntropyCoder.h"
#include <iostream>
#include <queue>
#include <functional>
#include <algorithm>
#include <map>

// --- 헤더 구조체 (인코딩/디코딩 시 파일과 메모리 구조를 일치시키기 위함) ---
#pragma pack(push, 1) // 1바이트 정렬을 보장하여 플랫폼 간 호환성 확보
struct HuffmanHeader {
    uint32_t primary_index;
    uint64_t total_bits;
    uint16_t code_count;
};
#pragma pack(pop)

// 우선순위 큐를 위한 비교 구조체
struct NodeComparator {
    bool operator()(const std::unique_ptr<HuffmanNode>& a, const std::unique_ptr<HuffmanNode>& b) const {
        return a->frequency > b->frequency;
    }
};

// --- Huffman Encode (코드북 저장 버그 수정) ---
std::vector<uint8_t> EntropyCoder::huffman_encode(const RleResult& rle_result) {
    const auto& symbol_stream = rle_result.rle_stream;
    if (symbol_stream.empty()) {
        HuffmanHeader header = { rle_result.primary_index, 0, 0 };
        std::vector<uint8_t> result(sizeof(header));
        memcpy(result.data(), &header, sizeof(header));
        return result;
    }

    // 1. 빈도수 계산
    std::map<uint16_t, int> frequencies;
    for (uint16_t symbol : symbol_stream) {
        frequencies[symbol]++;
    }

    // 2. 우선순위 큐 (Min Heap)를 이용한 허프만 트리 구축
    std::vector<std::unique_ptr<HuffmanNode>> node_heap;
    for (const auto& pair : frequencies) {
        node_heap.push_back(std::make_unique<HuffmanNode>(HuffmanNode{ pair.first, pair.second, nullptr, nullptr }));
    }
    std::make_heap(node_heap.begin(), node_heap.end(), NodeComparator{});

    if (node_heap.size() == 1) { // 엣지 케이스: 심볼이 하나일 경우
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

    // 3. 허프만 코드 생성
    const auto& root = node_heap.front();
    std::map<uint16_t, std::string> huffman_codes;
    _generate_codes(root.get(), "", huffman_codes);

    // 4. 데이터 직렬화 (헤더 + 코드북 + 데이터)
    std::vector<uint8_t> compressed_data;
    compressed_data.resize(sizeof(HuffmanHeader)); // 헤더 공간 확보

    // 4a. 코드북 직렬화
    std::vector<uint8_t> codebook_bytes;
    for (const auto& pair : huffman_codes) {
        uint16_t symbol = pair.first;
        const std::string& code = pair.second;
        uint8_t code_length = static_cast<uint8_t>(code.length());

        codebook_bytes.push_back(static_cast<uint8_t>(symbol >> 8));
        codebook_bytes.push_back(static_cast<uint8_t>(symbol & 0xFF));
        codebook_bytes.push_back(code_length);

        if (code_length > 0) {
            uint8_t byte = 0;
            for (int i = 0; i < code_length; ++i) {
                if (code[i] == '1') {
                    byte |= (1 << (7 - (i % 8)));
                }
                if ((i + 1) % 8 == 0) {
                    codebook_bytes.push_back(byte); // 수정된 부분: codebook_bytes에 추가
                    byte = 0;
                }
            }
            if (code_length % 8 != 0) {
                codebook_bytes.push_back(byte); // 수정된 부분: codebook_bytes에 추가
            }
        }
    }

    // 4b. 데이터 인코딩
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

    // 4c. 최종 헤더 채우기 및 데이터 병합
    HuffmanHeader header = {
        rle_result.primary_index,
        total_bits,
        static_cast<uint16_t>(huffman_codes.size())
    };
    memcpy(compressed_data.data(), &header, sizeof(header));
    compressed_data.insert(compressed_data.end(), codebook_bytes.begin(), codebook_bytes.end());
    compressed_data.insert(compressed_data.end(), data_bytes.begin(), data_bytes.end());

    std::cout << "Huffman encoding finished. RLE stream size: " << symbol_stream.size() << " -> Compressed size: " << compressed_data.size() << " bytes" << std::endl;
    return compressed_data;
}

// --- Huffman Decode (완전한 구현) ---
RleResult EntropyCoder::huffman_decode(const std::vector<uint8_t>& compressed_data) {
    if (compressed_data.size() < sizeof(HuffmanHeader)) {
        throw std::runtime_error("Invalid Huffman data: header is too small.");
    }

    // 1. 헤더 읽기
    HuffmanHeader header;
    memcpy(&header, compressed_data.data(), sizeof(header));

    if (header.code_count == 0 && header.total_bits == 0) {
        return { {}, header.primary_index };
    }

    const uint8_t* read_ptr = compressed_data.data() + sizeof(header);
    const uint8_t* data_end = compressed_data.data() + compressed_data.size();

    // 2. 허프만 트리 재구성
    auto root = std::make_unique<HuffmanNode>();
    size_t codebook_byte_size = 0;
    for (uint16_t i = 0; i < header.code_count; ++i) {
        // 심볼 (2바이트), 코드 길이 (1바이트) 읽기
        uint16_t symbol = (static_cast<uint16_t>(read_ptr[0]) << 8) | read_ptr[1];
        uint8_t code_length = read_ptr[2];
        read_ptr += 3;
        codebook_byte_size += 3;

        HuffmanNode* current_node = root.get();
        size_t code_byte_len = (code_length + 7) / 8;

        for (int j = 0; j < code_length; ++j) {
            bool is_one = (read_ptr[j / 8] >> (7 - (j % 8))) & 1;
            if (is_one) {
                if (!current_node->right) current_node->right = std::make_unique<HuffmanNode>();
                current_node = current_node->right.get();
            }
            else {
                if (!current_node->left) current_node->left = std::make_unique<HuffmanNode>();
                current_node = current_node->left.get();
            }
        }
        current_node->symbol = symbol;
        read_ptr += code_byte_len;
        codebook_byte_size += code_byte_len;
    }

    // 3. 데이터 디코딩
    std::vector<uint16_t> decoded_stream;
    decoded_stream.reserve(header.total_bits); // 대략적인 크기 예약

    const uint8_t* data_ptr = compressed_data.data() + sizeof(header) + codebook_byte_size;
    uint64_t bits_decoded = 0;
    HuffmanNode* current_node = root.get();

    while (bits_decoded < header.total_bits) {
        bool is_one = (*data_ptr >> (7 - (bits_decoded % 8))) & 1;

        current_node = is_one ? current_node->right.get() : current_node->left.get();

        if (!current_node->left && !current_node->right) {
            decoded_stream.push_back(current_node->symbol);
            current_node = root.get(); // 다음 심볼을 위해 루트에서 다시 시작
        }

        bits_decoded++;
        if (bits_decoded % 8 == 0) {
            data_ptr++;
        }
    }

    std::cout << "Huffman decoding applied. Compressed size: " << compressed_data.size() << " bytes -> RLE stream size: " << decoded_stream.size() << std::endl;
    return { decoded_stream, header.primary_index };
}


// --- _generate_codes (기존과 동일) ---
void EntropyCoder::_generate_codes(const HuffmanNode* node, std::string current_code, std::map<uint16_t, std::string>& huffman_codes) {
    if (!node) {
        return;
    }
    // 리프 노드에 도달하면 심볼과 코드를 맵에 저장
    if (!node->left && !node->right) {
        huffman_codes[node->symbol] = current_code.empty() ? "0" : current_code;
        return;
    }
    _generate_codes(node->left.get(), current_code + "0", huffman_codes);
    _generate_codes(node->right.get(), current_code + "1", huffman_codes);
}