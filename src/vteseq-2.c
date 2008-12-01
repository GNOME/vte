/* ANSI-C code produced by gperf version 3.0.3 */
/* Command-line: gperf -m 100 --no-strlen vteseq-2.gperf  */
/* Computed positions: -k'1-2' */

#if !((' ' == 32) && ('!' == 33) && ('"' == 34) && ('#' == 35) \
      && ('%' == 37) && ('&' == 38) && ('\'' == 39) && ('(' == 40) \
      && (')' == 41) && ('*' == 42) && ('+' == 43) && (',' == 44) \
      && ('-' == 45) && ('.' == 46) && ('/' == 47) && ('0' == 48) \
      && ('1' == 49) && ('2' == 50) && ('3' == 51) && ('4' == 52) \
      && ('5' == 53) && ('6' == 54) && ('7' == 55) && ('8' == 56) \
      && ('9' == 57) && (':' == 58) && (';' == 59) && ('<' == 60) \
      && ('=' == 61) && ('>' == 62) && ('?' == 63) && ('A' == 65) \
      && ('B' == 66) && ('C' == 67) && ('D' == 68) && ('E' == 69) \
      && ('F' == 70) && ('G' == 71) && ('H' == 72) && ('I' == 73) \
      && ('J' == 74) && ('K' == 75) && ('L' == 76) && ('M' == 77) \
      && ('N' == 78) && ('O' == 79) && ('P' == 80) && ('Q' == 81) \
      && ('R' == 82) && ('S' == 83) && ('T' == 84) && ('U' == 85) \
      && ('V' == 86) && ('W' == 87) && ('X' == 88) && ('Y' == 89) \
      && ('Z' == 90) && ('[' == 91) && ('\\' == 92) && (']' == 93) \
      && ('^' == 94) && ('_' == 95) && ('a' == 97) && ('b' == 98) \
      && ('c' == 99) && ('d' == 100) && ('e' == 101) && ('f' == 102) \
      && ('g' == 103) && ('h' == 104) && ('i' == 105) && ('j' == 106) \
      && ('k' == 107) && ('l' == 108) && ('m' == 109) && ('n' == 110) \
      && ('o' == 111) && ('p' == 112) && ('q' == 113) && ('r' == 114) \
      && ('s' == 115) && ('t' == 116) && ('u' == 117) && ('v' == 118) \
      && ('w' == 119) && ('x' == 120) && ('y' == 121) && ('z' == 122) \
      && ('{' == 123) && ('|' == 124) && ('}' == 125) && ('~' == 126))
/* The character set is not based on ISO-646.  */
#error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 14 "vteseq-2.gperf"
struct vteseq_2_struct {
	char seq[3];
	VteTerminalSequenceHandler handler;
};
#include <string.h>
/* maximum key range = 77, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
/*ARGSUSED*/
static unsigned int
vteseq_2_hash (register const char *str, register unsigned int len)
{
  static const unsigned char asso_values[] =
    {
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 39, 77, 77, 32, 77,
      39, 77, 37, 33, 34, 36, 34, 77, 33, 77,
      77, 33, 33, 31, 32, 30, 77, 30, 40, 77,
      77, 77, 77, 77, 77, 77, 77, 50, 45,  1,
      27, 32, 41, 29,  7, 21,  5, 15, 49,  0,
      25, 77, 12, 13, 24, 11,  8,  2, 18, 37,
       9, 31, 77,  2, 11, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77, 77, 77, 77, 77, 77, 77, 77, 77, 77,
      77
    };
  return asso_values[(unsigned char)str[1]+5] + asso_values[(unsigned char)str[0]];
}

#ifdef __GNUC__
__inline
#ifdef __GNUC_STDC_INLINE__
__attribute__ ((__gnu_inline__))
#endif
#endif
const struct vteseq_2_struct *
vteseq_2_lookup (register const char *str, register unsigned int len)
{
  enum
    {
      TOTAL_KEYWORDS = 74,
      MIN_WORD_LENGTH = 2,
      MAX_WORD_LENGTH = 2,
      MIN_HASH_VALUE = 0,
      MAX_HASH_VALUE = 76
    };

  static const struct vteseq_2_struct wordlist[] =
    {
#line 247 "vteseq-2.gperf"
      {"mh", VTE_SEQUENCE_HANDLER(vte_sequence_handler_mh)},
#line 166 "vteseq-2.gperf"
      {"ch", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ch)},
#line 251 "vteseq-2.gperf"
      {"mp", VTE_SEQUENCE_HANDLER(vte_sequence_handler_mp)},
#line 172 "vteseq-2.gperf"
      {"cv", VTE_SEQUENCE_HANDLER(vte_sequence_handler_cv)},
#line 285 "vteseq-2.gperf"
      {"up", VTE_SEQUENCE_HANDLER(vte_sequence_handler_up)},
#line 246 "vteseq-2.gperf"
      {"me", VTE_SEQUENCE_HANDLER(vte_sequence_handler_me)},
#line 165 "vteseq-2.gperf"
      {"ce", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ce)},
