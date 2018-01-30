// license:BSD-3-Clause
// copyright-holders:Olivier Galibert
/***************************************************************************

    dirom.h

    Interface to a rom, either through a memory map or a region

***************************************************************************/

#pragma once

#ifndef __EMU_H__
#error Dont include this file directly; include emu.h instead.
#endif

#ifndef MAME_EMU_DIROM_H
#define MAME_EMU_DIROM_H

#define MCFG_DEVICE_ROM(_rom_tag) \
	device_rom_interface::static_set_device_rom_tag(*device, _rom_tag);

class device_rom_interface : public device_memory_interface
{
public:
	device_rom_interface(const machine_config &mconfig, device_t &device, u8 addrwidth, endianness_t endian = ENDIANNESS_LITTLE, u8 datawidth = 8);
	virtual ~device_rom_interface();

	static void static_set_device_rom_tag(device_t &device, const char *tag);

	inline u8 read_byte(offs_t byteaddress) { return m_rom_direct->read_byte(byteaddress); }
	inline u16 read_word(offs_t byteaddress) { return m_rom_direct->read_word(byteaddress); }
	inline u32 read_dword(offs_t byteaddress) { return m_rom_direct->read_dword(byteaddress); }
	inline u64 read_qword(offs_t byteaddress) { return m_rom_direct->read_qword(byteaddress); }

	void set_rom(const void *base, u32 size);
	void set_rom_bank(int bank);

protected:
	virtual void rom_bank_updated() = 0;
	virtual space_config_vector memory_space_config() const override;

	void set_rom_endianness(endianness_t endian) { assert(!device().configured()); m_rom_config.m_endianness = endian; }
	void set_rom_data_width(u8 width) { assert(!device().configured()); m_rom_config.m_data_width = width; }
	void set_rom_addr_width(u8 width) { assert(!device().configured()); m_rom_config.m_addr_width = m_rom_config.m_logaddr_width = width; }

private:
	const char *m_rom_tag;
	address_space_config m_rom_config;
	direct_read_data<0> *m_rom_direct;

	memory_bank *m_bank;
	int m_cur_bank, m_bank_count;

	virtual void interface_pre_start() override;
	virtual void interface_post_load() override;
};

#endif // MAME_EMU_DIROM_H
