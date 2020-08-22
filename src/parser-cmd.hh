/*
 * Copyright © 2015 David Herrmann <dh.herrmann@gmail.com>
 * Copyright © 2018 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#if !defined(_VTE_CMD) || !defined(_VTE_NOP)
#error "Must define _VTE_CMD and _VTE_NOP before including this file"
#endif

/* Implemented in VTE: */

_VTE_CMD(NONE) /* placeholder */
_VTE_CMD(GRAPHIC) /* graphics character */
_VTE_CMD(ACS) /* announce code structure */
_VTE_CMD(BEL) /* bell */
_VTE_CMD(BS) /* backspace */
_VTE_CMD(CBT) /* cursor backward tabulation */
_VTE_CMD(CHA) /* cursor horizontal absolute */
_VTE_CMD(CHT) /* cursor horizontal forward tabulation */
_VTE_CMD(CNL) /* cursor next line */
_VTE_CMD(CPL) /* cursor previous line */
_VTE_CMD(CR) /* carriage return */
_VTE_CMD(CTC) /* cursor tabulation control */
_VTE_CMD(CUB) /* cursor backward */
_VTE_CMD(CUD) /* cursor down */
_VTE_CMD(CUF) /* cursor forward */
_VTE_CMD(CUP) /* cursor position */
_VTE_CMD(CUU) /* cursor up */
_VTE_CMD(CnD) /* Cn-designate */
_VTE_CMD(DA1) /* primary device attributes */
_VTE_CMD(DA2) /* secondary device attributes */
_VTE_CMD(DA3) /* tertiary device attributes */
_VTE_CMD(DCH) /* delete character */
_VTE_CMD(DECALN) /* screen alignment pattern */
_VTE_CMD(DECBI) /* back index */
_VTE_CMD(DECKPAM) /* keypad application mode */
_VTE_CMD(DECKPNM) /* keypad numeric mode */
_VTE_CMD(DECPCTERM_OR_XTERM_RPM) /* pcterm or xterm restore DEC private mode */
_VTE_CMD(DECRC) /* restore cursor */
_VTE_CMD(DECREQTPARM) /* request terminal parameters */
_VTE_CMD(DECRQCRA) /* request checksum of rectangular area */
_VTE_CMD(DECRQM_DEC) /* request mode dec */
_VTE_CMD(DECRQM_ECMA) /* request mode ecma */
_VTE_CMD(DECRQSS) /* request selection or setting */
_VTE_CMD(DECRQTSR) /* request terminal state report */
_VTE_CMD(DECSCL) /* select conformance level */
_VTE_CMD(DECSC) /* save cursor */
_VTE_CMD(DECSCUSR) /* set cursor style */
_VTE_CMD(DECSED) /* selective erase in display */
_VTE_CMD(DECSEL) /* selective erase in line */
_VTE_CMD(DECSGR) /* DEC select graphics rendition */
_VTE_CMD(DECSLPP) /* set lines per page */
_VTE_CMD(DECSLRM_OR_SCOSC) /* set left and right margins or SCO save cursor */
_VTE_CMD(DECSR) /* secure reset */
_VTE_CMD(DECST8C) /* set tab at every 8 columns */
_VTE_CMD(DECSTBM) /* set top and bottom margins */
_VTE_CMD(DECSTR) /* soft terminal reset */
_VTE_CMD(DL) /* delete line */
_VTE_CMD(DOCS) /* designate-other-coding-system */
_VTE_CMD(DSR_DEC) /* device status report dec */
_VTE_CMD(DSR_ECMA) /* device status report ecma */
_VTE_CMD(ECH) /* erase character */
_VTE_CMD(ED) /* erase in display */
_VTE_CMD(EL) /* erase in line */
_VTE_CMD(FF) /* form feed */
_VTE_CMD(GnDm) /* Gn-designate-9m-charset */
_VTE_CMD(GnDMm) /* Gn-designate-multibyte-9m-charset */
_VTE_CMD(HPA) /* horizontal position absolute */
_VTE_CMD(HPR) /* horizontal position relative */
_VTE_CMD(HT) /* horizontal tab */
_VTE_CMD(HTJ) /* character tabulation with justification */
_VTE_CMD(HTS) /* horizontal tab set */
_VTE_CMD(HVP) /* horizontal and vertical position */
_VTE_CMD(ICH) /* insert character */
_VTE_CMD(IL) /* insert line */
_VTE_CMD(IND) /* index */
_VTE_CMD(LF) /* line feed */
_VTE_CMD(LS0) /* locking shift 0 */
_VTE_CMD(LS1) /* locking shift 1 */
_VTE_CMD(LS1R) /* locking shift 1 right */
_VTE_CMD(LS2) /* locking shift 2 */
_VTE_CMD(LS2R) /* locking shift 2 right */
_VTE_CMD(LS3) /* locking shift 3 */
_VTE_CMD(LS3R) /* locking shift 3 right */
_VTE_CMD(NEL) /* next line */
_VTE_CMD(OSC) /* operating-system-command */
_VTE_CMD(REP) /* repeat */
_VTE_CMD(RI) /* reverse index */
_VTE_CMD(RIS) /* reset to initial state */
_VTE_CMD(RM_DEC) /* reset mode dec */
_VTE_CMD(RM_ECMA) /* reset mode ecma */
_VTE_CMD(SCORC) /* SCO restore cursor */
_VTE_CMD(SCOSC) /* SCO save cursor */
_VTE_CMD(SCP) /* select character path */
_VTE_CMD(SD) /* scroll down */
_VTE_CMD(SD_OR_XTERM_IHMT) /* scroll down or xterm initiate highlight mouse tracking */
_VTE_CMD(SGR) /* select graphics rendition */
_VTE_CMD(SM_DEC) /* set mode dec */
_VTE_CMD(SM_ECMA) /* set mode ecma */
_VTE_CMD(SPD) /* select presentation directions */
_VTE_CMD(SS2) /* single shift 2 */
_VTE_CMD(SS3) /* single shift 3 */
_VTE_CMD(SUB) /* substitute */
_VTE_CMD(SU) /* scroll up */
_VTE_CMD(TBC) /* tab clear */
_VTE_CMD(TCC) /* tabulation centred on character */
_VTE_CMD(TSR) /* tabulation stop remove */
_VTE_CMD(VPA) /* vertical line position absolute */
_VTE_CMD(VPR) /* vertical line position relative */
_VTE_CMD(VT) /* vertical tab */
_VTE_CMD(XTERM_RPM) /* xterm restore DEC private mode */
_VTE_CMD(XTERM_SPM) /* xterm save DEC private mode */
_VTE_CMD(XTERM_WM) /* xterm window management */

