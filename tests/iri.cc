#include "test.h"

URI parse(string str) {
    return URI(str, URI::Flags::dont_parse_uri);
}

TEST_CASE("uri is parsed sa iri", "[iri]") {
    URI uri = parse("https://user:password@ya.ru:99/b/page.html?k=v#my_fragment");
    CHECK(uri.scheme() == "https");
    CHECK(uri.user_info() == "user:password");
    CHECK(uri.host() == "ya.ru");
    CHECK(uri.port() == 99);
    CHECK(uri.path() == "/b/page.html");
    CHECK(uri.query_string() == "k=v");
    CHECK(uri.fragment() == "my_fragment");
}

TEST_CASE("error in uri(parsed sa iri)", "[iri]") {
    URI uri = parse("https:://us");
    CHECK(uri.scheme() == "");
    CHECK(uri.host() == "");
}

/* UTF8 encoding in the test. Don't use CP1251 in XXI century! */

TEST_CASE("uri", "[iri]") {
    URI uri = parse("https://вася:пупкин@москва.рф:99/кремль/президенты.html?💔=€#путин");
    CHECK(uri.scheme() == "https");
    CHECK(uri.user_info() == "вася:пупкин");
    CHECK(uri.host() == "xn--80adxhks.xn--p1ai");
    CHECK(uri.port() == 99);
    CHECK(uri.path() == "/кремль/президенты.html");
    CHECK(uri.query_string() == "💔=€");
    CHECK(uri.fragment() == "путин");
}

TEST_CASE("uri schemaless", "[iri]") {
    URI uri = parse("//москва.рф/кремль/президенты.html?💔=€#путин");
    CHECK(uri.host() == "xn--80adxhks.xn--p1ai");
    CHECK(uri.path() == "/кремль/президенты.html");
    CHECK(uri.query_string() == "💔=€");
    CHECK(uri.fragment() == "путин");
}

TEST_CASE("utf8 problems", "[iri]") {
    SECTION("incomplete ut8-sequences") {
        CHECK( parse("http://\xD0").scheme()         == "");
        CHECK( parse("http://\xE2\x82").scheme()     == "");
        CHECK( parse("http://\xF0\x9F\x92").scheme() == "");
    }
    SECTION("malformed utf8") {
        CHECK( parse("http://\xFF\xFF\xFF\xFF").scheme() == "");
    }
}

TEST_CASE("uri/to-string", "[iri]") {
    CHECK(parse("http://москва.рф").to_string() == "http://xn--80adxhks.xn--p1ai");
    // specially crafted value to mimic "."-symbol
    CHECK(parse("http://خ.бел").to_string() == "http://xn--tgb.xn--90ais");
}
