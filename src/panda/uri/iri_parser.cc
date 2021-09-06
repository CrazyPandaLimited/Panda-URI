#include "URI.h"
#include "utf8.h"
#include <limits>

namespace panda { namespace uri {

struct Result {
    size_t consumed;
    bool success;
};

struct UriData {
    string    _scheme;
    string    _user_info;
    string    _host;
    string    _path;
    string    _qstr;
    string    _fragment;
    uint16_t  _port = 0;
};

enum class Tag {
    scheme, user_info, host, port, path, query, fragment,
};

template<Tag t>
struct Consumer;

namespace atom {

struct Range {
    uint32_t min;
    uint32_t max;
};

template<uint32_t Min, uint32_t Max>
struct CRange {
    inline static Result consume(UriData&, string_view in) {
        auto u = unpack(in);
        if (u.consumed && u.code >= Min && u.code <= Max) { return {u.consumed, true}; };
        return {0, false};
    }
};

template<char c>
struct SRange {
    inline static Result consume(UriData&, string_view in) {
        auto u = unpack(in);
        if (u.consumed && u.code == c) { return {u.consumed, true}; };
        return {0, false};
    }
};

template<typename... Rs>
struct JRange;

template<typename R>
struct JRange<R> {
    inline static Result consume(UriData& data, string_view in) {
        return R::consume(data, in);
    }
};
template<typename T, typename... Ts>
struct JRange<T, Ts...> {
    inline static Result consume(UriData& data, string_view in) {
        Result r = JRange<T>::consume(data, in);
        if (r.success) { return r; }
        return JRange<Ts...>::consume(data, in);
    }
};

}



namespace op {

/* range */
template<typename T, uint32_t min = 1, uint32_t max =  std::numeric_limits<uint32_t>::max()>
struct R {
    inline static Result consume(UriData& data, string_view in) {
        uint32_t i = 0;
        size_t c = 0;
        // printf("in = %s\n", string(in).c_str());

        while (i < max) {
            Result r = T::consume(data, in);
            if (!r.success) { break; }
            ++i;
            c += r.consumed;
            in = in.substr(r.consumed);
            if (i >= max) { break; }
        }

        return {c , i >= min };
    }
};

/* optional */
template<typename T>
using O = R<T, 0, 1>;

/* sequence */
template<typename... Ts> struct S;
template<typename T> struct S<T> {
    static inline Result consume(UriData& data, string_view in) {
        return T::consume(data, in);
    }
};

template<typename T, typename... Ts> struct S<T, Ts...> {
    static inline Result consume(UriData& data, string_view in) {
        Result r1 = T::consume(data, in);
        if (!r1.success) { return r1; };
        Result r2 = S<Ts...>::consume(data, in.substr(r1.consumed));
        if (!r2.success) { return {r1.consumed, r2.success}; }
        return {r1.consumed + r2.consumed, true};
    }
};


/* union */
template<typename... Ts> struct U;
template<typename T> struct U<T> {
    static inline Result consume(UriData& data, string_view in) {
        return T::consume(data, in);
    }
};

template<typename T, typename... Ts> struct U<T, Ts...> {
    static inline Result consume(UriData& data, string_view in) {
        Result r = T::consume(data, in);
        if (r.success) { return r; };
        return U<Ts...>::consume(data, in);
    }
};

/* consumer */
template<typename T, Tag t> struct C{
    static inline Result consume(UriData& data, string_view in) {
        Result r = T::consume(data, in);
        if (r.success) {
            Consumer<t>::consume(data, r, in);
        }
        return r;
    }
};


};

namespace symbol {
    using Alpha = atom::JRange<atom::CRange<'a', 'z'>, atom::CRange<'A', 'Z'>>;
    using Digit = atom::CRange<'0', '9'>;
    using XDigit = op::U< atom::CRange<'0', '9'>,  atom::CRange<'a', 'f'>,  atom::CRange<'A', 'F'>>;
};

template<>
struct Consumer<Tag::scheme> {
    static void consume(UriData& data, Result r, string_view in) {
        data._scheme = in.substr(0, r.consumed);
    }
};

template<>
struct Consumer<Tag::port> {
    static void consume(UriData& data, Result r, string_view in) {
        auto port = in.substr(0, r.consumed);
        uint16_t p = 0;
        for(size_t i = 0; i < port.size(); ++i) {
            p *= 10;
            p += port[i] - '0';
        }
        data._port = p;
    }
};

template<>
struct Consumer<Tag::host> {
    static void consume(UriData& data, Result r, string_view in) {
        data._host = in.substr(0, r.consumed);
    }
};

template<>
struct Consumer<Tag::user_info> {
    static void consume(UriData& data, Result r, string_view in) {
        data._user_info = in.substr(0, r.consumed - 1); // exclude last char "@"
    }
};

template<>
struct Consumer<Tag::path> {
    static void consume(UriData& data, Result r, string_view in) {
        data._path = in.substr(0, r.consumed);
    }
};

template<>
struct Consumer<Tag::fragment> {
    static void consume(UriData& data, Result r, string_view in) {
        data._fragment = in.substr(0, r.consumed);
    }
};

template<>
struct Consumer<Tag::query> {
    static void consume(UriData& data, Result r, string_view in) {
        data._qstr = in.substr(0, r.consumed);
    }
};


namespace grammar {
    using scheme = op::C<op::S<
        op::R<symbol::Alpha>,
        op::R<
            op::U<
                symbol::Alpha,
                symbol::Digit,
                atom::SRange<'+'>,
                atom::SRange<'-'>,
                atom::SRange<'.'>
            >,
        0>
    >, Tag::scheme>;

