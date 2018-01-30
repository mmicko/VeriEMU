// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * An implementation of the Fairchild/Intergraph CLIPPER CPU family.
 *
 * Primary source: http://bitsavers.org/pdf/fairchild/clipper/Clipper_Instruction_Set_Oct85.pdf
 *
 * TODO:
 *   - save/restore state
 *   - unimplemented C400 instructions
 *   - correct boot logic
 *   - condition codes for multiply instructions
 *   - instruction timing
 *   - big endian support (not present in the wild)
 */

#include "emu.h"
#include "debugger.h"
#include "clipper.h"
#include "clipperd.h"

#define LOG_GENERAL   (1U << 0)
#define LOG_EXCEPTION (1U << 1)

//#define VERBOSE (LOG_GENERAL | LOG_EXCEPTION)

#include "logmacro.h"

// convenience macros for frequently used instruction fields
#define R1 (m_info.r1)
#define R2 (m_info.r2)

// macros for setting psw condition codes
#define FLAGS(C,V,Z,N) \
	m_psw = (m_psw & ~(PSW_C | PSW_V | PSW_Z | PSW_N)) | (((C) << 3) | ((V) << 2) | ((Z) << 1) | ((N) << 0));
#define FLAGS_CV(C,V) \
	m_psw = (m_psw & ~(PSW_C | PSW_V)) | (((C) << 3) | ((V) << 2));
#define FLAGS_ZN(Z,N) \
	m_psw = (m_psw & ~(PSW_Z | PSW_N)) | (((Z) << 1) | ((N) << 0));

// over/underflow for addition/subtraction from here: http://stackoverflow.com/questions/199333/how-to-detect-integer-overflow-in-c-c
#define OF_ADD(a, b) ((b > 0) && (a > INT_MAX - b))
#define UF_ADD(a, b) ((b < 0) && (a < INT_MIN - b))
#define OF_SUB(a, b) ((b < 0) && (a > INT_MAX + b))
#define UF_SUB(a, b) ((b > 0) && (a < INT_MIN + b))

// CLIPPER logic for carry and overflow flags
#define C_ADD(a, b) ((u32)a + (u32)b < (u32)a)
#define V_ADD(a, b) (OF_ADD((s32)a, (s32)b) || UF_ADD((s32)a, (s32)b))
#define C_SUB(a, b) ((u32)a < (u32)b)
#define V_SUB(a, b) (OF_SUB((s32)a, (s32)b) || UF_SUB((s32)a, (s32)b))

DEFINE_DEVICE_TYPE(CLIPPER_C100, clipper_c100_device, "clipper_c100", "C100 CLIPPER")
DEFINE_DEVICE_TYPE(CLIPPER_C300, clipper_c300_device, "clipper_c300", "C300 CLIPPER")
DEFINE_DEVICE_TYPE(CLIPPER_C400, clipper_c400_device, "clipper_c400", "C400 CLIPPER")

clipper_c100_device::clipper_c100_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: clipper_device(mconfig, CLIPPER_C100, tag, owner, clock, ENDIANNESS_LITTLE, 0)
{
}

clipper_c300_device::clipper_c300_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: clipper_device(mconfig, CLIPPER_C300, tag, owner, clock, ENDIANNESS_LITTLE, 0)
{
}

clipper_c400_device::clipper_c400_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: clipper_device(mconfig, CLIPPER_C400, tag, owner, clock, ENDIANNESS_LITTLE, SSW_ID_C400R4)
{
}

clipper_device::clipper_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock, const endianness_t endianness, const u32 cpuid)
	: cpu_device(mconfig, type, tag, owner, clock)
	, m_insn_config("insn", endianness, 32, 32, 0)
	, m_data_config("data", endianness, 32, 32, 0)
	, m_insn(nullptr)
	, m_data(nullptr)
	, m_icount(0)
	, m_psw(endianness == ENDIANNESS_BIG ? PSW_BIG : 0)
	, m_ssw(cpuid)
	, m_r(m_rs)
{
}

// rotate helpers to replace MSVC intrinsics
inline u32 rotl32(u32 x, u8 shift)
{
  shift &= 31;
  return (x << shift) | (x >> ((32 - shift) & 31));
}

inline u32 rotr32(u32 x, u8 shift)
{
  shift &= 31;
  return (x >> shift) | (x << ((32 - shift) & 31));
}

inline u64 rotl64(u64 x, u8 shift)
{
  shift &= 63;
  return (x << shift) | (x >> ((64 - shift) & 63));
}

inline u64 rotr64(u64 x, u8 shift)
{
  shift &= 63;
  return (x >> shift) | (x << ((64 - shift) & 63));
}

void clipper_device::device_start()
{
	// get our address spaces
	m_insn = &space(AS_PROGRAM);
	m_data = &space(AS_DATA);

	// set our instruction counter
	m_icountptr = &m_icount;

	save_item(NAME(m_pc));
	save_item(NAME(m_psw));
	save_item(NAME(m_ssw));
	save_item(NAME(m_ru));
	save_item(NAME(m_rs));
	save_item(NAME(m_f));
	save_item(NAME(m_fp_pc));
	save_item(NAME(m_fp_dst));

	save_item(NAME(m_nmi));
	save_item(NAME(m_irq));
	save_item(NAME(m_ivec));

	state_add(STATE_GENPC, "GENPC", m_ip).noshow();
	state_add(STATE_GENPCBASE, "CURPC", m_ip).noshow();
	state_add(STATE_GENSP, "GENSP", m_r[15]).noshow();
	state_add(STATE_GENFLAGS, "GENFLAGS", m_psw).mask(0xf).formatstr("%4s").noshow();

	state_add(CLIPPER_PC, "pc", m_ip);
	state_add(CLIPPER_PSW, "psw", m_psw);
	state_add(CLIPPER_SSW, "ssw", m_ssw);

	// integer regsters
	for (int i = 0; i < get_ireg_count(); i++)
		state_add(CLIPPER_IREG + i, util::string_format("r%d", i).c_str(), m_r[i]);

	// floating point registers
	for (int i = 0; i < get_freg_count(); i++)
		state_add(CLIPPER_FREG + i, util::string_format("f%d", i).c_str(), m_f[i]);
}

void clipper_device::device_reset()
{
	/*
	 * From C300 documentation, on reset:
	 *   psw: T cleared, BIG set from hardware, others undefined
	 *   ssw: EI, TP, M, U, K, KU, UU, P cleared, ID set from hardware, others undefined
	 */

	// clear the psw and ssw
	set_psw(0);
	set_ssw(0);

	// FIXME: figure out how to branch to the boot code properly
	m_pc = 0x6000;
	m_ip = 0x7f100000;

	m_irq = CLEAR_LINE;
	m_nmi = CLEAR_LINE;
	m_ivec = 0;
	m_exception = 0;
}

void clipper_device::state_string_export(const device_state_entry &entry, std::string &str) const
{
	switch (entry.index())
	{
	case STATE_GENFLAGS:
		str = string_format("%c%c%c%c",
			PSW(C) ? 'C' : '.',
			PSW(V) ? 'V' : '.',
			PSW(Z) ? 'Z' : '.',
			PSW(N) ? 'N' : '.');
		break;
	}
}

