// license:BSD-3-Clause
// copyright-holders:Aaron Giles
/***************************************************************************

    jagdasm.c
    Disassembler for the portable Jaguar DSP emulator.
    Written by Aaron Giles

***************************************************************************/

#ifndef MAME_CPU_JAGUAR_JAGDASM_H
#define MAME_CPU_JAGUAR_JAGDASM_H

#pragma once

class jaguar_disassembler : public util::disasm_interface
{
public:
	static constexpr unsigned JAGUAR_VARIANT_GPU = 0;
	static constexpr unsigned JAGUAR_VARIANT_DSP = 1;

	jaguar_disassembler(u32 variant);
	virtual ~jaguar_disassembler() = default;

	virtual u32 opcode_alignment() const override;
	virtual offs_t disassemble(std::ostream &stream, offs_t pc, const data_buffer &opcodes, const data_buffer &params) override;

private:
	static const uint8_t convert_zero[32];
	static const char *const condition[32];

	u32 m_variant;

	static std::string signed_16bit(int16_t val);
};

#endif
