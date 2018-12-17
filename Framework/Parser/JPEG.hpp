#pragma once
#include <iostream>
#include <vector>
#include <string>
#include "ImageParser.hpp"
#include "HuffmanTree.hpp"

namespace My {
#pragma pack(push, 1)

struct JFIF_FILEHEADER {
    uint16_t SOI;
};

struct JPEG_SEGMENT_HEADER {
    uint16_t Marker;
    uint16_t Length;
};

struct APP0 : public JPEG_SEGMENT_HEADER {
    char Identitfier[5];
};

struct JFIF_APP0 : public APP0 {
    uint8_t  MajorVersion;
    uint8_t  MinorVersion;
    uint8_t  DensityUnits;
    uint16_t Xdensity;
    uint16_t Ydensity;
    uint8_t  Xthumbnail;
    uint8_t  Ythumbnail;
};

struct JFXX_APP0 : public APP0 {
    uint8_t ThumbnailFormat;
};

struct FRAME_COMPONENT_SPEC_PARAMS {
public:
    uint8_t ComponentIdentifier;

private:
    uint8_t SamplingFactor;

public:
    uint8_t  QuantizationTableDestSelector;
    uint16_t HorizontalSamplingFactor() const { return SamplingFactor >> 4; }
    uint16_t VerticalSamplingFactor() const { return SamplingFactor & 0x07; }
};

struct FRAME_HEADER : public JPEG_SEGMENT_HEADER {
    uint8_t  SamplePrecision;
    uint16_t NumOfLines;
    uint16_t NumOfSamplesPerLine;
    uint8_t  NumOfComponentsInFrame;
};

struct SCAN_COMPONENT_SPEC_PARAMS {
public:
    uint8_t ComponentSelector;

private:
    uint8_t EntropyCodingTableDestSelector;

public:
    uint16_t DcEntropyCodingTableDestSelector() const {
        return EntropyCodingTableDestSelector >> 4;
    }
    uint16_t AcEntropyCodingTableDestSelector() const {
        return EntropyCodingTableDestSelector & 0x07;
    }
};

struct SCAN_HEADER : public JPEG_SEGMENT_HEADER {
    uint8_t NumfOfComponents;
};

struct QUANTIZATION_TABLE_SPEC {
private:
    uint8_t data;

public:
    uint16_t ElementPrecision() const { return data >> 4; }
    uint16_t DestinationIdentifier() const { return data & 0x07; }
};

struct HUFFMAN_TABLE_SPEC {
private:
    uint8_t data;

public:
    uint8_t NumOfHuffmanCodes[16];

    uint16_t TableClass() const { return data >> 4; }
    uint16_t DestinationIdentifier() const { return data & 0x07; }
};

struct RESTART_INTERVAL_DEF : public JPEG_SEGMENT_HEADER {
    uint16_t RestartInterval;
};

#pragma pack(pop)

class JfifParser : implements ImageParser {
private:
    const uint8_t m_zigzagIndex[64] = {
        0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
        12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
        35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
        58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63};

protected:
    HuffmanTree<uint8_t>                     m_treeHuffman[4];
    mat8                                     m_tableQuantization[4];
    std::vector<FRAME_COMPONENT_SPEC_PARAMS> m_tableFrameComponentsSpec;
    uint16_t                                 m_nSamplePrecision;
    uint16_t                                 m_nLines;
    uint16_t                                 m_nSamplesPerLine;
    uint16_t                                 m_nComponentsInFrame;
    uint16_t                                 m_nRestartInterval = 0;
    int                                      mcu_index;
    int                                      mcu_count_x;
    int                                      mcu_count_y;
    int                                      mcu_count;
    const SCAN_COMPONENT_SPEC_PARAMS*        pScsp;

};

}  // namespace My
