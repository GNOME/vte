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
/* maximum key range = 359, duplicates = 0 */

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
  static const unsigned short asso_values[] =
    {
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 168, 359, 122,
      359,  22,  32, 359, 157,  20,   3,  15,  29,  38,
       71,  66,  55,  43, 359, 213, 359, 359, 359, 359,
      359, 185, 211, 169, 209, 181, 197, 206,   7, 167,
      186, 164, 148, 205,   1,  56, 201,   4, 190,  11,
      177, 137, 130,  18,  16,   8,  47,   2, 359,   2,
      359, 359, 359, 133,  94,  67,  91,  82, 128,  12,
       99,  84,  95,  53,  79,  23, 175, 100,  96, 117,
      108,   0,  33,  28, 103,  23, 359, 359,  56,  72,
      111,  63,  59, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359,
      359, 359, 359, 359, 359, 359, 359, 359, 359, 359
    };
  return asso_values[(unsigned char)str[1]+6] + asso_values[(unsigned char)str[0]+14];
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
      TOTAL_KEYWORDS = 221,
      MIN_WORD_LENGTH = 2,
      MAX_WORD_LENGTH = 2,
      MIN_HASH_VALUE = 0,
      MAX_HASH_VALUE = 358
    };

  static const struct vteseq_2_struct wordlist[] =
    {
#line 228 "vteseq-2.gperf"
      {"ks", vte_sequence_handler_ks},
#line 216 "vteseq-2.gperf"
      {"kN", vte_sequence_handler_complain_key},
#line 102 "vteseq-2.gperf"
      {"FN", vte_sequence_handler_complain_key},
#line 197 "vteseq-2.gperf"
      {"k2", vte_sequence_handler_complain_key},
#line 81 "vteseq-2.gperf"
      {"F2", vte_sequence_handler_complain_key},
#line 105 "vteseq-2.gperf"
      {"FQ", vte_sequence_handler_complain_key},
#line 58 "vteseq-2.gperf"
      {"*2", vte_sequence_handler_complain_key},
#line 212 "vteseq-2.gperf"
      {"kH", vte_sequence_handler_complain_key},
#line 96 "vteseq-2.gperf"
      {"FH", vte_sequence_handler_complain_key},
#line 113 "vteseq-2.gperf"
      {"FY", vte_sequence_handler_complain_key},
#line 68 "vteseq-2.gperf"
      {"@2", vte_sequence_handler_complain_key},
#line 219 "vteseq-2.gperf"
      {"kS", vte_sequence_handler_complain_key},
#line 107 "vteseq-2.gperf"
      {"FS", vte_sequence_handler_complain_key},
#line 121 "vteseq-2.gperf"
      {"Fg", vte_sequence_handler_complain_key},
#line 135 "vteseq-2.gperf"
      {"K2", vte_sequence_handler_complain_key},
#line 198 "vteseq-2.gperf"
      {"k3", vte_sequence_handler_complain_key},
#line 82 "vteseq-2.gperf"
      {"F3", vte_sequence_handler_complain_key},
#line 112 "vteseq-2.gperf"
      {"FX", vte_sequence_handler_complain_key},
#line 59 "vteseq-2.gperf"
      {"*3", vte_sequence_handler_complain_key},
#line 111 "vteseq-2.gperf"
      {"FW", vte_sequence_handler_complain_key},
#line 196 "vteseq-2.gperf"
      {"k1", vte_sequence_handler_complain_key},
#line 80 "vteseq-2.gperf"
      {"F1", vte_sequence_handler_complain_key},
#line 69 "vteseq-2.gperf"
      {"@3", vte_sequence_handler_complain_key},
#line 57 "vteseq-2.gperf"
      {"*1", vte_sequence_handler_complain_key},
#line 127 "vteseq-2.gperf"
      {"Fm", vte_sequence_handler_complain_key},
#line 28 "vteseq-2.gperf"
      {"%2", vte_sequence_handler_complain_key},
#line 136 "vteseq-2.gperf"
      {"K3", vte_sequence_handler_complain_key},
#line 67 "vteseq-2.gperf"
      {"@1", vte_sequence_handler_complain_key},
#line 230 "vteseq-2.gperf"
      {"ku", vte_sequence_handler_complain_key},
#line 199 "vteseq-2.gperf"
      {"k4", vte_sequence_handler_complain_key},
#line 83 "vteseq-2.gperf"
      {"F4", vte_sequence_handler_complain_key},
#line 134 "vteseq-2.gperf"
      {"K1", vte_sequence_handler_complain_key},
#line 60 "vteseq-2.gperf"
      {"*4", vte_sequence_handler_complain_key},
#line 229 "vteseq-2.gperf"
      {"kt", vte_sequence_handler_complain_key},
#line 42 "vteseq-2.gperf"
      {"%g", vte_sequence_handler_complain_key},
#line 48 "vteseq-2.gperf"
      {"&2", vte_sequence_handler_complain_key},
#line 70 "vteseq-2.gperf"
      {"@4", vte_sequence_handler_complain_key},
#line 29 "vteseq-2.gperf"
      {"%3", vte_sequence_handler_complain_key},
#line 200 "vteseq-2.gperf"
      {"k5", vte_sequence_handler_complain_key},
#line 84 "vteseq-2.gperf"
      {"F5", vte_sequence_handler_complain_key},
#line 137 "vteseq-2.gperf"
      {"K4", vte_sequence_handler_complain_key},
#line 61 "vteseq-2.gperf"
      {"*5", vte_sequence_handler_complain_key},
#line 27 "vteseq-2.gperf"
      {"%1", vte_sequence_handler_complain_key},
#line 204 "vteseq-2.gperf"
      {"k9", vte_sequence_handler_complain_key},
#line 88 "vteseq-2.gperf"
      {"F9", vte_sequence_handler_complain_key},
#line 71 "vteseq-2.gperf"
      {"@5", vte_sequence_handler_complain_key},
#line 65 "vteseq-2.gperf"
      {"*9", vte_sequence_handler_complain_key},
#line 49 "vteseq-2.gperf"
      {"&3", vte_sequence_handler_complain_key},
#line 114 "vteseq-2.gperf"
      {"FZ", vte_sequence_handler_complain_key},
#line 138 "vteseq-2.gperf"
      {"K5", vte_sequence_handler_complain_key},
#line 75 "vteseq-2.gperf"
      {"@9", vte_sequence_handler_complain_key},
#line 30 "vteseq-2.gperf"
      {"%4", vte_sequence_handler_complain_key},
#line 47 "vteseq-2.gperf"
      {"&1", vte_sequence_handler_complain_key},
#line 170 "vteseq-2.gperf"
      {"cs", vte_sequence_handler_cs},
#line 125 "vteseq-2.gperf"
      {"Fk", vte_sequence_handler_complain_key},
#line 203 "vteseq-2.gperf"
      {"k8", vte_sequence_handler_complain_key},
#line 87 "vteseq-2.gperf"
      {"F8", vte_sequence_handler_complain_key},
#line 103 "vteseq-2.gperf"
      {"FO", vte_sequence_handler_complain_key},
#line 64 "vteseq-2.gperf"
      {"*8", vte_sequence_handler_complain_key},
#line 290 "vteseq-2.gperf"
      {"vs", vte_sequence_handler_vs},
#line 31 "vteseq-2.gperf"
      {"%5", vte_sequence_handler_complain_key},
#line 50 "vteseq-2.gperf"
      {"&4", vte_sequence_handler_complain_key},
#line 74 "vteseq-2.gperf"
      {"@8", vte_sequence_handler_complain_key},
#line 286 "vteseq-2.gperf"
      {"us", vte_sequence_handler_us},
#line 161 "vteseq-2.gperf"
      {"cS", vte_sequence_handler_cS},
#line 35 "vteseq-2.gperf"
      {"%9", vte_sequence_handler_complain_key},
#line 202 "vteseq-2.gperf"
      {"k7", vte_sequence_handler_complain_key},
#line 86 "vteseq-2.gperf"
      {"F7", vte_sequence_handler_complain_key},
#line 117 "vteseq-2.gperf"
      {"Fc", vte_sequence_handler_complain_key},
#line 63 "vteseq-2.gperf"
      {"*7", vte_sequence_handler_complain_key},
#line 51 "vteseq-2.gperf"
      {"&5", vte_sequence_handler_complain_key},
#line 201 "vteseq-2.gperf"
      {"k6", vte_sequence_handler_complain_key},
#line 85 "vteseq-2.gperf"
      {"F6", vte_sequence_handler_complain_key},
#line 73 "vteseq-2.gperf"
      {"@7", vte_sequence_handler_complain_key},
#line 62 "vteseq-2.gperf"
      {"*6", vte_sequence_handler_complain_key},
#line 55 "vteseq-2.gperf"
      {"&9", vte_sequence_handler_complain_key},
#line 168 "vteseq-2.gperf"
      {"cm", vte_sequence_handler_cm},
#line 34 "vteseq-2.gperf"
      {"%8", vte_sequence_handler_complain_key},
#line 72 "vteseq-2.gperf"
      {"@6", vte_sequence_handler_complain_key},
#line 226 "vteseq-2.gperf"
      {"kl", vte_sequence_handler_complain_key},
#line 126 "vteseq-2.gperf"
      {"Fl", vte_sequence_handler_complain_key},
#line 248 "vteseq-2.gperf"
      {"mk", vte_sequence_handler_mk},
#line 224 "vteseq-2.gperf"
      {"ke", vte_sequence_handler_ke},
#line 119 "vteseq-2.gperf"
      {"Fe", vte_sequence_handler_complain_key},
#line 157 "vteseq-2.gperf"
      {"as", vte_sequence_handler_as},
#line 123 "vteseq-2.gperf"
      {"Fi", vte_sequence_handler_complain_key},
#line 171 "vteseq-2.gperf"
      {"ct", vte_sequence_handler_ct},
#line 54 "vteseq-2.gperf"
      {"&8", vte_sequence_handler_complain_key},
#line 33 "vteseq-2.gperf"
      {"%7", vte_sequence_handler_complain_key},
#line 38 "vteseq-2.gperf"
      {"%c", vte_sequence_handler_complain_key},
#line 179 "vteseq-2.gperf"
      {"ec", vte_sequence_handler_ec},
#line 223 "vteseq-2.gperf"
      {"kd", vte_sequence_handler_complain_key},
#line 118 "vteseq-2.gperf"
      {"Fd", vte_sequence_handler_complain_key},
#line 32 "vteseq-2.gperf"
      {"%6", vte_sequence_handler_complain_key},
#line 222 "vteseq-2.gperf"
      {"kb", vte_sequence_handler_kb},
#line 116 "vteseq-2.gperf"
      {"Fb", vte_sequence_handler_complain_key},
#line 124 "vteseq-2.gperf"
      {"Fj", vte_sequence_handler_complain_key},
#line 130 "vteseq-2.gperf"
      {"Fp", vte_sequence_handler_complain_key},
#line 53 "vteseq-2.gperf"
      {"&7", vte_sequence_handler_complain_key},
#line 225 "vteseq-2.gperf"
      {"kh", vte_sequence_handler_complain_key},
#line 122 "vteseq-2.gperf"
      {"Fh", vte_sequence_handler_complain_key},
#line 129 "vteseq-2.gperf"
      {"Fo", vte_sequence_handler_complain_key},
#line 175 "vteseq-2.gperf"
      {"dm", vte_sequence_handler_noop},
#line 52 "vteseq-2.gperf"
      {"&6", vte_sequence_handler_complain_key},
#line 40 "vteseq-2.gperf"
      {"%e", vte_sequence_handler_complain_key},
#line 278 "vteseq-2.gperf"
      {"st", vte_sequence_handler_st},
#line 44 "vteseq-2.gperf"
      {"%i", vte_sequence_handler_complain_key},
#line 181 "vteseq-2.gperf"
      {"ei", vte_sequence_handler_ei},
#line 227 "vteseq-2.gperf"
      {"kr", vte_sequence_handler_complain_key},
#line 132 "vteseq-2.gperf"
      {"Fr", vte_sequence_handler_complain_key},
#line 246 "vteseq-2.gperf"
      {"me", vte_sequence_handler_me},
#line 282 "vteseq-2.gperf"
      {"ts", vte_sequence_handler_ts},
#line 243 "vteseq-2.gperf"
      {"ll", vte_sequence_handler_ll},
#line 39 "vteseq-2.gperf"
      {"%d", vte_sequence_handler_complain_key},
#line 180 "vteseq-2.gperf"
      {"ed", vte_sequence_handler_noop},
#line 242 "vteseq-2.gperf"
      {"le", vte_sequence_handler_le},
#line 37 "vteseq-2.gperf"
      {"%b", vte_sequence_handler_complain_key},
#line 45 "vteseq-2.gperf"
      {"%j", vte_sequence_handler_complain_key},
#line 131 "vteseq-2.gperf"
      {"Fq", vte_sequence_handler_complain_key},
#line 245 "vteseq-2.gperf"
      {"md", vte_sequence_handler_md},
#line 163 "vteseq-2.gperf"
      {"cc", vte_sequence_handler_noop},
#line 43 "vteseq-2.gperf"
      {"%h", vte_sequence_handler_complain_key},
#line 244 "vteseq-2.gperf"
      {"mb", vte_sequence_handler_mb},
#line 268 "vteseq-2.gperf"
      {"rc", vte_sequence_handler_rc},
#line 251 "vteseq-2.gperf"
      {"mp", vte_sequence_handler_mp},
#line 24 "vteseq-2.gperf"
      {"#2", vte_sequence_handler_complain_key},
#line 254 "vteseq-2.gperf"
      {"nw", vte_sequence_handler_nw},
#line 247 "vteseq-2.gperf"
      {"mh", vte_sequence_handler_mh},
#line 160 "vteseq-2.gperf"
      {"bt", vte_sequence_handler_bt},
#line 120 "vteseq-2.gperf"
      {"Ff", vte_sequence_handler_complain_key},
#line 283 "vteseq-2.gperf"
      {"uc", vte_sequence_handler_uc},
#line 110 "vteseq-2.gperf"
      {"FV", vte_sequence_handler_complain_key},
#line 167 "vteseq-2.gperf"
      {"cl", vte_sequence_handler_cl},
#line 221 "vteseq-2.gperf"
      {"ka", vte_sequence_handler_complain_key},
#line 115 "vteseq-2.gperf"
      {"Fa", vte_sequence_handler_complain_key},
#line 165 "vteseq-2.gperf"
      {"ce", vte_sequence_handler_ce},
#line 252 "vteseq-2.gperf"
      {"mr", vte_sequence_handler_mr},
#line 25 "vteseq-2.gperf"
      {"#3", vte_sequence_handler_complain_key},
#line 109 "vteseq-2.gperf"
      {"FU", vte_sequence_handler_complain_key},
#line 273 "vteseq-2.gperf"
      {"sc", vte_sequence_handler_sc},
#line 192 "vteseq-2.gperf"
      {"im", vte_sequence_handler_im},
#line 288 "vteseq-2.gperf"
      {"ve", vte_sequence_handler_ve},
#line 23 "vteseq-2.gperf"
      {"#1", vte_sequence_handler_complain_key},
#line 289 "vteseq-2.gperf"
      {"vi", vte_sequence_handler_vi},
#line 164 "vteseq-2.gperf"
      {"cd", vte_sequence_handler_cd},
#line 284 "vteseq-2.gperf"
      {"ue", vte_sequence_handler_ue},
#line 173 "vteseq-2.gperf"
      {"dc", vte_sequence_handler_dc},
#line 162 "vteseq-2.gperf"
      {"cb", vte_sequence_handler_cb},
#line 214 "vteseq-2.gperf"
      {"kL", vte_sequence_handler_complain_key},
#line 100 "vteseq-2.gperf"
      {"FL", vte_sequence_handler_complain_key},
#line 41 "vteseq-2.gperf"
      {"%f", vte_sequence_handler_complain_key},
#line 26 "vteseq-2.gperf"
      {"#4", vte_sequence_handler_complain_key},
#line 166 "vteseq-2.gperf"
      {"ch", vte_sequence_handler_ch},
#line 287 "vteseq-2.gperf"
      {"vb", vte_sequence_handler_vb},
#line 274 "vteseq-2.gperf"
      {"se", vte_sequence_handler_se},
#line 36 "vteseq-2.gperf"
      {"%a", vte_sequence_handler_complain_key},
#line 172 "vteseq-2.gperf"
      {"cv", vte_sequence_handler_cv},
#line 195 "vteseq-2.gperf"
      {"k0", vte_sequence_handler_complain_key},
#line 174 "vteseq-2.gperf"
      {"dl", vte_sequence_handler_dl},
#line 285 "vteseq-2.gperf"
      {"up", vte_sequence_handler_up},
#line 56 "vteseq-2.gperf"
      {"*0", vte_sequence_handler_complain_key},
#line 169 "vteseq-2.gperf"
      {"cr", vte_sequence_handler_cr},
#line 158 "vteseq-2.gperf"
      {"bc", vte_sequence_handler_le},
#line 156 "vteseq-2.gperf"
      {"al", vte_sequence_handler_al},
#line 66 "vteseq-2.gperf"
      {"@0", vte_sequence_handler_complain_key},
#line 99 "vteseq-2.gperf"
      {"FK", vte_sequence_handler_complain_key},
#line 155 "vteseq-2.gperf"
      {"ae", vte_sequence_handler_ae},
#line 213 "vteseq-2.gperf"
      {"kI", vte_sequence_handler_complain_key},
#line 97 "vteseq-2.gperf"
      {"FI", vte_sequence_handler_complain_key},
#line 208 "vteseq-2.gperf"
      {"kC", vte_sequence_handler_complain_key},
#line 91 "vteseq-2.gperf"
      {"FC", vte_sequence_handler_complain_key},
#line 21 "vteseq-2.gperf"
      {"!2", vte_sequence_handler_complain_key},
#line 276 "vteseq-2.gperf"
      {"so", vte_sequence_handler_so},
#line 133 "vteseq-2.gperf"
      {"IC", vte_sequence_handler_IC},
#line 159 "vteseq-2.gperf"
      {"bl", vte_sequence_handler_bl},
#line 183 "vteseq-2.gperf"
      {"fs", vte_sequence_handler_fs},
#line 128 "vteseq-2.gperf"
      {"Fn", vte_sequence_handler_complain_key},
#line 220 "vteseq-2.gperf"
      {"kT", vte_sequence_handler_complain_key},
#line 108 "vteseq-2.gperf"
      {"FT", vte_sequence_handler_complain_key},
#line 176 "vteseq-2.gperf"
      {"do", vte_sequence_handler_do},
#line 277 "vteseq-2.gperf"
      {"sr", vte_sequence_handler_sr},
#line 210 "vteseq-2.gperf"
      {"kE", vte_sequence_handler_complain_key},
#line 93 "vteseq-2.gperf"
      {"FE", vte_sequence_handler_complain_key},
#line 22 "vteseq-2.gperf"
      {"!3", vte_sequence_handler_complain_key},
#line 190 "vteseq-2.gperf"
      {"ic", vte_sequence_handler_ic},
#line 206 "vteseq-2.gperf"
      {"kA", vte_sequence_handler_complain_key},
#line 89 "vteseq-2.gperf"
      {"FA", vte_sequence_handler_complain_key},
#line 98 "vteseq-2.gperf"
      {"FJ", vte_sequence_handler_complain_key},
#line 20 "vteseq-2.gperf"
      {"!1", vte_sequence_handler_complain_key},
#line 46 "vteseq-2.gperf"
      {"&0", vte_sequence_handler_complain_key},
#line 218 "vteseq-2.gperf"
      {"kR", vte_sequence_handler_complain_key},
#line 106 "vteseq-2.gperf"
      {"FR", vte_sequence_handler_complain_key},
#line 151 "vteseq-2.gperf"
      {"SR", vte_sequence_handler_SR},
#line 280 "vteseq-2.gperf"
      {"te", vte_sequence_handler_noop},
#line 253 "vteseq-2.gperf"
      {"nd", vte_sequence_handler_nd},
#line 281 "vteseq-2.gperf"
      {"ti", vte_sequence_handler_noop},
#line 185 "vteseq-2.gperf"
      {"ho", vte_sequence_handler_ho},
#line 211 "vteseq-2.gperf"
      {"kF", vte_sequence_handler_complain_key},
#line 94 "vteseq-2.gperf"
      {"FF", vte_sequence_handler_complain_key},
#line 150 "vteseq-2.gperf"
      {"SF", vte_sequence_handler_SF},
#line 275 "vteseq-2.gperf"
      {"sf", vte_sequence_handler_sf},
#line 217 "vteseq-2.gperf"
      {"kP", vte_sequence_handler_complain_key},
#line 104 "vteseq-2.gperf"
      {"FP", vte_sequence_handler_complain_key},
#line 153 "vteseq-2.gperf"
      {"UP", vte_sequence_handler_UP},
#line 79 "vteseq-2.gperf"
      {"DO", vte_sequence_handler_DO},
#line 215 "vteseq-2.gperf"
      {"kM", vte_sequence_handler_complain_key},
#line 101 "vteseq-2.gperf"
      {"FM", vte_sequence_handler_complain_key},
#line 95 "vteseq-2.gperf"
      {"FG", vte_sequence_handler_complain_key},
#line 178 "vteseq-2.gperf"
      {"eA", vte_sequence_handler_eA},
#line 209 "vteseq-2.gperf"
      {"kD", vte_sequence_handler_complain_key},
#line 92 "vteseq-2.gperf"
      {"FD", vte_sequence_handler_complain_key},
#line 207 "vteseq-2.gperf"
      {"kB", vte_sequence_handler_complain_key},
#line 90 "vteseq-2.gperf"
      {"FB", vte_sequence_handler_complain_key},
#line 205 "vteseq-2.gperf"
      {"k;", vte_sequence_handler_complain_key},
#line 147 "vteseq-2.gperf"
      {"RI", vte_sequence_handler_RI},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 279 "vteseq-2.gperf"
      {"ta", vte_sequence_handler_ta},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 78 "vteseq-2.gperf"
      {"DL", vte_sequence_handler_DL},
      {""}, {""}, {""}, {""}, {""}, {""},
#line 182 "vteseq-2.gperf"
      {"ff", vte_sequence_handler_noop},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""},
#line 76 "vteseq-2.gperf"
      {"AL", vte_sequence_handler_AL},
      {""},
#line 77 "vteseq-2.gperf"
      {"DC", vte_sequence_handler_DC},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""}, {""},
      {""}, {""}, {""}, {""},
#line 139 "vteseq-2.gperf"
      {"LE", vte_sequence_handler_LE}
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
