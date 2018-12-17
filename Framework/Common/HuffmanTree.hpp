#pragma once
#include <unordered_map>
#include <cstdint>
#include <memory>
#include <queue>
#include <string>
#include <iostream>
#include <iomanip>

namespace My {

template <typename T>
class HuffmanNode {
public:
    HuffmanNode() = default;
    HuffmanNode(T value) : m_Value(value), m_isLeaf(true) {}

    HuffmanNode(std::shared_ptr<HuffmanNode> left,
                std::shared_ptr<HuffmanNode> right = nullptr)
        : m_pLeft(left), m_pRight(right) {}

    bool isLeaf() { return m_isLeaf; }

    T                                  getValue() const { return m_Value; }
    const std::shared_ptr<HuffmanNode> getLeft() const { return m_pLeft; }
    const std::shared_ptr<HuffmanNode> getRight() const { return m_pRight; }

protected:
    T                            m_Value;
    std::shared_ptr<HuffmanNode> m_pLeft;
    std::shared_ptr<HuffmanNode> m_pRight;
    bool                         m_isLeaf;
};

template <typename T>
class HuffmanTree {
public:
    HuffmanTree() = default;
    void dump() {
        std::string bits;
        dump(m_pRoot, bits);
    }

    void fillHuffmanTree(const uint8_t layer[16], const uint8_t* code) {
        size_t code_size = 0;
        for (int i = 0; i < 16; i++) {
            code_size += layer[i];
        }
        const uint8_t* p_end = code + code_size - 1;
        std::queue<std::shared_ptr<HuffmanNode<T>>> node_queue;

        bool found_bottom = false;

        for (int i = 15; i >= 0; i--) {
            uint8_t L = layer[i];
            if (!found_bottom) {
                if (L == 0)
                    // simple move to upper layer
                    continue;
                else
                    found_bottom = true;
            }

            size_t sz_lower_layer = node_queue.size();

            p_end -= L;
            const uint8_t* p_curr = p_end;
            // Push the i-th layer to node queue from left to right
            while (L != 0) {
                // *++p_curr avoid out of bounds
                auto node = std::make_shared<HuffmanNode<T>>(*++p_curr);
                node_queue.push(node);
                L--;
            }

            // Build lower layer Huffman tree
            // and push to the curr layer node queue
            while (sz_lower_layer > 0) {
                std::shared_ptr<HuffmanNode<T>> left, right;

                left = node_queue.front();
                node_queue.pop();
                sz_lower_layer--;

                if (sz_lower_layer != 0) {
                    right = node_queue.front();
                    node_queue.pop();
                    sz_lower_layer--;
                }
                auto node = std::make_shared<HuffmanNode<T>>(left, right);
                node_queue.push(node);
            }
        }

        if (node_queue.size() != 1 || node_queue.size() != 2) {
            std::cout << "Fill Huffman tree error!" << std::endl;
            return;
        }

        std::shared_ptr<HuffmanNode<T>> left, right;
        left = node_queue.front();
        node_queue.pop();
        if (!node_queue.empty()) {
            right = node_queue.front();
            node_queue.pop();
        }
        m_pRoot = std::make_shared<HuffmanNode<T>>(left, right);
    }

    std::vector<T> decode(const uint8_t* encoded_stream,
                          const size_t   encoded_stream_length) {
        std::vector<T> res;
        auto           curr_node = m_pRoot;

        for (size_t i = 0; i < encoded_stream_length; i++) {
            uint8_t data = encoded_stream[i];

            for (int j = 0; j < 8; j++) {
                uint8_t bit = (data & (0x1 << (7 - j))) >> (7 - j);
                curr_node =
                    bit == 0 ? curr_node->getLeft() : curr_node->getRight();

                if (!curr_node) {
                    std::cout << "Decode error at " << i * 8 + j << std::endl;
                    return std::vector<T>();
                }

                if (curr_node->isLeaf()) {
                    res.push_back(curr_node->getValue());
                    curr_node = m_pRoot;
                }
            }
        }
    }

    T decodeSingleValue(const uint8_t* encoded_stream,
                        const size_t encoded_stream_length, size_t& byte_offset,
                        uint8_t& bit_offset) {
        T    res       = 0;
        auto curr_node = m_pRoot;

        for (size_t i = byte_offset; i < encoded_stream_length; i++) {
            uint8_t data = encoded_stream[i];
            for (int j = bit_offset; j < 8; j++) {
                uint8_t bit = (data & (0x1 << (7 - j))) >> (7 - j);
                curr_node =
                    bit == 0 ? curr_node->getLeft() : curr_node->getRight();
                if (!curr_node) {
                    std::cout << "Decode error at " << i * 8 + j << std::endl;
                    return 0;
                }
                if (curr_node->isLeaf()) {
                    if (j == 7) {
                        byte_offset = i + 1;
                        bit_offset  = 0;
                    } else {
                        byte_offset = i;
                        bit_offset  = j + 1;
                    }
                    res = curr_node->getValue();
                    return res;
                }
            }
            bit_offset = 0;
        }
        std::cout << "Decode error: can't find value with encoded stream!"
                  << std::endl;
        byte_offset = -1;
        bit_offset  = -1;
        return res;
    }

private:
    std::shared_ptr<HuffmanNode<T>> m_pRoot;
    void dump(const std::shared_ptr<HuffmanNode<T>>& node, std::string& bits) {
        if (node) {
            if (node->isLeaf()) {
                std::cout << std::setw(5) << std::right << node->getValue()
                          << "|" << std::left << bits << std::endl;
            } else {
                dump(node->getLeft(), bits + '0');
                dump(node->getRight(), bits + '1');
            }
        }
    }
};
}  // namespace My
