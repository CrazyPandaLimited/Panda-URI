#include "test.h"
#include <panda/uri/punycode.h>

void test(string_view in, string_view expected) {
    char enc_buff[128];
    char dec_buff[256];
    punycode_uint ecn_sz = 128, dec_sz = 256;
    auto r = punycode_encode(in, &ecn_sz, enc_buff);
    CHECK(r == punycode_success);
    auto enc = string_view(enc_buff, ecn_sz);
    CHECK(enc == expected);

    r = punycode_decode(enc,  &dec_sz, dec_buff);
    CHECK(r == punycode_success);
    auto dec = string_view(dec_buff, dec_sz);
    CHECK(dec == in);
}

TEST_CASE("punycode", "[punycode]") {
    test("почемужеонинеговорятпорусски", "b1abfaaepdrnnbgefbadotcwatmq2g4l");
    test("минск.бел", ".-btbmjkjbj0b");
    //test("Į", "9ea"); - not casefolded
    test("į", "9ea");
}
