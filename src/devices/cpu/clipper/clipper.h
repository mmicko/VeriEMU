// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

#ifndef MAME_CPU_CLIPPER_CLIPPER_H
#define MAME_CPU_CLIPPER_CLIPPER_H

#pragma once

#include <limits.h>
#include "softfloat/milieu.h"
#include "softfloat/softfloat.h"

// convenience macros for dealing with the psw and ssw
#define PSW(mask) (m_psw & PSW_##mask)
#define SSW(mask) (m_ssw & SSW_##mask)

class clipper_device : public cpu_device
{
public:
	DECLARE_READ32_MEMBER(get_ssw) const { return m_ssw; }
	DECLARE_WRITE8_MEMBER(set_ivec) { m_ivec = data; }
	DECLARE_WRITE16_MEMBER(set_exception);

	enum addressing_modes : u8
	{
		ADDR_MODE_PC32  = 0x10, // pc relative with 32 bit displacement
		ADDR_MODE_ABS32 = 0x30, // 32 bit absolute
		ADDR_MODE_REL32 = 0x60, // relative with 32 bit displacement
		ADDR_MODE_PC16  = 0x90, // pc relative with 16 bit displacement
		ADDR_MODE_REL12 = 0xa0, // relative with 12 bit displacement
		ADDR_MODE_ABS16 = 0xb0, // 16 bit absolute
		ADDR_MODE_PCX   = 0xd0, // pc indexed
		ADDR_MODE_RELX  = 0xe0  // relative indexed
	};

	// branch conditions (first description for comparison, second for move/logical)
	enum branch_conditions : u8
	{
		BRANCH_T   = 0x0, // always
		BRANCH_LT  = 0x1, // less than                  greater than
		BRANCH_LE  = 0x2, // less or equal              greater or equal
		BRANCH_EQ  = 0x3, // equal
		BRANCH_GT  = 0x4, // greater than               less than
		BRANCH_GE  = 0x5, // greater or equal           less or equal
		BRANCH_NE  = 0x6, // not equal
		BRANCH_LTU = 0x7, // less than unsigned         greater than unsigned
		BRANCH_LEU = 0x8, // less or equal unsigned     greater or equal unsigned
		BRANCH_GTU = 0x9, // greater than unsigned      less than unsigned
		BRANCH_GEU = 0xa, // greater or equal unsigned  less or equal unsigned
		BRANCH_V   = 0xb, // overflow
		BRANCH_NV  = 0xc, // not overflow
		BRANCH_N   = 0xd, // negative
		BRANCH_NN  = 0xe, // not negative
		BRANCH_FN  = 0xf  // floating unordered
	};

	enum bf_conditions : u8
	{
		BF_ANY = 0x0, // floating any exception
		BF_BAD = 0x1  // floating bad result
	};

	enum psw : u32
	{
		PSW_N   = 0x00000001, // negative
		PSW_Z   = 0x00000002, // zero
		PSW_V   = 0x00000004, // overflow
		PSW_C   = 0x00000008, // carry out or borrow in
		PSW_FX  = 0x00000010, // floating inexact
		PSW_FU  = 0x00000020, // floating underflow
		PSW_FD  = 0x00000040, // floating divide by zero
		PSW_FV  = 0x00000080, // floating overflow
		PSW_FI  = 0x00000100, // floating invalid operation
		PSW_EFX = 0x00000200, // enable floating inexact trap
		PSW_EFU = 0x00000400, // enable floating underflow trap
		PSW_EFD = 0x00000800, // enable floating divide by zero trap
		PSW_EFV = 0x00001000, // enable floating overflow trap
		PSW_EFI = 0x00002000, // enable floating invalid operation trap
		PSW_EFT = 0x00004000, // enable floating trap
		PSW_FR  = 0x00018000, // floating rounding mode (2 bits)
							  // unused (3 bits)
		PSW_DSP = 0x00300000, // c400 - delay slot pointer (2 bits)
		PSW_BIG = 0x00400000, // c400 - big endian (hardware)
		PSW_T   = 0x00800000, // trace trap
		PSW_CTS = 0x0f000000, // cpu trap status (4 bits)
		PSW_MTS = 0xf0000000  // memory trap status (4 bits)
	};

