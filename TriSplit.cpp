#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <numeric>
#include <stdexcept>

#include "rANS_Coder.h"
#include "SeparationEngine.h"
#include "BwtEngine/BwtEngine.h"
#include "BwtEngine/EntropyCoder/EntropyCoder.h"

#include <cstdint>

// 블록의 메타데이터를 나타내는 구조체
// 파일에 한 번에 쓰고 읽어 관리의 편의성과 안정성을 높입니다.
#pragma pack(push, 1) // 1바이트 정렬
struct TriSplitBlockHeader {
    uint8_t  metadata_flags;
    uint8_t  reserved[7]; // 8바이트 정렬을 위한 패딩
    uint64_t compressed_bitmap_size;
    uint64_t compressed_mask_size;
    uint64_t compressed_reconstructed_size;
};
#pragma pack(pop)


// --- Enum for Engine Choice ---
enum class EngineType { BWT, RANS };

// --- 함수 선언 ---
void print_usage();
std::vector<uint8_t> compress_block(const std::vector<uint8_t>& block_data, EngineType engine_type);
std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& compressed_block_data);

// 프로그램 사용법 출력 함수
void print_usage() {
    std::cerr << "Usage: TriSplit.exe [mode] -e [engine] <input_file> <output_file>" << std::endl;
    std::cerr << "  mode:" << std::endl;
    std::cerr << "    -c : Compress" << std::endl;
    std::cerr << "    -d : Decompress" << std::endl;
    std::cerr << "  engine (for reconstructed stream):" << std::endl;
    std::cerr << "    bwt  : BWT -> MTF -> RLE -> Huffman pipeline" << std::endl;
    std::cerr << "    rans : On-the-fly bit-remapping rANS coder" << std::endl;
}

// --- 상수 정의 ---
constexpr size_t BLOCK_SIZE = 8 * 1024 * 1024; // 처리할 블록 크기 (8MB)

