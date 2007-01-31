/* C code produced by gperf version 3.0.2 */
/* Command-line: gperf -g -a -S 4 -t -m 100 -C -H vteseq_2_hash -N vteseq_2_in_word_set vteseq-2.gperf  */
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
error "gperf generated tables don't work with this execution character set. Please report a bug to <bug-gnu-gperf@gnu.org>."
#endif

#line 1 "vteseq-2.gperf"
struct vteseq_2_lookup { const guchar name[2]; VteTerminalSequenceHandler handler; };

#define TOTAL_KEYWORDS 221
#define MIN_WORD_LENGTH 2
#define MAX_WORD_LENGTH 2
#define MIN_HASH_VALUE 2
#define MAX_HASH_VALUE 360
/* maximum key range = 359, duplicates = 0 */

#ifdef __GNUC__
__inline
#else
#ifdef __cplusplus
inline
#endif
#endif
static guint
vteseq_2_hash (register const guchar *str)
{
  static const unsigned short asso_values[] =
    {
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 168, 361, 122,
      361,  22,  32, 361, 157,  20,   3,  15,  29,  38,
       71,  66,  55,  43, 361, 213, 361, 361, 361, 361,
      361, 185, 211, 169, 209, 181, 197, 206,   7, 167,
      186, 164, 148, 205,   1,  56, 201,   4, 190,  11,
      177, 137, 130,  18,  16,   8,  47,   2, 361,   2,
      361, 361, 361, 133,  94,  67,  91,  82, 128,  12,
       99,  84,  95,  53,  79,  23, 175, 100,  96, 117,
      108,   0,  33,  28, 103,  23, 361, 361,  56,  72,
      111,  63,  59, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361,
      361, 361, 361, 361, 361, 361, 361, 361, 361, 361
    };
  return 2 + asso_values[str[1]+6] + asso_values[str[0]+14];
}

