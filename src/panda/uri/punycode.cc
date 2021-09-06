#include "punycode.h"
#include "utf8.h"
#include <cstring>
#include <vector>

namespace panda { namespace uri {

/*** Bootstring parameters for Punycode ***/

enum { base = 36, tmin = 1, tmax = 26, skew = 38, damp = 700,
       initial_bias = 72, initial_n = 0x80, delimiter = 0x2D };

/* basic(cp) tests whether cp is a basic code point: */
#define basic(cp) ((punycode_uint)(cp) < 0x80)

/* delim(cp) tests whether cp is a delimiter: */
#define delim(cp) ((cp) == delimiter)

/* decode_digit(cp) returns the numeric value of a basic code */
/* point (for use in representing integers) in the range 0 to */
/* base-1, or base if cp is does not represent a value.       */

static punycode_uint decode_digit(punycode_uint cp)
{
  return  cp - 48 < 10 ? cp - 22 :  cp - 65 < 26 ? cp - 65 :
          cp - 97 < 26 ? cp - 97 : (punycode_uint) base;
}

static char encode_digit(punycode_uint d)
{
  static const constexpr int flag = 0;
  return d + 22 + 75 * (d < 26) - ((flag != 0) << 5);
  //return d + 22 + 75 * (d < 26);
  /*  0..25 map to ASCII a..z or A..Z */
  /* 26..35 map to ASCII 0..9         */
}

/* flagged(bcp) tests whether a basic code point is flagged */
/* (uppercase).  The behavior is undefined if bcp is not a  */
/* basic code point.                                        */

#define flagged(bcp) ((punycode_uint)(bcp) - 65 < 26)

/* encode_basic(bcp,flag) forces a basic code point to lowercase */
/* if flag is zero, uppercase if flag is nonzero, and returns    */
/* the resulting code point.  The code point is unchanged if it  */
/* is caseless.  The behavior is undefined if bcp is not a basic */
/* code point.                                                   */

static char encode_basic(punycode_uint bcp)
{
  static const constexpr int flag = 0;
  bcp -= (bcp - 97 < 26) << 5;
  return bcp + ((!flag && (bcp - 65 < 26)) << 5);
}

/*** Platform-specific constants ***/

/* maxint is the maximum value of a punycode_uint variable: */
static const punycode_uint maxint = -1;
/* Because maxint is unsigned, -1 becomes the maximum value. */

/*** Bias adaptation function ***/

static punycode_uint adapt(
  punycode_uint delta, punycode_uint numpoints, int firsttime )
{
  punycode_uint k;

  delta = firsttime ? delta / damp : delta >> 1;
  /* delta >> 1 is a faster way of doing delta / 2 */
  delta += delta / numpoints;

  for (k = 0;  delta > ((base - tmin) * tmax) / 2;  k += base) {
    delta /= base - tmin;
  }

  return k + (base - tmin + 1) * delta / (delta + skew);
}

using Container = std::vector<punycode_uint>;

bool decode(const string_view& raw_input, Container& out) {
    string_view in = raw_input;
    while(!in.empty()) {
        auto code = unpack(in);
        if (!code.consumed) return false;
        out.push_back(code.code);
        in = in.substr(code.consumed);
    }
    return true;
}

/*** Main encode function ***/
punycode_status punycode_encode(const string_view& raw_input, punycode_uint *output_length, char output[]) {
  punycode_uint n, delta, h, b, out, max_out, bias, j, m, q, k, t;
  Container input;
  input.reserve(raw_input.size());
  if (!decode(raw_input, input)) return punycode_bad_input;
  punycode_uint input_length = input.size();

  /* Initialize the state: */

  n = initial_n;
  delta = out = 0;
  max_out = *output_length;
  bias = initial_bias;

  /* Handle the basic code points: */
  for (j = 0;  j < input_length; ++j ) {
    if (basic(input[j])) {
        output[out++] = encode_basic(input[j]);
    }
  }

  h = b = out;

  /* h is the number of code points that have been handled, b is the  */
  /* number of basic code points, and out is the number of characters */
  /* that have been output.                                           */

  if (b > 0) output[out++] = delimiter;

  /* Main encoding loop: */

  while (h < input_length) {
    /* All non-basic code points < n have been     */
    /* handled already.  Find the next larger one: */

    for (m = maxint, j = 0;  j < input_length;  ++j) {
      /* if (basic(input[j])) continue; */
      /* (not needed for Punycode) */
      if (input[j] >= n && input[j] < m) m = input[j];
    }

    /* Increase delta enough to advance the decoder's    */
    /* <n,i> state to <m,0>, but guard against overflow: */

    if (m - n > (maxint - delta) / (h + 1)) return punycode_overflow;
    delta += (m - n) * (h + 1);
    n = m;

    for (j = 0;  j < input_length;  ++j) {
      /* Punycode does not need to check whether input[j] is basic: */
      if (input[j] < n /* || basic(input[j]) */ ) {
        if (++delta == 0) return punycode_overflow;
      }

      if (input[j] == n) {
        /* Represent delta as a generalized variable-length integer: */

        for (q = delta, k = base;  ;  k += base) {
          if (out >= max_out) return punycode_big_output;
          t = k <= bias /* + tmin */ ? (punycode_uint) tmin :     /* +tmin not needed */
              k >= bias + (punycode_uint) tmax ? (punycode_uint) tmax : k - bias;
          if (q < t) break;
          output[out++] = encode_digit(t + (q - t) % (base - t));
          q = (q - t) / (base - t);
        }

        output[out++] = encode_digit(q);
        bias = adapt(delta, h + 1, h == b);
        delta = 0;
        ++h;
      }
    }

    ++delta, ++n;
  }
  *output_length = out;
  return punycode_success;
}

/*** Main decode function ***/

punycode_status punycode_decode(const string_view& input, punycode_uint *output_length, char raw_output[]) {

  punycode_uint n, out, i, max_out, bias,
                 b, j, in, oldi, w, k, digit, t;

  punycode_uint input_length = input.length();
  punycode_uint output[input_length];
  /* Initialize the state: */

  n = initial_n;
  out = i = 0;
  max_out = *output_length;
  bias = initial_bias;

  /* Handle the basic code points:  Let b be the number of input code */
  /* points before the last delimiter, or 0 if there is none, then    */
  /* copy the first b code points to the output.                      */

  for (b = j = 0;  j < input_length;  ++j) if (delim(input[j])) b = j;
  if (b > max_out) return punycode_big_output;

  for (j = 0;  j < b;  ++j) {
    if (!basic(input[j])) return punycode_bad_input;
    output[out++] = input[j];
  }

  /* Main decoding loop:  Start just after the last delimiter if any  */
  /* basic code points were copied; start at the beginning otherwise. */

  for (in = b > 0 ? b + 1 : 0;  in < input_length;  ++out) {

    /* in is the index of the next character to be consumed, and */
    /* out is the number of code points in the output array.     */

    /* Decode a generalized variable-length integer into delta,  */
    /* which gets added to i.  The overflow checking is easier   */
    /* if we increase i as we go, then subtract off its starting */
    /* value at the end to obtain delta.                         */

    for (oldi = i, w = 1, k = base;  ;  k += base) {
      if (in >= input_length) return punycode_bad_input;
      digit = decode_digit(input[in++]);
      if (digit >= base) return punycode_bad_input;
      if (digit > (maxint - i) / w) return punycode_overflow;
      i += digit * w;
      t = k <= bias /* + tmin */ ? (punycode_uint) tmin :     /* +tmin not needed */
          k >= bias + (punycode_uint)tmax ? (punycode_uint)tmax : k - bias;
      if (digit < t) break;
      if (w > maxint / (base - t)) return punycode_overflow;
      w *= (base - t);
    }

    bias = adapt(i - oldi, out + 1, oldi == 0);

    /* i was supposed to wrap around from out+1 to 0,   */
    /* incrementing n each time, so we'll fix that now: */

    if (i / (out + 1) > maxint - n) return punycode_overflow;
    n += i / (out + 1);
    i %= (out + 1);

    /* Insert n at position i of the output: */
    if (out >= max_out) return punycode_big_output;

    /* not needed for Punycode: */
    /* if (decode_digit(n) <= base) return punycode_invalid_input; */

    std::memmove(output + i + 1, output + i, (out - i) * sizeof *output);
    output[i++] = n;
  }

  i = 0;
  string_view out_view(raw_output, *output_length);
  for(j = 0; j < out; ++j) {
      auto code = output[j];
      auto r = pack(code, out_view);
      if (!r) { return punycode_bad_input; }
      out_view = out_view.substr(r);
      i += r;
  }

  *output_length = i;

  return punycode_success;
}

}}