int main(int argc, char* argv[]) {
    // 1. 인자 파싱 및 파일 유효성 검사 (기존과 동일)
    if (argc != 6) {
        print_usage();
        return 1;
    }
    const std::string mode = argv[1];
    const std::string engine_arg = argv[3];
    const std::filesystem::path input_path = argv[4];
    const std::filesystem::path output_path = argv[5];

    if (mode != "-c" && mode != "-d") {
        std::cerr << "Error: Invalid mode '" << mode << "'" << std::endl;
        print_usage(); return 1;
    }
    if (argv[2] != std::string("-e")) {
        print_usage(); return 1;
    }

    EngineType selected_engine;
    if (engine_arg == "bwt") {
        selected_engine = EngineType::BWT;
    }
    else if (engine_arg == "rans") {
        selected_engine = EngineType::RANS;
    }
    else {
        std::cerr << "Error: Invalid engine type '" << engine_arg << "'" << std::endl;
        print_usage(); return 1;
    }

    if (!std::filesystem::exists(input_path)) {
        std::cerr << "Error: Input file not found -> " << input_path << std::endl;
        return 1;
    }

    // 2. 파일 스트림 열기 (기존과 동일)
    std::ifstream input_file(input_path, std::ios::binary);
    std::ofstream output_file(output_path, std::ios::binary);
    if (!input_file.is_open() || !output_file.is_open()) {
        std::cerr << "Error: Cannot open input or output file." << std::endl;
        return 1;
    }

    // 3. 모드에 따라 압축 또는 복호화 실행
    if (mode == "-c") {
        std::cout << "Compression mode selected. Engine: " << engine_arg << std::endl;
        std::vector<uint8_t> buffer(BLOCK_SIZE);
        while (input_file) {
            input_file.read(reinterpret_cast<char*>(buffer.data()), BLOCK_SIZE);
            size_t bytes_read = input_file.gcount();
            if (bytes_read == 0) break;
            buffer.resize(bytes_read);

            std::cout << "Processing block of " << bytes_read << " bytes..." << std::endl;
            std::vector<uint8_t> compressed_block = compress_block(buffer, selected_engine);

            // [수정] 블록의 크기를 먼저 쓰고 데이터를 쓰는 방식으로 변경
            uint64_t compressed_size = compressed_block.size();
            output_file.write(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
            if (compressed_size > 0) {
                output_file.write(reinterpret_cast<const char*>(compressed_block.data()), compressed_size);
            }
        }
        std::cout << "Compression finished." << std::endl;

    }
    else { // mode == "-d"
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

// --- 압축 로직 함수 (오류 수정 및 로직 개선) ---
std::vector<uint8_t> compress_block(const std::vector<uint8_t>& block_data, EngineType engine_type) {
    // [Phase 1] 스트림 분리
    std::cout << "  [1/4] Separating streams..." << std::endl;
    SeparationEngine separation_engine;
    SeparatedStreams streams = separation_engine.separate(block_data);

    // [Phase 2] 스트림 1, 2 압축
    std::cout << "  [2/4] Compressing Value Bitmap & Auxiliary Mask streams..." << std::endl;
    rANS_Coder byte_coder;
    std::vector<uint8_t> compressed_bitmap = byte_coder.encode(streams.value_bitmap);
    std::vector<uint8_t> compressed_mask = byte_coder.encode(streams.auxiliary_mask);

    // [Phase 3] Reconstructed Stream 압축 (엔진 선택)
    std::cout << "  [3/4] Compressing Reconstructed stream with " << (engine_type == EngineType::BWT ? "BWT" : "RANS") << " engine..." << std::endl;
    std::vector<uint8_t> compressed_reconstructed;

    bool is_placeholder_common = false;

    if (engine_type == EngineType::BWT) {
        BwtEngine engine_a;
        std::vector<uint16_t> recon_stream_u16(streams.reconstructed_stream.begin(), streams.reconstructed_stream.end());

        // [오류 수정] BwtEngine::process_stream은 RleResult를 반환합니다.
        RleResult engine_a_result = engine_a.process_stream(recon_stream_u16);

        EntropyCoder final_coder;
        // [오류 수정] huffman_encode는 RleResult를 인자로 받습니다.
        compressed_reconstructed = final_coder.huffman_encode(engine_a_result);
        std::cout << "    - Done. Reconstructed stream compressed size: " << compressed_reconstructed.size() << " bytes." << std::endl;
    }
    else { // engine_type == EngineType::RANS
        size_t n_placeholders = std::count(streams.reconstructed_stream.begin(), streams.reconstructed_stream.end(), 1);
        is_placeholder_common = (n_placeholders >= streams.reconstructed_stream.size() / 2);
        compressed_reconstructed = byte_coder.encode_reconstructed_stream(streams.reconstructed_stream, is_placeholder_common);
        std::cout << "    - Done. Reconstructed stream compressed size: " << compressed_reconstructed.size() << " bytes." << std::endl;
    }

    // [Phase 4] 최종 블록 조합 (헤더 구조체 사용)
    std::cout << "  [4/4] Assembling final block..." << std::endl;

    TriSplitBlockHeader header;
    memset(&header, 0, sizeof(header)); // 구조체 초기화

    header.metadata_flags = 0;
    if (streams.aux_mask_1_represents_11) header.metadata_flags |= (1 << 0);
    if (is_placeholder_common)            header.metadata_flags |= (1 << 1);
    if (engine_type == EngineType::RANS)  header.metadata_flags |= (1 << 2);

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

// --- 복호화 로직 함수 (BWT 역변환 포함 최종 버전) ---
std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& compressed_block_data) {
    // 1. 블록 헤더 파싱 (안전한 방식으로)
    std::cout << "  [1/4] Parsing block header..." << std::endl;
    if (compressed_block_data.size() < sizeof(TriSplitBlockHeader)) {
        std::cerr << "Error: Compressed data is smaller than header size." << std::endl;
        return {};
    }

    TriSplitBlockHeader header;
    memcpy(&header, compressed_block_data.data(), sizeof(header));

    const uint8_t* read_ptr = compressed_block_data.data() + sizeof(header);
    const uint8_t* data_end = compressed_block_data.data() + compressed_block_data.size();

    // 2. 압축된 청크 데이터 분리 (경계 검사 추가)
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

    // 3. 각 스트림 복원
    std::cout << "  [2/4] Decompressing Value Bitmap & Auxiliary Mask streams..." << std::endl;
    rANS_Coder byte_coder;
    std::vector<uint8_t> value_bitmap = byte_coder.decode(compressed_bitmap);
    std::vector<uint8_t> auxiliary_mask = byte_coder.decode(compressed_mask);

    std::vector<uint8_t> reconstructed_stream;

    bool used_rans_engine = (header.metadata_flags & (1 << 2));

    std::cout << "  [3/4] Decompressing Reconstructed stream with " << (used_rans_engine ? "RANS" : "BWT") << " engine..." << std::endl;
    if (used_rans_engine) {
        bool is_placeholder_common = (header.metadata_flags & (1 << 1));
        reconstructed_stream = byte_coder.decode_reconstructed_stream(compressed_reconstructed_data, is_placeholder_common);
    }
    else {
        // BWT 엔진으로 압축된 경우, 역변환 파이프라인을 호출합니다.
        BwtEngine engine_b;
        std::vector<uint16_t> recon_stream_u16 = engine_b.inverse_process_stream(compressed_reconstructed_data);
        // [수정된 부분] 데이터 손실 경고를 해결하기 위해 안전한 방식으로 변환
        reconstructed_stream.clear();
        reconstructed_stream.reserve(recon_stream_u16.size());
        for (uint16_t val : recon_stream_u16) {
            reconstructed_stream.push_back(static_cast<uint8_t>(val));
        }
    }

    // 4. 최종 데이터 조합
    std::cout << "  [4/4] Reconstructing final data..." << std::endl;
    SeparationEngine separation_engine;
    bool aux_mask_1_represents_11 = (header.metadata_flags & (1 << 0));

    std::vector<uint8_t> original_block = separation_engine.reconstruct(
        value_bitmap,
        auxiliary_mask,
        reconstructed_stream,
        aux_mask_1_represents_11
    );

    std::cout << "    - Done. Decompressed block size: " << original_block.size() << " bytes." << std::endl;
    return original_block;
}