#ifdef __GNUC__
__inline
#endif
static VteTerminalSequenceHandler
vteseq_2_lookup (register const guchar *str)
{
	static const struct vteseq_2_lookup wordlist[] =
	{
#line 211 "vteseq-2.gperf"
		{"ks", vte_sequence_handler_ks},
#line 199 "vteseq-2.gperf"
		{"kN", vte_sequence_handler_complain_key},
#line 85 "vteseq-2.gperf"
		{"FN", vte_sequence_handler_complain_key},
#line 180 "vteseq-2.gperf"
		{"k2", vte_sequence_handler_complain_key},
#line 64 "vteseq-2.gperf"
		{"F2", vte_sequence_handler_complain_key},
#line 88 "vteseq-2.gperf"
		{"FQ", vte_sequence_handler_complain_key},
#line 41 "vteseq-2.gperf"
		{"*2", vte_sequence_handler_complain_key},
#line 195 "vteseq-2.gperf"
		{"kH", vte_sequence_handler_complain_key},
#line 79 "vteseq-2.gperf"
		{"FH", vte_sequence_handler_complain_key},
#line 96 "vteseq-2.gperf"
		{"FY", vte_sequence_handler_complain_key},
#line 51 "vteseq-2.gperf"
		{"@2", vte_sequence_handler_complain_key},
#line 202 "vteseq-2.gperf"
		{"kS", vte_sequence_handler_complain_key},
#line 90 "vteseq-2.gperf"
		{"FS", vte_sequence_handler_complain_key},
#line 104 "vteseq-2.gperf"
		{"Fg", vte_sequence_handler_complain_key},
#line 118 "vteseq-2.gperf"
		{"K2", vte_sequence_handler_complain_key},
#line 181 "vteseq-2.gperf"
		{"k3", vte_sequence_handler_complain_key},
#line 65 "vteseq-2.gperf"
		{"F3", vte_sequence_handler_complain_key},
#line 95 "vteseq-2.gperf"
		{"FX", vte_sequence_handler_complain_key},
#line 42 "vteseq-2.gperf"
		{"*3", vte_sequence_handler_complain_key},
#line 94 "vteseq-2.gperf"
		{"FW", vte_sequence_handler_complain_key},
#line 179 "vteseq-2.gperf"
		{"k1", vte_sequence_handler_complain_key},
#line 63 "vteseq-2.gperf"
		{"F1", vte_sequence_handler_complain_key},
#line 52 "vteseq-2.gperf"
		{"@3", vte_sequence_handler_complain_key},
#line 40 "vteseq-2.gperf"
		{"*1", vte_sequence_handler_complain_key},
#line 110 "vteseq-2.gperf"
		{"Fm", vte_sequence_handler_complain_key},
#line 11 "vteseq-2.gperf"
		{"%2", vte_sequence_handler_complain_key},
#line 119 "vteseq-2.gperf"
		{"K3", vte_sequence_handler_complain_key},
#line 50 "vteseq-2.gperf"
		{"@1", vte_sequence_handler_complain_key},
#line 213 "vteseq-2.gperf"
		{"ku", vte_sequence_handler_complain_key},
#line 182 "vteseq-2.gperf"
		{"k4", vte_sequence_handler_complain_key},
#line 66 "vteseq-2.gperf"
		{"F4", vte_sequence_handler_complain_key},
#line 117 "vteseq-2.gperf"
		{"K1", vte_sequence_handler_complain_key},
#line 43 "vteseq-2.gperf"
		{"*4", vte_sequence_handler_complain_key},
#line 212 "vteseq-2.gperf"
		{"kt", vte_sequence_handler_complain_key},
#line 25 "vteseq-2.gperf"
		{"%g", vte_sequence_handler_complain_key},
#line 31 "vteseq-2.gperf"
		{"&2", vte_sequence_handler_complain_key},
#line 53 "vteseq-2.gperf"
		{"@4", vte_sequence_handler_complain_key},
#line 12 "vteseq-2.gperf"
		{"%3", vte_sequence_handler_complain_key},
#line 183 "vteseq-2.gperf"
		{"k5", vte_sequence_handler_complain_key},
#line 67 "vteseq-2.gperf"
		{"F5", vte_sequence_handler_complain_key},
#line 120 "vteseq-2.gperf"
		{"K4", vte_sequence_handler_complain_key},
#line 44 "vteseq-2.gperf"
		{"*5", vte_sequence_handler_complain_key},
#line 10 "vteseq-2.gperf"
		{"%1", vte_sequence_handler_complain_key},
#line 187 "vteseq-2.gperf"
		{"k9", vte_sequence_handler_complain_key},
#line 71 "vteseq-2.gperf"
		{"F9", vte_sequence_handler_complain_key},
#line 54 "vteseq-2.gperf"
		{"@5", vte_sequence_handler_complain_key},
#line 48 "vteseq-2.gperf"
		{"*9", vte_sequence_handler_complain_key},
#line 32 "vteseq-2.gperf"
		{"&3", vte_sequence_handler_complain_key},
#line 97 "vteseq-2.gperf"
		{"FZ", vte_sequence_handler_complain_key},
#line 121 "vteseq-2.gperf"
		{"K5", vte_sequence_handler_complain_key},
#line 58 "vteseq-2.gperf"
		{"@9", vte_sequence_handler_complain_key},
#line 13 "vteseq-2.gperf"
		{"%4", vte_sequence_handler_complain_key},
#line 30 "vteseq-2.gperf"
		{"&1", vte_sequence_handler_complain_key},
#line 153 "vteseq-2.gperf"
		{"cs", vte_sequence_handler_cs},
#line 108 "vteseq-2.gperf"
		{"Fk", vte_sequence_handler_complain_key},
#line 186 "vteseq-2.gperf"
		{"k8", vte_sequence_handler_complain_key},
#line 70 "vteseq-2.gperf"
		{"F8", vte_sequence_handler_complain_key},
#line 86 "vteseq-2.gperf"
		{"FO", vte_sequence_handler_complain_key},
#line 47 "vteseq-2.gperf"
		{"*8", vte_sequence_handler_complain_key},
#line 273 "vteseq-2.gperf"
		{"vs", vte_sequence_handler_vs},
#line 14 "vteseq-2.gperf"
		{"%5", vte_sequence_handler_complain_key},
#line 33 "vteseq-2.gperf"
		{"&4", vte_sequence_handler_complain_key},
#line 57 "vteseq-2.gperf"
		{"@8", vte_sequence_handler_complain_key},
#line 269 "vteseq-2.gperf"
		{"us", vte_sequence_handler_us},
#line 144 "vteseq-2.gperf"
		{"cS", vte_sequence_handler_cS},
#line 18 "vteseq-2.gperf"
		{"%9", vte_sequence_handler_complain_key},
#line 185 "vteseq-2.gperf"
		{"k7", vte_sequence_handler_complain_key},
#line 69 "vteseq-2.gperf"
		{"F7", vte_sequence_handler_complain_key},
#line 100 "vteseq-2.gperf"
		{"Fc", vte_sequence_handler_complain_key},
#line 46 "vteseq-2.gperf"
		{"*7", vte_sequence_handler_complain_key},
#line 34 "vteseq-2.gperf"
		{"&5", vte_sequence_handler_complain_key},
#line 184 "vteseq-2.gperf"
		{"k6", vte_sequence_handler_complain_key},
#line 68 "vteseq-2.gperf"
		{"F6", vte_sequence_handler_complain_key},
#line 56 "vteseq-2.gperf"
		{"@7", vte_sequence_handler_complain_key},
#line 45 "vteseq-2.gperf"
		{"*6", vte_sequence_handler_complain_key},
#line 38 "vteseq-2.gperf"
		{"&9", vte_sequence_handler_complain_key},
#line 151 "vteseq-2.gperf"
		{"cm", vte_sequence_handler_cm},
#line 17 "vteseq-2.gperf"
		{"%8", vte_sequence_handler_complain_key},
#line 55 "vteseq-2.gperf"
		{"@6", vte_sequence_handler_complain_key},
#line 209 "vteseq-2.gperf"
		{"kl", vte_sequence_handler_complain_key},
#line 109 "vteseq-2.gperf"
		{"Fl", vte_sequence_handler_complain_key},
#line 231 "vteseq-2.gperf"
		{"mk", vte_sequence_handler_mk},
#line 207 "vteseq-2.gperf"
		{"ke", vte_sequence_handler_ke},
#line 102 "vteseq-2.gperf"
		{"Fe", vte_sequence_handler_complain_key},
#line 140 "vteseq-2.gperf"
		{"as", vte_sequence_handler_as},
#line 106 "vteseq-2.gperf"
		{"Fi", vte_sequence_handler_complain_key},
#line 154 "vteseq-2.gperf"
		{"ct", vte_sequence_handler_ct},
#line 37 "vteseq-2.gperf"
		{"&8", vte_sequence_handler_complain_key},
#line 16 "vteseq-2.gperf"
		{"%7", vte_sequence_handler_complain_key},
#line 21 "vteseq-2.gperf"
		{"%c", vte_sequence_handler_complain_key},
#line 162 "vteseq-2.gperf"
		{"ec", vte_sequence_handler_ec},
#line 206 "vteseq-2.gperf"
		{"kd", vte_sequence_handler_complain_key},
#line 101 "vteseq-2.gperf"
		{"Fd", vte_sequence_handler_complain_key},
#line 15 "vteseq-2.gperf"
		{"%6", vte_sequence_handler_complain_key},
#line 205 "vteseq-2.gperf"
		{"kb", vte_sequence_handler_kb},
#line 99 "vteseq-2.gperf"
		{"Fb", vte_sequence_handler_complain_key},
#line 107 "vteseq-2.gperf"
		{"Fj", vte_sequence_handler_complain_key},
#line 113 "vteseq-2.gperf"
		{"Fp", vte_sequence_handler_complain_key},
#line 36 "vteseq-2.gperf"
		{"&7", vte_sequence_handler_complain_key},
#line 208 "vteseq-2.gperf"
		{"kh", vte_sequence_handler_complain_key},
#line 105 "vteseq-2.gperf"
		{"Fh", vte_sequence_handler_complain_key},
#line 112 "vteseq-2.gperf"
		{"Fo", vte_sequence_handler_complain_key},
#line 158 "vteseq-2.gperf"
		{"dm", vte_sequence_handler_noop},
#line 35 "vteseq-2.gperf"
		{"&6", vte_sequence_handler_complain_key},
#line 23 "vteseq-2.gperf"
		{"%e", vte_sequence_handler_complain_key},
#line 261 "vteseq-2.gperf"
		{"st", vte_sequence_handler_st},
#line 27 "vteseq-2.gperf"
		{"%i", vte_sequence_handler_complain_key},
#line 164 "vteseq-2.gperf"
		{"ei", vte_sequence_handler_ei},
#line 210 "vteseq-2.gperf"
		{"kr", vte_sequence_handler_complain_key},
#line 115 "vteseq-2.gperf"
		{"Fr", vte_sequence_handler_complain_key},
#line 229 "vteseq-2.gperf"
		{"me", vte_sequence_handler_me},
#line 265 "vteseq-2.gperf"
		{"ts", vte_sequence_handler_ts},
#line 226 "vteseq-2.gperf"
		{"ll", vte_sequence_handler_ll},
#line 22 "vteseq-2.gperf"
		{"%d", vte_sequence_handler_complain_key},
#line 163 "vteseq-2.gperf"
		{"ed", vte_sequence_handler_noop},
#line 225 "vteseq-2.gperf"
		{"le", vte_sequence_handler_le},
#line 20 "vteseq-2.gperf"
		{"%b", vte_sequence_handler_complain_key},
#line 28 "vteseq-2.gperf"
		{"%j", vte_sequence_handler_complain_key},
#line 114 "vteseq-2.gperf"
		{"Fq", vte_sequence_handler_complain_key},
#line 228 "vteseq-2.gperf"
		{"md", vte_sequence_handler_md},
#line 146 "vteseq-2.gperf"
		{"cc", vte_sequence_handler_noop},
#line 26 "vteseq-2.gperf"
		{"%h", vte_sequence_handler_complain_key},
#line 227 "vteseq-2.gperf"
		{"mb", vte_sequence_handler_mb},
#line 251 "vteseq-2.gperf"
		{"rc", vte_sequence_handler_rc},
#line 234 "vteseq-2.gperf"
		{"mp", vte_sequence_handler_mp},
#line 7 "vteseq-2.gperf"
		{"#2", vte_sequence_handler_complain_key},
#line 237 "vteseq-2.gperf"
		{"nw", vte_sequence_handler_nw},
#line 230 "vteseq-2.gperf"
		{"mh", vte_sequence_handler_mh},
#line 143 "vteseq-2.gperf"
		{"bt", vte_sequence_handler_bt},
#line 103 "vteseq-2.gperf"
		{"Ff", vte_sequence_handler_complain_key},
#line 266 "vteseq-2.gperf"
		{"uc", vte_sequence_handler_uc},
#line 93 "vteseq-2.gperf"
		{"FV", vte_sequence_handler_complain_key},
#line 150 "vteseq-2.gperf"
		{"cl", vte_sequence_handler_cl},
#line 204 "vteseq-2.gperf"
		{"ka", vte_sequence_handler_complain_key},
#line 98 "vteseq-2.gperf"
		{"Fa", vte_sequence_handler_complain_key},
#line 148 "vteseq-2.gperf"
		{"ce", vte_sequence_handler_ce},
#line 235 "vteseq-2.gperf"
		{"mr", vte_sequence_handler_mr},
#line 8 "vteseq-2.gperf"
		{"#3", vte_sequence_handler_complain_key},
#line 92 "vteseq-2.gperf"
		{"FU", vte_sequence_handler_complain_key},
#line 256 "vteseq-2.gperf"
		{"sc", vte_sequence_handler_sc},
#line 175 "vteseq-2.gperf"
		{"im", vte_sequence_handler_im},
#line 271 "vteseq-2.gperf"
		{"ve", vte_sequence_handler_ve},
#line 6 "vteseq-2.gperf"
		{"#1", vte_sequence_handler_complain_key},
#line 272 "vteseq-2.gperf"
		{"vi", vte_sequence_handler_vi},
#line 147 "vteseq-2.gperf"
		{"cd", vte_sequence_handler_cd},
#line 267 "vteseq-2.gperf"
		{"ue", vte_sequence_handler_ue},
#line 156 "vteseq-2.gperf"
		{"dc", vte_sequence_handler_dc},
#line 145 "vteseq-2.gperf"
		{"cb", vte_sequence_handler_cb},
#line 197 "vteseq-2.gperf"
		{"kL", vte_sequence_handler_complain_key},
#line 83 "vteseq-2.gperf"
		{"FL", vte_sequence_handler_complain_key},
#line 24 "vteseq-2.gperf"
		{"%f", vte_sequence_handler_complain_key},
#line 9 "vteseq-2.gperf"
		{"#4", vte_sequence_handler_complain_key},
#line 149 "vteseq-2.gperf"
		{"ch", vte_sequence_handler_ch},
#line 270 "vteseq-2.gperf"
		{"vb", vte_sequence_handler_vb},
#line 257 "vteseq-2.gperf"
		{"se", vte_sequence_handler_se},
#line 19 "vteseq-2.gperf"
		{"%a", vte_sequence_handler_complain_key},
#line 155 "vteseq-2.gperf"
		{"cv", vte_sequence_handler_cv},
#line 178 "vteseq-2.gperf"
		{"k0", vte_sequence_handler_complain_key},
#line 157 "vteseq-2.gperf"
		{"dl", vte_sequence_handler_dl},
#line 268 "vteseq-2.gperf"
		{"up", vte_sequence_handler_up},
#line 39 "vteseq-2.gperf"
		{"*0", vte_sequence_handler_complain_key},
#line 152 "vteseq-2.gperf"
		{"cr", vte_sequence_handler_cr},
#line 141 "vteseq-2.gperf"
		{"bc", vte_sequence_handler_le},
#line 139 "vteseq-2.gperf"
		{"al", vte_sequence_handler_al},
#line 49 "vteseq-2.gperf"
		{"@0", vte_sequence_handler_complain_key},
#line 82 "vteseq-2.gperf"
		{"FK", vte_sequence_handler_complain_key},
#line 138 "vteseq-2.gperf"
		{"ae", vte_sequence_handler_ae},
#line 196 "vteseq-2.gperf"
		{"kI", vte_sequence_handler_complain_key},
#line 80 "vteseq-2.gperf"
		{"FI", vte_sequence_handler_complain_key},
#line 191 "vteseq-2.gperf"
		{"kC", vte_sequence_handler_complain_key},
#line 74 "vteseq-2.gperf"
		{"FC", vte_sequence_handler_complain_key},
#line 4 "vteseq-2.gperf"
		{"!2", vte_sequence_handler_complain_key},
#line 259 "vteseq-2.gperf"
		{"so", vte_sequence_handler_so},
#line 116 "vteseq-2.gperf"
		{"IC", vte_sequence_handler_IC},
#line 142 "vteseq-2.gperf"
		{"bl", vte_sequence_handler_bl},
#line 166 "vteseq-2.gperf"
		{"fs", vte_sequence_handler_fs},
#line 111 "vteseq-2.gperf"
		{"Fn", vte_sequence_handler_complain_key},
#line 203 "vteseq-2.gperf"
		{"kT", vte_sequence_handler_complain_key},
#line 91 "vteseq-2.gperf"
		{"FT", vte_sequence_handler_complain_key},
#line 159 "vteseq-2.gperf"
		{"do", vte_sequence_handler_do},
#line 260 "vteseq-2.gperf"
		{"sr", vte_sequence_handler_sr},
#line 193 "vteseq-2.gperf"
		{"kE", vte_sequence_handler_complain_key},
#line 76 "vteseq-2.gperf"
		{"FE", vte_sequence_handler_complain_key},
#line 5 "vteseq-2.gperf"
		{"!3", vte_sequence_handler_complain_key},
#line 173 "vteseq-2.gperf"
		{"ic", vte_sequence_handler_ic},
#line 189 "vteseq-2.gperf"
		{"kA", vte_sequence_handler_complain_key},
#line 72 "vteseq-2.gperf"
		{"FA", vte_sequence_handler_complain_key},
#line 81 "vteseq-2.gperf"
		{"FJ", vte_sequence_handler_complain_key},
#line 3 "vteseq-2.gperf"
		{"!1", vte_sequence_handler_complain_key},
#line 29 "vteseq-2.gperf"
		{"&0", vte_sequence_handler_complain_key},
#line 201 "vteseq-2.gperf"
		{"kR", vte_sequence_handler_complain_key},
#line 89 "vteseq-2.gperf"
		{"FR", vte_sequence_handler_complain_key},
#line 134 "vteseq-2.gperf"
		{"SR", vte_sequence_handler_SR},
#line 263 "vteseq-2.gperf"
		{"te", vte_sequence_handler_noop},
#line 236 "vteseq-2.gperf"
		{"nd", vte_sequence_handler_nd},
#line 264 "vteseq-2.gperf"
		{"ti", vte_sequence_handler_noop},
#line 168 "vteseq-2.gperf"
		{"ho", vte_sequence_handler_ho},
#line 194 "vteseq-2.gperf"
		{"kF", vte_sequence_handler_complain_key},
#line 77 "vteseq-2.gperf"
		{"FF", vte_sequence_handler_complain_key},
#line 133 "vteseq-2.gperf"
		{"SF", vte_sequence_handler_SF},
#line 258 "vteseq-2.gperf"
		{"sf", vte_sequence_handler_sf},
#line 200 "vteseq-2.gperf"
		{"kP", vte_sequence_handler_complain_key},
#line 87 "vteseq-2.gperf"
		{"FP", vte_sequence_handler_complain_key},
#line 136 "vteseq-2.gperf"
		{"UP", vte_sequence_handler_UP},
#line 62 "vteseq-2.gperf"
		{"DO", vte_sequence_handler_DO},
#line 198 "vteseq-2.gperf"
		{"kM", vte_sequence_handler_complain_key},
#line 84 "vteseq-2.gperf"
		{"FM", vte_sequence_handler_complain_key},
#line 78 "vteseq-2.gperf"
		{"FG", vte_sequence_handler_complain_key},
#line 161 "vteseq-2.gperf"
		{"eA", vte_sequence_handler_eA},
#line 192 "vteseq-2.gperf"
		{"kD", vte_sequence_handler_complain_key},
#line 75 "vteseq-2.gperf"
		{"FD", vte_sequence_handler_complain_key},
#line 190 "vteseq-2.gperf"
		{"kB", vte_sequence_handler_complain_key},
#line 73 "vteseq-2.gperf"
		{"FB", vte_sequence_handler_complain_key},
#line 188 "vteseq-2.gperf"
		{"k;", vte_sequence_handler_complain_key},
#line 130 "vteseq-2.gperf"
		{"RI", vte_sequence_handler_RI},
#line 262 "vteseq-2.gperf"
		{"ta", vte_sequence_handler_ta},
#line 61 "vteseq-2.gperf"
		{"DL", vte_sequence_handler_DL},
#line 165 "vteseq-2.gperf"
		{"ff", vte_sequence_handler_noop},
#line 59 "vteseq-2.gperf"
		{"AL", vte_sequence_handler_AL},
#line 60 "vteseq-2.gperf"
		{"DC", vte_sequence_handler_DC},
#line 122 "vteseq-2.gperf"
		{"LE", vte_sequence_handler_LE}
	};

	register int key = vteseq_2_hash (str);

	if (key <= MAX_HASH_VALUE && key >= MIN_HASH_VALUE)
	{
		register const struct vteseq_2_lookup *resword;

		if (key < 113)
		{
			if (key < 58)
			{
				switch (key - 2)
				{
					case 0:
						resword = &wordlist[0];
						goto compare;
					case 1:
						resword = &wordlist[1];
						goto compare;
					case 2:
						resword = &wordlist[2];
						goto compare;
					case 3:
						resword = &wordlist[3];
						goto compare;
					case 4:
						resword = &wordlist[4];
						goto compare;
					case 5:
						resword = &wordlist[5];
						goto compare;
					case 6:
						resword = &wordlist[6];
						goto compare;
					case 7:
						resword = &wordlist[7];
						goto compare;
					case 8:
						resword = &wordlist[8];
						goto compare;
					case 9:
						resword = &wordlist[9];
						goto compare;
					case 10:
						resword = &wordlist[10];
						goto compare;
					case 11:
						resword = &wordlist[11];
						goto compare;
					case 12:
						resword = &wordlist[12];
						goto compare;
					case 13:
						resword = &wordlist[13];
						goto compare;
					case 14:
						resword = &wordlist[14];
						goto compare;
					case 15:
						resword = &wordlist[15];
						goto compare;
					case 16:
						resword = &wordlist[16];
						goto compare;
					case 17:
						resword = &wordlist[17];
						goto compare;
					case 18:
						resword = &wordlist[18];
						goto compare;
					case 19:
						resword = &wordlist[19];
						goto compare;
					case 20:
						resword = &wordlist[20];
						goto compare;
					case 21:
						resword = &wordlist[21];
						goto compare;
					case 22:
						resword = &wordlist[22];
						goto compare;
					case 23:
						resword = &wordlist[23];
						goto compare;
					case 24:
						resword = &wordlist[24];
						goto compare;
					case 25:
						resword = &wordlist[25];
						goto compare;
					case 26:
						resword = &wordlist[26];
						goto compare;
					case 27:
						resword = &wordlist[27];
						goto compare;
					case 28:
						resword = &wordlist[28];
						goto compare;
					case 29:
						resword = &wordlist[29];
						goto compare;
					case 30:
						resword = &wordlist[30];
						goto compare;
					case 31:
						resword = &wordlist[31];
						goto compare;
					case 32:
						resword = &wordlist[32];
						goto compare;
					case 33:
						resword = &wordlist[33];
						goto compare;
					case 34:
						resword = &wordlist[34];
						goto compare;
					case 35:
						resword = &wordlist[35];
						goto compare;
					case 36:
						resword = &wordlist[36];
						goto compare;
					case 37:
						resword = &wordlist[37];
						goto compare;
					case 38:
						resword = &wordlist[38];
						goto compare;
					case 39:
						resword = &wordlist[39];
						goto compare;
					case 40:
						resword = &wordlist[40];
						goto compare;
					case 41:
						resword = &wordlist[41];
						goto compare;
					case 42:
						resword = &wordlist[42];
						goto compare;
					case 43:
						resword = &wordlist[43];
						goto compare;
					case 44:
						resword = &wordlist[44];
						goto compare;
					case 45:
						resword = &wordlist[45];
						goto compare;
					case 46:
						resword = &wordlist[46];
						goto compare;
					case 47:
						resword = &wordlist[47];
						goto compare;
					case 48:
						resword = &wordlist[48];
						goto compare;
					case 49:
						resword = &wordlist[49];
						goto compare;
					case 50:
						resword = &wordlist[50];
						goto compare;
					case 51:
						resword = &wordlist[51];
						goto compare;
					case 52:
						resword = &wordlist[52];
						goto compare;
					case 53:
						resword = &wordlist[53];
						goto compare;
					case 54:
						resword = &wordlist[54];
						goto compare;
					case 55:
						resword = &wordlist[55];
						goto compare;
				}
			}
			else
			{
				switch (key - 58)
				{
					case 0:
						resword = &wordlist[56];
						goto compare;
					case 1:
						resword = &wordlist[57];
						goto compare;
					case 2:
						resword = &wordlist[58];
						goto compare;
					case 3:
						resword = &wordlist[59];
						goto compare;
					case 4:
						resword = &wordlist[60];
						goto compare;
					case 5:
						resword = &wordlist[61];
						goto compare;
					case 6:
						resword = &wordlist[62];
						goto compare;
					case 7:
						resword = &wordlist[63];
						goto compare;
					case 8:
						resword = &wordlist[64];
						goto compare;
					case 9:
						resword = &wordlist[65];
						goto compare;
					case 10:
						resword = &wordlist[66];
						goto compare;
					case 11:
						resword = &wordlist[67];
						goto compare;
					case 12:
						resword = &wordlist[68];
						goto compare;
					case 13:
						resword = &wordlist[69];
						goto compare;
					case 14:
						resword = &wordlist[70];
						goto compare;
					case 15:
						resword = &wordlist[71];
						goto compare;
					case 16:
						resword = &wordlist[72];
						goto compare;
					case 17:
						resword = &wordlist[73];
						goto compare;
					case 18:
						resword = &wordlist[74];
						goto compare;
					case 19:
						resword = &wordlist[75];
						goto compare;
					case 20:
						resword = &wordlist[76];
						goto compare;
					case 21:
						resword = &wordlist[77];
						goto compare;
					case 22:
						resword = &wordlist[78];
						goto compare;
					case 23:
						resword = &wordlist[79];
						goto compare;
					case 24:
						resword = &wordlist[80];
						goto compare;
					case 25:
						resword = &wordlist[81];
						goto compare;
					case 26:
						resword = &wordlist[82];
						goto compare;
					case 27:
						resword = &wordlist[83];
						goto compare;
					case 28:
						resword = &wordlist[84];
						goto compare;
					case 29:
						resword = &wordlist[85];
						goto compare;
					case 30:
						resword = &wordlist[86];
						goto compare;
					case 31:
						resword = &wordlist[87];
						goto compare;
					case 32:
						resword = &wordlist[88];
						goto compare;
					case 33:
						resword = &wordlist[89];
						goto compare;
					case 34:
						resword = &wordlist[90];
						goto compare;
					case 35:
						resword = &wordlist[91];
						goto compare;
					case 36:
						resword = &wordlist[92];
						goto compare;
					case 37:
						resword = &wordlist[93];
						goto compare;
					case 38:
						resword = &wordlist[94];
						goto compare;
					case 39:
						resword = &wordlist[95];
						goto compare;
					case 40:
						resword = &wordlist[96];
						goto compare;
					case 41:
						resword = &wordlist[97];
						goto compare;
					case 42:
						resword = &wordlist[98];
						goto compare;
					case 43:
						resword = &wordlist[99];
						goto compare;
					case 44:
						resword = &wordlist[100];
						goto compare;
					case 45:
						resword = &wordlist[101];
						goto compare;
					case 46:
						resword = &wordlist[102];
						goto compare;
					case 47:
						resword = &wordlist[103];
						goto compare;
					case 48:
						resword = &wordlist[104];
						goto compare;
					case 49:
						resword = &wordlist[105];
						goto compare;
					case 50:
						resword = &wordlist[106];
						goto compare;
					case 51:
						resword = &wordlist[107];
						goto compare;
					case 52:
						resword = &wordlist[108];
						goto compare;
					case 53:
						resword = &wordlist[109];
						goto compare;
					case 54:
						resword = &wordlist[110];
						goto compare;
				}
			}
		}
		else
		{
			if (key < 168)
			{
				switch (key - 113)
				{
					case 0:
						resword = &wordlist[111];
						goto compare;
					case 1:
						resword = &wordlist[112];
						goto compare;
					case 2:
						resword = &wordlist[113];
						goto compare;
					case 3:
						resword = &wordlist[114];
						goto compare;
					case 4:
						resword = &wordlist[115];
						goto compare;
					case 5:
						resword = &wordlist[116];
						goto compare;
					case 6:
						resword = &wordlist[117];
						goto compare;
					case 7:
						resword = &wordlist[118];
						goto compare;
					case 8:
						resword = &wordlist[119];
						goto compare;
					case 9:
						resword = &wordlist[120];
						goto compare;
					case 10:
						resword = &wordlist[121];
						goto compare;
					case 11:
						resword = &wordlist[122];
						goto compare;
					case 12:
						resword = &wordlist[123];
						goto compare;
					case 13:
						resword = &wordlist[124];
						goto compare;
					case 14:
						resword = &wordlist[125];
						goto compare;
					case 15:
						resword = &wordlist[126];
						goto compare;
					case 16:
						resword = &wordlist[127];
						goto compare;
					case 17:
						resword = &wordlist[128];
						goto compare;
					case 18:
						resword = &wordlist[129];
						goto compare;
					case 19:
						resword = &wordlist[130];
						goto compare;
					case 20:
						resword = &wordlist[131];
						goto compare;
					case 21:
						resword = &wordlist[132];
						goto compare;
					case 22:
						resword = &wordlist[133];
						goto compare;
					case 23:
						resword = &wordlist[134];
						goto compare;
					case 24:
						resword = &wordlist[135];
						goto compare;
					case 25:
						resword = &wordlist[136];
						goto compare;
					case 26:
						resword = &wordlist[137];
						goto compare;
					case 27:
						resword = &wordlist[138];
						goto compare;
					case 28:
						resword = &wordlist[139];
						goto compare;
					case 29:
						resword = &wordlist[140];
						goto compare;
					case 30:
						resword = &wordlist[141];
						goto compare;
					case 31:
						resword = &wordlist[142];
						goto compare;
					case 32:
						resword = &wordlist[143];
						goto compare;
					case 33:
						resword = &wordlist[144];
						goto compare;
					case 34:
						resword = &wordlist[145];
						goto compare;
					case 35:
						resword = &wordlist[146];
						goto compare;
					case 36:
						resword = &wordlist[147];
						goto compare;
					case 37:
						resword = &wordlist[148];
						goto compare;
					case 38:
						resword = &wordlist[149];
						goto compare;
					case 39:
						resword = &wordlist[150];
						goto compare;
					case 40:
						resword = &wordlist[151];
						goto compare;
					case 41:
						resword = &wordlist[152];
						goto compare;
					case 42:
						resword = &wordlist[153];
						goto compare;
					case 43:
						resword = &wordlist[154];
						goto compare;
					case 44:
						resword = &wordlist[155];
						goto compare;
					case 45:
						resword = &wordlist[156];
						goto compare;
					case 46:
						resword = &wordlist[157];
						goto compare;
					case 47:
						resword = &wordlist[158];
						goto compare;
					case 48:
						resword = &wordlist[159];
						goto compare;
					case 49:
						resword = &wordlist[160];
						goto compare;
					case 50:
						resword = &wordlist[161];
						goto compare;
					case 51:
						resword = &wordlist[162];
						goto compare;
					case 52:
						resword = &wordlist[163];
						goto compare;
					case 53:
						resword = &wordlist[164];
						goto compare;
					case 54:
						resword = &wordlist[165];
						goto compare;
				}
			}
			else
			{
				switch (key - 168)
				{
					case 0:
						resword = &wordlist[166];
						goto compare;
					case 1:
						resword = &wordlist[167];
						goto compare;
					case 2:
						resword = &wordlist[168];
						goto compare;
					case 3:
						resword = &wordlist[169];
						goto compare;
					case 4:
						resword = &wordlist[170];
						goto compare;
					case 5:
						resword = &wordlist[171];
						goto compare;
					case 6:
						resword = &wordlist[172];
						goto compare;
					case 7:
						resword = &wordlist[173];
						goto compare;
					case 8:
						resword = &wordlist[174];
						goto compare;
					case 9:
						resword = &wordlist[175];
						goto compare;
					case 10:
						resword = &wordlist[176];
						goto compare;
					case 11:
						resword = &wordlist[177];
						goto compare;
					case 12:
						resword = &wordlist[178];
						goto compare;
					case 13:
						resword = &wordlist[179];
						goto compare;
					case 14:
						resword = &wordlist[180];
						goto compare;
					case 15:
						resword = &wordlist[181];
						goto compare;
					case 16:
						resword = &wordlist[182];
						goto compare;
					case 17:
						resword = &wordlist[183];
						goto compare;
					case 18:
						resword = &wordlist[184];
						goto compare;
					case 19:
						resword = &wordlist[185];
						goto compare;
					case 20:
						resword = &wordlist[186];
						goto compare;
					case 21:
						resword = &wordlist[187];
						goto compare;
					case 22:
						resword = &wordlist[188];
						goto compare;
					case 23:
						resword = &wordlist[189];
						goto compare;
					case 24:
						resword = &wordlist[190];
						goto compare;
					case 25:
						resword = &wordlist[191];
						goto compare;
					case 26:
						resword = &wordlist[192];
						goto compare;
					case 27:
						resword = &wordlist[193];
						goto compare;
					case 28:
						resword = &wordlist[194];
						goto compare;
					case 29:
						resword = &wordlist[195];
						goto compare;
					case 30:
						resword = &wordlist[196];
						goto compare;
					case 31:
						resword = &wordlist[197];
						goto compare;
					case 32:
						resword = &wordlist[198];
						goto compare;
					case 33:
						resword = &wordlist[199];
						goto compare;
					case 34:
						resword = &wordlist[200];
						goto compare;
					case 35:
						resword = &wordlist[201];
						goto compare;
					case 36:
						resword = &wordlist[202];
						goto compare;
					case 37:
						resword = &wordlist[203];
						goto compare;
					case 38:
						resword = &wordlist[204];
						goto compare;
					case 39:
						resword = &wordlist[205];
						goto compare;
					case 40:
						resword = &wordlist[206];
						goto compare;
					case 41:
						resword = &wordlist[207];
						goto compare;
					case 42:
						resword = &wordlist[208];
						goto compare;
					case 43:
						resword = &wordlist[209];
						goto compare;
					case 44:
						resword = &wordlist[210];
						goto compare;
					case 45:
						resword = &wordlist[211];
						goto compare;
					case 46:
						resword = &wordlist[212];
						goto compare;
					case 47:
						resword = &wordlist[213];
						goto compare;
					case 48:
						resword = &wordlist[214];
						goto compare;
					case 78:
						resword = &wordlist[215];
						goto compare;
					case 130:
						resword = &wordlist[216];
						goto compare;
					case 137:
						resword = &wordlist[217];
						goto compare;
					case 149:
						resword = &wordlist[218];
						goto compare;
					case 151:
						resword = &wordlist[219];
						goto compare;
					case 192:
						resword = &wordlist[220];
						goto compare;
				}
			}
		}
		return 0;
compare:
		{
			register const guchar *s = resword->name;

			if (str[0] == s[0] && str[1] == s[1])
				return resword->handler;
		}
	}
	return 0;
}