	enum psw_fr : u32
	{
		FR_0 = 0x00000000, // round to nearest
		FR_1 = 0x00008000, // round toward + infinity
		FR_2 = 0x00010000, // round toward - infinity
		FR_3 = 0x00018000  // round toward zero
	};

	enum ssw : u32
	{
		SSW_IN  = 0x0000000f, // interrupt number (4 bits)
		SSW_IL  = 0x000000f0, // interrupt level (4 bits)
		SSW_EI  = 0x00000100, // enable interrupts
		SSW_ID  = 0x0001fe00, // cpu rev # and type (8 bits)
							  // unused (5 bits)
		SSW_FRD = 0x00400000, // floating registers dirty
		SSW_TP  = 0x00800000, // trace trap pending
		SSW_ECM = 0x01000000, // enable corrected memory error
		SSW_DF  = 0x02000000, // fpu disabled
		SSW_M   = 0x04000000, // mapped mode
		SSW_KU  = 0x08000000, // user protect key
		SSW_UU  = 0x10000000, // user data mode
		SSW_K   = 0x20000000, // protect key
		SSW_U   = 0x40000000, // user mode
		SSW_P   = 0x80000000  // previous mode
	};

	enum ssw_id : u32
	{
		SSW_ID_C400R0 = 0x00000,
		SSW_ID_C400R1 = 0x04000,
		SSW_ID_C400R2 = 0x08000,
		SSW_ID_C400R3 = 0x0c000,
		SSW_ID_C400R4 = 0x10000
	};

	enum exception_vectors : u16
	{
		// data memory trap group
		EXCEPTION_D_CORRECTED_MEMORY_ERROR     = 0x108,
		EXCEPTION_D_UNCORRECTABLE_MEMORY_ERROR = 0x110,
		EXCEPTION_D_ALIGNMENT_FAULT            = 0x120,
		EXCEPTION_D_PAGE_FAULT                 = 0x128,
		EXCEPTION_D_READ_PROTECT_FAULT         = 0x130,
		EXCEPTION_D_WRITE_PROTECT_FAULT        = 0x138,

		// floating-point arithmetic trap group
		EXCEPTION_FLOATING_INEXACT             = 0x180,
		EXCEPTION_FLOATING_UNDERFLOW           = 0x188,
		EXCEPTION_FLOATING_DIVIDE_BY_ZERO      = 0x190,
		EXCEPTION_FLOATING_OVERFLOW            = 0x1a0,
		EXCEPTION_FLOATING_INVALID_OPERATION   = 0x1c0,

		// integer arithmetic trap group
		EXCEPTION_INTEGER_DIVIDE_BY_ZERO       = 0x208,

		// instruction memory trap group
		EXCEPTION_I_CORRECTED_MEMORY_ERROR     = 0x288,
		EXCEPTION_I_UNCORRECTABLE_MEMORY_ERROR = 0x290,
		EXCEPTION_I_ALIGNMENT_FAULT            = 0x2a0,
		EXCEPTION_I_PAGE_FAULT                 = 0x2a8,
		EXCEPTION_I_EXECUTE_PROTECT_FAULT      = 0x2b0,

		// illegal operation trap group
		EXCEPTION_ILLEGAL_OPERATION            = 0x300,
		EXCEPTION_PRIVILEGED_INSTRUCTION       = 0x308,

		// diagnostic trap group
		EXCEPTION_TRACE                        = 0x380,

		// supervisor calls (0x400-0x7f8)
		EXCEPTION_SUPERVISOR_CALL_BASE         = 0x400,

		// prioritized interrupts (0x800-0xff8)
		EXCEPTION_INTERRUPT_BASE               = 0x800
	};

	// trap source values are shifted into the correct field in the psw
	static const int CTS_SHIFT = 24;
	enum cpu_trap_sources : u32
	{
		CTS_NO_CPU_TRAP            = 0 << CTS_SHIFT,
		CTS_DIVIDE_BY_ZERO         = 2 << CTS_SHIFT,
		CTS_ILLEGAL_OPERATION      = 4 << CTS_SHIFT,
		CTS_PRIVILEGED_INSTRUCTION = 5 << CTS_SHIFT,
		CTS_TRACE_TRAP             = 7 << CTS_SHIFT
	};

