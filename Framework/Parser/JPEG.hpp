#pragma once
#include <iostream>
#include <vector>
#include <string>
#include "portable.hpp"
#include "ImageParser.hpp"
#include "HuffmanTree.hpp"
#include "ColorSpaceConversion.hpp"
#include <cassert>

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
    size_t parseScanData(const uint8_t* pScanData, const uint8_t* pDataEnd,
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
#if DUMP_DETAILS
                std::cout << "Found RST while scan the ECS." << std::endl;
#endif
                bLastRestartSegment = false;
            }
#if DUMP_DETAILS
            std::cout << "Size Of Scan: " << scanLength << " bytes"
                      << std::endl;
            std::cout "Size Of Scan (after remove bitstuff): "
                << scan_data.size() << " bytes" << std::endl;
#endif
        }

        // 4 is max num of components defined by ITU-T81
        int16_t previous_dc[4];
        memset(previous_dc, 0x00, sizeof(previous_dc));
        size_t  byte_offset = 0;
        uint8_t bit_offset  = 0;

        while (byte_offset < scan_data.size() && mcu_index < mcu_count) {
#if DUMP_DETAILS
            std::cout << "MCU: " << mcu_index << std::endl;
#endif
            mat8 block[4];  // 4 is the max num of component
            memset(&block, 0x00, sizeof(block));

            for (uint8_t i = 0; i < m_nComponentsInFrame; i++) {
                const FRAME_COMPONENT_SPEC_PARAMS& fcsp =
                    m_tableFrameComponentsSpec[i];
#if DUMP_DETAILS
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
                        .decodeSingleValue(scan_data.data(), scan_data.size(),
                                           byte_offset, bit_offset);
                uint8_t  dc_bit_length = dc_code & 0x0F;
                int16_t  dc_value;
                uint32_t tmp_value;

                // read dc_bit_length bits
                if (!dc_code) {
#if DUMP_DETAILS
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
                                .decodeSingleValue(scan_data.data(),
                                                   scan_data.size(),
                                                   byte_offset, bit_offset);

                    if (!ac_code) {
                        // ac_index to 64 is zero
#if DUMP_DETAILS
                        std::cout << "Found EOB when decode AC!" << std::endl;
#endif
                        break;
                    } else if (ac_code == 0xF0) {
                        // the next 16 element is zero
#if DUMP_DETAILS
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
                    std::cout << "AC Bit Length: %d\n"
                              << std::dec << ac_bit_length << std::endl;
                    std::cout<<"AC Value: %d\n", ac_value);
#endif

                    int index = m_zigzagIndex[ac_index];
                    block[i][index >> 3][index & 0x07] = ac_value;

                    // update bit_offset and byte_offset
                    bit_offset += dc_bit_length & 0x07u;
                    byte_offset += dc_bit_length >> 3;
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
                    rgb = ConvertYCbCr2RGB(ycbcr);
                    reinterpret_cast<R8G8B8A8Unorm*>(pBuf)->r = (uint8_t)rgb.r;
                    reinterpret_cast<R8G8B8A8Unorm*>(pBuf)->g = (uint8_t)rgb.g;
                    reinterpret_cast<R8G8B8A8Unorm*>(pBuf)->b = (uint8_t)rgb.b;
                    reinterpret_cast<R8G8B8A8Unorm*>(pBuf)->a = 255;
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
};

}  // namespace My
