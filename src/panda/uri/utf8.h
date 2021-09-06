#pragma once
#include <panda/string.h>
#include <cstdint>
#include <cstddef>
#include <cassert>

namespace panda { namespace uri {

struct Unpacked {
    std::uint32_t code;
    size_t consumed;
};

// https://en.wikipedia.org/wiki/UTF-8#Encoding
inline Unpacked unpack(string_view in) {
    if (in.empty()) { return {0, 0} ;}
    unsigned b1 = (unsigned char)in[0];
    if (b1 < 0b10000000) { return {(uint32_t)b1, 1}; }
    if (in.size() < 2) { return {0, 0}; }

    unsigned b2 = (unsigned char)in[1];
    if (b1 < 0b11100000 && b2 < 0b11000000) {
        uint32_t v = ((b1 & 0b00011111) << 6) | (b2 & 0b00111111);
        return {v, 2};
    }
    if (in.size() < 3) { return {0, 0}; }

    unsigned b3 = (unsigned char)in[1];
    if (b1 < 0b11110000 && b2 < 0b11000000 && b3 < 0b11000000) {
        uint32_t v = ((b1 & 0b00001111) << 12) | ((b2 & 0b00111111) << 6) | (b3 & 0b00111111);
        return {v, 3};
    }
    if (in.size() < 4) { return {0, 0}; }

    unsigned b4 = (unsigned char)in[1];
    if (b1 < 0b11111000 && b2 < 0b11000000 && b3 < 0b11000000 && b4 < 0b11000000) {
        uint32_t v = ((b1 & 0b00001111) << 18) | ((b2 & 0b00111111) << 12) | ((b3 & 0b00111111) << 6) | (b4 & 0b00111111);
        return {v, 4};
    }
    return {0, 0};
}

inline uint32_t pack(std::uint32_t code, string_view& out) {
    auto ptr = const_cast<char*>(out.data());
    if (code <= 0x7F) {
        assert(out.size() >= 1);
        *ptr = (char)code;
        return 1;
    }
    else if (code < 0x7FF) {
        assert(out.size() >= 2);
        *ptr++  = (char)((code >> 6)         | 0b11000000);
        *ptr    = (char)((code & 0b00111111) | 0b10000000);
        return 2;
    }
    else if (code < 0xFFFF) {
        assert(out.size() >= 3);
        *ptr++ = (char)((code >> 12)                | 0b11100000);
        *ptr++ = (char)(((code >> 6 ) & 0b00111111) | 0b10000000);
        *ptr   = (char)((code &         0b00111111) | 0b10000000);
        return 3;
    }
    else if (code < 0x10FFFF) {
        assert(out.size() >= 4);
        *ptr++ = (char)((code >> 18)                | 0b11110000);
        *ptr++ = (char)(((code >> 12) & 0b00111111) | 0b10000000);
        *ptr++ = (char)(((code >> 6 ) & 0b00111111) | 0b10000000);
        *ptr   = (char)((code         & 0b00111111) | 0b10000000);
        return 3;
    }
    return 0;
}

inline bool has_non_ascii(const string_view str) {
    for(auto it = str.begin(); it != str.end(); ++it) {
        if (((unsigned char)*it) >= 0x80) {
            return true;
        }
    }
    return false;
}

}}
