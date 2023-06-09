// SPDX-FileCopyrightText: 2023 billow <billow.fun@gmail.com>
// SPDX-License-Identifier: LGPL-3.0-only

#include <rz_il/rz_il_opbuilder_begin.h>

#define LOAD32(addr) LOADW(32, addr)

static inline RzILOpPure *bits(RzILOpBitVector *bv, ut32 i, ut32 n) {
	return LOGAND(SHIFTR0(bv, U32(i)), U32((1 << n) - 1));
}
static inline RzILOpPure *bit(RzILOpBitVector *bv, ut32 i) {
	return bits(bv, i, 1);
}
static inline RzILOpPure *bits_updated(RzILOpPure *val, ut32 i, ut32 n, RzILOpPure *x) {
	ut32 mask = (1 << n) - 1;
	return LOGOR(LOGAND(val, U32(~(mask << i))), SHIFTL0((LOGAND(x, U32(mask))), U32(i)));
}
static inline RzILOpPure *bits_updated_v(RzILOpPure *val, RzILOpPure *i, RzILOpPure *n, RzILOpPure *x) {
	RzILOpPure *mask = SUB(SHIFTL0(U32(1), n), U32(1));
	return LOGOR(LOGAND(val, LOGNOT(SHIFTL0(mask, i))), SHIFTL0((LOGAND(x, DUP(mask))), DUP(i)));
}

static inline RzILOpEffect *setg_bits(const char *name, ut32 i, ut32 n, RzILOpPure *x) {
	return SETG(name, bits_updated(VARG(name), i, n, x));
}
static inline RzILOpEffect *setg_bit(const char *name, ut32 i, RzILOpBitVector *x) {
	return setg_bits(name, i, 1, x);
}

static inline RzILOpEffect *setg_bits_v(const char *name, RzILOpPure *i, RzILOpPure *n, RzILOpPure *x) {
	return SETG(name, bits_updated_v(VARG(name), i, n, x));
}
static inline RzILOpEffect *setg_bit_v(const char *name, RzILOpPure *i, RzILOpBitVector *x) {
	return setg_bits_v(name, i, U32(1), x);
}