#line 284 "vteseq-2.gperf"
      {"ue", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ue)},
#line 163 "vteseq-2.gperf"
      {"cc", VTE_SEQUENCE_HANDLER(vte_sequence_handler_noop)},
#line 283 "vteseq-2.gperf"
      {"uc", VTE_SEQUENCE_HANDLER(vte_sequence_handler_uc)},
#line 170 "vteseq-2.gperf"
      {"cs", VTE_SEQUENCE_HANDLER(vte_sequence_handler_cs)},
#line 286 "vteseq-2.gperf"
      {"us", VTE_SEQUENCE_HANDLER(vte_sequence_handler_us)},
#line 248 "vteseq-2.gperf"
      {"mk", VTE_SEQUENCE_HANDLER(vte_sequence_handler_mk)},
#line 280 "vteseq-2.gperf"
      {"te", VTE_SEQUENCE_HANDLER(vte_sequence_handler_noop)},
#line 167 "vteseq-2.gperf"
      {"cl", VTE_SEQUENCE_HANDLER(vte_sequence_handler_cl)},
#line 185 "vteseq-2.gperf"
      {"ho", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ho)},
#line 274 "vteseq-2.gperf"
      {"se", VTE_SEQUENCE_HANDLER(vte_sequence_handler_se)},
#line 282 "vteseq-2.gperf"
      {"ts", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ts)},
#line 273 "vteseq-2.gperf"
      {"sc", VTE_SEQUENCE_HANDLER(vte_sequence_handler_sc)},
#line 276 "vteseq-2.gperf"
      {"so", VTE_SEQUENCE_HANDLER(vte_sequence_handler_so)},
#line 224 "vteseq-2.gperf"
      {"ke", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ke)},
#line 245 "vteseq-2.gperf"
      {"md", VTE_SEQUENCE_HANDLER(vte_sequence_handler_md)},
#line 164 "vteseq-2.gperf"
      {"cd", VTE_SEQUENCE_HANDLER(vte_sequence_handler_cd)},
#line 288 "vteseq-2.gperf"
      {"ve", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ve)},
#line 228 "vteseq-2.gperf"
      {"ks", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ks)},
#line 168 "vteseq-2.gperf"
      {"cm", VTE_SEQUENCE_HANDLER(vte_sequence_handler_cm)},
#line 275 "vteseq-2.gperf"
      {"sf", VTE_SEQUENCE_HANDLER(vte_sequence_handler_sf)},
#line 290 "vteseq-2.gperf"
      {"vs", VTE_SEQUENCE_HANDLER(vte_sequence_handler_vs)},
#line 190 "vteseq-2.gperf"
      {"ic", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ic)},
#line 244 "vteseq-2.gperf"
      {"mb", VTE_SEQUENCE_HANDLER(vte_sequence_handler_mb)},
#line 162 "vteseq-2.gperf"
      {"cb", VTE_SEQUENCE_HANDLER(vte_sequence_handler_cb)},
#line 268 "vteseq-2.gperf"
      {"rc", VTE_SEQUENCE_HANDLER(vte_sequence_handler_rc)},
#line 171 "vteseq-2.gperf"
      {"ct", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ct)},
#line 281 "vteseq-2.gperf"
      {"ti", VTE_SEQUENCE_HANDLER(vte_sequence_handler_noop)},
#line 173 "vteseq-2.gperf"
      {"dc", VTE_SEQUENCE_HANDLER(vte_sequence_handler_dc)},
#line 176 "vteseq-2.gperf"
      {"do", VTE_SEQUENCE_HANDLER(vte_sequence_handler_do)},
#line 254 "vteseq-2.gperf"
      {"nw", VTE_SEQUENCE_HANDLER(vte_sequence_handler_nw)},
#line 252 "vteseq-2.gperf"
      {"mr", VTE_SEQUENCE_HANDLER(vte_sequence_handler_mr)},
#line 169 "vteseq-2.gperf"
      {"cr", VTE_SEQUENCE_HANDLER(vte_sequence_handler_cr)},
#line 179 "vteseq-2.gperf"
      {"ec", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ec)},
#line 174 "vteseq-2.gperf"
      {"dl", VTE_SEQUENCE_HANDLER(vte_sequence_handler_dl)},
#line 161 "vteseq-2.gperf"
      {"cS", VTE_SEQUENCE_HANDLER(vte_sequence_handler_cS)},
#line 278 "vteseq-2.gperf"
      {"st", VTE_SEQUENCE_HANDLER(vte_sequence_handler_st)},