void clipper_device::execute_run()
{
	// check for non-maskable and prioritised interrupts
	if (m_nmi)
	{
		// acknowledge non-maskable interrupt
		standard_irq_callback(INPUT_LINE_NMI);

		LOGMASKED(LOG_EXCEPTION, "non-maskable interrupt\n");
		m_ip = intrap(EXCEPTION_INTERRUPT_BASE, m_ip);
	}
	else if (SSW(EI) && m_irq)
	{
		LOGMASKED(LOG_EXCEPTION, "received prioritised interrupt ivec 0x%02x\n", m_ivec);

		// allow equal/higher priority interrupts
		if ((m_ivec & IVEC_LEVEL) <= SSW(IL))
		{
			// acknowledge interrupt
			standard_irq_callback(INPUT_LINE_IRQ0);

			LOGMASKED(LOG_EXCEPTION, "transferring control to ivec 0x%02x\n", m_ivec);
			m_ip = intrap(EXCEPTION_INTERRUPT_BASE + m_ivec * 8, m_ip);
		}
	}

	while (m_icount > 0)
	{
		debugger_instruction_hook(this, m_ip);

		// fetch and decode an instruction
		if (decode_instruction())
		{
			float_exception_flags = 0;

			// execute instruction
			execute_instruction();

			// check floating point exceptions
			if (float_exception_flags)
				fp_exception();
		}

		if (m_exception)
		{
			/*
			 * For traced instructions which are interrupted or cause traps, the TP
			 * flag is set by hardware when the interrupt or trap occurs to ensure
			 * that the trace trap will occur immediately after the interrupt or other
			 * trap has been serviced.
			 */
			// FIXME: don't know why/when the trace pending flag is needed
			if (PSW(T))
				m_ssw |= SSW_TP;

			switch (m_exception)
			{
				// data memory trap group
			case EXCEPTION_D_CORRECTED_MEMORY_ERROR:
			case EXCEPTION_D_UNCORRECTABLE_MEMORY_ERROR:
			case EXCEPTION_D_ALIGNMENT_FAULT:
			case EXCEPTION_D_PAGE_FAULT:
			case EXCEPTION_D_READ_PROTECT_FAULT:
			case EXCEPTION_D_WRITE_PROTECT_FAULT:

				// instruction memory trap group
			case EXCEPTION_I_CORRECTED_MEMORY_ERROR:
			case EXCEPTION_I_UNCORRECTABLE_MEMORY_ERROR:
			case EXCEPTION_I_ALIGNMENT_FAULT:
			case EXCEPTION_I_PAGE_FAULT:
			case EXCEPTION_I_EXECUTE_PROTECT_FAULT:

				// illegal operation trap group
			case EXCEPTION_ILLEGAL_OPERATION:
			case EXCEPTION_PRIVILEGED_INSTRUCTION:
				// return address is faulting instruction
				m_ip = intrap(m_exception, m_pc);
				break;

			default:
				// return address is following instruction
				m_ip = intrap(m_exception, m_ip);
				break;
			}
		}

		// FIXME: trace trap logic not working properly yet
		//else if (PSW(T))
		//  m_ip = intrap(EXCEPTION_TRACE, m_ip);

		// FIXME: some instructions take longer (significantly) than one cycle
		// and also the timings are often slower for the C100 and C300
		m_icount--;
	}
}

void clipper_device::execute_set_input(int inputnum, int state)
{
	switch (inputnum)
	{
	case INPUT_LINE_IRQ0:
		m_irq = state;
		break;

	case INPUT_LINE_NMI:
		m_nmi = state;
		break;
	}
}

/*
 * The CLIPPER has a true Harvard architecture. In the InterPro, these are tied back together
 * again by the MMU, which then directs the access to one of 3 address spaces: main, i/o or boot.
 */
device_memory_interface::space_config_vector clipper_device::memory_space_config() const
{
	return space_config_vector {
		std::make_pair(AS_PROGRAM, &m_insn_config),
		std::make_pair(AS_DATA,    &m_data_config)
	};
}

WRITE16_MEMBER(clipper_device::set_exception)
{
	LOGMASKED(LOG_EXCEPTION, "external exception 0x%04x triggered\n", data);

	// check if corrected memory errors are masked
	if (!SSW(ECM) && (data == EXCEPTION_D_CORRECTED_MEMORY_ERROR || data == EXCEPTION_I_CORRECTED_MEMORY_ERROR))
		return;

	m_exception = data;
}

/*
 * Fetch and decode an instruction and compute an effective address (for
 * instructions with addressing modes). The results are contained in the m_info
 * structure to simplify passing between here and execute_instruction().
 */
bool clipper_device::decode_instruction()
{
	// fetch instruction word
	u16 insn = m_insn->read_word(m_ip + 0);
	if (m_exception)
		return false;

	// decode the primary parcel
	m_info.opcode = insn >> 8;
	m_info.subopcode = insn & 0xff;
	m_info.r1 = (insn & 0x00f0) >> 4;
	m_info.r2 = insn & 0x000f;

	// initialise the other fields
	m_info.imm = 0;
	m_info.macro = 0;
	m_info.address = 0;

	// default instruction size is 2 bytes
	int size = 2;

	if ((insn & 0xf800) == 0x3800)
	{
		// instruction has a 16 bit immediate operand

		// fetch 16 bit immediate and sign extend
		m_info.imm = (s16)m_insn->read_word(m_ip + 2);
		if (m_exception)
			return false;
		size = 4;
	}
	else if ((insn & 0xd300) == 0x8300)
	{
		// instruction has an immediate operand, either 16 or 32 bit
		if (insn & 0x0080)
		{
			// fetch 16 bit immediate and sign extend
			m_info.imm = (s16)m_insn->read_word(m_ip + 2);
			if (m_exception)
				return false;
			size = 4;
		}
		else
		{
			// fetch 32 bit immediate
			m_info.imm = m_insn->read_dword_unaligned(m_ip + 2);
			if (m_exception)
				return false;
			size = 6;
		}
	}
	else if ((insn & 0xc000) == 0x4000)
	{
		// instructions with addresses
		if (insn & 0x0100)
		{
			// instructions with complex modes
			u16 temp;

			switch (insn & 0x00f0)
			{
			case ADDR_MODE_PC32:
				m_info.address = m_ip + m_insn->read_dword_unaligned(m_ip + 2);
				if (m_exception)
					return false;
				size = 6;
				break;

			case ADDR_MODE_ABS32:
				m_info.address = m_insn->read_dword_unaligned(m_ip + 2);
				if (m_exception)
					return false;
				size = 6;
				break;

			case ADDR_MODE_REL32:
				m_info.r2 = m_insn->read_word(m_ip + 2) & 0xf;
				if (m_exception)
					return false;

				m_info.address = m_r[insn & 0xf] + m_insn->read_dword_unaligned(m_ip + 4);
				if (m_exception)
					return false;
				size = 8;
				break;

			case ADDR_MODE_PC16:
				m_info.address = m_ip + (s16)m_insn->read_word(m_ip + 2);
				if (m_exception)
					return false;
				size = 4;
				break;

			case ADDR_MODE_REL12:
				temp = m_insn->read_word(m_ip + 2);
				if (m_exception)
					return false;

				m_info.r2 = temp & 0xf;
				m_info.address = m_r[insn & 0xf] + ((s16)temp >> 4);
				size = 4;
				break;

			case ADDR_MODE_ABS16:
				m_info.address = (s16)m_insn->read_word(m_ip + 2);
				if (m_exception)
					return false;
				size = 4;
				break;

			case ADDR_MODE_PCX:
				temp = m_insn->read_word(m_ip + 2);
				if (m_exception)
					return false;

				m_info.r2 = temp & 0xf;
				m_info.address = m_ip + m_r[(temp >> 4) & 0xf];
				size = 4;
				break;

			case ADDR_MODE_RELX:
				temp = m_insn->read_word(m_ip + 2);
				if (m_exception)
					return false;

				m_info.r2 = temp & 0xf;
				m_info.address = m_r[insn & 0xf] + m_r[(temp >> 4) & 0xf];
				size = 4;
				break;

			default:
				m_exception = EXCEPTION_ILLEGAL_OPERATION;
				return false;
			}
		}
		else
			// relative addressing mode
			m_info.address = m_r[m_info.r1];
	}
	else if ((insn & 0xfd00) == 0xb400)
	{
		// macro instructions
		m_info.macro = m_insn->read_word(m_ip + 2);
		if (m_exception)
			return false;
		size = 4;
	}

	// instruction fetch and decode complete
	m_pc = m_ip;
	m_ip = m_pc + size;

	return true;
}