    using dec_octet = op::U<
        symbol::Digit,
        op::S< atom::CRange<'1', '9'>, symbol::Digit >,
        op::S< atom::SRange<'1'>, op::R< symbol::Digit, 2, 2 > >,
        op::S< atom::SRange<'2'>, atom::CRange<'0', '4'>, symbol::Digit>,
        op::S< atom::SRange<'2'>, atom::SRange<'2'>, atom::CRange<'0', '5'>>
    >;
    using IPv4address = op::S<dec_octet, atom::SRange<'.'>, dec_octet, atom::SRange<'.'>, dec_octet, atom::SRange<'.'>, dec_octet>;

    using h16 = op::R<symbol::XDigit, 1, 4>;
    using ls32 = op::U< op::S<h16, atom::SRange<':'>, h16>, IPv4address>;
    using pct_encoded = op::S<atom::SRange<'%'>, symbol::XDigit, symbol::XDigit>;
    using unreserved = op::U<symbol::Alpha, symbol::Digit, atom::SRange<'-'>, atom::SRange<'.'>, atom::SRange<'_'>, atom::SRange<'~'>>;
    using sub_delims = op::U<atom::SRange<'!'>,  atom::SRange<'$'>, atom::SRange<'&'>, atom::SRange<'\''>, atom::SRange<'('>, atom::SRange<')'>,
        atom::SRange<'*'>, atom::SRange<'+'>, atom::SRange<','>, atom::SRange<';'>, atom::SRange<'='>
    >;
    using ucschar = op::U<
        atom::CRange<0xA0, 0xD7FF>, atom::CRange<0xF900, 0xFDCF>, atom::CRange<0xFDF0, 0xFFEF>,
        atom::CRange<0x10000, 0x1FFFD>, atom::CRange<0x20000, 0x2FFFD>, atom::CRange<0x30000, 0x3FFFD>,
        atom::CRange<0x40000, 0x4FFFD>, atom::CRange<0x50000, 0x5FFFD>, atom::CRange<0x60000, 0x6FFFD>,
        atom::CRange<0x70000, 0x7FFFD>, atom::CRange<0x80000, 0x8FFFD>, atom::CRange<0x90000, 0x9FFFD>,
        atom::CRange<0xA0000, 0xAFFFD>, atom::CRange<0xB0000, 0xBFFFD>, atom::CRange<0xC0000, 0xCFFFD>,
        atom::CRange<0xD0000, 0xDFFFD>, atom::CRange<0xE0000, 0xEFFFD>
    >;
    using iprivate  = op::U<atom::CRange<0xE000, 0xF8FF>, atom::CRange<0xF0000, 0xFFFFD>,atom::CRange<0x100000, 0x10FFFD>>;
    using iunreserved = op::U<symbol::Alpha, symbol::Digit, atom::SRange<'-'>, atom::SRange<'.'>, atom::SRange<'_'>, atom::SRange<'~'>, ucschar>;
    using ipchar = op::U<iunreserved, pct_encoded, sub_delims, atom::SRange<':'>, atom::SRange<'@'>>;
    using IPv6address = op::U<
                                                                                                         op::S<op::R< op::S<h16, atom::SRange<':'> >,6,6>, ls32>,
                                                                          op::S<op::R<atom::SRange<':'>,2,2>,  op::R< op::S<h16, atom::SRange<':'> >,5,5>, ls32>,
                                                              op::S<op::O<h16>, op::R<atom::SRange<':'>,2,2>,  op::R< op::S<h16, atom::SRange<':'> >,4,4>, ls32>,
        op::S<op::O< op::S< op::R< op::S<h16, atom::SRange<':'>>,0, 1>, h16 >>, op::R<atom::SRange<':'>,2,2>,  op::R< op::S<h16, atom::SRange<':'> >,3,3>, ls32>,
        op::S<op::O< op::S< op::R< op::S<h16, atom::SRange<':'>>,0, 2>, h16 >>, op::R<atom::SRange<':'>,2,2>,  op::R< op::S<h16, atom::SRange<':'> >,2,2>, ls32>,
        op::S<op::O< op::S< op::R< op::S<h16, atom::SRange<':'>>,0, 3>, h16 >>, op::R<atom::SRange<':'>,2,2>,  op::R< op::S<h16, atom::SRange<':'> >,1,1>, ls32>,
        op::S<op::O< op::S< op::R< op::S<h16, atom::SRange<':'>>,0, 4>, h16 >>, op::R<atom::SRange<':'>,2,2>,                                              ls32>,
        op::S<op::O< op::S< op::R< op::S<h16, atom::SRange<':'>>,0, 5>, h16 >>, op::R<atom::SRange<':'>,2,2>,                                              h16>,
        op::S<op::O< op::S< op::R< op::S<h16, atom::SRange<':'>>,0, 6>, h16 >>, op::R<atom::SRange<':'>,2,2>                                                   >
    >;
    using IPvFuture = op::S<
        atom::SRange<'v'>, op::R<symbol::XDigit, 1>, atom::SRange<'.'>, op::R<
            op::U<unreserved, sub_delims, atom::SRange<':'>>
        , 1>
    >;
    using iuserinfo = op::C<
        op::S<
            op::R<op::U<iunreserved, pct_encoded, sub_delims, atom::SRange<':'>>>,
            atom::SRange<'@'>
        >,
        Tag::user_info
    >;
    using ireg_name = op::R< op::U<iunreserved, pct_encoded, sub_delims>, 0 >;
    using IP_literal = op::S<
        atom::SRange<'['>, op::U<IPv6address, IPvFuture>,  atom::SRange<']'>
    >;
    using ihost = op::C< op::U< IP_literal, IPv4address, ireg_name >, Tag::host >;
    using port = op::C< op::R<symbol::Digit>, Tag::port >;
    using iauthority =op::S<
        op::O< iuserinfo >,
        ihost,
        op::O< op::S< atom::SRange<':'>, port >>
    >;
    using ichar = op::U<iunreserved, pct_encoded, sub_delims, atom::SRange<':'>, atom::SRange<'@'>>;
    using isegment = op::R<ichar, 0>;
    using isegment_nz = op::R<ichar, 1>;
    using isegment_nz_nc = op::R<op::U<iunreserved, pct_encoded, sub_delims, atom::SRange<'@'>>, 1>;
    using ipath_abempty = op::R<op::S<atom::SRange<'/'>, isegment>, 0>;
    using ipath_absolute = op::S<atom::SRange<'/'>, op::O<op::S<isegment_nz, op::R<op::S<atom::SRange<'/'>, isegment>,0>>>>;
    using ipath_noscheme = op::C<op::S<isegment_nz_nc, op::O<op::S<isegment_nz, op::R<op::S<atom::SRange<'/'>, isegment>,0>>>>, Tag::path>;
    using ipath_rootless = op::S<isegment_nz, op::O<op::S<isegment_nz, op::R<op::S<atom::SRange<'/'>, isegment>,0>>>>;
    using ipath_empty = op::R<ichar, 0, 0>;
    using ihier_part = op::S<
        atom::SRange<'/'>, atom::SRange<'/'>,  iauthority,
        op::C<op::U<ipath_absolute, ipath_rootless, ipath_empty>, Tag::path>
    >;
    using iquery = op::C<op::R<op::U<ipchar, iprivate, atom::SRange<'/'>, atom::SRange<'?'>>, 0>, Tag::query>;
    using ifragment = op::C<op::R<op::U<ipchar, atom::SRange<'/'>, atom::SRange<'?'>>, 0>, Tag::fragment>;
    using irelative_part = op::S<atom::SRange<'/'>, atom::SRange<'/'>, iauthority,
        op::C<op::U< ipath_abempty, ipath_absolute, ipath_noscheme, ipath_empty >, Tag::path>
    >;
    using irelative_ref = op::S<irelative_part, op::O<op::S<atom::SRange<'?'>, iquery>>, op::O<op::S<atom::SRange<'#'>, ifragment>>>;
    using absolute_IRI = op::S<
        scheme,  atom::SRange<':'>, ihier_part,
        op::O< op::S< atom::SRange<'?'>,  iquery > >
    >;
    using IRI = op::S<
        scheme,  atom::SRange<':'>, ihier_part,
        op::O< op::S< atom::SRange<'?'>,  iquery>>,
        op::O< op::S< atom::SRange<'#'>,  ifragment>>
    >;
    using IRI_reference = op::U<IRI, irelative_ref>;

}

bool URI::_parse_iri (const string& in) {
    UriData data;
    //auto r = grammar::scheme::consume(data, in);
    auto r = grammar::IRI_reference::consume(data, in);
    if (r.success && r.consumed == in.size()) {
        _scheme    = data._scheme;
        _user_info = data._user_info;
        _host      = data._host;
        _port      = data._port;
        _path      = data._path;
        _qstr      = data._qstr;
        _fragment  = data._fragment;
        return r.success;
    }
    return false;
}


}}