#define REG_FIELD(regname, fieldname, i, n) \
	static inline RzILOpPure *regname##_##fieldname() { \
		return bits(VARG((#regname)), (i), (n)); \
	} \
	static inline RzILOpEffect *set_##regname##_##fieldname(RzILOpPure *x) { \
		return setg_bits((#regname), (i), (n), (x)); \
	}

#define REG_FIELD_V(v, regname, fieldname, i, n) \
	static inline RzILOpPure *regname##_##fieldname##_v##v() { \
		return bits(VARG((#regname)), (i), (n)); \
	} \
	static inline RzILOpEffect *set_##regname##_##fieldname##_v##v(RzILOpPure *x) { \
		return setg_bits((#regname), (i), (n), (x)); \
	}

#define VAR_PSW VARG("PSW") ///<  Program Status Word Register
REG_FIELD(PSW, C, 31, 1)
REG_FIELD(PSW, V, 30, 1)
REG_FIELD(PSW, SV, 29, 1)
REG_FIELD(PSW, AV, 28, 1)
REG_FIELD(PSW, SAV, 27, 1)
REG_FIELD(PSW, PRS, 12, 2)
REG_FIELD(PSW, IO, 10, 2)
REG_FIELD(PSW, IS, 9, 1)
REG_FIELD(PSW, GW, 8, 1)
REG_FIELD(PSW, CDE, 7, 1)
REG_FIELD(PSW, CDC, 0, 7)

#define VAR_FCX VARG("FCX") ///< Previous Context Pointer Register.
REG_FIELD(FCX, FCXS, 16, 4)
REG_FIELD(FCX, FCXO, 0, 15)

#define VAR_PCXI VARG("PCXI") ///< Previous Context Information Register.
REG_FIELD(PCXI, PCXS, 16, 4)
REG_FIELD(PCXI, PCXO, 0, 16)
// tc162
REG_FIELD_V(162, PCXI, PCPN, 22, 8)
REG_FIELD_V(162, PCXI, PIE, 21, 1)
REG_FIELD_V(162, PCXI, UL, 20, 1)
// tc160
REG_FIELD_V(160, PCXI, PCPN, 24, 8)
REG_FIELD_V(160, PCXI, PIE, 23, 1)
REG_FIELD_V(160, PCXI, UL, 22, 1)

#define VAR_LCX VARG("LCX")

#define VAR_ICR VARG("ICR") ///< ICU Interrupt Control Register
REG_FIELD(ICR, CCPN, 0, 8)
REG_FIELD(ICR, IE, 8, 1)
REG_FIELD(ICR, PIPN, 16, 8)

enum trap_kind_t {
	/// Class 0 — MMU
	VAF, ///< Virtual Address Fill.
	VAP, ///< Virtual Address Protection.

	/// Class 1 — Internal Protection Traps
	PRIV, ///< Privileged Instruction.
	MPR, ///< Memory Protection Read.
	MPW, ///< Memory Protection Write.
	MPX, ///< Memory Protection Execution.
	MPP, ///< Memory Protection Peripheral Access.
	MPN, ///< Memory Protection Null Address.
	GRWP, ///< Global Register Write Protection.

	/// Class 2 — Instruction Errors
	IOPC, ///< Illegal Opcode.
	UOPC, ///< Unimplemented Opcode.
	OPD, ///< Invalid Operand specification.
	ALN, ///< Data Address Alignment.
	MEM, ///< Invalid Local Memory Address.

	/// Class 3 — Context Management
	FCD, ///< Free Context List Depletion (FCX = LCX).
	CDO, ///< Call Depth Overflow.
	CDU, ///< Call Depth Underflow.
	FCU, ///< Free Context List Underflow (FCX = 0).
	CSU, ///< Call Stack Underflow (PCX = 0).
	CTYP, ///< Context Type (PCXI.UL wrong).
	NEST, ///< Nesting Error: RFE with non-zero call depth.

	/// Class 4 — System Bus and Peripheral Errors
	PSE, ///< Program Fetch Synchronous Error.
	DSE, ///< Data Access Synchronous Error.
	DAE, ///< Data Access Asynchronous Error.
	CAE, ///< Coprocessor Trap Asynchronous Error.
	PIE, ///< Program Memory Integrity Error.
	DIE, ///< Data Memory Integrity Error.
	TAE, ///< Temporal Asynchronous Error

	/// Class 5 — Assertion Traps
	OVF, ///< Arithmetic Overflow.
	SOVF, ///< Sticky Arithmetic Overflow.

	/// Class 6 — System Call
	SYS, ///< System Call.

	/// Class 7 — Non-Maskable Interrupt
	NMI ///< Non-Maskable Interrupt.
};

static inline cs_tricore_op *get_op(cs_insn *insn, int idx) {
	if (idx >= insn->detail->tricore.op_count) {
		return NULL;
	}
	return &insn->detail->tricore.operands[idx];
}

static inline const char *get_regname(csh handle, cs_insn *insn, int idx) {
	return cs_reg_name(handle, get_op(insn, 0)->reg);
}

static inline RzILOpPure *get_reg(csh handle, cs_insn *insn, int idx) {
	return VARG(get_regname(handle, insn, idx));
}

static inline RzILOpEffect *trap(enum trap_kind_t kind) {
	return GOTO("trap");
}

static inline RzILOpPure *PSW_CDC_COUNT() {
	return LET("CDC", PSW_CDC(),
		ITE(EQ(bits(VARLP("CDC"), 6, 1), U32(0)),
			bits(VARLP("CDC"), 0, 6),
			ITE(EQ(bits(VARLP("CDC"), 5, 2), U32(0b10)),
				bits(VARLP("CDC"), 0, 5),
				ITE(EQ(bits(VARLP("CDC"), 4, 3), U32(0b110)),
					bits(VARLP("CDC"), 0, 4),
					ITE(EQ(bits(VARLP("CDC"), 3, 4), U32(0b1110)),
						bits(VARLP("CDC"), 0, 3),
						ITE(EQ(bits(VARLP("CDC"), 2, 5), U32(0b11110)),
							bits(VARLP("CDC"), 0, 2),
							ITE(EQ(bits(VARLP("CDC"), 1, 6), U32(0b111110)),
								bits(VARLP("CDC"), 0, 1),
								U32(0))))))));
}

static inline RzILOpPure *PSW_CDC_COUNT_LEN() {
	return LET("CDC", PSW_CDC(),
		ITE(EQ(bits(VARLP("CDC"), 6, 1), U32(0b0)),
			U32(6),
			ITE(EQ(bits(VARLP("CDC"), 5, 2), U32(0b10)),
				U32(5),
				ITE(EQ(bits(VARLP("CDC"), 4, 3), U32(0b110)),
					U32(4),
					ITE(EQ(bits(VARLP("CDC"), 3, 4), U32(0b1110)),
						U32(3),
						ITE(EQ(bits(VARLP("CDC"), 2, 5), U32(0b11110)),
							U32(2),
							ITE(EQ(bits(VARLP("CDC"), 1, 6), U32(0b111110)),
								U32(1),
								U32(0))))))));
}

static inline RzILOpEffect *set_PSW_CDC_COUNT(RzILOpPure *val, RzILOpEffect *overflow, RzILOpEffect *underflow) {
	return SEQ5(
		SETL("CDC", PSW_CDC()),
		SETL("CDC_COUNT", PSW_CDC_COUNT()),
		SETL("CDC_i", PSW_CDC_COUNT_LEN()),
		setg_bits_v("PSW", U32(0), VARL("CDC_i"), val),
		overflow ? (BRANCH(EQ(VARL("CDC_COUNT"), SUB(SHIFTL0(U32(1), VARL("CDC_i")), U32(1))), overflow, NOP()))
			 : (underflow ? BRANCH(EQ(VARL("CDC_COUNT"), U32(0x0)), underflow, NOP())
				      : NOP()));
}

/**
 * If PSW.CDC == 7’b1111111 returns FALSE, otherwise decrements
 * PSW.CDC.COUNT and returns TRUE if PSW.CDC.COUNT underflows,
 * otherwise returns FALSE
 */
static inline RzILOpEffect *cdc_decrement(RzILOpEffect *eff_true) {
	RzILOpEffect *dec = set_PSW_CDC_COUNT(SUB(PSW_CDC_COUNT(), U32(1)), NULL, eff_true);
	return BRANCH(EQ(PSW_CDC(), U32(0x7F)), dec, NOP());
}

/**
 * If PSW.CDC == 7'b1111111 returns FALSE, otherwise increments
 * PSW.CDC.COUNT and returns TRUE if PSW.CDC.COUNT overflows, otherwise
 * returns FALSE
 */
static inline RzILOpEffect *cdc_increment(RzILOpEffect *eff_true) {
	RzILOpEffect *inc = set_PSW_CDC_COUNT(ADD(PSW_CDC_COUNT(), U32(1)), eff_true, NULL);
	return BRANCH(EQ(PSW_CDC(), U32(0x7F)), inc, NOP());
}

/**
 * Returns TRUE if PCW.CDC.COUNT == 0 or if PSW.CDC == 7'b1111111,
 * otherwise returns FALSE
 */
static inline RzILOpPure *cdc_zero() {
	return OR(EQ(PSW_CDC(), U32(0x7F)), EQ(PSW_CDC_COUNT(), U32(0)));
}

static RzAnalysisLiftedILOp fast_call(cs_insn *insn, cs_mode mode, RzILOpPure *target) {
	ut64 ret = insn->address + 4;
	RzILOpEffect *EA = SETL("EA", SUB(VARG("a10"), U32(4)));
	RzILOpEffect *M = STOREW(VARL("EA"), VARG("a11"));
	RzILOpEffect *jmp = JMP(target);
	RzILOpEffect *a11 = SETG("a11", U32(ret));
	RzILOpEffect *a10 = SETG("a10", VARL("EA"));
	return SEQ5(EA, M, a11, a10, jmp);
}

static RzAnalysisLiftedILOp call_abs(cs_insn *insn, cs_mode mode, RzILOpPure *target) {
	RzILOpEffect *cde = set_PSW_CDE(U32(1));

	RzILOpEffect *tmp_FCX = SETL("tmp_FCX", VAR_FCX);

	RzILOpEffect *EA = SETL("EA", LOGAND(SHIFTL0(FCX_FCXS(), U32(28)), SHIFTL0(FCX_FCXO(), U32(6))));

	RzILOpEffect *new_FCX = SETL("new_FCX", LOADW(32, VARL("EA")));

	RzILOpEffect *M = SEQN(16,
		STOREW(VARL("EA"), VARG("d15")),
		STOREW(ADD(VARL("EA"), U32(1)), VARG("d14")),
		STOREW(ADD(VARL("EA"), U32(2)), VARG("d13")),
		STOREW(ADD(VARL("EA"), U32(3)), VARG("d12")),
		STOREW(ADD(VARL("EA"), U32(4)), VARG("a15")),
		STOREW(ADD(VARL("EA"), U32(5)), VARG("a14")),
		STOREW(ADD(VARL("EA"), U32(6)), VARG("a13")),
		STOREW(ADD(VARL("EA"), U32(7)), VARG("a12")),
		STOREW(ADD(VARL("EA"), U32(8)), VARG("d11")),
		STOREW(ADD(VARL("EA"), U32(9)), VARG("d10")),
		STOREW(ADD(VARL("EA"), U32(10)), VARG("d9")),
		STOREW(ADD(VARL("EA"), U32(11)), VARG("d8")),
		STOREW(ADD(VARL("EA"), U32(12)), VARG("a11")),
		STOREW(ADD(VARL("EA"), U32(13)), VARG("a10")),
		STOREW(ADD(VARL("EA"), U32(14)), VAR_PSW),
		STOREW(ADD(VARL("EA"), U32(15)), VAR_PCXI));

	RzILOpEffect *pcxi = NULL;
	switch (mode) {
	case CS_MODE_TRICORE_162: {
		pcxi = SEQ4(set_PCXI_PCPN_v162(ICR_CCPN()),
			set_PCXI_PCPN_v162(ICR_IE()),
			set_PCXI_PCPN_v162(U32(1)),
			setg_bits("PCXI", 0, 20, bits(VARG("FCX"), 0, 20)));
		break;
	}
	case CS_MODE_TRICORE_160: {
		pcxi = SEQ4(set_PCXI_PCPN_v160(ICR_CCPN()),
			set_PCXI_PCPN_v160(ICR_IE()),
			set_PCXI_PCPN_v160(U32(1)),
			setg_bits("PCXI", 0, 20, bits(VARL("new_FCX"), 0, 20)));
		break;
	}
	default: {
		RZ_LOG_ERROR("Unknown tricore version: %d\n", mode);
		return NULL;
	}
	}

	RzILOpEffect *fcx = setg_bits("FCX", 0, 20, bits(VARL("new_FCX"), 0, 20));

	RzILOpEffect *jmp = JMP(target);

	ut64 ret = insn->address + 4;
	RzILOpEffect *a11 = SETG("a11", U32(ret));

	return BRANCH(IS_ZERO(VAR_FCX), trap(FCU),
		BRANCH(NON_ZERO(PSW_CDE()), cdc_increment(trap(CDO)),
			SEQ9(cde, tmp_FCX, EA, new_FCX, M, pcxi, fcx, a11,
				BRANCH(EQ(VARL("new_FCX"), VAR_LCX), trap(FCD), jmp))));
}

static RzAnalysisLiftedILOp fret() {
	RzILOpEffect *PC = JMP(SHIFTL0(VARL("A11"), U32(1)));
	RzILOpEffect *EA = SETL("EA", VARG("a10"));
	RzILOpEffect *A11 = SETG("a11", LOAD32(VARL("EA")));
	RzILOpEffect *A10 = SETG("a10", ADD(VARG("a10"), U32(4)));
	return SEQ5(SETL("A11", VARG("a11")), EA, A11, A10, PC);
}

static RzILOpPure *is_inf(RzILOpPure *val) {
	// TODO: check for float Inf
	return IL_FALSE;
}

static RzILOpPure *is_nan(RzILOpPure *val) {
	// TODO: check for float NaN
	return IL_FALSE;
}

static RzILOpPure *sign_32bit(RzILOpPure *val) {
	// TODO: check for signed infinity
	return IL_FALSE;
}

static const ut32 ADD_NAN = 0x7fc00001;
static const ut32 DIV_NAN = 0x7fc00008;
static const ut32 MUL_NAN = 0x7fc00002;
static const ut32 SQRT_NAN = 0x7fc00004;

static const ut32 HP_MAX_VALUE = 65504;
static const ut32 HP_MIN_NORMAL = 1024 * 16;

static const ut32 HP_NEG_INFINITY = 0xfc00;
static const ut32 HP_POS_INFINITY = 0x7c00;

static const ut32 NEG_INFINITY = 0xff800000;
static const ut32 POS_INFINITY = 0x7f800000;

/**
 * \brief Convert single precision to a half precision
 *
 * Convert the contents of data register D[a] from IEEE-754-2008 32-bit single precision floating point to IEEE-754-2008
 * 16-bit half precision (data interchange) floating point format. The rounded result is put in data register D[c][15:0].
 * D[c][31:16] is set to zero.
 */
static RzAnalysisLiftedILOp ftohp(csh handle, cs_insn *insn) {
	RzILOpPure *a = get_reg(handle, insn, 1);
	const char *cname = get_regname(handle, insn, 0);

	// if(sign_32bit(D[a])) then {
	//	D[c][15:0] = HP_NEG_INFINITY;
	// } else {
	//	D[c][15:0] = HP_POS_INFINITY;
	// }
	RzILOpEffect *inf_eff = BRANCH(sign_32bit(DUP(a)),
		SETG(cname, U32(HP_NEG_INFINITY)),
		SETG(cname, U32(HP_POS_INFINITY)));

	// D[c][15] = sign_32bit(D[a]);
	// D[c][14:10] = 1FH;
	// D[c][9:8] = D[a][22:21];
	// D[c][7:0] = D[a][7:0];
	//// Ensure NaN value is preserved
	// if ((D[c][9:0] == 0)) then {
	//	D[c][8] = 1B;
	// }
	RzILOpPure *c = SHIFTL0(BOOL_TO_BV(sign_32bit(DUP(a)), 32), U32(15));
	c = LOGOR(c, SHIFTL0(U32(0x1f), U32(10)));
	c = LOGOR(c, SHIFTL0(bits(DUP(a), 21, 2), U32(8)));
	c = LOGOR(c, bits(DUP(a), 0, 8));
	RzILOpEffect *nan_eff = SETG(cname,
		ITE(EQ(bits(DUP(c), 0, 10), U32(0)), bits_updated(c, 8, 1, U32(1)), DUP(c)));

	// TODO: ftohp/else_eff
	// f = denorm_to_zero(D[a]);
	// f_rounded = ieee754_round_16bit(f, PSW.RM);
	//// Handle overflow & underflow and convert to 16-bit format
	// D[a][15:0] = ieee754_16bit_format(f_rounded);
	RzILOpEffect *else_eff = SETG(cname, U32(0));

	return BRANCH(is_inf(a), inf_eff,
		BRANCH(is_nan(a), nan_eff, else_eff));
}

static RzAnalysisLiftedILOp rz_analysis_tricore_il_op(RzAnalysis *a, cs_insn *insn, csh handle, cs_mode mode) {
	switch (insn->id) {
	default: {
		break;
	}
	case TRICORE_INS_FCALLI: {
		return fast_call(insn, mode, VARG(cs_reg_name(handle, get_op(insn, 0)->reg)));
	}
	case TRICORE_INS_FCALLA:
	case TRICORE_INS_FCALL: {
		return fast_call(insn, mode, U32(get_op(insn, 0)->imm));
	}
	case TRICORE_INS_CALLI: {
		return call_abs(insn, mode, VARG(cs_reg_name(handle, get_op(insn, 0)->reg)));
	}
	case TRICORE_INS_CALLA:
	case TRICORE_INS_CALL: {
		return call_abs(insn, mode, U32(get_op(insn, 0)->imm));
	}
	case TRICORE_INS_FRET: {
		return fret();
	}
	case TRICORE_INS_FTOHP: return ftohp(handle, insn);
	case TRICORE_INS_FTOIZ:
	case TRICORE_INS_FTOI:
	case TRICORE_INS_FTOQ31Z:
	case TRICORE_INS_FTOQ31:
	case TRICORE_INS_FTOUZ:
	case TRICORE_INS_FTOU:
	case TRICORE_INS_HPTOF:
	case TRICORE_INS_ITOF:
	case TRICORE_INS_Q31TOF:
	case TRICORE_INS_UTOF: {
		break;
	}
	case TRICORE_INS_CMP_F: {
		break;
	}
	case TRICORE_INS_DIV_F: {
		break;
	}
	case TRICORE_INS_ADD_F:
	case TRICORE_INS_MADD_F: {
		break;
	}
	case TRICORE_INS_MSUB_F:
	case TRICORE_INS_SUB_F: {
		break;
	}
	case TRICORE_INS_MUL_F: {
		break;
	}
	case TRICORE_INS_QSEED_F:
	case TRICORE_INS_UPDFL:
	case TRICORE_INS_UNPACK:
	case TRICORE_INS_PACK: {
		break;
	}
	case TRICORE_INS_ABSDIFS_B:
	case TRICORE_INS_ABSDIFS_H:
	case TRICORE_INS_ABSDIFS:
	case TRICORE_INS_ABSDIF_B:
	case TRICORE_INS_ABSDIF_H:
	case TRICORE_INS_ABSDIF:
	case TRICORE_INS_ABSS_B:
	case TRICORE_INS_ABSS_H:
	case TRICORE_INS_ABSS:
	case TRICORE_INS_ABS_B:
	case TRICORE_INS_ABS_H:
	case TRICORE_INS_ABS: {
		break;
	}
	case TRICORE_INS_ADDC:
	case TRICORE_INS_ADDIH_A:
	case TRICORE_INS_ADDIH:
	case TRICORE_INS_ADDI:
	case TRICORE_INS_ADDSC_AT:
	case TRICORE_INS_ADDSC_A:
	case TRICORE_INS_ADDS_BU:
	case TRICORE_INS_ADDS_B:
	case TRICORE_INS_ADDS_H:
	case TRICORE_INS_ADDS_HU:
	case TRICORE_INS_ADDS_U:
	case TRICORE_INS_ADDS:
	case TRICORE_INS_ADDX:
	case TRICORE_INS_ADD_A:
	case TRICORE_INS_ADD_B:
	case TRICORE_INS_ADD_H:
	case TRICORE_INS_ADD:
	case TRICORE_INS_CADDN_A:
	case TRICORE_INS_CADDN:
	case TRICORE_INS_CADD_A:
	case TRICORE_INS_CADD: {
		break;
	}
	case TRICORE_INS_ANDN_T:
	case TRICORE_INS_ANDN:
	case TRICORE_INS_AND_ANDN_T:
	case TRICORE_INS_AND_AND_T:
	case TRICORE_INS_AND_EQ:
	case TRICORE_INS_AND_GE_U:
	case TRICORE_INS_AND_GE:
	case TRICORE_INS_AND_LT_U:
	case TRICORE_INS_AND_LT:
	case TRICORE_INS_AND_NE:
	case TRICORE_INS_AND_NOR_T:
	case TRICORE_INS_AND_OR_T:
	case TRICORE_INS_AND_T:
	case TRICORE_INS_AND: {
		break;
	}
	case TRICORE_INS_BISR:
	case TRICORE_INS_SYSCALL:
	case TRICORE_INS_DISABLE:
	case TRICORE_INS_ENABLE:
	case TRICORE_INS_SVLCX:
	case TRICORE_INS_WAIT:
	case TRICORE_INS_RESTORE: {
		break;
	}
	case TRICORE_INS_CACHEA_I:
	case TRICORE_INS_CACHEA_WI:
	case TRICORE_INS_CACHEA_W:
	case TRICORE_INS_CACHEI_I:
	case TRICORE_INS_CACHEI_WI:
	case TRICORE_INS_CACHEI_W:
	case TRICORE_INS_CLO_B:
	case TRICORE_INS_CLO_H:
	case TRICORE_INS_CLO:
	case TRICORE_INS_CLS_B:
	case TRICORE_INS_CLS_H:
	case TRICORE_INS_CLS:
	case TRICORE_INS_CLZ_B:
	case TRICORE_INS_CLZ_H:
	case TRICORE_INS_CLZ: {
		// Supervisor mode only
		break;
	}
	case TRICORE_INS_CRC32B_W:
	case TRICORE_INS_CRC32L_W:
	case TRICORE_INS_CRC32_B:
	case TRICORE_INS_CRCN: {
		break;
	}
	case TRICORE_INS_DIV_U:
	case TRICORE_INS_DIV: {
		break;
	}
	case TRICORE_INS_DEBUG:
	case TRICORE_INS_NOP: {
		break;
	}
	case TRICORE_INS_NOR_T:
	case TRICORE_INS_NOR: {
		break;
	}
	case TRICORE_INS_DEXTR:
	case TRICORE_INS_EXTR_U:
	case TRICORE_INS_EXTR:
	case TRICORE_INS_INSERT:
	case TRICORE_INS_INSN_T:
	case TRICORE_INS_INS_T: {
		break;
	}
	case TRICORE_INS_DIFSC_A: {
		break;
	}
	case TRICORE_INS_DSYNC: {
		break;
	}
	case TRICORE_INS_DVADJ:
	case TRICORE_INS_DVINIT_BU:
	case TRICORE_INS_DVINIT_B:
	case TRICORE_INS_DVINIT_HU:
	case TRICORE_INS_DVINIT_H:
	case TRICORE_INS_DVINIT_U:
	case TRICORE_INS_DVINIT:
	case TRICORE_INS_DVSTEP_U:
	case TRICORE_INS_DVSTEP:
	case TRICORE_INS_IMASK: {
		break;
	}
	case TRICORE_INS_ISYNC: {
		break;
	}
	case TRICORE_INS_IXMAX_U:
	case TRICORE_INS_IXMAX:
	case TRICORE_INS_IXMIN_U:
	case TRICORE_INS_IXMIN: {
		break;
	}
	case TRICORE_INS_JA:
	case TRICORE_INS_JEQ_A:
	case TRICORE_INS_JEQ:
	case TRICORE_INS_JGEZ:
	case TRICORE_INS_JGE_U:
	case TRICORE_INS_JGE:
	case TRICORE_INS_JGTZ:
	case TRICORE_INS_JI:
	case TRICORE_INS_JLA:
	case TRICORE_INS_JLEZ:
	case TRICORE_INS_JLI:
	case TRICORE_INS_JLTZ:
	case TRICORE_INS_JLT_U:
	case TRICORE_INS_JLT:
	case TRICORE_INS_JL:
	case TRICORE_INS_JNED:
	case TRICORE_INS_JNEI:
	case TRICORE_INS_JNE_A:
	case TRICORE_INS_JNE:
	case TRICORE_INS_JNZ_A:
	case TRICORE_INS_JNZ_T:
	case TRICORE_INS_JNZ:
	case TRICORE_INS_JZ_A:
	case TRICORE_INS_JZ_T:
	case TRICORE_INS_JZ:
	case TRICORE_INS_J: {
		break;
	}
	case TRICORE_INS_LDLCX:
	case TRICORE_INS_LDMST:
	case TRICORE_INS_LDUCX:
	case TRICORE_INS_LD_A:
	case TRICORE_INS_LD_BU:
	case TRICORE_INS_LD_B:
	case TRICORE_INS_LD_DA:
	case TRICORE_INS_LD_D:
	case TRICORE_INS_LD_HU:
	case TRICORE_INS_LD_H:
	case TRICORE_INS_LD_Q:
	case TRICORE_INS_LD_W: {
		break;
	}
	case TRICORE_INS_LEA:
	case TRICORE_INS_LHA: {
		break;
	}
	case TRICORE_INS_LOOPU:
	case TRICORE_INS_LOOP: {
		break;
	}
	case TRICORE_INS_LT_A:
	case TRICORE_INS_LT_B:
	case TRICORE_INS_LT_BU:
	case TRICORE_INS_LT_H:
	case TRICORE_INS_LT_HU:
	case TRICORE_INS_LT_U:
	case TRICORE_INS_LT_W:
	case TRICORE_INS_LT_WU:
	case TRICORE_INS_LT:
	case TRICORE_INS_GE_A:
	case TRICORE_INS_GE_U:
	case TRICORE_INS_GE:
	case TRICORE_INS_EQANY_B:
	case TRICORE_INS_EQANY_H:
	case TRICORE_INS_EQZ_A:
	case TRICORE_INS_EQ_A:
	case TRICORE_INS_EQ_B:
	case TRICORE_INS_EQ_H:
	case TRICORE_INS_EQ_W:
	case TRICORE_INS_EQ:
	case TRICORE_INS_CMPSWAP_W: {
		break;
	}
	case TRICORE_INS_MADDMS_H:
	case TRICORE_INS_MADDMS_U:
	case TRICORE_INS_MADDMS:
	case TRICORE_INS_MADDM_H:
	case TRICORE_INS_MADDM_Q:
	case TRICORE_INS_MADDM_U:
	case TRICORE_INS_MADDM:
	case TRICORE_INS_MADDRS_H:
	case TRICORE_INS_MADDRS_Q:
	case TRICORE_INS_MADDR_H:
	case TRICORE_INS_MADDR_Q:
	case TRICORE_INS_MADDSUMS_H:
	case TRICORE_INS_MADDSUM_H:
	case TRICORE_INS_MADDSURS_H:
	case TRICORE_INS_MADDSUR_H:
	case TRICORE_INS_MADDSUS_H:
	case TRICORE_INS_MADDSU_H:
	case TRICORE_INS_MADDS_H:
	case TRICORE_INS_MADDS_Q:
	case TRICORE_INS_MADDS_U:
	case TRICORE_INS_MADDS:
	case TRICORE_INS_MADD_H:
	case TRICORE_INS_MADD_Q:
	case TRICORE_INS_MADD_U:
	case TRICORE_INS_MADD: {
		break;
	}
	case TRICORE_INS_MAX_B:
	case TRICORE_INS_MAX_BU:
	case TRICORE_INS_MAX_H:
	case TRICORE_INS_MAX_HU:
	case TRICORE_INS_MAX_U:
	case TRICORE_INS_MAX:
	case TRICORE_INS_MIN_B:
	case TRICORE_INS_MIN_BU:
	case TRICORE_INS_MIN_H:
	case TRICORE_INS_MIN_HU:
	case TRICORE_INS_MIN_U:
	case TRICORE_INS_MIN: {
		break;
	}
	case TRICORE_INS_MOVH_A:
	case TRICORE_INS_MOVH:
	case TRICORE_INS_MOVZ_A:
	case TRICORE_INS_MOV_AA:
	case TRICORE_INS_MOV_A:
	case TRICORE_INS_MOV_D:
	case TRICORE_INS_MOV_U:
	case TRICORE_INS_MOV:
	case TRICORE_INS_CMOVN:
	case TRICORE_INS_CMOV:
	case TRICORE_INS_MFCR:
	case TRICORE_INS_MTCR:
	case TRICORE_INS_BMERGE:
	case TRICORE_INS_BSPLIT:
	case TRICORE_INS_SHUFFLE: {
		break;
	}
	case TRICORE_INS_MSUBADMS_H:
	case TRICORE_INS_MSUBADM_H:
	case TRICORE_INS_MSUBADRS_H:
	case TRICORE_INS_MSUBADR_H:
	case TRICORE_INS_MSUBADS_H:
	case TRICORE_INS_MSUBAD_H:
	case TRICORE_INS_MSUBMS_H:
	case TRICORE_INS_MSUBMS_U:
	case TRICORE_INS_MSUBMS:
	case TRICORE_INS_MSUBM_H:
	case TRICORE_INS_MSUBM_Q:
	case TRICORE_INS_MSUBM_U:
	case TRICORE_INS_MSUBM:
	case TRICORE_INS_MSUBRS_H:
	case TRICORE_INS_MSUBRS_Q:
	case TRICORE_INS_MSUBR_H:
	case TRICORE_INS_MSUBR_Q:
	case TRICORE_INS_MSUBS_H:
	case TRICORE_INS_MSUBS_Q:
	case TRICORE_INS_MSUBS_U:
	case TRICORE_INS_MSUBS:
	case TRICORE_INS_MSUB_H:
	case TRICORE_INS_MSUB_Q:
	case TRICORE_INS_MSUB_U:
	case TRICORE_INS_MSUB:
	case TRICORE_INS_CSUBN_A:
	case TRICORE_INS_CSUBN:
	case TRICORE_INS_CSUB_A:
	case TRICORE_INS_CSUB:
	case TRICORE_INS_SUBC:
	case TRICORE_INS_SUBSC_A:
	case TRICORE_INS_SUBS_BU:
	case TRICORE_INS_SUBS_B:
	case TRICORE_INS_SUBS_HU:
	case TRICORE_INS_SUBS_H:
	case TRICORE_INS_SUBS_U:
	case TRICORE_INS_SUBS:
	case TRICORE_INS_SUBX:
	case TRICORE_INS_SUB_A:
	case TRICORE_INS_SUB_B:
	case TRICORE_INS_SUB_H:
	case TRICORE_INS_SUB:
	case TRICORE_INS_RSUBS_U:
	case TRICORE_INS_RSUBS:
	case TRICORE_INS_RSUB: {
		break;
	}
	case TRICORE_INS_MULMS_H:
	case TRICORE_INS_MULM_H:
	case TRICORE_INS_MULM_U:
	case TRICORE_INS_MULM:
	case TRICORE_INS_MULR_H:
	case TRICORE_INS_MULR_Q:
	case TRICORE_INS_MULS_U:
	case TRICORE_INS_MULS:
	case TRICORE_INS_MUL_H:
	case TRICORE_INS_MUL_Q:
	case TRICORE_INS_MUL_U:
	case TRICORE_INS_MUL: {
		break;
	}
	case TRICORE_INS_NAND_T:
	case TRICORE_INS_NAND:
	case TRICORE_INS_NEZ_A:
	case TRICORE_INS_NE_A:
	case TRICORE_INS_NE: {
		break;
	}
	case TRICORE_INS_NOT: {
		break;
	}
	case TRICORE_INS_ORN_T:
	case TRICORE_INS_ORN:
	case TRICORE_INS_OR_ANDN_T:
	case TRICORE_INS_OR_AND_T:
	case TRICORE_INS_OR_EQ:
	case TRICORE_INS_OR_GE_U:
	case TRICORE_INS_OR_GE:
	case TRICORE_INS_OR_LT_U:
	case TRICORE_INS_OR_LT:
	case TRICORE_INS_OR_NE:
	case TRICORE_INS_OR_NOR_T:
	case TRICORE_INS_OR_OR_T:
	case TRICORE_INS_OR_T:
	case TRICORE_INS_OR: {
		break;
	}
	case TRICORE_INS_PARITY:
	case TRICORE_INS_POPCNT_W: {
		break;
	}
	case TRICORE_INS_RET:
	case TRICORE_INS_RFE:
	case TRICORE_INS_RFM: {
		break;
	}
	case TRICORE_INS_RSLCX:
	case TRICORE_INS_RSTV:
	case TRICORE_INS_SAT_BU:
	case TRICORE_INS_SAT_B:
	case TRICORE_INS_SAT_HU:
	case TRICORE_INS_SAT_H: {
		break;
	}
	case TRICORE_INS_SELN_A:
	case TRICORE_INS_SELN:
	case TRICORE_INS_SEL_A:
	case TRICORE_INS_SEL:
	case TRICORE_INS_SHAS:
	case TRICORE_INS_SHA_B:
	case TRICORE_INS_SHA_H:
	case TRICORE_INS_SHA:
	case TRICORE_INS_SH_ANDN_T:
	case TRICORE_INS_SH_AND_T:
	case TRICORE_INS_SH_B:
	case TRICORE_INS_SH_EQ:
	case TRICORE_INS_SH_GE_U:
	case TRICORE_INS_SH_GE:
	case TRICORE_INS_SH_H:
	case TRICORE_INS_SH_LT_U:
	case TRICORE_INS_SH_LT:
	case TRICORE_INS_SH_NAND_T:
	case TRICORE_INS_SH_NE:
	case TRICORE_INS_SH_NOR_T:
	case TRICORE_INS_SH_ORN_T:
	case TRICORE_INS_SH_OR_T:
	case TRICORE_INS_SH_XNOR_T:
	case TRICORE_INS_SH_XOR_T:
	case TRICORE_INS_SH: {
		break;
	}
	case TRICORE_INS_STLCX:
	case TRICORE_INS_STUCX:
	case TRICORE_INS_ST_A:
	case TRICORE_INS_ST_B:
	case TRICORE_INS_ST_DA:
	case TRICORE_INS_ST_D:
	case TRICORE_INS_ST_H:
	case TRICORE_INS_ST_Q:
	case TRICORE_INS_ST_T:
	case TRICORE_INS_ST_W: {
		break;
	}
	case TRICORE_INS_SWAPMSK_W:
	case TRICORE_INS_SWAP_A:
	case TRICORE_INS_SWAP_W:
	case TRICORE_INS_TLBDEMAP:
	case TRICORE_INS_TLBFLUSH_A:
	case TRICORE_INS_TLBFLUSH_B:
	case TRICORE_INS_TLBMAP:
	case TRICORE_INS_TLBPROBE_A:
	case TRICORE_INS_TLBPROBE_I: {
		break;
	}
	case TRICORE_INS_TRAPSV:
	case TRICORE_INS_TRAPV: {
		break;
	}
	case TRICORE_INS_XNOR_T:
	case TRICORE_INS_XNOR:
	case TRICORE_INS_XOR_EQ:
	case TRICORE_INS_XOR_GE_U:
	case TRICORE_INS_XOR_GE:
	case TRICORE_INS_XOR_LT_U:
	case TRICORE_INS_XOR_LT:
	case TRICORE_INS_XOR_NE:
	case TRICORE_INS_XOR:
	case TRICORE_INS_XOR_T: {
		break;
	}
	}
	return NULL;
}

#include "rz_il/rz_il_opbuilder_end.h"

static void trap_hook(RzILVM *vm, RzILOpEffect *op) {}

static RzAnalysisILConfig *tricore_il_config(RZ_NONNULL RzAnalysis *analysis) {
	rz_return_val_if_fail(analysis, NULL);
	static const char *regs_bound[] = {
		"a0", "a1", "a2", "a3", "a4", "a5", "a6", "a7", "a8", "a9", "a10", "a11", "a12", "a13", "a14", "a15",
		"d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15",
		"PSW", "PCXI", "FCX", "LCX", "ISP", "ICR", "PIPN", "BIV", "BTV",
		NULL
	};

	RzAnalysisILConfig *cfg = rz_analysis_il_config_new(32, false, 32);
	cfg->reg_bindings = regs_bound;

	RzILEffectLabel *int_label = rz_il_effect_label_new("trap", EFFECT_LABEL_SYSCALL);
	int_label->hook = trap_hook;
	rz_analysis_il_config_add_label(cfg, int_label);

	return cfg;
}
