// KO: 이 파일은 TriSplit 압축/복호화 프로그램의 메인 진입점(main function)입니다.
//     명령줄 인자를 파싱하여 압축 또는 복호화 모드를 결정하고,
//     파일을 블록 단위로 읽어와 SeparationEngine과 rANS_Coder를 사용하여 작업을 수행합니다.
// EN: This file is the main entry point for the TriSplit compression/decompression program.
//     It parses command-line arguments to determine the mode (compress or decompress),
//     and processes files block by block using the SeparationEngine and rANS_Coder.
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <numeric>
#include <stdexcept>

#include "rANS_Coder/rANS_Coder.h"
#include "SeparationEngine/SeparationEngine.h"

#include <cstdint>

// KO: 메모리 정렬(padding)을 비활성화하여 구조체를 파일에 쓰거나 읽을 때 크기가 그대로 유지되도록 합니다.
// EN: Disables memory alignment (padding) to ensure the struct's size remains consistent when writing to or reading from a file.
#pragma pack(push, 1)
// KO: 압축된 각 블록의 시작 부분에 위치하는 헤더 정보입니다.
//     복호화에 필요한 모든 메타데이터와 각 데이터 스트림의 크기를 담고 있습니다.
// EN: The header information located at the beginning of each compressed block.
//     It contains all the necessary metadata for decompression and the size of each data stream.
struct TriSplitBlockHeader {
    // KO: 비트 플래그 필드. 
    //     - 0번 비트: aux_mask_1_represents_11 (1이면 true)
    //     - 1번 비트: is_placeholder_common (1이면 true)
    //     - 2번 비트: 사용된 엔진 (항상 1, rANS 의미)
    // EN: A bitfield for flags.
    //     - Bit 0: aux_mask_1_represents_11 (1 if true)
    //     - Bit 1: is_placeholder_common (1 if true)
    //     - Bit 2: Engine used (always 1, means rANS)
    uint8_t  metadata_flags;
    uint8_t  reserved[7]; // KO: 예약된 공간. 향후 확장을 위함. / EN: Reserved space for future expansion.
    uint64_t original_data_size; // KO: 원본 블록 데이터의 크기 / EN: The size of the original block data.
    uint64_t compressed_bitmap_size; // KO: 압축된 value_bitmap 스트림의 크기 / EN: The size of the compressed value_bitmap stream.
    uint64_t compressed_mask_size; // KO: 압축된 auxiliary_mask 스트림의 크기 / EN: The size of the compressed auxiliary_mask stream.
    uint64_t compressed_reconstructed_size; // KO: 압축된 reconstructed_stream의 크기 / EN: The size of the compressed reconstructed_stream.
};
#pragma pack(pop)

void print_usage();
std::vector<uint8_t> compress_block(const std::vector<uint8_t>& block_data);
std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& compressed_block_data);

void print_usage() {
    std::cerr << "Usage: TriSplit.exe [mode] <input_file> <output_file>" << std::endl;
    std::cerr << "  mode:" << std::endl;
    std::cerr << "    -c : Compress" << std::endl;
    std::cerr << "    -d : Decompress" << std::endl;
}

// KO: 파일을 처리할 블록의 크기를 정의합니다. (8MB)
// EN: Defines the size of the blocks for file processing. (8MB)
constexpr size_t BLOCK_SIZE = 8 * 1024 * 1024;

