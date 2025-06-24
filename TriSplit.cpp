#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <filesystem>

#include "rANS_Coder.h"
#include "SeparationEngine.h"
#include "BwtEngine/BwtEngine.h"
#include "BwtEngine/EntropyCoder/EntropyCoder.h"

// 프로그램 사용법 출력 함수
void print_usage() {
    std::cerr << "Usage: TriSplit.exe [mode] <input_file> <output_file>" << std::endl;
    std::cerr << "  mode:" << std::endl;
    std::cerr << "    -c : Compress" << std::endl;
    std::cerr << "    -d : Decompress" << std::endl;
}

// --- 함수 선언 ---
void print_usage();
std::vector<uint8_t> compress_block(const std::vector<uint8_t>& block_data);
std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& compressed_block_data);

// --- 상수 정의 ---
constexpr size_t BLOCK_SIZE = 8 * 1024 * 1024; // 처리할 블록 크기 (8MB)


int main(int argc, char* argv[]) {
    // 1. 인자 파싱 및 파일 유효성 검사
    if (argc != 4) {
        print_usage();
        return 1;
    }
    const std::string mode = argv[1];
    const std::filesystem::path input_path = argv[2];
    const std::filesystem::path output_path = argv[3];

    if (mode != "-c" && mode != "-d") {
        std::cerr << "Error: Invalid mode '" << mode << "'" << std::endl;
        print_usage();
        return 1;
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
        std::cout << "Compression mode selected." << std::endl;

        // TODO: 파일 시그니처, 버전 등 전체 파일을 위한 헤더를 쓸 수 있습니다.
        // output_file.write("C3C_", 4);

        std::vector<uint8_t> buffer(BLOCK_SIZE);
        while (input_file) {
            input_file.read(reinterpret_cast<char*>(buffer.data()), BLOCK_SIZE);
            size_t bytes_read = input_file.gcount();
            if (bytes_read == 0) break;

            buffer.resize(bytes_read);

            std::cout << "Processing block of " << bytes_read << " bytes..." << std::endl;

            // 한 블록을 압축합니다.
            std::vector<uint8_t> compressed_block = compress_block(buffer);

            // [블록 크기(8바이트)] [압축된 블록 데이터] 순서로 파일에 씁니다.
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

            // 한 블록을 복호화합니다.
            std::vector<uint8_t> decompressed_block = decompress_block(compressed_buffer);

            // 복원된 데이터를 파일에 씁니다.
            output_file.write(reinterpret_cast<const char*>(decompressed_block.data()), decompressed_block.size());
        }
        std::cout << "Decompression finished." << std::endl;
    }

    input_file.close();
    output_file.close();
    return 0;
}

// --- 압축 로직 함수 ---
std::vector<uint8_t> compress_block(const std::vector<uint8_t>& block_data) {
    // 1. 스트림 분리
    SeparationEngine separation_engine;
    SeparatedStreams streams = separation_engine.separate(block_data);

    // 2. rANS로 2개의 스트림 압축
    rANS_Coder coder;
    std::vector<uint8_t> compressed_bitmap = coder.encode(streams.value_bitmap);
    std::vector<uint8_t> compressed_mask = coder.encode(streams.auxiliary_mask);

    // 3. BwtEngine으로 1개의 스트림 처리
    BwtEngine engine_a;
    auto& recon_stream_u8 = streams.reconstructed_stream;
    std::vector<uint16_t> recon_stream_u16(recon_stream_u8.begin(), recon_stream_u8.end());
    std::vector<uint16_t> engine_a_result_u16 = engine_a.process_stream(recon_stream_u16);

    // ★★★ 수정된 부분 ★★★
    // 4. BwtEngine의 결과를 최종 엔트로피 코더로 압축합니다.
    EntropyCoder final_coder; // 허프만 코더 객체 생성
    std::vector<uint8_t> compressed_reconstructed = final_coder.huffman_encode(engine_a_result_u16);

    std::cout << "Engine A result entropy-coded: "
        << engine_a_result_u16.size() * 2 << " bytes -> "
        << compressed_reconstructed.size() << " bytes" << std::endl;

    // 5. 압축된 블록을 하나의 데이터로 조합 (파일 포맷에 맞춰)
    size_t header_size = 1 + 3 + 8 + 8 + 8; // 메타데이터(1), 예비(3), 각 청크 크기(8*3) = 28바이트
    size_t total_size = header_size + compressed_bitmap.size() + compressed_mask.size() + compressed_reconstructed.size();

    std::vector<uint8_t> final_block(total_size);
    char* write_ptr = reinterpret_cast<char*>(final_block.data());

    // 블록 헤더 쓰기
    uint8_t metadata_flag = streams.aux_mask_1_represents_11 ? 1 : 0;
    *write_ptr = metadata_flag; write_ptr += 4; // 예비 공간 포함

    uint64_t chunk1_size = compressed_bitmap.size();
    uint64_t chunk2_size = compressed_mask.size();
    uint64_t chunk3_size = compressed_reconstructed.size();
    memcpy(write_ptr, &chunk1_size, 8); write_ptr += 8;
    memcpy(write_ptr, &chunk2_size, 8); write_ptr += 8;
    memcpy(write_ptr, &chunk3_size, 8); write_ptr += 8;

    // 데이터 청크 쓰기
    if (chunk1_size > 0) { memcpy(write_ptr, compressed_bitmap.data(), chunk1_size); write_ptr += chunk1_size; }
    if (chunk2_size > 0) { memcpy(write_ptr, compressed_mask.data(), chunk2_size); write_ptr += chunk2_size; }
    if (chunk3_size > 0) { memcpy(write_ptr, compressed_reconstructed.data(), chunk3_size); }

    return final_block;
}

// --- 복호화 로직 함수 ---
std::vector<uint8_t> decompress_block(const std::vector<uint8_t>& compressed_block_data) {
    // TODO: 복호화 로직 구현 필요
    // 1. 블록 헤더 파싱 (메타데이터, 각 청크 크기)
    // 2. 각 청크를 버퍼에서 분리
    // 3. RansCoder와 BwtEngine의 역함수로 각 청크 복원
    // 4. 복원된 3개의 스트림을 조합하여 원본 블록 데이터 생성
    std::cout << " (Decompression not implemented yet)" << std::endl;
    return {};
}