#line 289 "vteseq-2.gperf"
      {"vi", VTE_SEQUENCE_HANDLER(vte_sequence_handler_vi)},
#line 222 "vteseq-2.gperf"
      {"kb", VTE_SEQUENCE_HANDLER(vte_sequence_handler_kb)},
#line 192 "vteseq-2.gperf"
      {"im", VTE_SEQUENCE_HANDLER(vte_sequence_handler_im)},
#line 253 "vteseq-2.gperf"
      {"nd", VTE_SEQUENCE_HANDLER(vte_sequence_handler_nd)},
#line 287 "vteseq-2.gperf"
      {"vb", VTE_SEQUENCE_HANDLER(vte_sequence_handler_vb)},
#line 277 "vteseq-2.gperf"
      {"sr", VTE_SEQUENCE_HANDLER(vte_sequence_handler_sr)},
#line 279 "vteseq-2.gperf"
      {"ta", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ta)},
#line 183 "vteseq-2.gperf"
      {"fs", VTE_SEQUENCE_HANDLER(vte_sequence_handler_fs)},
#line 175 "vteseq-2.gperf"
      {"dm", VTE_SEQUENCE_HANDLER(vte_sequence_handler_noop)},
#line 158 "vteseq-2.gperf"
      {"bc", VTE_SEQUENCE_HANDLER(vte_sequence_handler_le)},
#line 180 "vteseq-2.gperf"
      {"ed", VTE_SEQUENCE_HANDLER(vte_sequence_handler_noop)},
#line 242 "vteseq-2.gperf"
      {"le", VTE_SEQUENCE_HANDLER(vte_sequence_handler_le)},
#line 155 "vteseq-2.gperf"
      {"ae", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ae)},
#line 182 "vteseq-2.gperf"
      {"ff", VTE_SEQUENCE_HANDLER(vte_sequence_handler_noop)},
#line 181 "vteseq-2.gperf"
      {"ei", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ei)},
#line 159 "vteseq-2.gperf"
      {"bl", VTE_SEQUENCE_HANDLER(vte_sequence_handler_bl)},
#line 157 "vteseq-2.gperf"
      {"as", VTE_SEQUENCE_HANDLER(vte_sequence_handler_as)},
#line 153 "vteseq-2.gperf"
      {"UP", VTE_SEQUENCE_HANDLER(vte_sequence_handler_UP)},
#line 151 "vteseq-2.gperf"
      {"SR", VTE_SEQUENCE_HANDLER(vte_sequence_handler_SR)},
#line 243 "vteseq-2.gperf"
      {"ll", VTE_SEQUENCE_HANDLER(vte_sequence_handler_ll)},
#line 156 "vteseq-2.gperf"
      {"al", VTE_SEQUENCE_HANDLER(vte_sequence_handler_al)},
#line 79 "vteseq-2.gperf"
      {"DO", VTE_SEQUENCE_HANDLER(vte_sequence_handler_DO)},
#line 78 "vteseq-2.gperf"
      {"DL", VTE_SEQUENCE_HANDLER(vte_sequence_handler_DL)},
#line 147 "vteseq-2.gperf"
      {"RI", VTE_SEQUENCE_HANDLER(vte_sequence_handler_RI)},
#line 150 "vteseq-2.gperf"
      {"SF", VTE_SEQUENCE_HANDLER(vte_sequence_handler_SF)},
#line 139 "vteseq-2.gperf"
      {"LE", VTE_SEQUENCE_HANDLER(vte_sequence_handler_LE)},
#line 77 "vteseq-2.gperf"
      {"DC", VTE_SEQUENCE_HANDLER(vte_sequence_handler_DC)},
#line 133 "vteseq-2.gperf"
      {"IC", VTE_SEQUENCE_HANDLER(vte_sequence_handler_IC)},
#line 178 "vteseq-2.gperf"
      {"eA", VTE_SEQUENCE_HANDLER(vte_sequence_handler_eA)},
#line 76 "vteseq-2.gperf"
      {"AL", VTE_SEQUENCE_HANDLER(vte_sequence_handler_AL)},
      {""}, {""}, {""},
#line 160 "vteseq-2.gperf"
      {"bt", VTE_SEQUENCE_HANDLER(vte_sequence_handler_bt)}
    };

  if (len <= MAX_WORD_LENGTH && len >= MIN_WORD_LENGTH)
    {
      register int key = vteseq_2_hash (str, len);

      if (key <= MAX_HASH_VALUE && key >= 0)
        {
          register const char *s = wordlist[key].seq;

          if (*str == *s && !strncmp (str + 1, s + 1, len - 1) && s[len] == '\0')
            return &wordlist[key];
        }
    }
  return 0;
}
