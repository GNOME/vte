/* $Id$ */

#ifndef ESC_H
#define ESC_H 1

#define BEL 0007
#define ESC 0033
#define CSI 0233
#define SS3 0217
#define DCS 0220
#define ST  0234

/* ANSI modes for DECRQM, DECRPM, SM and RM */
#define GATM      1    /* guarded area transfer (disabled) */
#define AM        2    /* keyboard action */
#define CRM       3    /* control representation */
#define IRM       4    /* insert/replace */
#define SRTM      5    /* status reporting transfer (disabled) */
#define VEM       7    /* vertical editing (disabled) */
#define HEM      10    /* horizontal editing (disabled) */
#define PUM      11    /* positioning unit (disabled) */
#define SRM      12    /* send/receive */
#define FEAM     13    /* format effector action (disabled) */
#define FETM     14    /* format effector transfer (disabled) */
#define MATM     15    /* multiple area transfer (disabled) */
#define TTM      16    /* transfer termination (disabled) */
#define SATM     17    /* selected area transfer (disabled) */
#define TSM      18    /* tabulation stop (disabled) */
#define EBM      19    /* editing boundary (disabled) */
#define LNM      20    /* line feed/new line */

/* DEC modes for DECRQM, DECRPM, SM and RM */
#define DECCKM    1    /* cursor keys */
#define DECANM    2    /* ANSI */
#define DECCOLM   3    /* column */
#define DECSCLM   4    /* scrolling */
#define DECSCNM   5    /* screen */
#define DECOM     6    /* origin */
#define DECAWM    7    /* autowrap */
#define DECARM    8    /* autorepeat */
#define DECPFF   18    /* print form feed */
#define DECPEX   19    /* printer extent */
#define DECTCEM  25    /* text cursor enable */
#define DECNRCM  42    /* national replacement character set */
#define DECHCCM  60    /* horizontal cursor coupling */
#define DECVCCM  61    /* vertical cursor coupling */
#define DECPCCM  64    /* page cursor coupling */
#define DECNKM   66    /* numeric keypad */
#define DECBKM   67    /* backarrow key */
#define DECKBUM  68    /* keyboard usage */
#define DECVSSM  69    /* vertical split */
#define DECXRLM  73    /* transmit rate linking */
#define DECKPM   81    /* keyboard positioning */

/* esc.c */
char *csi_input(void);
char *csi_output(void);
char *dcs_input(void);
char *dcs_output(void);
char *get_reply(void);
char *instr(void);
char *osc_input(void);
char *osc_output(void);
char *ss3_input(void);
char *ss3_output(void);
char *st_input(void);
char *st_output(void);
char inchar(void);
void brc(int pn, int c);
void brc2(int pn1, int pn2, int c);
void brc3(int pn1, int pn2, int pn3, int c);
void cbt(int pn);
void cha(int pn);
void cht(int pn);
void cnl(int pn);
void cpl(int pn);
void cub(int pn);
void cud(int pn);
void cuf(int pn);
void cup(int pn1, int pn2);
void cuu(int pn);
void da(void);
void dch(int pn);
void decaln(void);
void decbi(void);
void decbkm(int flag);
void deccara(int top, int left, int bottom, int right, int attr);
void deccolm(int flag);
void deccra(int Pts, int Pl, int Pbs, int Prs, int Pps, int Ptd, int Pld, int Ppd);
void decdc(int pn);
void decdhl(int lower);
void decdwl(void);
void decefr(int top, int left, int bottom, int right);
void decelr(int all_or_one, int pixels_or_cells);
void decera(int top, int left, int bottom, int right);
void decfi(void);
void decfra(int c, int top, int left, int bottom, int right);
void decic(int pn);
void decid(void);
void deckbum(int flag);
void deckpam(void);
void deckpm(int flag);
void deckpnm(void);
void decll(char *ps);
void decnkm(int flag);
void decnrcm(int flag);
void decpex(int flag);
void decpff(int flag);
void decrara(int top, int left, int bottom, int right, int attr);
void decrc(void);
void decreqtparm(int pn);
void decrqlp(int mode);
void decrqss(char *pn);
void decsace(int flag);
void decsasd(int pn);
void decsc(void);
void decsca(int pn1);
void decsclm(int flag);
void decscnm(int flag);
void decsed(int pn1);
void decsel(int pn1);
void decsera(int top, int left, int bottom, int right);
void decsle(int mode);
void decsnls(int pn);
void decssdt(int pn);
void decstbm(int pn1, int pn2);
void decstr(void);
void decswl(void);
void dectst(int pn);
void dl(int pn);
void do_csi(char *fmt, ...) GCC_PRINTFLIKE(1,2);
void do_dcs(char *fmt, ...) GCC_PRINTFLIKE(1,2);
void do_osc(char *fmt, ...) GCC_PRINTFLIKE(1,2);
void dsr(int pn);
void ech(int pn);
void ed(int pn);
void el(int pn);
void esc(char *s);
void extra_padding(int msecs);
void holdit(void);
void hpa(int pn);
void hts(void);
void hvp(int pn1, int pn2);
void ich(int pn);
void il(int pn);
void ind(void);
void inflush(void);
void inputline(char *s);
void mc_autoprint(int flag);
void mc_print_all_pages(void);
void mc_print_composed(void);
void mc_print_cursor_line(void);
void mc_print_page(void);
void mc_printer_assign(int flag);
void mc_printer_controller(int flag);
void mc_printer_start(int flag);
void nel(void);
void padding(int msecs);
void println(char *s);
void put_char(FILE *fp, int c);
void put_string(FILE *fp, char *s);
void readnl(void);
void rep(int pn);
void reset_inchar(void);
void ri(void);
void ris(void);
void rm(char *ps);
void s8c1t(int flag);
void scs(int g, int c);
void sd(int pn);
void sgr(char *ps);
void sl(int pn);
void sm(char *ps);
void sr(int pn);
void srm(int flag);
void su(int pn);
void tbc(int pn);
void vpa(int pn);
void vt52cub1(void);
void vt52cud1(void);
void vt52cuf1(void);
void vt52cup(int l, int c);
void vt52cuu1(void);
void vt52ed(void);
void vt52el(void);
void vt52home(void);
void vt52ri(void);
void zleep(int t);

#endif /* ESC_H */
