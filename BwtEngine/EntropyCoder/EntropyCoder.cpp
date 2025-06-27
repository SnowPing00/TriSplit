#include "EntropyCoder.h"
#include <iostream>
#include <queue>
#include <functional>
#include <algorithm>
#include <map>
#include <cstring> // memcpy를 위해 추가

// --- 헤더 구조체 ---
// 파일/메모리의 데이터 구조를 일치시키고,
// MTF 역변환에 필요한 모든 정보를 포함하도록 수정합니다.
#pragma pack(push, 1)
struct HuffmanHeader {
    uint32_t primary_index;
    uint64_t total_bits;
    uint16_t code_count;
    uint16_t alphabet_size; // [추가] 초기 사전의 크기를 저장
};
#pragma pack(pop)

// 우선순위 큐(Min Heap)를 위한 비교 구조체
struct NodeComparator {
    bool operator()(const std::unique_ptr<HuffmanNode>& a, const std::unique_ptr<HuffmanNode>& b) const {
        return a->frequency > b->frequency;
    }
};

// --- 허프만 인코딩 함수 ---
std::vector<uint8_t> EntropyCoder::huffman_encode(const RleResult& rle_result) {
    const auto& symbol_stream = rle_result.rle_stream;
    const auto& initial_alphabet = rle_result.initial_alphabet;

    // 스트림이 비어있으면, 메타데이터만 담은 헤더를 반환합니다.
    if (symbol_stream.empty()) {
        HuffmanHeader header = { rle_result.primary_index, 0, 0, static_cast<uint16_t>(initial_alphabet.size()) };
        std::vector<uint8_t> result(sizeof(header));
        memcpy(result.data(), &header, sizeof(header));

        // [추가] 비어있더라도 초기 사전 정보는 저장해야 합니다.
        if (!initial_alphabet.empty()) {
            size_t alphabet_bytes_size = initial_alphabet.size() * sizeof(uint16_t);
            result.resize(sizeof(header) + alphabet_bytes_size);
            memcpy(result.data() + sizeof(header), initial_alphabet.data(), alphabet_bytes_size);
        }
        return result;
    }

    // 1. 빈도수 계산
    std::map<uint16_t, int> frequencies;
    for (uint16_t symbol : symbol_stream) {
        frequencies[symbol]++;
    }

    // 2. 우선순위 큐를 이용한 허프만 트리 구축
    std::priority_queue<std::unique_ptr<HuffmanNode>, std::vector<std::unique_ptr<HuffmanNode>>, NodeComparator> min_heap;
    for (const auto& pair : frequencies) {
        min_heap.push(std::make_unique<HuffmanNode>(HuffmanNode{ pair.first, pair.second, nullptr, nullptr }));
    }

    // 심볼이 하나뿐인 엣지 케이스 처리
    if (min_heap.size() == 1) {
        auto single_node = std::move(const_cast<std::unique_ptr<HuffmanNode>&>(min_heap.top()));
        min_heap.pop();
        auto parent = std::make_unique<HuffmanNode>(HuffmanNode{ 0, single_node->frequency, std::move(single_node), nullptr });
        min_heap.push(std::move(parent));
    }

    while (min_heap.size() > 1) {
        auto left = std::move(const_cast<std::unique_ptr<HuffmanNode>&>(min_heap.top()));
        min_heap.pop();
        auto right = std::move(const_cast<std::unique_ptr<HuffmanNode>&>(min_heap.top()));
        min_heap.pop();

        int sum_freq = left->frequency + right->frequency;
        auto parent = std::make_unique<HuffmanNode>(HuffmanNode{ 0, sum_freq, std::move(left), std::move(right) });
        min_heap.push(std::move(parent));
    }

    // 3. 허프만 코드 생성
    const auto& root = min_heap.top();
    std::map<uint16_t, std::string> huffman_codes;
    _generate_codes(root.get(), "", huffman_codes);

    // 4. 데이터 직렬화 (헤더 + 코드북 + 초기 사전 + 압축 데이터)
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

        uint8_t byte = 0;
        for (int i = 0; i < code_length; ++i) {
            if (code[i] == '1') {
                byte |= (1 << (7 - (i % 8)));
            }
            if ((i + 1) % 8 == 0 || i == code_length - 1) {
                codebook_bytes.push_back(byte);
                byte = 0;
            }
        }
    }

    // 4b. 초기 사전(alphabet) 직렬화
    std::vector<uint8_t> alphabet_bytes(initial_alphabet.size() * sizeof(uint16_t));
    if (!initial_alphabet.empty()) {
        memcpy(alphabet_bytes.data(), initial_alphabet.data(), alphabet_bytes.size());
    }

    // 4c. 데이터 인코딩
    uint64_t total_bits = 0;
    std::vector<uint8_t> data_bytes;
    uint8_t current_byte = 0;
    int bit_position = 0;
    for (uint16_t symbol : symbol_stream) {
        const std::string& code = huffman_codes.at(symbol);
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

    // 4d. 최종 헤더 채우기 및 데이터 병합
    HuffmanHeader header = {
        rle_result.primary_index,
        total_bits,
        static_cast<uint16_t>(huffman_codes.size()),
        static_cast<uint16_t>(initial_alphabet.size())
    };
    memcpy(compressed_data.data(), &header, sizeof(header));
    compressed_data.insert(compressed_data.end(), codebook_bytes.begin(), codebook_bytes.end());
    compressed_data.insert(compressed_data.end(), alphabet_bytes.begin(), alphabet_bytes.end());
    compressed_data.insert(compressed_data.end(), data_bytes.begin(), data_bytes.end());

    std::cout << "Huffman encoding finished. RLE stream size: " << symbol_stream.size() << " -> Compressed size: " << compressed_data.size() << " bytes" << std::endl;
    return compressed_data;
}