/* Unimplemented in VTE: */

_VTE_NOP(ACK) /* acknowledge */
_VTE_NOP(BPH) /* break permitted here */
_VTE_NOP(CCH) /* cancel character */
_VTE_NOP(CMD) /* coding method delimiter */
_VTE_NOP(CVT) /* cursor line tabulation */
_VTE_NOP(DAQ) /* define area qualification */
_VTE_NOP(DC1) /* device control 1 / XON */
_VTE_NOP(DC2) /* devince control 2 */
_VTE_NOP(DC3) /* device control 3 / XOFF */
_VTE_NOP(DC4) /* device control 4 */
_VTE_NOP(DECAC) /* assign color */
_VTE_NOP(DECANM) /* ansi mode */
_VTE_NOP(DECARR) /* auto repeat rate */
_VTE_NOP(DECATC) /* alternate text color */
_VTE_NOP(DECAUPSS) /* assign user preferred supplemental sets */
_VTE_NOP(DECCARA) /* change attributes in rectangular area */
_VTE_NOP(DECCKD) /* copy key default */
_VTE_NOP(DECCRA) /* copy rectangular area */
_VTE_NOP(DECCRTST) /* CRT saver time */
_VTE_NOP(DECDC) /* delete column */
_VTE_NOP(DECDHL_BH) /* double width double height line: bottom half */
_VTE_NOP(DECDHL_TH) /* double width double height line: top half */
_VTE_NOP(DECDLDA) /* down line load allocation */
_VTE_NOP(DECDLD) /* dynamically redefinable character sets extension */
_VTE_NOP(DECDMAC) /* define macro */
_VTE_NOP(DECDWL) /* double width single height line */
_VTE_NOP(DECEFR) /* enable filter rectangle */
_VTE_NOP(DECELF) /* enable local functions */
_VTE_NOP(DECELR) /* enable locator reporting */
_VTE_NOP(DECERA) /* erase rectangular area */
_VTE_NOP(DECES) /* enable session */
_VTE_NOP(DECFI) /* forward index */
_VTE_NOP(DECFNK) /* function key */
_VTE_NOP(DECFRA) /* fill rectangular area */
_VTE_NOP(DECIC) /* insert column */
_VTE_NOP(DECINVM) /* invoke macro */
_VTE_NOP(DECKBD) /* keyboard language selection */
_VTE_NOP(DECLANS) /* load answerback message */
_VTE_NOP(DECLBAN) /* load banner message */
_VTE_NOP(DECLBD) /* locator button define */
_VTE_NOP(DECLFKC) /* local function key control */
_VTE_NOP(DECLL) /* load leds */
_VTE_NOP(DECLTOD) /* load time of day */
_VTE_NOP(DECPAK) /* program alphanumeric key */
_VTE_NOP(DECPCTERM) /* pcterm */
_VTE_NOP(DECPFK) /* program function key */
_VTE_NOP(DECPKA) /* program key action */
_VTE_NOP(DECPKFMR) /* program key free memory report */
_VTE_NOP(DECPS) /* play sound */
_VTE_NOP(DECRARA) /* reverse attributes in rectangular area */
_VTE_NOP(DECREGIS) /* ReGIS graphics */
_VTE_NOP(DECRPAK) /* report all modifier/alphanumeric key state */
_VTE_NOP(DECRPDE) /* report displayed extent */
_VTE_NOP(DECRPFK) /* report function key definition */
_VTE_NOP(DECRPKT) /* report key type */
_VTE_NOP(DECRQDE) /* request display extent */
_VTE_NOP(DECRQKT) /* request key type */
_VTE_NOP(DECRQLP) /* request locator position */
_VTE_NOP(DECRQPKFM) /* request program key free memory */
_VTE_NOP(DECRQPSR) /* request presentation state report */
_VTE_NOP(DECRQUPSS) /* request user preferred supplemental set */
_VTE_NOP(DECRSPS) /* restore presentation state */
_VTE_NOP(DECRSTS) /* restore terminal state */
_VTE_NOP(DECSACE) /* select attribute change extent */
_VTE_NOP(DECSASD) /* select active status display */
_VTE_NOP(DECSCA) /* select character protection attribute */
_VTE_NOP(DECSCPP) /* select columns per page */
_VTE_NOP(DECSCP) /* select communication port */
_VTE_NOP(DECSCS) /* select communication speed */
_VTE_NOP(DECSDDT) /* select disconnect delay time */
_VTE_NOP(DECSDPT) /* select digital printed data type */
_VTE_NOP(DECSERA) /* selective erase rectangular area */
_VTE_NOP(DECSEST) /* energy saver time */
_VTE_NOP(DECSFC) /* select flow control */
_VTE_NOP(DECSIXEL) /* SIXEL graphics */
_VTE_NOP(DECSKCV) /* set key click volume */
_VTE_NOP(DECSLCK) /* set lock key style */
_VTE_NOP(DECSLE) /* select locator events */
_VTE_NOP(DECSLRM) /* set left and right margins */
_VTE_NOP(DECSMBV) /* set margin bell volume */
_VTE_NOP(DECSMKR) /* select modifier key reporting */
_VTE_NOP(DECSNLS) /* set lines per screen */
_VTE_NOP(DECSPMA) /* session page memory allocation */
_VTE_NOP(DECSPPCS) /* select pro printer character set */
_VTE_NOP(DECSPP) /* set port parameter */
_VTE_NOP(DECSPRTT) /* select printer type */
_VTE_NOP(DECSRFR) /* select refresh rate */
_VTE_NOP(DECSSCLS) /* set scroll speed */
_VTE_NOP(DECSSDT) /* select status display line type */
_VTE_NOP(DECSSL) /* select setup language */
_VTE_NOP(DECSTGLT) /* select color lookup table */
_VTE_NOP(DECSTRL) /* set transmit rate limit */
_VTE_NOP(DECSTUI) /* set terminal unit id */
_VTE_NOP(DECSWBV) /* set warning bell volume */
_VTE_NOP(DECSWL) /* single width single height line */
_VTE_NOP(DECSZS) /* select zero symbol */
_VTE_NOP(DECTID) /* select terminal id */
_VTE_NOP(DECTME) /* terminal mode emulation */
_VTE_NOP(DECTST) /* invoke confidence test */
_VTE_NOP(DECUDK) /* user defined keys */
_VTE_NOP(DECUS) /* update session */
_VTE_NOP(DLE) /* data link escape */
_VTE_NOP(DMI) /* disable manual input */
_VTE_NOP(DTA) /* dimension text area */
_VTE_NOP(EA) /* erase in area */
_VTE_NOP(EF) /* erase in field */
_VTE_NOP(EM) /* end of medium */
_VTE_NOP(EMI) /* enable manual input */
_VTE_NOP(ENQ) /* enquire */
_VTE_NOP(EOT) /* end of transmission */
_VTE_NOP(EPA) /* end of guarded area */
_VTE_NOP(ESA) /* end of selected area */
_VTE_NOP(ETB) /* end of transmission block */
_VTE_NOP(ETX) /* end of text */
_VTE_NOP(FNK) /* function key */
_VTE_NOP(FNT) /* font selection */
_VTE_NOP(GCC) /* graphic character combination */
_VTE_NOP(GSM) /* graphic size modification */
_VTE_NOP(GSS) /* graphic size selection */
_VTE_NOP(HPB) /* horizontal position backward */
_VTE_NOP(IDCS) /* identify DCS */
_VTE_NOP(IGS) /* identify graphic subrepertoire */
_VTE_NOP(INT) /* interrupt */
_VTE_NOP(IRR) /* identify-revised-registration */
_VTE_NOP(IS1) /* information separator 1 / unit separator (US) */
_VTE_NOP(IS2) /* information separator 2 / record separator (RS) */
_VTE_NOP(IS3) /* information separator 3 / group separator (GS)*/
_VTE_NOP(IS4) /* information separator 4 / file separator (FS) */
_VTE_NOP(JFY) /* justify */
_VTE_NOP(MC_DEC) /* media copy dec */
_VTE_NOP(MC_ECMA) /* media copy ecma */
_VTE_NOP(MW) /* message waiting */
_VTE_NOP(NAK) /* negative acknowledge */
_VTE_NOP(NBH) /* no break permitted here */
_VTE_NOP(NP) /* next page */
_VTE_NOP(NUL) /* nul */
_VTE_NOP(PEC) /* presentation expand or contract */
_VTE_NOP(PFS) /* page format selection */
_VTE_NOP(PLD) /* partial line forward */
_VTE_NOP(PLU) /* partial line backward */
_VTE_NOP(PPA) /* page position absolute */
_VTE_NOP(PPB) /* page position backward */
_VTE_NOP(PP) /* preceding page */
_VTE_NOP(PPR) /* page position relative */
_VTE_NOP(PTX) /* parallel texts */
_VTE_NOP(PU1) /* private use 1 */
_VTE_NOP(PU2) /* private use 2 */
_VTE_NOP(QUAD) /* quad */
_VTE_NOP(RLOGIN_MML) /* RLogin music macro language */
_VTE_NOP(SACS) /* set additional character separation */
_VTE_NOP(SAPV) /* select alternative presentation variants */
_VTE_NOP(SCO) /* select character orientation */
_VTE_NOP(SCS) /* set character spacing */
_VTE_NOP(SDS) /* start directed string */
_VTE_NOP(SEE) /* select editing extent */
_VTE_NOP(SEF) /* sheet eject and feed */
_VTE_NOP(SHS) /* select character spacing */
_VTE_NOP(SIMD) /* select implicit movement direction */
_VTE_NOP(SLH) /* set line home */
_VTE_NOP(SLL) /* set line limit */
_VTE_NOP(SL) /* scroll left */
_VTE_NOP(SLS) /* set line spacing */
_VTE_NOP(SOH) /* start of heading */
_VTE_NOP(SPA) /* start of protected area */
_VTE_NOP(SPH) /* set page home */
_VTE_NOP(SPI) /* spacing increment */
_VTE_NOP(SPL) /* set page limit */
_VTE_NOP(SPQR) /* select print quality and rapidity */
_VTE_NOP(SRCS) /* set reduced character separation */
_VTE_NOP(SR) /* scroll right */
_VTE_NOP(SRS) /* start reversed string */
_VTE_NOP(SSA) /* start of selected area */
_VTE_NOP(SSU) /* set size unit */
_VTE_NOP(SSW) /* set space width */
_VTE_NOP(ST) /* string terminator */
_VTE_NOP(STAB) /* selective tabulation */
_VTE_NOP(STS) /* set transmit state */
_VTE_NOP(STX) /* start of text */
_VTE_NOP(SVS) /* select line spacing */
_VTE_NOP(SYN) /* synchronize */
_VTE_NOP(TAC) /* tabulation aligned centre */
_VTE_NOP(TALE) /* tabulation aligned leading edge */
_VTE_NOP(TATE) /* tabulation aligned trailing edge */
_VTE_NOP(TSS) /* thine space specification */
_VTE_NOP(VTS) /* line tabulation set */
_VTE_NOP(VPB) /* line position backward */
_VTE_NOP(WYCAA) /* redefine character display attribute association */
_VTE_NOP(WYDHL_BH) /* single width double height line: bottom half */
_VTE_NOP(WYDHL_TH) /* single width double height line: top half */
_VTE_NOP(WYLSFNT) /* load soft font */
_VTE_NOP(WYSCRATE) /* set smooth scroll rate */
_VTE_NOP(XDGSYNC) /* synchronous update */
_VTE_NOP(XTERM_CHECKSUM_MODE) /* xterm DECRQCRA checksum mode */
_VTE_NOP(XTERM_IHMT) /* xterm initiate highlight mouse tracking */
_VTE_NOP(XTERM_MLHP) /* xterm memory lock hp bugfix */
_VTE_NOP(XTERM_MUHP) /* xterm memory unlock hp bugfix */
_VTE_NOP(XTERM_PTRMODE) /* xterm set pointer mode */
_VTE_NOP(XTERM_RQTCAP) /* xterm request termcap/terminfo */
_VTE_NOP(XTERM_RRV) /* xterm reset resource value */
_VTE_NOP(XTERM_RTM) /* xterm reset title mode */
_VTE_NOP(XTERM_SGFX) /* xterm sixel graphics */
_VTE_NOP(XTERM_SGR_STACK_POP) /* xterm pop SGR stack */
_VTE_NOP(XTERM_SGR_STACK_PUSH) /* xterm push SGR stack */
_VTE_NOP(XTERM_SGR_REPORT) /* xterm SGR report */
_VTE_NOP(XTERM_SRV) /* xterm set resource value */
_VTE_NOP(XTERM_STCAP) /* xterm set termcap/terminfo */
_VTE_NOP(XTERM_STM) /* xterm set title mode */
