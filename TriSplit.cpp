#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <numeric>
#include <stdexcept>

#include "rANS_Coder.h"
#include "SeparationEngine.h"
// #include "BwtEngine/BwtEngine.h" // BwtEngine 헤더 포함 제거
// #include "BwtEngine/EntropyCoder/EntropyCoder.h" // BwtEngine 헤더 포함 제거

#include <cstdint>

#pragma pack(push, 1)
struct TriSplitBlockHeader {
    uint8_t  metadata_flags;
    uint8_t  reserved[7];
    uint64_t original_data_size;
    uint64_t compressed_bitmap_size;
    uint64_t compressed_mask_size;
    uint64_t compressed_reconstructed_size;
};
#pragma pack(pop)

// BWT 엔진 관련 enum과 함수 선언 제거
// enum class EngineType { BWT, RANS }; -> 제거
void print_usage();
std::vector<uint8_t> compress_block(const std::vector<uint8_t>& block_data); // 엔진 타입 인자 제거
std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& compressed_block_data);

void print_usage() {
    // 사용법 안내 메시지에서 엔진 선택 부분 제거
    std::cerr << "Usage: TriSplit.exe [mode] <input_file> <output_file>" << std::endl;
    std::cerr << "  mode:" << std::endl;
    std::cerr << "    -c : Compress" << std::endl;
    std::cerr << "    -d : Decompress" << std::endl;
}

constexpr size_t BLOCK_SIZE = 8 * 1024 * 1024;