int main(int argc, char* argv[]) {
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

    std::ifstream input_file(input_path, std::ios::binary);
    std::ofstream output_file(output_path, std::ios::binary);
    if (!input_file.is_open() || !output_file.is_open()) {
        std::cerr << "Error: Cannot open input or output file." << std::endl;
        return 1;
    }

    if (mode == "-c") {
        // --- 압축 모드 ---
        // --- Compression Mode ---
        std::cout << "Compression mode selected." << std::endl;
        std::vector<uint8_t> buffer(BLOCK_SIZE);
        while (input_file) {
            input_file.read(reinterpret_cast<char*>(buffer.data()), BLOCK_SIZE);
            size_t bytes_read = input_file.gcount();
            if (bytes_read == 0) break;
            buffer.resize(bytes_read);

            std::cout << "Processing block of " << bytes_read << " bytes..." << std::endl;
            std::vector<uint8_t> compressed_block = compress_block(buffer);

            // KO: 압축된 블록의 크기를 먼저 기록하고, 그 다음에 실제 블록 데이터를 기록합니다. (프레이밍)
            // EN: First write the size of the compressed block, and then write the actual block data. (Framing)
            uint64_t compressed_size = compressed_block.size();
            output_file.write(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
            if (compressed_size > 0) {
                output_file.write(reinterpret_cast<const char*>(compressed_block.data()), compressed_size);
            }
        }
        std::cout << "Compression finished." << std::endl;
    }
    else { // mode == "-d"
        // --- 복호화 모드 ---
        // --- Decompression Mode ---
        std::cout << "Decompression mode selected." << std::endl;
        uint64_t compressed_size;
        // KO: 블록 크기를 먼저 읽고, 해당 크기만큼 블록 데이터를 읽어 복호화를 진행합니다.
        // EN: Reads the block size first, then reads that much block data to proceed with decompression.
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

// KO: 단일 데이터 블록을 압축하는 전체 과정을 수행합니다.
// EN: Performs the entire process of compressing a single data block.
std::vector<uint8_t> compress_block(const std::vector<uint8_t>& block_data) {
    // --- 1단계: 스트림 분리 ---
    // --- Step 1: Separate Streams ---
    std::cout << "  [1/3] Separating streams..." << std::endl;
    SeparationEngine separation_engine;
    SeparatedStreams streams = separation_engine.separate(block_data);

    // --- 2단계: 각 스트림 압축 ---
    // --- Step 2: Compress Each Stream ---
    std::cout << "  [2/3] Compressing Value Bitmap & Auxiliary Mask streams..." << std::endl;
    rANS_Coder byte_coder;
    std::vector<uint8_t> compressed_bitmap = byte_coder.encode(streams.value_bitmap);
    std::vector<uint8_t> compressed_mask = byte_coder.encode(streams.auxiliary_mask);

    std::cout << "  [2/3] Compressing Reconstructed stream with rANS engine..." << std::endl;
    size_t n_placeholders = std::count(streams.reconstructed_stream.begin(), streams.reconstructed_stream.end(), 1);
    bool is_placeholder_common = (n_placeholders >= streams.reconstructed_stream.size() / 2);
    std::vector<uint8_t> compressed_reconstructed = byte_coder.encode_reconstructed_stream(streams.reconstructed_stream, is_placeholder_common);
    std::cout << "    - Done. Reconstructed stream compressed size: " << compressed_reconstructed.size() << " bytes." << std::endl;

    // --- 3단계: 최종 블록 조립 ---
    // --- Step 3: Assemble Final Block ---
    std::cout << "  [3/3] Assembling final block..." << std::endl;
    TriSplitBlockHeader header;
    memset(&header, 0, sizeof(header));
    header.original_data_size = block_data.size();

    // KO: 복호화에 필요한 플래그들을 'metadata_flags' 비트 필드에 설정합니다.
    // EN: Set the flags required for decompression in the 'metadata_flags' bitfield.
    header.metadata_flags = 0;
    if (streams.aux_mask_1_represents_11) header.metadata_flags |= (1 << 0);
    if (is_placeholder_common)            header.metadata_flags |= (1 << 1);
    header.metadata_flags |= (1 << 2); // rANS engine used

    header.compressed_bitmap_size = compressed_bitmap.size();
    header.compressed_mask_size = compressed_mask.size();
    header.compressed_reconstructed_size = compressed_reconstructed.size();

    // KO: 최종 블록을 위한 메모리를 할당하고 헤더와 압축된 데이터들을 순서대로 복사합니다.
    // EN: Allocate memory for the final block and copy the header and compressed data streams in order.
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

// KO: 단일 압축 블록을 복호화하는 전체 과정을 수행합니다.
// EN: Performs the entire process of decompressing a single compressed block.
std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& compressed_block_data) {
    // --- 1단계: 블록 헤더 파싱 ---
    // --- Step 1: Parse Block Header ---
    std::cout << "  [1/4] Parsing block header..." << std::endl;
    if (compressed_block_data.size() < sizeof(TriSplitBlockHeader)) {
        std::cerr << "Error: Compressed data is smaller than header size." << std::endl;
        return {};
    }

    TriSplitBlockHeader header;
    memcpy(&header, compressed_block_data.data(), sizeof(header));

    const uint8_t* read_ptr = compressed_block_data.data() + sizeof(header);
    const uint8_t* data_end = compressed_block_data.data() + compressed_block_data.size();

    // KO: 헤더에 기록된 크기 정보가 실제 데이터 크기와 맞는지 검증하여 데이터 손상을 확인합니다.
    // EN: Validates if the size information in the header matches the actual data size to check for corruption.
    if (read_ptr + header.compressed_bitmap_size > data_end ||
        read_ptr + header.compressed_bitmap_size + header.compressed_mask_size > data_end ||
        read_ptr + header.compressed_bitmap_size + header.compressed_mask_size + header.compressed_reconstructed_size > data_end) {
        std::cerr << "Error: Corrupted block header, size mismatch." << std::endl;
        return {};
    }

    // KO: 헤더 정보를 바탕으로 각 압축 스트림을 별도의 벡터로 분리합니다.
    // EN: Separates each compressed stream into its own vector based on the header information.
    std::vector<uint8_t> compressed_bitmap(read_ptr, read_ptr + header.compressed_bitmap_size);
    read_ptr += header.compressed_bitmap_size;
    std::vector<uint8_t> compressed_mask(read_ptr, read_ptr + header.compressed_mask_size);
    read_ptr += header.compressed_mask_size;
    std::vector<uint8_t> compressed_reconstructed_data(read_ptr, read_ptr + header.compressed_reconstructed_size);

    // --- 2단계: 각 스트림 복호화 ---
    // --- Step 2: Decompress Each Stream ---
    std::cout << "  [2/4] Decompressing Value Bitmap & Auxiliary Mask streams..." << std::endl;
    rANS_Coder byte_coder;
    std::vector<uint8_t> value_bitmap = byte_coder.decode(compressed_bitmap);
    std::vector<uint8_t> auxiliary_mask = byte_coder.decode(compressed_mask);

    std::cout << "  [3/4] Decompressing Reconstructed stream with rANS engine..." << std::endl;
    // KO: 헤더의 메타데이터 플래그를 읽어 복호화 함수에 전달합니다.
    // EN: Reads the metadata flags from the header and passes them to the decompression function.
    bool is_placeholder_common = (header.metadata_flags & (1 << 1));
    std::vector<uint8_t> reconstructed_stream = byte_coder.decode_reconstructed_stream(compressed_reconstructed_data, is_placeholder_common);

    // --- 3단계: 최종 데이터 재조립 ---
    // --- Step 3: Reconstruct Final Data ---
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