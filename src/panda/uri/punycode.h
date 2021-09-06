#pragma once
#include <panda/string.h>

namespace panda { namespace uri {

/*
adopted punycode.c from RFC 3492
*/


#include <limits.h>

enum punycode_status {
  punycode_success,
  punycode_bad_input,   /* Input is invalid.                       */
  punycode_big_output,  /* Output would exceed the space provided. */
  punycode_overflow     /* Input needs wider integers to process.  */
};

#if UINT_MAX >= (1 << 26) - 1
typedef unsigned int punycode_uint;
#else
typedef unsigned long punycode_uint;
#endif

// assumes utf8 input to be case-folded, see http://www.unicode.org/reports/tr46/#Mapping
punycode_status punycode_encode(const string_view& raw_input, punycode_uint *output_length, char output[]);

// assumes that output buff is enought to store decode, precompute it as input.size() x 4
punycode_status punycode_decode(const string_view& input, punycode_uint *output_length, char output[]);


}}