int main(int argc, char* argv[]) {
    // 인자 개수 4개로 변경 (엔진 선택 -e bwt/rans 부분 제거)
    if (argc != 4) {
        print_usage();
        return 1;
    }
    const std::string mode = argv[1];
    const std::filesystem::path input_path = argv[2];
    const std::filesystem::path output_path = argv[3];

    if (mode != "-c" && mode != "-d") {
        std::cerr << "Error: Invalid mode '" << mode << "'" << std::endl;
        print_usage(); return 1;
    }

    // 엔진 파싱 로직 전체 제거
    // ...

    std::ifstream input_file(input_path, std::ios::binary);
    std::ofstream output_file(output_path, std::ios::binary);
    if (!input_file.is_open() || !output_file.is_open()) {
        std::cerr << "Error: Cannot open input or output file." << std::endl;
        return 1;
    }

    if (mode == "-c") {
        // 압축 시 엔진 선택 로그 제거
        std::cout << "Compression mode selected." << std::endl;
        std::vector<uint8_t> buffer(BLOCK_SIZE);
        while (input_file) {
            input_file.read(reinterpret_cast<char*>(buffer.data()), BLOCK_SIZE);
            size_t bytes_read = input_file.gcount();
            if (bytes_read == 0) break;
            buffer.resize(bytes_read);

            std::cout << "Processing block of " << bytes_read << " bytes..." << std::endl;
            // 압축 함수 호출 시 엔진 인자 제거
            std::vector<uint8_t> compressed_block = compress_block(buffer);

            uint64_t compressed_size = compressed_block.size();
            output_file.write(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
            if (compressed_size > 0) {
                output_file.write(reinterpret_cast<const char*>(compressed_block.data()), compressed_size);
            }
        }
        std::cout << "Compression finished." << std::endl;
    }
    else {
        std::cout << "Decompression mode selected." << std::endl;
        uint64_t compressed_size;
        while (output_file && input_file.read(reinterpret_cast<char*>(&compressed_size), sizeof(compressed_size))) {
            if (compressed_size == 0) continue;
            std::vector<uint8_t> compressed_buffer(compressed_size);
            input_file.read(reinterpret_cast<char*>(compressed_buffer.data()), compressed_size);

            std::cout << "Decompressing block of " << compressed_size << " bytes..." << std::endl;
            std::vector<uint8_t> decompressed_block = decompress_block(compressed_buffer);

            if (!decompressed_block.empty()) {
                output_file.write(reinterpret_cast<const char*>(decompressed_block.data()), decompressed_block.size());
            }
        }
        std::cout << "Decompression finished." << std::endl;
    }

    input_file.close();
    output_file.close();
    return 0;
}

// 압축 함수에서 엔진 선택 로직 제거
std::vector<uint8_t> compress_block(const std::vector<uint8_t>& block_data) {
    std::cout << "  [1/3] Separating streams..." << std::endl;
    SeparationEngine separation_engine;
    SeparatedStreams streams = separation_engine.separate(block_data);

    std::cout << "  [2/3] Compressing Value Bitmap & Auxiliary Mask streams..." << std::endl;
    rANS_Coder byte_coder;
    std::vector<uint8_t> compressed_bitmap = byte_coder.encode(streams.value_bitmap);
    std::vector<uint8_t> compressed_mask = byte_coder.encode(streams.auxiliary_mask);

    // Reconstructed Stream은 항상 rANS로 압축
    std::cout << "  [2/3] Compressing Reconstructed stream with rANS engine..." << std::endl;
    size_t n_placeholders = std::count(streams.reconstructed_stream.begin(), streams.reconstructed_stream.end(), 1);
    bool is_placeholder_common = (n_placeholders >= streams.reconstructed_stream.size() / 2);
    std::vector<uint8_t> compressed_reconstructed = byte_coder.encode_reconstructed_stream(streams.reconstructed_stream, is_placeholder_common);
    std::cout << "    - Done. Reconstructed stream compressed size: " << compressed_reconstructed.size() << " bytes." << std::endl;

    std::cout << "  [3/3] Assembling final block..." << std::endl;
    TriSplitBlockHeader header;
    memset(&header, 0, sizeof(header));
    header.original_data_size = block_data.size();

    // metadata_flags에서 엔진 정보 비트(2번 비트)는 항상 1(rANS)로 설정
    header.metadata_flags = 0;
    if (streams.aux_mask_1_represents_11) header.metadata_flags |= (1 << 0);
    if (is_placeholder_common)            header.metadata_flags |= (1 << 1);
    header.metadata_flags |= (1 << 2); // rANS engine used

    header.compressed_bitmap_size = compressed_bitmap.size();
    header.compressed_mask_size = compressed_mask.size();
    header.compressed_reconstructed_size = compressed_reconstructed.size();

    size_t total_size = sizeof(header) + header.compressed_bitmap_size + header.compressed_mask_size + header.compressed_reconstructed_size;
    std::vector<uint8_t> final_block(total_size);

    uint8_t* write_ptr = final_block.data();
    memcpy(write_ptr, &header, sizeof(header));
    write_ptr += sizeof(header);

    if (header.compressed_bitmap_size > 0) { memcpy(write_ptr, compressed_bitmap.data(), header.compressed_bitmap_size); write_ptr += header.compressed_bitmap_size; }
    if (header.compressed_mask_size > 0) { memcpy(write_ptr, compressed_mask.data(), header.compressed_mask_size); write_ptr += header.compressed_mask_size; }
    if (header.compressed_reconstructed_size > 0) { memcpy(write_ptr, compressed_reconstructed.data(), header.compressed_reconstructed_size); }

    std::cout << "    - Done. Final block size: " << final_block.size() << " bytes." << std::endl;
    return final_block;
}

// 복호화 함수에서 엔진 선택 로직 제거
std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& compressed_block_data) {
    std::cout << "  [1/4] Parsing block header..." << std::endl;
    if (compressed_block_data.size() < sizeof(TriSplitBlockHeader)) {
        std::cerr << "Error: Compressed data is smaller than header size." << std::endl;
        return {};
    }

    TriSplitBlockHeader header;
    memcpy(&header, compressed_block_data.data(), sizeof(header));

    const uint8_t* read_ptr = compressed_block_data.data() + sizeof(header);
    const uint8_t* data_end = compressed_block_data.data() + compressed_block_data.size();

    if (read_ptr + header.compressed_bitmap_size > data_end ||
        read_ptr + header.compressed_bitmap_size + header.compressed_mask_size > data_end ||
        read_ptr + header.compressed_bitmap_size + header.compressed_mask_size + header.compressed_reconstructed_size > data_end) {
        std::cerr << "Error: Corrupted block header, size mismatch." << std::endl;
        return {};
    }

    std::vector<uint8_t> compressed_bitmap(read_ptr, read_ptr + header.compressed_bitmap_size);
    read_ptr += header.compressed_bitmap_size;
    std::vector<uint8_t> compressed_mask(read_ptr, read_ptr + header.compressed_mask_size);
    read_ptr += header.compressed_mask_size;
    std::vector<uint8_t> compressed_reconstructed_data(read_ptr, read_ptr + header.compressed_reconstructed_size);

    std::cout << "  [2/4] Decompressing Value Bitmap & Auxiliary Mask streams..." << std::endl;
    rANS_Coder byte_coder;
    std::vector<uint8_t> value_bitmap = byte_coder.decode(compressed_bitmap);
    std::vector<uint8_t> auxiliary_mask = byte_coder.decode(compressed_mask);

    // Reconstructed Stream은 항상 rANS로 복호화
    std::cout << "  [3/4] Decompressing Reconstructed stream with rANS engine..." << std::endl;
    bool is_placeholder_common = (header.metadata_flags & (1 << 1));
    std::vector<uint8_t> reconstructed_stream = byte_coder.decode_reconstructed_stream(compressed_reconstructed_data, is_placeholder_common);

    std::cout << "  [4/4] Reconstructing final data..." << std::endl;
    SeparationEngine separation_engine;
    bool aux_mask_1_represents_11 = (header.metadata_flags & (1 << 0));

    std::vector<uint8_t> original_block = separation_engine.reconstruct(
        value_bitmap,
        auxiliary_mask,
        reconstructed_stream,
        aux_mask_1_represents_11,
        header.original_data_size
    );

    std::cout << "    - Done. Decompressed block size: " << original_block.size() << " bytes." << std::endl;
    return original_block;
}