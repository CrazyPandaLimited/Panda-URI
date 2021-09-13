#include <panda/uri/encode.h>
#include <climits>

namespace panda { namespace uri {

#include "encode_gen.icc"

typedef unsigned char uchar;

inline static char* encode_char_impl(uchar uc, char* buf) noexcept {
    *buf++ = '%';
    *buf++ = _forward[uc][0];
    *buf++ = _forward[uc][1];
    return buf;
}

char* encode_char(uchar uc, char* buf) noexcept {
    return encode_char_impl(uc, buf);
}

size_t encode_uri_component (const string_view src, char* dest, const char* unsafe) {
    const char* str = src.data();
    const char*const end = str + src.length();
    char* buf = dest;

    while (str != end) {
        uchar uc = *str++;
        if (unsafe[uc] != 0) *buf++ = unsafe[uc];
        else                    buf = encode_char_impl(uc, buf);
    }

    return buf - dest;
}

size_t decode_uri_component (const string_view src, char* dest) {
    const char* str = src.data();
    const char*const end = str + src.length();
    char* buf = dest;

    while (str != end) {
        char res = _restore[(uchar)*str++];
        if (res != 0) *buf++ = res;
        else if (str < end-1) {
            *buf++ = _backward[(uchar)str[0]][0] | _backward[(uchar)str[1]][1];
            str += 2;
        }
    }

    return buf - dest;
}

}}