// --- 허프만 디코딩 함수 ---
RleResult EntropyCoder::huffman_decode(const std::vector<uint8_t>& compressed_data) {
    if (compressed_data.size() < sizeof(HuffmanHeader)) {
        throw std::runtime_error("Invalid Huffman data: header is too small.");
    }

    // 1. 헤더 읽기
    HuffmanHeader header;
    memcpy(&header, compressed_data.data(), sizeof(header));

    const uint8_t* read_ptr = compressed_data.data() + sizeof(header);

    // [수정] 스트림이 비어있던 경우, 복원된 사전 정보와 함께 반환
    if (header.code_count == 0 && header.total_bits == 0) {
        std::vector<uint16_t> initial_alphabet;
        if (header.alphabet_size > 0) {
            initial_alphabet.resize(header.alphabet_size);
            memcpy(initial_alphabet.data(), read_ptr, header.alphabet_size * sizeof(uint16_t));
        }
        return { {}, initial_alphabet, header.primary_index };
    }

    // 2. 허프만 트리 재구성
    auto root = std::make_unique<HuffmanNode>();
    size_t codebook_byte_size = 0;
    for (uint16_t i = 0; i < header.code_count; ++i) {
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

    // 3. 초기 사전(alphabet) 복원
    std::vector<uint16_t> initial_alphabet(header.alphabet_size);
    size_t alphabet_bytes_size = header.alphabet_size * sizeof(uint16_t);
    if (header.alphabet_size > 0) {
        memcpy(initial_alphabet.data(), read_ptr, alphabet_bytes_size);
    }

    // 4. 데이터 디코딩
    std::vector<uint16_t> decoded_stream;
    decoded_stream.reserve(header.total_bits); // 대략적인 크기 예약

    const uint8_t* data_ptr = compressed_data.data() + sizeof(header) + codebook_byte_size + alphabet_bytes_size;
    uint64_t bits_decoded = 0;
    HuffmanNode* current_node = root.get();

    while (bits_decoded < header.total_bits) {
        bool is_one = (*data_ptr >> (7 - (bits_decoded % 8))) & 1;

        current_node = is_one ? current_node->right.get() : current_node->left.get();

        if (current_node && !current_node->left && !current_node->right) {
            decoded_stream.push_back(current_node->symbol);
            current_node = root.get();
        }

        bits_decoded++;
        if (bits_decoded % 8 == 0) {
            data_ptr++;
        }
    }

    std::cout << "Huffman decoding applied. Compressed size: " << compressed_data.size() << " bytes -> RLE stream size: " << decoded_stream.size() << std::endl;
    // [수정] 복원된 모든 정보를 RleResult에 담아 반환
    return { decoded_stream, initial_alphabet, header.primary_index };
}

// --- 재귀적으로 허프만 코드를 생성하는 내부 함수 ---
void EntropyCoder::_generate_codes(const HuffmanNode* node, std::string current_code, std::map<uint16_t, std::string>& huffman_codes) {
    if (!node) {
        return;
    }
    // 리프 노드(자식이 없는 노드)에 도달하면 심볼과 코드를 맵에 저장
    if (!node->left && !node->right) {
        // 엣지 케이스: 트리에 노드가 하나뿐일 경우 코드는 "0"
        huffman_codes[node->symbol] = current_code.empty() ? "0" : current_code;
        return;
    }
    _generate_codes(node->left.get(), current_code + "0", huffman_codes);
    _generate_codes(node->right.get(), current_code + "1", huffman_codes);
}