void clipper_device::execute_instruction()
{
	// memory fetch temporary
	u64 temp;

	switch (m_info.opcode)
	{
	case 0x00: // noop
		break;

	case 0x10:
		// movwp: move word to processor register
		// treated as a noop if target ssw in user mode
		// R1 == 3 means "fast" mode - avoids pipeline flush
		if (R1 == 0)
			set_psw(m_r[R2]);
		else if (!SSW(U) && (R1 == 1 || R1 == 3))
			set_ssw(m_r[R2]);
		// FLAGS: CVZN
		break;
	case 0x11:
		// movpw: move processor register to word
		switch (R1)
		{
		case 0: m_r[R2] = m_psw; break;
		case 1: m_r[R2] = m_ssw; break;
		}
		break;
	case 0x12:
		// calls: call supervisor
		m_exception = EXCEPTION_SUPERVISOR_CALL_BASE + (m_info.subopcode & 0x7f) * 8;
		break;
	case 0x13:
		// ret: return from subroutine
		temp = m_data->read_dword(m_r[R2]);
		if (!m_exception)
		{
			m_ip = temp;
			m_r[R2] += 4;
		}
		// TRAPS: C,U,A,P,R
		break;
	case 0x14:
		// pushw: push word
		m_data->write_dword(m_r[R1] - 4, m_r[R2]);
		m_r[R1] -= 4;
		// TRAPS: A,P,W
		break;

	case 0x16:
		// popw: pop word
		m_r[R1] += 4;
		temp = m_data->read_dword(m_r[R1] - 4);
		if (!m_exception)
			m_r[R2] = temp;
		// TRAPS: C,U,A,P,R
		break;

	case 0x20:
		// adds: add single floating
		set_fp(R2, float32_add(get_fp32(R2), get_fp32(R1)), F_IVUX);
		// TRAPS: F_IVUX
		break;
	case 0x21:
		// subs: subtract single floating
		set_fp(R2, float32_sub(get_fp32(R2), get_fp32(R1)), F_IVUX);
		// TRAPS: F_IVUX
		break;
	case 0x22:
		// addd: add double floating
		set_fp(R2, float64_add(get_fp64(R2), get_fp64(R1)), F_IVUX);
		// TRAPS: F_IVUX
		break;
	case 0x23:
		// subd: subtract double floating
		set_fp(R2, float64_sub(get_fp64(R2), get_fp64(R1)), F_IVUX);
		// TRAPS: F_IVUX
		break;
	case 0x24:
		// movs: move single floating
		set_fp(R2, get_fp32(R1), F_NONE);
		break;
	case 0x25:
		// cmps: compare single floating
		FLAGS(0, 0, float32_eq(get_fp32(R2), get_fp32(R1)), float32_lt(get_fp32(R2), get_fp32(R1)))
		// flag unordered
		if (float_exception_flags & float_flag_invalid)
			m_psw |= PSW_Z | PSW_N;
		float_exception_flags &= F_NONE;
		break;
	case 0x26:
		// movd: move double floating
		set_fp(R2, get_fp64(R1), F_NONE);
		break;
	case 0x27:
		// cmpd: compare double floating
		FLAGS(0, 0, float64_eq(get_fp64(R2), get_fp64(R1)), float64_lt(get_fp64(R2), get_fp64(R1)))
		// flag unordered
		if (float_exception_flags & float_flag_invalid)
			m_psw |= PSW_Z | PSW_N;
		float_exception_flags &= F_NONE;
		break;
	case 0x28:
		// muls: multiply single floating
		set_fp(R2, float32_mul(get_fp32(R2), get_fp32(R1)), F_IVUX);
		// TRAPS: F_IVUX
		break;
	case 0x29:
		// divs: divide single floating
		set_fp(R2, float32_div(get_fp32(R2), get_fp32(R1)), F_IVDUX);
		// TRAPS: F_IVDUX
		break;
	case 0x2a:
		// muld: multiply double floating
		set_fp(R2, float64_mul(get_fp64(R2), get_fp64(R1)), F_IVUX);
		// TRAPS: F_IVUX
		break;
	case 0x2b:
		// divd: divide double floating
		set_fp(R2, float64_div(get_fp64(R2), get_fp64(R1)), F_IVDUX);
		// TRAPS: F_IVDUX
		break;
	case 0x2c:
		// movsw: move single floating to word
		m_r[R2] = get_fp32(R1);
		break;
	case 0x2d:
		// movws: move word to single floating
		set_fp(R2, m_r[R1], F_NONE);
		break;
	case 0x2e:
		// movdl: move double floating to longword
		((u64 *)m_r)[R2 >> 1] = get_fp64(R1);
		break;
	case 0x2f:
		// movld: move longword to double floating
		set_fp(R2, ((u64 *)m_r)[R1 >> 1], F_NONE);
		break;
	case 0x30:
		// shaw: shift arithmetic word
		if ((s32)m_r[R1] > 0)
		{
			// save the bits that will be shifted out plus new sign bit
			s32 v = (s32)m_r[R2] >> (31 - m_r[R1]);

			m_r[R2] <<= m_r[R1];

			// overflow is set if sign changes during shift
			FLAGS(0, v != 0 && v != -1, m_r[R2] == 0, (s32)m_r[R2] < 0)
		}
		else
		{
			((s32 *)m_r)[R2] >>= -m_r[R1];
			FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		}
		// FLAGS: 0VZN
		break;
	case 0x31:
		// shal: shift arithmetic longword
		if ((s32)m_r[R1] > 0)
		{
			// save the bits that will be shifted out plus new sign bit
			s64 v = ((s64 *)m_r)[R2 >> 1] >> (63 - m_r[R1]);

			((u64 *)m_r)[R2 >> 1] <<= m_r[R1];

			// overflow is set if sign changes during shift
			FLAGS(0, v != 0 && v != -1, ((u64 *)m_r)[R2 >> 1] == 0, ((s64 *)m_r)[R2 >> 1] < 0)
		}
		else
		{
			((s64 *)m_r)[R2 >> 1] >>= -m_r[R1];
			FLAGS(0, 0, ((u64 *)m_r)[R2 >> 1] == 0, ((s64 *)m_r)[R2 >> 1] < 0)
		}
		// FLAGS: 0VZN
		break;
	case 0x32:
		// shlw: shift logical word
		if ((s32)m_r[R1] > 0)
			m_r[R2] <<= m_r[R1];
		else
			m_r[R2] >>= -m_r[R1];
		// FLAGS: 00ZN
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0);
		break;
	case 0x33:
		// shll: shift logical longword
		if ((s32)m_r[R1] > 0)
			((u64 *)m_r)[R2 >> 1] <<= m_r[R1];
		else
			((u64 *)m_r)[R2 >> 1] >>= -m_r[R1];
		// FLAGS: 00ZN
		FLAGS(0, 0, ((u64 *)m_r)[R2 >> 1] == 0, ((s64 *)m_r)[R2 >> 1] < 0);
		break;
	case 0x34:
		// rotw: rotate word
		if ((s32)m_r[R1] > 0)
			m_r[R2] = rotl32(m_r[R2], m_r[R1]);
		else
			m_r[R2] = rotr32(m_r[R2], -m_r[R1]);
		// FLAGS: 00ZN
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0);
		break;
	case 0x35:
		// rotl: rotate longword
		if ((s32)m_r[R1] > 0)
			((u64 *)m_r)[R2 >> 1] = rotl64(((u64 *)m_r)[R2 >> 1], m_r[R1]);
		else
			((u64 *)m_r)[R2 >> 1] = rotr64(((u64 *)m_r)[R2 >> 1], -m_r[R1]);
		// FLAGS: 00ZN
		FLAGS(0, 0, ((u64 *)m_r)[R2 >> 1] == 0, ((s64 *)m_r)[R2 >> 1] < 0);
		break;

	case 0x38:
		// shai: shift arithmetic immediate
		if ((s32)m_info.imm > 0)
		{
			// save the bits that will be shifted out plus new sign bit
			s32 v = (s32)m_r[R2] >> (31 - m_info.imm);

			m_r[R2] <<= m_info.imm;

			// overflow is set if sign changes during shift
			FLAGS(0, v != 0 && v != -1, m_r[R2] == 0, (s32)m_r[R2] < 0)
		}
		else
		{
			((s32 *)m_r)[R2] >>= -m_info.imm;
			FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		}
		// FLAGS: 0VZN
		// TRAPS: I
		break;
	case 0x39:
		// shali: shift arithmetic longword immediate
		if ((s32)m_info.imm > 0)
		{
			// save the bits that will be shifted out plus new sign bit
			s64 v = ((s64 *)m_r)[R2 >> 1] >> (63 - m_info.imm);

			((u64 *)m_r)[R2 >> 1] <<= m_info.imm;

			// overflow is set if sign changes during shift
			FLAGS(0, v != 0 && v != -1, ((u64 *)m_r)[R2 >> 1] == 0, ((s64 *)m_r)[R2 >> 1] < 0)
		}
		else
		{
			((s64 *)m_r)[R2 >> 1] >>= -m_info.imm;
			FLAGS(0, 0, ((u64 *)m_r)[R2 >> 1] == 0, ((s64 *)m_r)[R2 >> 1] < 0)
		}
		// FLAGS: 0VZN
		// TRAPS: I
		break;
	case 0x3a:
		// shli: shift logical immediate
		if ((s32)m_info.imm > 0)
			m_r[R2] <<= m_info.imm;
		else
			m_r[R2] >>= -m_info.imm;
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0);
		// FLAGS: 00ZN
		// TRAPS: I
		break;
	case 0x3b:
		// shlli: shift logical longword immediate
		if ((s32)m_info.imm > 0)
			((u64 *)m_r)[R2 >> 1] <<= m_info.imm;
		else
			((u64 *)m_r)[R2 >> 1] >>= -m_info.imm;
		FLAGS(0, 0, ((u64 *)m_r)[R2 >> 1] == 0, ((s64 *)m_r)[R2 >> 1] < 0);
		// FLAGS: 00ZN
		// TRAPS: I
		break;
	case 0x3c:
		// roti: rotate immediate
		if ((s32)m_info.imm > 0)
			m_r[R2] = rotl32(m_r[R2], m_info.imm);
		else
			m_r[R2] = rotr32(m_r[R2], -m_info.imm);
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0);
		// FLAGS: 00ZN
		// TRAPS: I
		break;
	case 0x3d:
		// rotli: rotate longword immediate
		if ((s32)m_info.imm > 0)
			((u64 *)m_r)[R2 >> 1] = rotl64(((u64 *)m_r)[R2 >> 1], m_info.imm);
		else
			((u64 *)m_r)[R2 >> 1] = rotr64(((u64 *)m_r)[R2 >> 1], -m_info.imm);
		FLAGS(0, 0, ((u64 *)m_r)[R2 >> 1] == 0, ((s64 *)m_r)[R2 >> 1] < 0);
		// FLAGS: 00ZN
		// TRAPS: I
		break;

	case 0x44:
	case 0x45:
		// call: call subroutine
		m_data->write_dword(m_r[R2] - 4, m_ip);
		if (!m_exception)
		{
			m_ip = m_info.address;
			m_r[R2] -= 4;
		}
		// TRAPS: A,P,W
		break;

	case 0x48:
	case 0x49:
		// b*: branch on condition
		if (evaluate_branch())
			m_ip = m_info.address;
		// TRAPS: A,I
		break;

	case 0x4c:
	case 0x4d:
		// bf*: branch on floating exception
		// FIXME: documentation is not clear, implementation is guesswork
		switch (R2)
		{
		case BF_ANY:
			// bfany: floating any exception
			if (m_psw & (PSW_FI | PSW_FV | PSW_FD | PSW_FU | PSW_FX))
				m_ip = m_info.address;
			break;
		case BF_BAD:
			// bfbad: floating bad result
			if (m_psw & (PSW_FI | PSW_FD))
				m_ip = m_info.address;
			break;
		default:
			// reserved
			// FIXME: not sure if this should trigger an exception?
			m_exception = EXCEPTION_ILLEGAL_OPERATION;
			break;
		}
		break;

	case 0x60:
	case 0x61:
		// loadw: load word
		temp = m_data->read_dword(m_info.address);
		if (!m_exception)
			m_r[R2] = temp;
		// TRAPS: C,U,A,P,R,I
		break;
	case 0x62:
	case 0x63:
		// loada: load address
		m_r[R2] = m_info.address;
		// TRAPS: I
		break;
	case 0x64:
	case 0x65:
		// loads: load single floating
		temp = m_data->read_dword(m_info.address);
		if (!m_exception)
			set_fp(R2, (float32)temp, F_NONE);
		// TRAPS: C,U,A,P,R,I
		break;
	case 0x66:
	case 0x67:
		// loadd: load double floating
		temp = m_data->read_qword(m_info.address);
		if (!m_exception)
			set_fp(R2, (float64)temp, F_NONE);
		// TRAPS: C,U,A,P,R,I
		break;
	case 0x68:
	case 0x69:
		// loadb: load byte
		temp = s64(s8(m_data->read_byte(m_info.address)));
		if (!m_exception)
			m_r[R2] = temp;
		// TRAPS: C,U,A,P,R,I
		break;
	case 0x6a:
	case 0x6b:
		// loadbu: load byte unsigned
		temp = m_data->read_byte(m_info.address);
		if (!m_exception)
			m_r[R2] = temp;
		// TRAPS: C,U,A,P,R,I
		break;
	case 0x6c:
	case 0x6d:
		// loadh: load halfword
		temp = s64(s16(m_data->read_word(m_info.address)));
		if (!m_exception)
			m_r[R2] = temp;
		// TRAPS: C,U,A,P,R,I
		break;
	case 0x6e:
	case 0x6f:
		// loadhu: load halfword unsigned
		temp = m_data->read_word(m_info.address);
		if (!m_exception)
			m_r[R2] = temp;
		// TRAPS: C,U,A,P,R,I
		break;
	case 0x70:
	case 0x71:
		// storw: store word
		m_data->write_dword(m_info.address, m_r[R2]);
		// TRAPS: A,P,W,I
		break;
	case 0x72:
	case 0x73:
		// tsts: test and set
		temp = m_data->read_dword(m_info.address);
		if (!m_exception)
		{
			m_data->write_dword(m_info.address, temp | 0x80000000U);
			if (!m_exception)
				m_r[R2] = temp;
		}
		// TRAPS: C,U,A,P,R,W,I
		break;
	case 0x74:
	case 0x75:
		// stors: store single floating
		m_data->write_dword(m_info.address, get_fp32(R2));
		// TRAPS: A,P,W,I
		break;
	case 0x76:
	case 0x77:
		// stord: store double floating
		m_data->write_qword(m_info.address, get_fp64(R2));
		// TRAPS: A,P,W,I
		break;
	case 0x78:
	case 0x79:
		// storb: store byte
		m_data->write_byte(m_info.address, m_r[R2]);
		// TRAPS: A,P,W,I
		break;

	case 0x7c:
	case 0x7d:
		// storh: store halfword
		m_data->write_word(m_info.address, m_r[R2]);
		// TRAPS: A,P,W,I
		break;

	case 0x80:
		// addw: add word
		FLAGS_CV(C_ADD(m_r[R2], m_r[R1]), V_ADD(m_r[R2], m_r[R1]))
		m_r[R2] += m_r[R1];
		FLAGS_ZN(m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: CVZN
		break;

	case 0x82:
		// addq: add quick
		FLAGS_CV(C_ADD(m_r[R2], R1), V_ADD(m_r[R2], R1))
		m_r[R2] += R1;
		FLAGS_ZN(m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: CVZN
		break;
	case 0x83:
		// addi: add immediate
		FLAGS_CV(C_ADD(m_r[R2], m_info.imm), V_ADD(m_r[R2], m_info.imm))
		m_r[R2] += m_info.imm;
		FLAGS_ZN(m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: CVZN
		// TRAPS: I
		break;
	case 0x84:
		// movw: move word
		m_r[R2] = m_r[R1];
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: 00ZN
		break;

	case 0x86:
		// loadq: load quick
		m_r[R2] = R1;
		FLAGS(0, 0, m_r[R2] == 0, 0)
		// FLAGS: 00Z0
		break;
	case 0x87:
		// loadi: load immediate
		m_r[R2] = m_info.imm;
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: 00ZN
		// TRAPS: I
		break;
	case 0x88:
		// andw: and word
		m_r[R2] &= m_r[R1];
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: 00ZN
		break;

	case 0x8b:
		// andi: and immediate
		m_r[R2] &= m_info.imm;
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: 00ZN
		// TRAPS: I
		break;
	case 0x8c:
		// orw: or word
		m_r[R2] |= m_r[R1];
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: 00ZN
		break;

	case 0x8f:
		// ori: or immediate
		m_r[R2] |= m_info.imm;
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: 00ZN
		// TRAPS: I
		break;
	case 0x90:
		// addwc: add word with carry
		FLAGS_CV(C_ADD(m_r[R2], (m_r[R1] + (PSW(C) ? 1 : 0))), V_ADD(m_r[R2], (m_r[R1] + (PSW(C) ? 1 : 0))))
		m_r[R2] += m_r[R1] + (PSW(C) ? 1 : 0);
		FLAGS_ZN(m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: CVZN
		break;
	case 0x91:
		// subwc: subtract word with carry
		FLAGS_CV(C_SUB(m_r[R2], (m_r[R1] + (PSW(C) ? 1 : 0))), V_SUB(m_r[R2], (m_r[R1] + (PSW(C) ? 1 : 0))))
		m_r[R2] -= m_r[R1] + (PSW(C) ? 1 : 0);
		FLAGS_ZN(m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: CVZN
		break;

	case 0x93:
		// negw: negate word
		FLAGS_CV(m_r[R1] != 0, m_r[R1] == INT32_MIN)
		m_r[R2] = -m_r[R1];
		FLAGS_ZN(m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: CVZN
		break;

	case 0x98:
		// mulw: multiply word
		m_r[R2] = (s32)m_r[R2] * (s32)m_r[R1];
		// FLAGS: 0V00
		break;
	case 0x99:
		// mulwx: multiply word extended
		((s64 *)m_r)[R2 >> 1] = (s64)m_r[R2] * (s64)m_r[R1];
		// FLAGS: 0V00
		break;
	case 0x9a:
		// mulwu: multiply word unsigned
		m_r[R2] = m_r[R2] * m_r[R1];
		// FLAGS: 0V00
		break;
	case 0x9b:
		// mulwux: multiply word unsigned extended
		((u64 *)m_r)[R2 >> 1] = (u64)m_r[R2] * (u64)m_r[R1];
		// FLAGS: 0V00
		break;
	case 0x9c:
		// divw: divide word
		if (m_r[R1] != 0)
		{
			// FLAGS: 0V00
			FLAGS(0, m_r[R2] == INT32_MIN && m_r[R1] == -1, 0, 0)
			m_r[R2] = (s32)m_r[R2] / (s32)m_r[R1];
		}
		else
			// TRAPS: D
			m_exception = EXCEPTION_INTEGER_DIVIDE_BY_ZERO;
		break;
	case 0x9d:
		// modw: modulus word
		if (m_r[R1] != 0)
		{
			// FLAGS: 0V00
			FLAGS(0, m_r[R2] == INT32_MIN && m_r[R1] == -1, 0, 0)
			m_r[R2] = (s32)m_r[R2] % (s32)m_r[R1];
		}
		else
			// TRAPS: D
			m_exception = EXCEPTION_INTEGER_DIVIDE_BY_ZERO;
		break;
	case 0x9e:
		// divwu: divide word unsigned
		if (m_r[R1] != 0)
		{
			m_r[R2] = m_r[R2] / m_r[R1];
			// FLAGS: 0000
			FLAGS(0, 0, 0, 0)
		}
		else
			// TRAPS: D
			m_exception = EXCEPTION_INTEGER_DIVIDE_BY_ZERO;
		break;
	case 0x9f:
		// modwu: modulus word unsigned
		if (m_r[R1] != 0)
		{
			m_r[R2] = m_r[R2] % m_r[R1];
			// FLAGS: 0000
			FLAGS(0, 0, 0, 0)
		}
		else
			// TRAPS: D
			m_exception = EXCEPTION_INTEGER_DIVIDE_BY_ZERO;
		break;
	case 0xa0:
		// subw: subtract word
		FLAGS_CV(C_SUB(m_r[R2], m_r[R1]), V_SUB(m_r[R2], m_r[R1]))
		m_r[R2] -= m_r[R1];
		FLAGS_ZN(m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: CVZN
		break;

	case 0xa2:
		// subq: subtract quick
		FLAGS_CV(C_SUB(m_r[R2], R1), V_SUB(m_r[R2], R1))
		m_r[R2] -= R1;
		FLAGS_ZN(m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: CVZN
		break;
	case 0xa3:
		// subi: subtract immediate
		FLAGS_CV(C_SUB(m_r[R2], m_info.imm), V_SUB(m_r[R2], m_info.imm))
		m_r[R2] -= m_info.imm;
		FLAGS_ZN(m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: CVZN
		// TRAPS: I
		break;
	case 0xa4:
		// cmpw: compare word
		FLAGS(C_SUB(m_r[R2], m_r[R1]), V_SUB(m_r[R2], m_r[R1]), m_r[R2] == m_r[R1], (s32)m_r[R2] < (s32)m_r[R1])
		// FLAGS: CVZN
		break;

	case 0xa6:
		// cmpq: compare quick
		FLAGS(C_SUB(m_r[R2], R1), V_SUB(m_r[R2], R1), m_r[R2] == R1, (s32)m_r[R2] < (s32)R1)
		// FLAGS: CVZN
		break;
	case 0xa7:
		// cmpi: compare immediate
		FLAGS(C_SUB(m_r[R2], m_info.imm), V_SUB(m_r[R2], m_info.imm), m_r[R2] == m_info.imm, (s32)m_r[R2] < (s32)m_info.imm)
		// FLAGS: CVZN
		// TRAPS: I
		break;
	case 0xa8:
		// xorw: exclusive or word
		m_r[R2] ^= m_r[R1];
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: 00ZN
		break;

	case 0xab:
		// xori: exclusive or immediate
		m_r[R2] ^= m_info.imm;
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: 00ZN
		// TRAPS: I
		break;
	case 0xac:
		// notw: not word
		m_r[R2] = ~m_r[R1];
		FLAGS(0, 0, m_r[R2] == 0, (s32)m_r[R2] < 0)
		// FLAGS: 00ZN
		break;

	case 0xae:
		// notq: not quick
		m_r[R2] = ~R1;
		FLAGS(0, 0, 0, 1)
		// FLAGS: 0001
		break;

	case 0xb4:
		// unprivileged macro instructions
		switch (m_info.subopcode)
		{
		case 0x00: case 0x01: case 0x02: case 0x03:
		case 0x04: case 0x05: case 0x06: case 0x07:
		case 0x08: case 0x09: case 0x0a: case 0x0b:
		case 0x0c:
			// savew0..savew12: push registers rN:r14

			// store ri at sp - 4 * (15 - i)
			for (int i = R2; i < 15 && !m_exception; i++)
				m_data->write_dword(m_r[15] - 4 * (15 - i), m_r[i]);

			// decrement sp after push to allow restart on exceptions
			if (!m_exception)
				m_r[15] -= 4 * (15 - R2);
			// TRAPS: A,P,W
			break;
		// NOTE: the movc, initc and cmpc macro instructions are implemented in a very basic way because
		// at some point they will need to be improved to deal with possible exceptions (e.g. page faults)
		// that may occur during execution. The implementation here is intended to allow the instructions
		// to be "continued" after such exceptions.
		case 0x0d:
			// movc: copy r0 bytes from r1 to r2

			while (m_r[0])
			{
				const u8 byte = m_data->read_byte(m_r[1]);
				if (m_exception)
					break;

				m_data->write_byte(m_r[2], byte);
				if (m_exception)
					break;

				m_r[0]--;
				m_r[1]++;
				m_r[2]++;
			}
			// TRAPS: C,U,P,R,W
			break;
		case 0x0e:
			// initc: initialise r0 bytes at r1 with value in r2
			while (m_r[0])
			{
				m_data->write_byte(m_r[1], m_r[2]);
				if (m_exception)
					break;

				m_r[0]--;
				m_r[1]++;
				m_r[2] = rotr32(m_r[2], 8);
			}
			// TRAPS: P,W
			break;
		case 0x0f:
			// cmpc: compare r0 bytes at r1 with r2

			// set condition codes assuming strings match
			FLAGS(0, 0, 1, 0);

			while (m_r[0])
			{
				// set condition codes and abort the loop if the current byte does not match
				s32 byte1 = s8(m_data->read_byte(m_r[1]));
				if (m_exception)
					break;
				s32 byte2 = s8(m_data->read_byte(m_r[2]));
				if (m_exception)
					break;
				if (byte1 != byte2)
				{
					FLAGS(C_SUB(byte2, byte1), V_SUB(byte2, byte1), byte2 == byte1, byte2 < byte1)
					break;
				}

				m_r[0]--;
				m_r[1]++;
				m_r[2]++;
			}
			// TRAPS: C,U,P,R
			break;
		case 0x10: case 0x11: case 0x12: case 0x13:
		case 0x14: case 0x15: case 0x16: case 0x17:
		case 0x18: case 0x19: case 0x1a: case 0x1b:
		case 0x1c:
			// restwN..restw12: pop registers rN:r14

			// load ri from sp + 4 * (i - N)
			for (int i = R2; i < 15; i++)
			{
				temp = m_data->read_dword(m_r[15] + 4 * (i - R2));
				if (m_exception)
					break;
				m_r[i] = temp;
			}

			// increment sp after pop to allow restart on exceptions
			if (!m_exception)
				m_r[15] += 4 * (15 - R2);
			// TRAPS: C,U,A,P,R
			break;

		case 0x20: case 0x21: case 0x22: case 0x23:
		case 0x24: case 0x25: case 0x26: case 0x27:
			// saved0..saved7: push registers fN:f7

			// store fi at sp - 8 * (8 - i)
			for (int i = R2; i < 8 && !m_exception; i++)
				m_data->write_qword(m_r[15] - 8 * (8 - i), get_fp64(i));

			// decrement sp after push to allow restart on exceptions
			if (!m_exception)
				m_r[15] -= 8 * (8 - R2);
			// TRAPS: A,P,W
			break;
		case 0x28: case 0x29: case 0x2a: case 0x2b:
		case 0x2c: case 0x2d: case 0x2e: case 0x2f:
			// restd0..restd7: pop registers fN:f7

			// load fi from sp + 8 * (i - N)
			for (int i = R2; i < 8; i++)
			{
				temp = m_data->read_qword(m_r[15] + 8 * (i - R2));
				if (m_exception)
					break;
				set_fp(i, (float64)temp, F_NONE);
			}

			// increment sp after pop to allow restart on exceptions
			if (!m_exception)
				m_r[15] += 8 * (8 - R2);
			// TRAPS: C,U,A,P,R
			break;
		case 0x30:
			// cnvsw: convert single floating to word
			m_fp_pc = m_pc;

			m_r[m_info.macro & 0xf] = float32_to_int32(get_fp32((m_info.macro >> 4) & 0xf));
			// TRAPS: F_IX
			float_exception_flags &= F_IX;
			break;
		case 0x31:
			// cnvrsw: convert rounding single floating to word (non-IEEE +0.5/-0.5 rounding)
			m_fp_pc = m_pc;

			if (float32_lt(get_fp32((m_info.macro >> 4) & 0xf), 0))
				m_r[m_info.macro & 0xf] = float32_to_int32_round_to_zero(float32_sub(get_fp32((m_info.macro >> 4) & 0xf),
					float32_div(int32_to_float32(1), int32_to_float32(2))));
			else
				m_r[m_info.macro & 0xf] = float32_to_int32_round_to_zero(float32_add(get_fp32((m_info.macro >> 4) & 0xf),
					float32_div(int32_to_float32(1), int32_to_float32(2))));
			// TRAPS: F_IX
			float_exception_flags &= F_IX;
			break;
		case 0x32:
			// cnvtsw: convert truncating single floating to word
			m_fp_pc = m_pc;

			m_r[m_info.macro & 0xf] = float32_to_int32_round_to_zero(get_fp32((m_info.macro >> 4) & 0xf));
			// TRAPS: F_IX
			float_exception_flags &= F_IX;
			break;
		case 0x33:
			// cnvws: convert word to single floating
			set_fp(m_info.macro & 0xf, int32_to_float32(m_r[(m_info.macro >> 4) & 0xf]), F_X);
			// TRAPS: F_X
			break;
		case 0x34:
			// cnvdw: convert double floating to word
			m_fp_pc = m_pc;

			m_r[m_info.macro & 0xf] = float64_to_int32(get_fp64((m_info.macro >> 4) & 0xf));
			// TRAPS: F_IX
			float_exception_flags &= F_IX;
			break;
		case 0x35:
			// cnvrdw: convert rounding double floating to word (non-IEEE +0.5/-0.5 rounding)
			m_fp_pc = m_pc;

			if (float64_lt(get_fp64((m_info.macro >> 4) & 0xf), 0))
				m_r[m_info.macro & 0xf] = float64_to_int32_round_to_zero(float64_sub(get_fp64((m_info.macro >> 4) & 0xf),
					float64_div(int32_to_float64(1), int32_to_float64(2))));
			else
				m_r[m_info.macro & 0xf] = float64_to_int32_round_to_zero(float64_add(get_fp64((m_info.macro >> 4) & 0xf),
					float64_div(int32_to_float64(1), int32_to_float64(2))));
			// TRAPS: F_IX
			float_exception_flags &= F_IX;
			break;
		case 0x36:
			// cnvtdw: convert truncating double floating to word
			m_fp_pc = m_pc;

			m_r[m_info.macro & 0xf] = float64_to_int32_round_to_zero(get_fp64((m_info.macro >> 4) & 0xf));
			// TRAPS: F_IX
			float_exception_flags &= F_IX;
			break;
		case 0x37:
			// cnvwd: convert word to double floating
			set_fp(m_info.macro & 0xf, int32_to_float64(m_r[(m_info.macro >> 4) & 0xf]), F_NONE);
			break;
		case 0x38:
			// cnvsd: convert single to double floating
			set_fp(m_info.macro & 0xf, float32_to_float64(get_fp32((m_info.macro >> 4) & 0xf)), F_I);
			// TRAPS: F_I
			break;
		case 0x39:
			// cnvds: convert double to single floating
			set_fp(m_info.macro & 0xf, float64_to_float32(get_fp64((m_info.macro >> 4) & 0xf)), F_IVUX);
			// TRAPS: F_IVUX
			break;
		case 0x3a:
			// negs: negate single floating
			set_fp(m_info.macro & 0xf, float32_mul(get_fp32((m_info.macro >> 4) & 0xf), int32_to_float32(-1)), F_NONE);
			break;
		case 0x3b:
			// negd: negate double floating
			set_fp(m_info.macro & 0xf, float64_mul(get_fp64((m_info.macro >> 4) & 0xf), int32_to_float64(-1)), F_NONE);
			break;
		case 0x3c:
			/*
			 * This implementation for scalbd and scalbs is a bit opaque, but
			 * essentially we check if the integer value is within range, and
			 * directly create a floating constant representing 2^n or NaN
			 * respectively, which is used as an input to a multiply, producing
			 * the desired result. While doing an actual multiply is both
			 * inefficient and unnecessary, it's a tidy way to ensure the
			 * correct exception flags are set.
			 */
			// scalbs: scale by, single floating
			set_fp(m_info.macro & 0xf, float32_mul(get_fp32(m_info.macro & 0xf),
				(((s32)m_r[(m_info.macro >> 4) & 0xf] > -127 && (s32)m_r[(m_info.macro >> 4) & 0xf] < 128)
					? (float32)(((s32)m_r[(m_info.macro >> 4) & 0xf] + 127) << 23)
					: (float32)~u32(0))), F_IVUX);
			// TRAPS: F_IVUX
			break;
		case 0x3d:
			// scalbd: scale by, double floating
			set_fp(m_info.macro & 0xf, float64_mul(get_fp64(m_info.macro & 0xf),
				((s32)m_r[(m_info.macro >> 4) & 0xf] > -1023 && (s32)m_r[(m_info.macro >> 4) & 0xf] < 1024)
					? (float64)((u64)((s32)m_r[(m_info.macro >> 4) & 0xf] + 1023) << 52)
					: (float64)~u64(0)), F_IVUX);
			// TRAPS: F_IVUX
			break;
		case 0x3e:
			// trapfn: trap floating unordered
			// TRAPS: I
			if (PSW(Z) && PSW(N))
				m_exception = EXCEPTION_ILLEGAL_OPERATION;
			break;
		case 0x3f:
			// loadfs: load floating status
			m_r[(m_info.macro >> 4) & 0xf] = m_fp_pc;
			m_f[m_info.macro & 0xf] = m_fp_dst;
			m_ssw |= SSW_FRD;
			break;

		default:
			m_exception = EXCEPTION_ILLEGAL_OPERATION;
			break;
		}
		break;

	case 0xb6:
		// privileged macro instructions
		if (!SSW(U))
		{
			switch (m_info.subopcode)
			{
			case 0x00:
				// movus: move user to supervisor
				m_rs[m_info.macro & 0xf] = m_ru[(m_info.macro >> 4) & 0xf];
				FLAGS(0, 0, m_rs[m_info.macro & 0xf] == 0, (s32)m_rs[m_info.macro & 0xf] < 0)
				// FLAGS: 00ZN
				// TRAPS: S
				break;
			case 0x01:
				// movsu: move supervisor to user
				m_ru[m_info.macro & 0xf] = m_rs[(m_info.macro >> 4) & 0xf];
				FLAGS(0, 0, m_ru[m_info.macro & 0xf] == 0, (s32)m_ru[m_info.macro & 0xf] < 0)
				// FLAGS: 00ZN
				// TRAPS: S
				break;
			case 0x02:
				// saveur: save user registers
				for (int i = 0; i < 16 && !m_exception; i++)
					m_data->write_dword(m_rs[(m_info.macro >> 4) & 0xf] - 4 * (i + 1), m_ru[15 - i]);

				if (!m_exception)
					m_rs[(m_info.macro >> 4) & 0xf] -= 64;
				// TRAPS: A,P,W,S
				break;
			case 0x03:
				// restur: restore user registers
				for (int i = 0; i < 16; i++)
				{
					temp = m_data->read_dword(m_rs[(m_info.macro >> 4) & 0xf] + 4 * i);
					if (m_exception)
						break;
					m_ru[i] = temp;
				}

				if (!m_exception)
					m_rs[(m_info.macro >> 4) & 0xf] += 64;
				// TRAPS: C,U,A,P,R,S
				break;
			case 0x04:
				// reti: restore psw, ssw and pc from supervisor stack
				m_ip = reti();
				// TRAPS: S
				break;
			case 0x05:
				// wait: wait for interrupt
				m_ip = m_pc;
				// TRAPS: S
				break;

			default:
				m_exception = EXCEPTION_ILLEGAL_OPERATION;
				break;
			}
		}
		else
			m_exception = EXCEPTION_PRIVILEGED_INSTRUCTION;
		break;

	default:
		m_exception = EXCEPTION_ILLEGAL_OPERATION;
		break;
	}
}

u32 clipper_device::reti()
{
	// fetch the psw, ssw and pc from the supervisor stack
	const u32 new_psw = m_data->read_dword(m_rs[(m_info.macro >> 4) & 0xf] + 0);
	if (m_exception)
		fatalerror("reti unrecoverable fault 0x%04x read psw address 0x%08x pc 0x%08x\n", m_exception, m_rs[(m_info.macro >> 4) & 0xf] + 0, m_pc);

	const u32 new_ssw = m_data->read_dword(m_rs[(m_info.macro >> 4) & 0xf] + 4);
	if (m_exception)
		fatalerror("reti unrecoverable fault 0x%04x read ssw address 0x%08x pc 0x%08x\n", m_exception, m_rs[(m_info.macro >> 4) & 0xf] + 4, m_pc);

	const u32 new_pc = m_data->read_dword(m_rs[(m_info.macro >> 4) & 0xf] + 8);
	if (m_exception)
		fatalerror("reti unrecoverable fault 0x%04x read pc address 0x%08x pc 0x%08x\n", m_exception, m_rs[(m_info.macro >> 4) & 0xf] + 8, m_pc);

	LOGMASKED(LOG_EXCEPTION, "reti r%d ssp 0x%08x pc 0x%08x ssw 0x%08x psw 0x%08x new_pc 0x%08x new_ssw 0x%08x new_psw 0x%08x\n",
		(m_info.macro >> 4) & 0xf, m_rs[(m_info.macro >> 4) & 0xf], m_pc, m_ssw, m_psw, new_pc, new_ssw, new_psw);

	// pop the stack
	m_rs[(m_info.macro >> 4) & 0xf] += 12;

	// restore the psw and ssw
	set_psw(new_psw);
	set_ssw(new_ssw);

	// return the restored pc
	return new_pc;
}

/*
 * Common entry point for transferring control in the event of an interrupt or
 * exception. Reading between the lines, it appears this logic was implemented
 * using the macro instruction ROM and a special macro instruction (intrap).
 */
u32 clipper_device::intrap(const u16 vector, const u32 old_pc)
{
	const u32 old_ssw = m_ssw;
	const u32 old_psw = m_psw;

	// clear ssw bits to enable supervisor memory access
	m_ssw &= ~(SSW_U | SSW_K | SSW_UU | SSW_KU);

	// clear exception state
	m_exception = 0;

	// fetch next pc and ssw from interrupt vector
	const u32 new_pc = m_data->read_dword(vector + get_evpc_offset());
	if (m_exception)
		fatalerror("intrap unrecoverable fault 0x%04x read pc address 0x%08x pc 0x%08x\n", m_exception, vector + get_evpc_offset(), old_pc);

	const u32 new_ssw = m_data->read_dword(vector + get_evssw_offset());
	if (m_exception)
		fatalerror("intrap unrecoverable fault 0x%04x read ssw address 0x%08x pc 0x%08x\n", m_exception, vector + get_evssw_offset(), old_pc);

	LOGMASKED(LOG_EXCEPTION, "intrap vector 0x%04x pc 0x%08x ssp 0x%08x new_pc 0x%08x new_ssw 0x%08x\n", vector, old_pc, m_rs[15], new_pc, new_ssw);

	// derive cts and mts from vector
	u32 source = 0;
	switch (vector)
	{
		// data memory trap group
	case EXCEPTION_D_CORRECTED_MEMORY_ERROR:
	case EXCEPTION_D_UNCORRECTABLE_MEMORY_ERROR:
	case EXCEPTION_D_ALIGNMENT_FAULT:
	case EXCEPTION_D_PAGE_FAULT:
	case EXCEPTION_D_READ_PROTECT_FAULT:
	case EXCEPTION_D_WRITE_PROTECT_FAULT:

		// instruction memory trap group
	case EXCEPTION_I_CORRECTED_MEMORY_ERROR:
	case EXCEPTION_I_UNCORRECTABLE_MEMORY_ERROR:
	case EXCEPTION_I_ALIGNMENT_FAULT:
	case EXCEPTION_I_PAGE_FAULT:
	case EXCEPTION_I_EXECUTE_PROTECT_FAULT:
		source = (vector & MTS_VMASK) << (MTS_SHIFT - MTS_VSHIFT);
		break;

		// integer arithmetic trap group
	case EXCEPTION_INTEGER_DIVIDE_BY_ZERO:
		source = CTS_DIVIDE_BY_ZERO;
		break;

		// illegal operation trap group
	case EXCEPTION_ILLEGAL_OPERATION:
		source = CTS_ILLEGAL_OPERATION;
		break;
	case EXCEPTION_PRIVILEGED_INSTRUCTION:
		source = CTS_PRIVILEGED_INSTRUCTION;
		break;

		// diagnostic trap group
	case EXCEPTION_TRACE:
		source = CTS_TRACE_TRAP;
		break;
	}

	// push pc, ssw and psw onto supervisor stack
	m_data->write_dword(m_rs[15] - 0x4, old_pc);
	if (m_exception)
		fatalerror("intrap unrecoverable fault 0x%04x write pc address 0x%08x pc 0x%08x\n", m_exception, m_rs[15] - 0x4, old_pc);
	m_data->write_dword(m_rs[15] - 0x8, old_ssw);
	if (m_exception)
		fatalerror("intrap unrecoverable fault 0x%04x write ssw address 0x%08x pc 0x%08x\n", m_exception, m_rs[15] - 0x8, old_pc);
	m_data->write_dword(m_rs[15] - 0xc, (old_psw & ~(PSW_CTS | PSW_MTS)) | source);
	if (m_exception)
		fatalerror("intrap unrecoverable fault 0x%04x write psw address 0x%08x pc 0x%08x\n", m_exception, m_rs[15] - 0xc, old_pc);

	// decrement supervisor stack pointer
	m_rs[15] -= get_eframe_size();

	// set ssw from vector and previous mode
	set_ssw((new_ssw & ~SSW_P) | (old_ssw & SSW_U) << 1);

	// clear psw
	set_psw(0);

	// return new pc from trap vector
	return new_pc;
}

bool clipper_device::evaluate_branch() const
{
	switch (m_info.r2)
	{
	case BRANCH_T:
		return true;

	case BRANCH_LT:
		return (!PSW(V) && !PSW(Z) && !PSW(N))
			|| (PSW(V) && !PSW(Z) && PSW(N));

	case BRANCH_LE:
		return (!PSW(V) && !PSW(N))
			|| (PSW(V) && !PSW(Z) && PSW(N));

	case BRANCH_EQ:
		return PSW(Z) && !PSW(N);

	case BRANCH_GT:
		return (!PSW(V) && !PSW(Z) && PSW(N))
			|| (PSW(V) && !PSW(N));

	case BRANCH_GE:
		return (PSW(V) && !PSW(N))
			|| (!PSW(V) && !PSW(Z) && PSW(N))
			|| (PSW(Z) && !PSW(N));

	case BRANCH_NE:
		return (!PSW(Z))
			|| (PSW(Z) && PSW(N));

	case BRANCH_LTU:
		return (!PSW(C) && !PSW(Z));

	case BRANCH_LEU:
		return !PSW(C);

	case BRANCH_GTU:
		return PSW(C);

	case BRANCH_GEU:
		return PSW(C) || PSW(Z);

	case BRANCH_V:
		return PSW(V);
	case BRANCH_NV:
		return !PSW(V);

	case BRANCH_N:
		return !PSW(Z) && PSW(N);
	case BRANCH_NN:
		return !PSW(N);

	case BRANCH_FN:
		return PSW(Z) && PSW(N);
	}

	return false;
}

void clipper_device::set_psw(const u32 psw)
{
	// retain read-only endianness field
	m_psw = (m_psw & PSW_BIG) | (psw & ~PSW_BIG);

	// set the softfloat rounding mode based on the psw rounding mode
	switch (PSW(FR))
	{
	case FR_0: float_rounding_mode = float_round_nearest_even; break;
	case FR_1: float_rounding_mode = float_round_up; break;
	case FR_2: float_rounding_mode = float_round_down; break;
	case FR_3: float_rounding_mode = float_round_to_zero; break;
	}
}

void clipper_device::set_ssw(const u32 ssw)
{
	// retain read-only id field
	m_ssw = (m_ssw & SSW_ID) | (ssw & ~SSW_ID);

	// select the register file
	m_r = SSW(U) ? m_ru : m_rs;
}

void clipper_device::fp_exception()
{
	u16 exception = 0;

	/*
	 * Set the psw floating exception flags, and identify any enabled
	 * exceptions. The order here is important, but since the documentation
	 * doesn't explicitly specify, this is a guess. Simply put, exceptions
	 * are considered in sequence with an increasing order of priority.
	 */
	if (float_exception_flags & float_flag_inexact)
	{
		m_psw |= PSW_FX;
		if (PSW(EFX))
			exception = EXCEPTION_FLOATING_INEXACT;
	}
	if (float_exception_flags & float_flag_underflow)
	{
		m_psw |= PSW_FU;
		if (PSW(EFU))
			exception = EXCEPTION_FLOATING_UNDERFLOW;
	}
	if (float_exception_flags & float_flag_overflow)
	{
		m_psw |= PSW_FV;
		if (PSW(EFV))
			exception = EXCEPTION_FLOATING_OVERFLOW;
	}
	if (float_exception_flags & float_flag_divbyzero)
	{
		m_psw |= PSW_FD;
		if (PSW(EFD))
			exception = EXCEPTION_FLOATING_DIVIDE_BY_ZERO;
	}
	if (float_exception_flags & float_flag_invalid)
	{
		m_psw |= PSW_FI;
		if (PSW(EFI))
			exception = EXCEPTION_FLOATING_INVALID_OPERATION;
	}

	// trigger a floating point exception
	if (PSW(EFT) && exception)
		m_exception = exception;
}

void clipper_c400_device::execute_instruction()
{
	switch (m_info.opcode)
	{
#ifdef UNIMPLEMENTED_C400
	case 0x46:
	case 0x47:
		// loadd2:
		break;

	case 0x4a:
	case 0x4b:
		// cdb:
		break;
	case 0x4c:
	case 0x4d:
		// cdbeq:
		break;
	case 0x4e:
	case 0x4f:
		// cdbne:
		break;
	case 0x50:
	case 0x51:
		// db*:
		break;

	case 0xb0:
		// abss: absolute value single floating?
		break;

	case 0xb2:
		// absd: absolute value double floating?
		break;

	case 0xb6:
		// privileged macro instructions
		if (!SSW(U))
		{
			switch (m_info.subopcode)
			{
			case 0x07:
				// loadts: unknown?
				break;
			}
		}
		break;

	case 0xbc:
		// waitd:
		break;

	case 0xc0:
		// s*:
		break;
#endif

	default:
		clipper_device::execute_instruction();
		break;
	}
}

util::disasm_interface *clipper_device::create_disassembler()
{
	return new clipper_disassembler;
}
