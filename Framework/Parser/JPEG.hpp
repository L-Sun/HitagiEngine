#pragma once
#include <iostream>
#include <vector>
#include <string>
#include "portable.hpp"
#include "ImageParser.hpp"
#include "HuffmanTree.hpp"
#include "ColorSpaceConversion.hpp"
#include <cassert>

// #define DUMP_DETAILS 1

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
    char Identifier[5];
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
    uint8_t NumOfComponents;
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

    uint16_t m_nSamplePrecision;
    uint16_t m_nLines;
    uint16_t m_nSamplesPerLine;
    uint16_t m_nComponentsInFrame;
    uint16_t m_nRestartInterval = 0;

    int mcu_index;
    int mcu_count_x;
    int mcu_count_y;
    int mcu_count;

    const SCAN_COMPONENT_SPEC_PARAMS* pScsp;

protected:
    size_t ParseScanData(const uint8_t* pScanData, const uint8_t* pDataEnd,
                         Image& img) {
        std::vector<uint8_t> scan_data;
        size_t               scanLength          = 0;
        bool                 bLastRestartSegment = true;

        {
            const uint8_t* p = pScanData;

            // scan for scan data buffer size and remove bitstuff
            bool bitstuff = false;

            // promise there is no segment flag like 0xFF00
            while (p < pDataEnd && (*p != 0xFF || *(p + 1) == 0x00)) {
                if (!bitstuff) {
                    scan_data.push_back(*p);
                } else {
                    // skip the bitstuff
                    assert(*p == 0x00);
                    bitstuff = false;
                }

                // 0xFF00 is Equivalent to 0xFF because of JPEG standard
                // All binary storage in JPEG is Big Endian but x86 is Small
                // Endian, so the type_cast of p is 0xFF00, and the value of
                // 0xFF00 is 00 FF in memory. So we need change bytes storage
                // style and compare both of them.
                if (*(uint16_t*)p ==
                    endian_net_unsigned_int((uint16_t)0xFF00)) {
                    bitstuff = true;
                }
                p++;
                scanLength++;
            }

            if (*p == 0xFF && *(p + 1) >= 0xD0 && *(p + 1) <= 0xD7) {
                // found restart mark
#ifdef DUMP_DETAILS
                std::cout << "Found RST while scan the ECS." << std::endl;
#endif
                bLastRestartSegment = false;
            }
#ifdef DUMP_DETAILS
            std::cout << "Size Of Scan: " << scanLength << " bytes"
                      << std::endl;
            std::cout << "Size Of Scan (after remove bitstuff): "
                      << scan_data.size() << " bytes" << std::endl;
#endif
        }

        // 4 is max num of components defined by ITU-T81
        int16_t previous_dc[4];
        memset(previous_dc, 0x00, sizeof(previous_dc));
        size_t  byte_offset = 0;
        uint8_t bit_offset  = 0;

        while (byte_offset < scan_data.size() && mcu_index < mcu_count) {
#ifdef DUMP_DETAILS
            std::cout << "MCU: " << mcu_index << std::endl;
#endif
            mat8 block[4];  // 4 is the max num of component
            memset(&block, 0x00, sizeof(block));

            for (uint8_t i = 0; i < m_nComponentsInFrame; i++) {
                const FRAME_COMPONENT_SPEC_PARAMS& fcsp =
                    m_tableFrameComponentsSpec[i];
#ifdef DUMP_DETAILS
                std::cout << "\tComponent Selector: "
                          << (uint16_t)pScsp[i].ComponentSelector << std::endl;
                std::cout << "\tQuantization Table Destination Selector: "
                          << (uint16_t)fcsp.QuantizationTableDestSelector
                          << std::endl;
                std::cout
                    << "\tDC Entropy Coding Table Destination Selector: "
                    << (uint16_t)pScsp[i].DcEntropyCodingTableDestSelector()
                    << std::endl;
                std::cout
                    << "\tAC Entropy Coding Table Destination Selector: "
                    << (uint16_t)pScsp[i].AcEntropyCodingTableDestSelector()
                    << std::endl;
#endif
                // Decode DC
                uint8_t dc_code =
                    m_treeHuffman[pScsp[i].DcEntropyCodingTableDestSelector()]
                        .DecodeSingleValue(scan_data.data(), scan_data.size(),
                                           byte_offset, bit_offset);
                uint8_t  dc_bit_length = dc_code & 0x0F;
                int16_t  dc_value;
                uint32_t tmp_value;

                // read dc_bit_length bits
                if (!dc_code) {
#ifdef DUMP_DETAILS
                    std::cout << "Found EOB when decode DC!" << std::endl;
#endif
                    dc_value = 0;
                } else {
                    if (dc_bit_length + bit_offset <= 8) {
                        // dc value in a bytes
                        tmp_value = ((scan_data[byte_offset] &
                                      ((0x01u << (8 - bit_offset)) - 1)) >>
                                     (8 - dc_bit_length - bit_offset));
                    } else {
                        // dc value spanning one or more bytes
                        uint8_t bits_in_first_byte = 8 - bit_offset;
                        uint8_t append_full_bytes =
                            (dc_bit_length - bits_in_first_byte) / 8;
                        uint8_t bits_in_last_byte = dc_bit_length -
                                                    bits_in_first_byte -
                                                    8 * append_full_bytes;
                        tmp_value = (scan_data[byte_offset] &
                                     ((0x01u << (8 - bit_offset)) - 1));
                        for (int m = 1; m <= append_full_bytes; m++) {
                            tmp_value <<= 8;
                            tmp_value += scan_data[byte_offset + m];
                        }
                        tmp_value <<= bits_in_last_byte;
                        tmp_value +=
                            (scan_data[byte_offset + append_full_bytes + 1] >>
                             (8 - bits_in_last_byte));
                    }

                    // decode dc bits to value
                    if ((tmp_value >> (dc_bit_length - 1)) == 0) {
                        // https://users.ece.utexas.edu/~ryerraballi/MSB/pdfs/M4L1.pdf
                        // size and value table
                        // if the MSB is zero, bitwise NOT the effective number
                        // of bitsdc_value and negate it
                        dc_value = -(int16_t)(~tmp_value &
                                              ((0x0001u << dc_bit_length) - 1));
                    } else {
                        dc_value = tmp_value;
                    }
                }

                // add with previous DC value
                dc_value += previous_dc[i];
                // save the value for next DC
                previous_dc[i] = dc_value;

#ifdef DUMP_DETAILS
                std::cout << "DC Code: " << std::hex << dc_code << std::endl;
                std::cout << "DC Bit Length" << std::dec << dc_bit_length
                          << std::endl;
                std::cout << "DC Value: " << dc_value << std::endl;
#endif
                block[i][0][0] = dc_value;

                // update bit_offset and byte_offset
                bit_offset += dc_bit_length & 0x07u;
                byte_offset += dc_bit_length >> 3;
                if (bit_offset >= 8) {
                    bit_offset -= 8;
                    byte_offset++;
                }

                // Decode AC
                int ac_index = 1;
                while (byte_offset < scan_data.size() && ac_index < 64) {
                    uint8_t ac_code =
                        m_treeHuffman
                            [2 + pScsp[i].AcEntropyCodingTableDestSelector()]
                                .DecodeSingleValue(scan_data.data(),
                                                   scan_data.size(),
                                                   byte_offset, bit_offset);

                    if (!ac_code) {
                        // ac_index to 64 is zero
#ifdef DUMP_DETAILS
                        std::cout << "Found EOB when decode AC!" << std::endl;
#endif
                        break;
                    } else if (ac_code == 0xF0) {
                        // the next 16 element is zero
#ifdef DUMP_DETAILS
                        std::cout << "Found ZRL when decode AC!" << std::endl;
#endif
                        ac_index += 16;
                        continue;
                    }

                    uint8_t ac_zero_length = ac_code >> 4;
                    ac_index += ac_zero_length;
                    uint8_t ac_bit_length = ac_code & 0x0F;
                    int16_t ac_value;

                    if (ac_bit_length + bit_offset <= 8) {
                        tmp_value = ((scan_data[byte_offset] &
                                      ((0x01u << (8 - bit_offset)) - 1)) >>
                                     (8 - ac_bit_length - bit_offset));
                    } else {
                        uint8_t bits_in_first_byte = 8 - bit_offset;
                        uint8_t append_full_bytes =
                            (ac_bit_length - bits_in_first_byte) / 8;
                        uint8_t bits_in_last_byte = ac_bit_length -
                                                    bits_in_first_byte -
                                                    8 * append_full_bytes;
                        tmp_value = (scan_data[byte_offset] &
                                     ((0x01u << (8 - bit_offset)) - 1));
                        for (int m = 1; m <= append_full_bytes; m++) {
                            tmp_value <<= 8;
                            tmp_value += scan_data[byte_offset + m];
                        }
                        tmp_value <<= bits_in_last_byte;
                        tmp_value +=
                            (scan_data[byte_offset + append_full_bytes + 1] >>
                             (8 - bits_in_last_byte));
                    }

                    // decode ac value
                    if ((tmp_value >> (ac_bit_length - 1)) == 0) {
                        // MSB = 1, turn it to minus value
                        ac_value = -(int16_t)(~tmp_value &
                                              ((0x0001u << ac_bit_length) - 1));
                    } else {
                        ac_value = tmp_value;
                    }

#ifdef DUMP_DETAILS
                    std::cout << "AC Code: " << std::hex << ac_code
                              << std::endl;
                    std::cout << "AC Bit Length: " << std::dec << ac_bit_length
                              << std::endl;
                    std::cout << "AC Value: " << ac_value << std::endl;
#endif

                    int index = m_zigzagIndex[ac_index];
                    block[i][index >> 3][index & 0x07] = ac_value;

                    // update bit_offset and byte_offset
                    bit_offset += ac_bit_length & 0x07u;
                    byte_offset += ac_bit_length >> 3;
                    if (bit_offset >= 8) {
                        bit_offset -= 8;
                        byte_offset++;
                    }

                    ac_index++;
                }

#ifdef DUMP_DETAILS
                std::cout << "Extracted Component[" << i
                          << "] 8x8 block: " << std::endl;
                std::cout << block[i];
#endif
                block[i] = block[i].mulByElement(
                    m_tableQuantization[fcsp.QuantizationTableDestSelector]);
#ifdef DUMP_DETAILS
                std::cout << "After Quantization: " << block[i];
#endif
                block[i][0][0] += 1024.0f;  // level shift. same as +128 to each
                                            // element after IDCT
                block[i] = IDCT8x8(block[i]);
#ifdef DUMP_DETAILS
                std::cout << "After IDCT: " << block[i];
#endif
            }
            assert(m_nComponentsInFrame <= 4);

            YCbCr    ycbcr;
            RGB      rgb;
            int      mcu_index_x = mcu_index % mcu_count_x;
            int      mcu_index_y = mcu_index / mcu_count_x;
            uint8_t* pBuf;

            for (int i = 0; i < 8; i++) {
                for (int j = 0; j < 8; j++) {
                    for (int k = 0; k < m_nComponentsInFrame; k++) {
                        ycbcr[k] = block[k][i][j];
                    }

                    pBuf = reinterpret_cast<uint8_t*>(img.data) +
                           (img.pitch * (mcu_index_y * 8 + i) +
                            (mcu_index_x * 8 + j) * (img.bitcount >> 3));
                    rgb     = ConvertYCbCr2RGB(ycbcr);
                    pBuf[0] = (uint8_t)rgb.r;
                    pBuf[1] = (uint8_t)rgb.g;
                    pBuf[2] = (uint8_t)rgb.b;
                    pBuf[3] = 255;
                }
            }

            mcu_index++;

            if (m_nRestartInterval != 0 &&
                (mcu_index % m_nRestartInterval == 0)) {
                if (bit_offset) {
                    // finish current byte
                    bit_offset = 0;
                    byte_offset++;
                }
                assert(byte_offset == scan_data.size());
                break;
            }
        }

        return scanLength;
    }