	static const int MTS_SHIFT = 28;
	enum memory_trap_sources : u32
	{
		MTS_NO_MEMORY_TRAP                = 0 << MTS_SHIFT,
		MTS_CORRECTED_MEMORY_ERROR        = 1 << MTS_SHIFT,
		MTS_UNCORRECTABLE_MEMORY_ERROR    = 2 << MTS_SHIFT,
		MTS_ALIGNMENT_FAULT               = 4 << MTS_SHIFT,
		MTS_PAGE_FAULT                    = 5 << MTS_SHIFT,
		MTS_READ_OR_EXECUTE_PROTECT_FAULT = 6 << MTS_SHIFT,
		MTS_WRITE_PROTECT_FAULT           = 7 << MTS_SHIFT
	};

	// extract an mts code from a vector
	static const u32 MTS_VMASK  = 0x00000038;
	static const int MTS_VSHIFT = 3;

	enum ivec_mask : u8
	{
		IVEC_NUMBER = 0x0f,
		IVEC_LEVEL  = 0xf0
	};

	// combinations of floating point exceptions (from softfloat flags)
	enum fp_exception_mask : u8
	{
		F_NONE  = (0),
		F_I     = (float_flag_invalid),
		F_X     = (float_flag_inexact),
		F_IX    = (float_flag_invalid | float_flag_inexact),
		F_IVUX  = (float_flag_invalid | float_flag_overflow | float_flag_underflow | float_flag_inexact),
		F_IVDUX = (float_flag_invalid | float_flag_overflow | float_flag_divbyzero | float_flag_underflow | float_flag_inexact)
	};

protected:
	clipper_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock, const endianness_t endianness, const u32 cpuid);

	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	// device_execute_interface overrides
	virtual u32 execute_min_cycles() const override { return 1; }
	virtual u32 execute_max_cycles() const override { return 1; } // FIXME: don't know, especially macro instructions
	virtual u32 execute_input_lines() const override { return 2; } // number of input/interrupt lines (irq/nmi)
	virtual void execute_run() override;
	virtual void execute_set_input(int inputnum, int state) override;

	// device_memory_interface overrides
	virtual space_config_vector memory_space_config() const override;

	// device_state_interface overrides
	virtual void state_string_export(const device_state_entry &entry, std::string &str) const override;

	// device_disasm_interface overrides
	virtual util::disasm_interface *create_disassembler() override;

	// cpu execution logic
	bool decode_instruction();
	virtual void execute_instruction();
	bool evaluate_branch() const;

	// exception entry and return helpers
	u32 intrap(const u16 vector, const u32 old_pc);
	u32 reti();

	// cpu state helpers
	void set_psw(const u32 psw);
	void set_ssw(const u32 ssw);
	void fp_exception();

	// register count helpers
	virtual int get_ireg_count() const { return 16; }
	virtual int get_freg_count() const { return 8; }

	// exception vector and frame helpers
	virtual int get_eframe_size() const { return 12; }
	virtual int get_evpc_offset() const { return 0; }
	virtual int get_evssw_offset() const { return 4; }

	// floating point helpers
	float32 get_fp32(const u8 reg) const { return m_f[reg & 0xf]; }
	float64 get_fp64(const u8 reg) const { return m_f[reg & 0xf]; }
	template <typename T> void set_fp(const u8 reg, const T data, const fp_exception_mask exception_mask)
	{
		// suppress unexpected exceptions
		float_exception_flags &= exception_mask;

		// save floating exception state
		m_fp_pc = m_pc;
		m_fp_dst = m_f[reg & 0xf];

		// assign data
		if (float_exception_flags & float_flag_overflow && PSW(EFV))
		{
			/*
			 * If the EFV flag is set, the computed result is delivered to the
			 * destination with the normalized rounded fraction of the true
			 * result (though the delivered exponent is usually wrong because
			 * of missing additional leading bits in the exponent field). For
			 * single-precision overflows, if the biased exponent of the true
			 * result is 255, then biased exponent 255 is delivered to the
			 * destination. If the true biased exponent of the result is
			 * 256..408, then the true biased exponent minus 256 is delivered
			 * to the destination. Note that this is not the exponent wrapped
			 * result called for by the IEEE 754 specification; the wrap must
			 * be adjusted by system software before delivery to a user's trap
			 * handler. This is done to allow the user to provide software that
			 * handles traps in an application-specific way. For double-
			 * precision, the overflow exponents (biased) lie in the range
			 * 2047..3120. These are mapped to 2047 and 0..1072 respectively.
			 * These must be adjusted by (3/4)x2^11 (1536) to obtain the IEEE
			 * Standard wrapped exponent.
			 */
			// FIXME: implement non-IEEE behaviour described above
			m_f[reg & 0xf] = data;
		}
		else if (float_exception_flags & float_flag_underflow && PSW(EFU))
		{
			/*
			 * If EFU is set, the floating underflow exception is signalled
			 * when the result of an operation (before rounding) has a biased
			 * exponent less than the minimum representable biased exponent for
			 * a normalized number. If the true biased exponent of the result
			 * is zero, then biased exponent zero is delivered to the
			 * destination. If the true biased exponent is less than zero, then
			 * the exponent delivered to the destination is true biased
			 * exponent plus 256 (2048 for double). The exponent must be
			 * adjusted by system software before delivery to the program's
			 * trap handler in order to conform to the IEEE 754 specification.
			 * The range of underflowed biased exponents for single-precision
			 * is 0..-275; for double-precision the range is 0..-1125.
			 */
			// FIXME: implement non-IEEE behaviour described above
			m_f[reg & 0xf] = data;
		}
		else
			m_f[reg & 0xf] = data;

		// set floating dirty flag
		m_ssw |= SSW_FRD;
	};

	// emulation state
	address_space_config m_insn_config;
	address_space_config m_data_config;

	address_space *m_insn;
	address_space *m_data;

	enum registers
	{
		CLIPPER_IREG = 0,
		CLIPPER_FREG = 16,
		CLIPPER_PSW  = 32,
		CLIPPER_SSW  = 33,
		CLIPPER_PC   = 34,
	};

	int m_icount;    // instruction cycle count

	// program-visible cpu state
	u32 m_pc;  // current instruction address
	u32 m_psw; // program status word
	u32 m_ssw; // system status word

	u32 *m_r;     // active registers
	u32 m_ru[16]; // user registers
	u32 m_rs[16]; // supervisor registers

	u64 m_f[16];  // floating point registers
	u32 m_fp_pc;  // address of floating point instruction causing exception
	u64 m_fp_dst; // original value of destination register during fp exception

	// non-visible cpu state
	u32 m_ip;        // next instruction address
	int m_irq;       // interrupt request state
	int m_nmi;       // non-maskable interrupt state
	u8 m_ivec;       // interrupt vector
	u16 m_exception; // pending exception

	// decoded instruction information
	struct decode
	{
		u8 opcode;    // primary instruction opcode
		u8 subopcode; // secondary instruction opcode
		u8 r1;        // r1 instruction operand
		u8 r2;        // r2 instruction operand
		u32 imm;      // immediate value operand
		u16 macro;    // macro instruction operands
		u32 address;  // computed effective address
	}
	m_info;
};

class clipper_c100_device : public clipper_device
{
public:
	clipper_c100_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);
};

class clipper_c300_device : public clipper_device
{
public:
	clipper_c300_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);
};

class clipper_c400_device : public clipper_device
{
public:
	clipper_c400_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock);

protected:
	// C400 has additional 8 floating point registers
	virtual int get_freg_count() const override { return 16; }

	// C400 creates a 24 byte exception frame (C100/C300 is 12 bytes), but the
	// service routine must increment sp by 12 prior to executing reti
	// exception frame size
	virtual int get_eframe_size() const override { return 24; }

	// C400 pc and ssw are reversed in exception vector compared to C100/C300
	virtual int get_evpc_offset() const override { return 4; }
	virtual int get_evssw_offset() const override { return 0; }

	virtual void execute_instruction() override;
};

DECLARE_DEVICE_TYPE(CLIPPER_C100, clipper_c100_device)
DECLARE_DEVICE_TYPE(CLIPPER_C300, clipper_c300_device)
DECLARE_DEVICE_TYPE(CLIPPER_C400, clipper_c400_device)

#endif // MAME_CPU_CLIPPER_CLIPPER_H
