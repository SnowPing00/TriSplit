#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>
#include <numeric> // std::count를 위해 추가

#include "rANS_Coder.h"
#include "SeparationEngine.h"
#include "BwtEngine/BwtEngine.h"
#include "BwtEngine/EntropyCoder/EntropyCoder.h"

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
    // 1. 인자 파싱 및 파일 유효성 검사
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

    // 2. 파일 스트림 열기
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

            uint64_t compressed_size = compressed_block.size();
            output_file.write(reinterpret_cast<const char*>(&compressed_size), sizeof(compressed_size));
            output_file.write(reinterpret_cast<const char*>(compressed_block.data()), compressed_size);
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

            output_file.write(reinterpret_cast<const char*>(decompressed_block.data()), decompressed_block.size());
        }
        std::cout << "Decompression finished." << std::endl;
    }

    input_file.close();
    output_file.close();
    return 0;
}

// --- 압축 로직 함수 (수정된 버전) ---
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

    // ======================= [오류 수정 지점] =======================
    // is_placeholder_common 변수를 if 블록 이전에 선언하고 초기화합니다.
    // 이렇게 하면 if-else 블록이 끝난 후에도 변수에 접근할 수 있습니다.
    bool is_placeholder_common = false;

    if (engine_type == EngineType::BWT) {
        BwtEngine engine_a;
        std::vector<uint16_t> recon_stream_u16(streams.reconstructed_stream.begin(), streams.reconstructed_stream.end());
        std::vector<uint16_t> engine_a_result_u16 = engine_a.process_stream(recon_stream_u16);
        EntropyCoder final_coder;
        compressed_reconstructed = final_coder.huffman_encode(engine_a_result_u16);
        std::cout << "    - Done. " << recon_stream_u16.size() * 2 << " bytes -> " << compressed_reconstructed.size() << " bytes." << std::endl;
    }
    else { // engine_type == EngineType::RANS
        // RANS 엔진을 사용할 때만 is_placeholder_common 값을 계산하여 설정합니다.
        size_t n_placeholders = std::count(streams.reconstructed_stream.begin(), streams.reconstructed_stream.end(), 1);
        is_placeholder_common = (n_placeholders >= streams.reconstructed_stream.size() / 2);
        compressed_reconstructed = byte_coder.encode_reconstructed_stream(streams.reconstructed_stream, is_placeholder_common);

        std::cout << "    - Done. " << streams.reconstructed_stream.size() << " symbols -> " << compressed_reconstructed.size() << " bytes." << std::endl;
    }
    // ===============================================================

    // [Phase 4] 최종 블록 조합
    std::cout << "  [4/4] Assembling final block..." << std::endl;
    uint8_t metadata_flag = 0;
    if (streams.aux_mask_1_represents_11) metadata_flag |= (1 << 0);
    if (is_placeholder_common)            metadata_flag |= (1 << 1); // 이제 오류 없이 접근 가능
    if (engine_type == EngineType::RANS)  metadata_flag |= (1 << 2);

    size_t header_size = 1 + 8 + 8 + 8;
    size_t total_size = header_size + compressed_bitmap.size() + compressed_mask.size() + compressed_reconstructed.size();
    std::vector<uint8_t> final_block(total_size);
    char* write_ptr = reinterpret_cast<char*>(final_block.data());

    *reinterpret_cast<uint8_t*>(write_ptr) = metadata_flag; write_ptr += 1;
    uint64_t chunk1_size = compressed_bitmap.size();
    uint64_t chunk2_size = compressed_mask.size();
    uint64_t chunk3_size = compressed_reconstructed.size();
    memcpy(write_ptr, &chunk1_size, 8); write_ptr += 8;
    memcpy(write_ptr, &chunk2_size, 8); write_ptr += 8;
    memcpy(write_ptr, &chunk3_size, 8); write_ptr += 8;

    if (chunk1_size > 0) { memcpy(write_ptr, compressed_bitmap.data(), chunk1_size); write_ptr += chunk1_size; }
    if (chunk2_size > 0) { memcpy(write_ptr, compressed_mask.data(), chunk2_size); write_ptr += chunk2_size; }
    if (chunk3_size > 0) { memcpy(write_ptr, compressed_reconstructed.data(), chunk3_size); }

    std::cout << "    - Done. Final block size: " << final_block.size() << " bytes." << std::endl;
    return final_block;
}

// --- 복호화 로직 함수 (구현 완료) ---
std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& compressed_block_data) {
    // 1. 블록 헤더 파싱
    std::cout << "  [1/4] Parsing block header..." << std::endl;
    const char* read_ptr = reinterpret_cast<const char*>(compressed_block_data.data());

    uint8_t metadata_flag = *reinterpret_cast<const uint8_t*>(read_ptr);
    read_ptr += 1;

    uint64_t chunk1_size, chunk2_size, chunk3_size;
    memcpy(&chunk1_size, read_ptr, 8); read_ptr += 8;
    memcpy(&chunk2_size, read_ptr, 8); read_ptr += 8;
    memcpy(&chunk3_size, read_ptr, 8); read_ptr += 8;

    // 2. 압축된 청크 데이터 분리
    std::vector<uint8_t> compressed_bitmap(read_ptr, read_ptr + chunk1_size);
    read_ptr += chunk1_size;
    std::vector<uint8_t> compressed_mask(read_ptr, read_ptr + chunk2_size);
    read_ptr += chunk2_size;
    std::vector<uint8_t> compressed_reconstructed_data(read_ptr, read_ptr + chunk3_size);

    // 3. 각 스트림 복원
    std::cout << "  [2/4] Decompressing Value Bitmap & Auxiliary Mask streams..." << std::endl;
    rANS_Coder byte_coder;
    std::vector<uint8_t> value_bitmap = byte_coder.decode(compressed_bitmap);
    std::vector<uint8_t> auxiliary_mask = byte_coder.decode(compressed_mask);

    std::vector<uint8_t> reconstructed_stream;
    bool used_rans_engine = (metadata_flag & (1 << 2));

    std::cout << "  [3/4] Decompressing Reconstructed stream with " << (used_rans_engine ? "RANS" : "BWT") << " engine..." << std::endl;
    if (used_rans_engine) {
        // 메타데이터 플래그에서 is_placeholder_common 값을 직접 읽어옵니다.
        bool is_placeholder_common = (metadata_flag & (1 << 1));
        reconstructed_stream = byte_coder.decode_reconstructed_stream(compressed_reconstructed_data, is_placeholder_common);
    }
    else {
        // BWT 엔진으로 압축된 경우
        // TODO: BwtEngine 역변환 로직 구현 필요
        std::cout << "    - BWT decompression is not implemented yet." << std::endl;
    }

    // 4. 최종 데이터 조합
    std::cout << "  [4/4] Reconstructing final data..." << std::endl;
    SeparationEngine separation_engine;
    bool aux_mask_1_represents_11 = (metadata_flag & (1 << 0));

    std::vector<uint8_t> original_block = separation_engine.reconstruct(
        value_bitmap,
        auxiliary_mask,
        reconstructed_stream,
        aux_mask_1_represents_11
    );

    return original_block;
}