public:
    virtual Image Parse(const Buffer& buf) {
        Image img;

        const uint8_t* pData    = buf.GetData();
        const uint8_t* pDataEnd = buf.GetData() + buf.GetDataSize();

        const JFIF_FILEHEADER* pFileHeader =
            reinterpret_cast<const JFIF_FILEHEADER*>(pData);
        pData += sizeof(JFIF_FILEHEADER);
        if (pFileHeader->SOI == endian_net_unsigned_int((uint16_t)0xFFD8)) {
            std::cout << "Asset is JPEG file" << std::endl;

            while (pData < pDataEnd) {
                bool   foundStartOfScan   = false;
                bool   foundRestartOfScan = false;
                size_t scanLength         = 0;

                const JPEG_SEGMENT_HEADER* pSegmentHeader =
                    reinterpret_cast<const JPEG_SEGMENT_HEADER*>(pData);
#ifdef DUMP_DETAILS
                std::cout << "=====================" << std::endl;
#endif
                // clang-format off
                switch (endian_net_unsigned_int(pSegmentHeader->Marker)) {
                    case 0xFFC0:
                    case 0xFFC2: {
                        if (endian_net_unsigned_int(pSegmentHeader->Marker) ==
                            0xFFC0) {
                            std::cout << "StartOfFrame0 (baseline DCT)" << std::endl;
                        } else {
                            std::cout << "Start Of Frame2 (progressive DCT)" << std::endl;
                        }
                        std::cout << "---------------------------" << std::endl;

                        const FRAME_HEADER* pFrameHeader = reinterpret_cast<const FRAME_HEADER*>(pData);

                        m_nSamplePrecision   = pFrameHeader->SamplePrecision;
                        m_nLines             = endian_net_unsigned_int((uint16_t)pFrameHeader->NumOfLines);
                        m_nSamplesPerLine    = endian_net_unsigned_int((uint16_t)pFrameHeader->NumOfSamplesPerLine);
                        m_nComponentsInFrame = pFrameHeader->NumOfComponentsInFrame;
                        mcu_index   = 0;
                        mcu_count_x = ((m_nSamplesPerLine + 7) >> 3);
                        mcu_count_y = ((m_nLines + 7) >> 3);
                        mcu_count   = mcu_count_x * mcu_count_y;

                        std::cout << "Sample Precision: " << m_nSamplePrecision << std::endl;
                        std::cout << "Num of Lines: " << m_nLines << std::endl;
                        std::cout << "Num of Samples per Line: " << m_nSamplesPerLine << std::endl;
                        std::cout << "Num of Components In Frame: " << m_nComponentsInFrame << std::endl;
                        std::cout << "Total MCU counr: " << mcu_count << std::endl;

                        const uint8_t* pTmp = pData + sizeof(FRAME_HEADER);
                        const FRAME_COMPONENT_SPEC_PARAMS* pFcsp = reinterpret_cast<const FRAME_COMPONENT_SPEC_PARAMS*>(pTmp);

                        for (uint8_t i = 0; i < pFrameHeader->NumOfComponentsInFrame; i++) {
                            std::cout << "\tComponent Indentifier: " << (uint16_t)pFcsp->ComponentIdentifier << std::endl;
                            std::cout << "\tHorizontal Sampling Factor: " << (uint16_t)pFcsp->HorizontalSamplingFactor() << std::endl;
                            std::cout << "\tVertical Sampling Factor: " << (uint16_t)pFcsp->VerticalSamplingFactor() << std::endl;
                            std::cout << "\tQuantization Table Destination Select: " << (uint16_t)pFcsp->QuantizationTableDestSelector << std::endl;
                            std::cout << std::endl;
                            m_tableFrameComponentsSpec.push_back(*pFcsp);
                            pFcsp++;
                        }

                        img.Width     = m_nSamplesPerLine;
                        img.Height    = m_nLines;
                        img.bitcount  = 32;
                        img.pitch     = mcu_count_x * 8 * (img.bitcount >> 3);
                        img.data_size = img.pitch * mcu_count_y * 8 * (img.bitcount >> 3);
                        img.data      = g_pMemoryManager->Allocate(img.data_size);

                        pData += endian_net_unsigned_int(pSegmentHeader->Length) + 2; // length of marker
                    } break;
                    case 0xFFC4:
                    {
                        std::cout << "Define Huffman Table" << std::endl;
                        std::cout << "-------------------------------" << std::endl;

                        size_t segmentLength = endian_net_unsigned_int(pSegmentHeader->Length) - 2;

                        const uint8_t* pTmp = pData + sizeof(JPEG_SEGMENT_HEADER);
                        
                        while(segmentLength > 0) {
                            const HUFFMAN_TABLE_SPEC* pHtable = reinterpret_cast<const HUFFMAN_TABLE_SPEC*>(pTmp);
                            std::cout << "Table Class: " << pHtable->TableClass() << std::endl;
                            std::cout << "Destination Identifier: " << pHtable->DestinationIdentifier() << std::endl;

                            const uint8_t* pCodeValueStart = reinterpret_cast<const uint8_t*>(pHtable) + sizeof(HUFFMAN_TABLE_SPEC);

                            auto num_symbo = m_treeHuffman[(pHtable->TableClass() << 1) | pHtable->DestinationIdentifier()].FillHuffmanTree(pHtable->NumOfHuffmanCodes, pCodeValueStart);

#ifdef DUMP_DETAILS
                            m_treeHuffman[(pHtable->TableClass() << 1) | pHtable->DestinationIdentifier()].Dump();
#endif
                            size_t processed_length = sizeof(HUFFMAN_TABLE_SPEC) + num_symbo;
                            pTmp += processed_length;
                            segmentLength -= processed_length;
                        }
                        pData += endian_net_unsigned_int(pSegmentHeader->Length) + 2;
                    } break;
                    case 0XFFDB:
                    {
                        std::cout << "Define Quantization Table" << std::endl;
                        std::cout << "-------------------------------" << std::endl;

                        size_t segmentLength = endian_net_unsigned_int(pSegmentHeader->Length) - 2;
                    
                        const uint8_t* pTmp = pData + sizeof(JPEG_SEGMENT_HEADER);

                        while(segmentLength > 0){
                            const QUANTIZATION_TABLE_SPEC* pQtable = reinterpret_cast<const QUANTIZATION_TABLE_SPEC*>(pTmp);
                            std::cout << "Element Precision: " << pQtable->ElementPrecision() << std::endl;
                            std::cout << "Destination Identifier: " << pQtable->DestinationIdentifier() << std::endl;

                            const uint8_t* pElementDataStart = reinterpret_cast<const uint8_t*>(pQtable) + sizeof(QUANTIZATION_TABLE_SPEC);
                                
                            for(int i = 0; i < 64; i++) {
                                int index = m_zigzagIndex[i];
                                
                                if (pQtable->ElementPrecision() == 0) {
                                    m_tableQuantization[pQtable->DestinationIdentifier()][index >> 3][index & 0x7] = pElementDataStart[i];
                                } else {
                                    m_tableQuantization[pQtable->DestinationIdentifier()][index >> 3][index & 0x7] = endian_net_unsigned_int(*((uint16_t*)pElementDataStart + i));
                                }
                            }
#ifdef DUMP_DETAILS
                            std::cout << m_tableQuantization[pQtable->DestinationIdentifier()];
#endif
                            size_t processed_length = sizeof(QUANTIZATION_TABLE_SPEC) + 64 * (pQtable->ElementPrecision() + 1);
                            pTmp += processed_length;
                            segmentLength -= processed_length;
                        }
                        pData += endian_net_unsigned_int(pSegmentHeader->Length) + 2;
                    } break;
                    case 0xFFDD:
                    {
                        std::cout << "Define Restart Interval" << std::endl;
                        std::cout << "-------------------------------" << std::endl;

                        const RESTART_INTERVAL_DEF* pRestartHeader = reinterpret_cast<const RESTART_INTERVAL_DEF*>(pData);
                        m_nRestartInterval = endian_net_unsigned_int((uint16_t)pRestartHeader->RestartInterval);
                        std::cout << "Restart interval: " << m_nRestartInterval << std::endl;
                        pData += endian_net_unsigned_int(pSegmentHeader->Length) + 2;
                        } break;
                    case 0xFFDA:
                    {
                        foundStartOfScan = true;
                        std::cout << "Start Of Scan" << std::endl;
                        std::cout << "-------------------------------" << std::endl;

                        const SCAN_HEADER* pScanHeader =reinterpret_cast<const SCAN_HEADER*>(pData);
                        std::cout << "Image Components in Scan: " << (uint16_t)pScanHeader->NumOfComponents << std::endl;
                        assert(pScanHeader->NumOfComponents == m_nComponentsInFrame);

                        const uint8_t* pTmp = pData + sizeof(SCAN_HEADER);
                        pScsp = reinterpret_cast<const SCAN_COMPONENT_SPEC_PARAMS*>(pTmp);

                        const uint8_t* pScanData = pData + endian_net_unsigned_int((uint16_t)pScanHeader->Length) + 2;
                        scanLength = ParseScanData(pScanData, pDataEnd, img);
                        pData += endian_net_unsigned_int(pSegmentHeader->Length) + 2 + scanLength;
                    } break;
                    case 0xFFD0:
                    case 0xFFD1:
                    case 0xFFD2:
                    case 0xFFD3:
                    case 0xFFD4:
                    case 0xFFD5:
                    case 0xFFD6:
                    case 0xFFD7:
                    {
                        foundRestartOfScan = true;
#ifdef DUMP_DETAILS
                        std::cout << "Restart Of Scan" << std::endl;
                        std::cout << "-------------------------------" << std::endl;
#endif

                        const uint8_t* pScanData = pData + 2;
                        scanLength = ParseScanData(pScanData, pDataEnd, img);
                        pData += 2 + scanLength;
                    } break;
                    case 0xFFD9:
                    {
                        foundStartOfScan = false;
                        std::cout << "End Of Scan" << std::endl;
                        std::cout << "-------------------------------" << std::endl;
                        pData += 2;
                    } break;
                    case 0xFFE0:
                    {
                        const APP0* pApp0 = reinterpret_cast<const APP0*>(pData);
                        switch (endian_net_unsigned_int(*(uint32_t*)pApp0->Identifier)) {
                            case "JFIF\0"_u32: 
                            {
                                const JFIF_APP0* pJfifApp0 = reinterpret_cast<const JFIF_APP0*>(pApp0);
                                std::cout << "JFIF-APP0" << std::endl;
                                std::cout << "-------------------------------" << std::endl;
                                std::cout << "JFIF Version: " << (uint16_t)pJfifApp0->MajorVersion << "." 
                                    << (uint16_t)pJfifApp0->MinorVersion << std::endl;
                                std::cout << "Density Units: " << 
                                    ((pJfifApp0->DensityUnits == 0)? "No units" : 
                                    ((pJfifApp0->DensityUnits == 1)? "Pixels per inch" : "Pixels per centimeter" )) 
                                    << std::endl;
                                std::cout << "Density: " << endian_net_unsigned_int(pJfifApp0->Xdensity) << "*" 
                                    << endian_net_unsigned_int(pJfifApp0->Ydensity) << std::endl;
                                if (pJfifApp0->Xthumbnail && pJfifApp0->Ythumbnail) {
                                    std::cout << "Thumbnail Dimesions [w*h]: " << (uint16_t)pJfifApp0->Xthumbnail << "*" 
                                        << (uint16_t)pJfifApp0->Ythumbnail << std::endl;
                                } else {
                                    std::cout << "No thumbnail defined in JFIF-APP0 segment!" << std::endl;
                                }
                            }break;
                            case "JFXX\0"_u32:
                            {
                                const JFXX_APP0* pJfxxApp0 = reinterpret_cast<const JFXX_APP0*>(pApp0);
                                std::cout << "Thumbnail Format: ";
                                switch (pJfxxApp0->ThumbnailFormat) {
                                    case 0x10:
                                        std::cout << "JPEG format";
                                        break;
                                    case 0x11:
                                        std::cout << "1 byte per pixel palettized format";
                                        break;
                                    case 0x13:
                                        std::cout << "3 byte per pixel RGB format";
                                        break;
                                    default:
                                        std::printf("Unrecognized Thumbnail Format: %x\n", pJfxxApp0->ThumbnailFormat);
                                }
                                            std::cout << std::endl;
                            }
                            default:
                                std::cout << "Ignor Unrecognized APP0 segment." << std::endl;
                            }
                        pData += endian_net_unsigned_int(pSegmentHeader->Length) + 2 /* length of marker */;
                    } break;
                    case 0xFFFE:
                    {
                        std::cout << "Text Comment" << std::endl;
                        std::cout << "-------------------------------" << std::endl;
                        pData += endian_net_unsigned_int(pSegmentHeader->Length) + 2;
                    } break;
                    default:
                    {
                        std::printf("Ignor Unrecognized Segment. Marker=%0x\n", endian_net_unsigned_int(pSegmentHeader->Marker));
                        pData += endian_net_unsigned_int(pSegmentHeader->Length) + 2;
                    } break;
                }
                // clang-format on
            }
        } else {
            std::cout << "File is not a JPEG file!" << std::endl;
        }

        return img;
    }
};

}  // namespace My
