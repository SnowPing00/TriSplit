#pragma once
#include "BwtTransform/BwtTransform.h"
#include "MtfTransform/MtfTransform.h"
#include "RleTransform/RleTransform.h"
#include <vector>
#include <cstdint>

class BwtEngine {
public:
    BwtEngine();

    std::vector<uint16_t> process_stream(const std::vector<uint16_t>& token_block);

private:
    BwtTransform bwt_transformer_;
    MtfTransform mtf_transformer_;
    RleTransform rle_transformer_;
};