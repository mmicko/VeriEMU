// license:BSD-3-Clause
// copyright-holders:David Haywood, Xing Xing, Andreas Naive
/* PGM 2 hardware.

    Motherboard is bare bones stuff, and does not contain any ROMs.
    The IGS036 used by the games is an ARM based CPU, like IGS027A used on PGM1 it has internal ROM.
    Decryption should be correct in most cases.
    The ARM appears to be ARMv5T, probably an ARM9.

    PGM2 Motherboard Components:

     IS61LV25616AL(SRAM)
     IGS037(GFX PROCESSOR)
     YMZ774-S(SOUND)
     R5F21256SN(extra MCU for ICcard communication)
      - Appears to be referred to by the games as MPU

    Cartridges
     IGS036 (MAIN CPU) (differs per game, internal code)
     ROMs
     Custom program ROM module (KOV3 only)
      - on some games ROM socket contains Flash ROM + SRAM

     QFP100 chip (Xlinx CPLD)

     Single PCB versions of some of the titles were also available

    Only 5 Games were released for this platform, 3 of which are just updates / re-releases of older titles!
    The platform has since been superseded by PGM3, see pgm3.cpp

    Oriental Legend 2
    The King of Fighters '98 - Ultimate Match - Hero
    Knights of Valour 2 New Legend
    Dodonpachi Daioujou Tamashii
    Knights of Valour 3

    These were only released as single board PGM2 based hardware, seen for sale in Japan for around $250-$300

    Jigsaw World Arena
    Puzzle of Ocha / Ochainu No Pazuru


    ToDo (emulation issues):

    Support remaining games (need IGS036 dumps)
    Identify which regions each game was released in and either dump alt. internal ROMs for each region, or
      create them until that can be done.
    properly implement RTC (integrated into the CPU)
    Verify Sprite Zoom (check exactly which pixels are doubled / missed on hardware for flipped , non-flipped cases etc.)
    Fix Save States (is this a driver problem or an ARM core problem, they don't work unless you get through the startup tests)
    Determine motherboard card reader MCU internal ROM size and add as NO_DUMP to the sets
    See if kov2nl needs another idle skip, after Game Over there is a period where the current one is ineffective

    Debug features (require DIP SW1:8 On and SW1:1 Off):
    - QC TEST mode: hold P1 A+B during boot
    - Debug/Cheat mode: hold P1 B+C during boot
      orleg2 and kov2nl: when ingame pressing P1 Start skips to next location, where might be more unknown debug features.


    Holographic Stickers

    The IGS036 CPUs have holographic stickers on them, there is a number printed on each sticker but it doesn't seem connected to the
    game code / revision contained within, it might just be to mark the date the board was produced as it seems to coincide with the
    design of the hologram.  For reference the ones being used for dumping are

    Dodonpachi Daioujou Tamashi (China) - W10
    King of Fighter 98 UMH (China) - C11
    Knights of Valour 2 (China) - V21
    Knights of Valour 3 (China) - V21
    Oriental Legend 2 (Oversea) - V21
    Oriental Legend 2 (China) - A8

    GPU registers, located at 301200xx, 16bit access.
    00 - bg scroll x
    02 - bg scroll y
    04 - zoom something, 0F-7F, default 1F
    06 - zoom something, 0F-7F, default 1F
    08 - fg scroll x
    0a - fg scroll y
    0e - resolution, 0 - low (kof98), 1 - high (rest of games)
    10 - ? orleg2 - 0x13, kov2nl, kof98, ddpdojt - 0x14 at init
    14 - sprite enable ? set to 0 before spriteram update, to 1 after
    16 - set to 1 before access to vrams/palettes, reset after. bits: 0 - bg ram and palette, 1 - fg ram and palette, 2 - sprite palette.
    18 - vblank ack
    1a - ? 0 at init
    1c - ? orleg2 - 5, kov2nl, kof, ddpdojt - 7 at init
    1e - ? 2 at init
    32 - shared RAM bank
    34, 36 - ? 0 at init
    38, 3a - sprite mask xor key

*/

#include "includes/pgm2.h"

// checked on startup, or doesn't boot
READ32_MEMBER(pgm2_state::unk_startup_r)
{
	logerror("%s: unk_startup_r\n", machine().describe_context().c_str());
	return 0x00000180;
}

READ32_MEMBER(pgm2_state::rtc_r)
{
	// write to FFFFFD20 if bit 18 set (0x40000) probably reset this RTC timer
	// TODO: somehow hook here current time/date, which is a bit complicated because value is relative, later to it added "base time" stored in SRAM
	return machine().time().seconds();
}

READ8_MEMBER(pgm2_state::encryption_r)
{
	return m_encryption_table[offset];
}

WRITE8_MEMBER(pgm2_state::encryption_w)
{
	m_encryption_table[offset] = data;
}

WRITE32_MEMBER(pgm2_state::sprite_encryption_w)
{
	COMBINE_DATA(&m_spritekey);

	if (!m_sprite_predecrypted)
		m_realspritekey = bitswap<32>(m_spritekey ^ 0x90055555, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31);
}

void pgm2_state::postload()
{
	// make sure the encrypted area is in the correct state after we load a savestate because we don't want to have to save the whole rom.

	memcpy(memregion("user1")->base(), &m_encrypted_copy[0], memregion("user1")->bytes());

	if (m_has_decrypted_kov3_module)
	{
		decrypt_kov3_module(module_key->addr_xor, module_key->data_xor);
	}

	if (m_has_decrypted)
	{
		igs036_decryptor decrypter(m_encryption_table);

		if (m_romboard_ram)
		{
			decrypter.decrypter_rom((uint16_t*)memregion("user1")->base(), memregion("user1")->bytes(), 0x0200000);
		}
		else
		{
			decrypter.decrypter_rom((uint16_t*)memregion("user1")->base(), memregion("user1")->bytes(), 0);
		}
	}
}

WRITE32_MEMBER(pgm2_state::encryption_do_w)
{
	if (m_romboard_ram)
	{
		igs036_decryptor decrypter(m_encryption_table);
		decrypter.decrypter_rom((uint16_t*)&m_romboard_ram[0], 0x0200000, 0);
		decrypter.decrypter_rom((uint16_t*)memregion("user1")->base(), memregion("user1")->bytes(), 0x0200000);   // assume the rom at 0x0200000 also gets decrypted as if it was at 0x0200000 even if it isn't used (the game has already copied it to RAM where it properly decrypted)
		m_has_decrypted = 1;
	}
	else
	{
		igs036_decryptor decrypter(m_encryption_table);
		decrypter.decrypter_rom((uint16_t*)memregion("user1")->base(), memregion("user1")->bytes(), 0);
		m_has_decrypted = 1;
	}
}


INTERRUPT_GEN_MEMBER(pgm2_state::igs_interrupt)
{
	m_arm_aic->set_irq(12, ASSERT_LINE);
}

WRITE16_MEMBER(pgm2_state::share_bank_w)
{
	COMBINE_DATA(&m_share_bank);
}

READ8_MEMBER(pgm2_state::shareram_r)
{
	return m_shareram[offset + (m_share_bank & 1) * 128];
}
WRITE8_MEMBER(pgm2_state::shareram_w)
{
	m_shareram[offset + (m_share_bank & 1) * 128] = data;
}


TIMER_DEVICE_CALLBACK_MEMBER(pgm2_state::igs_interrupt2)
{
	m_arm_aic->set_irq(3, ASSERT_LINE);
}

// "MPU" MCU HLE starts here
// command delays are far from correct, might not work in other games
// command results probably incorrect (except for explicit checked bytes)
void pgm2_state::mcu_command(address_space &space, bool is_command)
{
	uint8_t cmd = m_mcu_regs[0] & 0xff;
	//  if (is_command && cmd != 0xf6)
	//      logerror("MCU command %08x %08x\n", m_mcu_regs[0], m_mcu_regs[1]);

	if (is_command)
	{
		m_mcu_last_cmd = cmd;
		uint8_t status = 0xf7; // "command accepted" status
		int delay = 1;

		uint8_t arg1 = m_mcu_regs[0] >> 8;
		uint8_t arg2 = m_mcu_regs[0] >> 16;
		uint8_t arg3 = m_mcu_regs[0] >> 24;
		switch (cmd)
		{
		case 0xf6:  // get result
			m_mcu_regs[3] = m_mcu_result0;
			m_mcu_regs[4] = m_mcu_result1;
			m_mcu_last_cmd = 0;
			break;
		case 0xe0: // command port test
			m_mcu_result0 = m_mcu_regs[0];
			m_mcu_result1 = m_mcu_regs[1];
			break;
		case 0xe1: // shared ram access (unimplemented)
		{
			// MCU access to RAM shared at 0x30100000, 2x banks, in the same time CPU and MCU access different banks
			uint8_t mode = m_mcu_regs[0] >> 16; // 0 - ???, 1 - read, 2 - write
			uint8_t data = m_mcu_regs[0] >> 24;
			if (mode == 2)
			{
				// where is offset ? so far assume this command fill whole page
				memset(&m_shareram[(~m_share_bank & 1) * 128], data, 128);
			}
			m_mcu_result0 = cmd;
			m_mcu_result1 = 0;
		}
		break;
		// C0-C9 commands is IC Card RW comms
		case 0xc0: // insert card or/and check card presence. result: F7 - ok, F4 - no card
			if (m_memcard[arg1 & 3]->present() == -1)
				status = 0xf4;
			m_mcu_result0 = cmd;
			break;
		case 0xc1: // check ready/busy ?
			if (m_memcard[arg1 & 3]->present() == -1)
				status = 0xf4;
			m_mcu_result0 = cmd;
			break;
		case 0xc2: // read data to shared ram
			for (int i = 0; i < arg3; i++)
			{
				if (m_memcard[arg1 & 3]->present() != -1)
					m_shareram[i + (~m_share_bank & 1) * 128] = m_memcard[arg1 & 3]->read(space, arg2 + i);
				else
					status = 0xf4;
			}
			m_mcu_result0 = cmd;
			break;
		case 0xc3: // save data from shared ram
			for (int i = 0; i < arg3; i++)
			{
				if (m_memcard[arg1 & 3]->present() != -1)
					m_memcard[arg1 & 3]->write(space, arg2 + i, m_shareram[i + (~m_share_bank & 1) * 128]);
				else
					status = 0xf4;
			}
			m_mcu_result0 = cmd;
			break;
		case 0xc4: // presumable read security mem (password only?)
			if (m_memcard[arg1 & 3]->present() != -1)
			{
				m_mcu_result1 = m_memcard[arg1 & 3]->read_sec(space, 1) |
					(m_memcard[arg1 & 3]->read_sec(space, 2) << 8) |
					(m_memcard[arg1 & 3]->read_sec(space, 3) << 16);
			}
			else
				status = 0xf4;
			m_mcu_result0 = cmd;
			break;
		case 0xc5: // write security mem
			if (m_memcard[arg1 & 3]->present() != -1)
				m_memcard[arg1 & 3]->write_sec(space, arg2 & 3, arg3);
			else
				status = 0xf4;
			m_mcu_result0 = cmd;
			break;
		case 0xc6: // presumable write protection mem
			if (m_memcard[arg1 & 3]->present() != -1)
				m_memcard[arg1 & 3]->write_prot(space, arg2 & 3, arg3);
			else
				status = 0xf4;
			m_mcu_result0 = cmd;
			break;
		case 0xc7: // read protection mem
			if (m_memcard[arg1 & 3]->present() != -1)
			{
				m_mcu_result1 = m_memcard[arg1 & 3]->read_prot(space, 0) |
					(m_memcard[arg1 & 3]->read_prot(space, 1) << 8) |
					(m_memcard[arg1 & 3]->read_prot(space, 2) << 16) |
					(m_memcard[arg1 & 3]->read_prot(space, 3) << 24);
			}
			else
				status = 0xf4;
			m_mcu_result0 = cmd;
			break;
		case 0xc8: // write data mem
			if (m_memcard[arg1 & 3]->present() != -1)
				m_memcard[arg1 & 3]->write(space, arg2, arg3);
			else
				status = 0xf4;
			m_mcu_result0 = cmd;
			break;
		case 0xc9: // card authentication
			if (m_memcard[arg1 & 3]->present() != -1)
				m_memcard[arg1 & 3]->auth(arg2, arg3, m_mcu_regs[1] & 0xff);
			else
				status = 0xf4;
			m_mcu_result0 = cmd;
			break;
		default:
			logerror("MCU unknown command %08x %08x\n", m_mcu_regs[0], m_mcu_regs[1]);
			status = 0xf4; // error
			break;
		}
		m_mcu_regs[3] = (m_mcu_regs[3] & 0xff00ffff) | (status << 16);
		m_mcu_timer->adjust(attotime::from_msec(delay));
	}
	else // next step
	{
		if (m_mcu_last_cmd)
		{
			m_mcu_regs[3] = (m_mcu_regs[3] & 0xff00ffff) | 0x00F20000;  // set "command done and return data" status
			m_mcu_timer->adjust(attotime::from_usec(100));
			m_mcu_last_cmd = 0;
		}
	}
}

READ32_MEMBER(pgm2_state::mcu_r)
{
	return m_mcu_regs[(offset >> 15) & 7];
}

WRITE32_MEMBER(pgm2_state::mcu_w)
{
	int reg = (offset >> 15) & 7;
	COMBINE_DATA(&m_mcu_regs[reg]);

	if (reg == 2 && m_mcu_regs[2]) // irq to mcu
		mcu_command(space, true);
	if (reg == 5 && m_mcu_regs[5]) // ack to mcu (written at the end of irq handler routine)
	{
		mcu_command(space, false);
		m_arm_aic->set_irq(3, CLEAR_LINE);
	}
}

WRITE16_MEMBER(pgm2_state::vbl_ack_w)
{
	m_arm_aic->set_irq(12, CLEAR_LINE);
}

WRITE16_MEMBER(pgm2_state::unk30120014_w)
{
	if (offset == 0)
	{
		// 0/1 toggles (maybe sprite dma triggers?)
	}
	else
	{
		// interesting data
		//printf("unk30120014_w %d %04x\n", offset, data);
	}
}

/*
 KOV3 ROM board uses special module intead of program ROM, tiny PCB with IC stamped "HW1" (might be FPGA, CPLD or ASIC) and BGA Flash ROM stamped "IG-L".
 This module uses few pins for serial comms (wired to IGS036 GPIO), it can not be dumped as regular ROM until special unlock procedure (return weird data pattern while locked).

 In case of KOV3 unlock sequence is:
  1) send via serial 0x0d and 64bit xor_value, result must be A3A3A3A36D6D6D6D
  2) send via serial 0x25 and 64bit xor_value, result is 64bit key^xor_value
  3) read first 10h bytes from ROM area (at this point ROM area read as scrambled or random data)
  4) write "key" to ROM area, using 2x 16bit writes, offsets and data is bitfields of 64bit key:
      u32 key0, key1;
      u16 *rom = (u16*)0x10000000;
      rom[((key0 & 0x3f) << 16) | (key1 >> 16)] = key1 & 0xffff;
      rom[key0 >> 22] = (key0 >> 6) & 0xffff;
     it is possible, 22bit address xor value derived from 1st write offset.
     meaning of other 10bit offset and 2x data words is not clear - each of them can be either "key bits" or "magic word" expected by security device.
  5) write static sequence of 4x words to ROM area, which switch module to special mode - next 4x reads will return checksum^key parts instead of rom data.
  6) read checksum from ROM area 10000002-10000009
  7) read first 10h bytes from ROM area and check they are not same as was at step 3)
  8) perform whole ROM summing, result must match key^checksum read at step 6)

 It is not clear if/how real address/data xor values derived from written "key",
 or security chip just waiting to be be written magic value at specific address in ROM area, and if this happen enable descrambling using hardcoded values.
 */

READ_LINE_MEMBER(pgm2_state::module_data_r)
{
	return module_out_latch ? ASSERT_LINE : CLEAR_LINE;
}
WRITE_LINE_MEMBER(pgm2_state::module_data_w)
{
	module_in_latch = (state == ASSERT_LINE) ? 1 : 0;
}
WRITE_LINE_MEMBER(pgm2_state::module_clk_w)
{
	if (module_prev_state != state && state == CLEAR_LINE)
	{
		if (module_clk_cnt < 80)
		{
			int offs = module_clk_cnt / 8;
			int bit = (module_clk_cnt & 7) ^ 7;
			module_rcv_buf[offs] &= ~(1 << bit);
			module_rcv_buf[offs] |= module_in_latch << bit;

			++module_clk_cnt;
			if (module_clk_cnt >= 80)
			{
				switch (module_rcv_buf[0])
				{
				case 0x0d: // init or status check
					module_send_buf[0] = module_send_buf[1] = module_send_buf[2] = module_send_buf[3] = 0xa3;
					module_send_buf[4] = module_send_buf[5] = module_send_buf[6] = module_send_buf[7] = 0x6d;
					break;
				case 0x25: // get key
					for (int i = 0; i < 8; i++)
						module_send_buf[i] = module_key->key[i ^ 3] ^ module_rcv_buf[i + 1];
					break;
				default:
					logerror("unknown FPGA command %02X!\n", module_rcv_buf[0]);
					break;
				}

				module_send_buf[8] = 0;
				for (int i = 0; i < 8; i++) // sum reply bytes
					module_send_buf[8] += module_send_buf[i];
			}
		}
		else
		{
			int offs = (module_clk_cnt - 80) / 8;
			int bit = (module_clk_cnt & 7) ^ 7;
			module_out_latch = (module_send_buf[offs] >> bit) & 1;
			++module_clk_cnt;
			if (module_clk_cnt >= 152)
				module_clk_cnt = 0;
		}
	}
	module_prev_state = state;
}

READ16_MEMBER(pgm2_state::module_rom_r)
{
	if (module_sum_read && offset > 0 && offset < 5) // checksum read mode
	{
		if (offset == 4)
			module_sum_read = false;
		uint32_t offs = ((offset - 1) * 2) ^ 2;
		return (module_key->sum[offs] ^ module_key->key[offs]) | ((module_key->sum[offs + 1] ^ module_key->key[offs + 1]) << 8);
	}

	return ((uint16_t *)memregion("user1")->base())[offset];
}

WRITE16_MEMBER(pgm2_state::module_rom_w)
{
	//printf("module write %04X at %08X\n", data, offset);
	uint32_t dec_val = ((module_key->key[0] | (module_key->key[1] << 8) | (module_key->key[2] << 16)) >> 6) & 0xffff;
	if (data == dec_val)
	{
		// might be wrong and normal data access enabled only after whole sequence complete
		decrypt_kov3_module(module_key->addr_xor, module_key->data_xor);
	}
	else
		switch (data)
		{
			// following might be wrong, and trigger is address or both
		case 0x00c2: // checksum read mode enable, step 1 and 4
			module_sum_read = true;
			if (offset != 0xe5a7 && offset != 0xa521)
				popmessage("module write %04X at %08X\n", data, offset);
			break;
		case 0x0084: // checksum read mode enable, step 2 and 3
			if (offset != 0x5e7a && offset != 0x5a12)
				popmessage("module write %04X at %08X\n", data, offset);
			break;
		default:
			logerror("module write %04X at %08X\n", data, offset);
			break;
		}
}

// very primitive Atmel ARM PIO simulation, should be improved and devicified
WRITE32_MEMBER(pgm2_state::pio_sodr_w)
{
	pio_out_data |= data & mem_mask;
	module_data_w((pio_out_data & 0x100) ? ASSERT_LINE : CLEAR_LINE);
	module_clk_w((pio_out_data & 0x200) ? ASSERT_LINE : CLEAR_LINE);
}
WRITE32_MEMBER(pgm2_state::pio_codr_w)
{
	pio_out_data &= ~(data & mem_mask);
	module_data_w((pio_out_data & 0x100) ? ASSERT_LINE : CLEAR_LINE);
	module_clk_w((pio_out_data & 0x200) ? ASSERT_LINE : CLEAR_LINE);
}
READ32_MEMBER(pgm2_state::pio_pdsr_r)
{
	return (module_data_r() == ASSERT_LINE ? 1 : 0) << 8; // fpga data read and status (bit 7, must be 0)
}

static ADDRESS_MAP_START( pgm2_map, AS_PROGRAM, 32, pgm2_state )
	AM_RANGE(0x00000000, 0x00003fff) AM_ROM //AM_REGION("user1", 0x00000) // internal ROM

	AM_RANGE(0x02000000, 0x0200ffff) AM_RAM AM_SHARE("sram") // 'battery ram' (in CPU?)

	AM_RANGE(0x03600000, 0x036bffff) AM_READWRITE(mcu_r, mcu_w)

	AM_RANGE(0x03900000, 0x03900003) AM_READ_PORT("INPUTS0")
	AM_RANGE(0x03a00000, 0x03a00003) AM_READ_PORT("INPUTS1")

	AM_RANGE(0x20000000, 0x2007ffff) AM_RAM AM_SHARE("mainram")

	AM_RANGE(0x30000000, 0x30001fff) AM_RAM AM_SHARE("sp_videoram") // spriteram ('move' ram in test mode)

	AM_RANGE(0x30020000, 0x30021fff) AM_RAM_WRITE(bg_videoram_w) AM_SHARE("bg_videoram")
	AM_RANGE(0x30040000, 0x30045fff) AM_RAM_WRITE(fg_videoram_w) AM_SHARE("fg_videoram")

	AM_RANGE(0x30060000, 0x30063fff) AM_RAM_DEVWRITE("sp_palette", palette_device, write32) AM_SHARE("sp_palette")

	AM_RANGE(0x30080000, 0x30081fff) AM_RAM_DEVWRITE("bg_palette", palette_device, write32) AM_SHARE("bg_palette")

	AM_RANGE(0x300a0000, 0x300a07ff) AM_RAM_DEVWRITE("tx_palette", palette_device, write32) AM_SHARE("tx_palette")

	AM_RANGE(0x300c0000, 0x300c01ff) AM_RAM AM_SHARE("sp_zoom") // sprite zoom table - it uploads the same data 4x, maybe xshrink,xgrow,yshrink,ygrow or just redundant mirrors

	/* linescroll ram - it clears to 0x3bf on startup which is enough bytes for 240 lines if each rowscroll value was 8 bytes, but each row is 4,
	so only half of this is used? or tx can do it too (unlikely, as orl2 writes 256 lines of data) maybe just bad mem check bounds on orleg2.
	It reports pass even if it fails the first byte but if the first byte passes it attempts to test 0x10000 bytes, which is far too big so
	what is the real size? */
	AM_RANGE(0x300e0000, 0x300e03ff) AM_RAM AM_SHARE("lineram") AM_MIRROR(0x000fc00)

	AM_RANGE(0x30100000, 0x301000ff) AM_READWRITE8(shareram_r, shareram_w, 0x00ff00ff)

	AM_RANGE(0x30120000, 0x30120003) AM_RAM AM_SHARE("bgscroll") // scroll
	AM_RANGE(0x30120008, 0x3012000b) AM_RAM AM_SHARE("fgscroll")
	AM_RANGE(0x3012000c, 0x3012000f) AM_RAM AM_SHARE("vidmode")
	AM_RANGE(0x30120014, 0x30120017) AM_WRITE16(unk30120014_w, 0xffffffff)
	AM_RANGE(0x30120018, 0x3012001b) AM_WRITE16(vbl_ack_w, 0x0000ffff)
	AM_RANGE(0x30120030, 0x30120033) AM_WRITE16(share_bank_w, 0xffff0000)
	AM_RANGE(0x30120038, 0x3012003b) AM_WRITE(sprite_encryption_w)
	// there are other 0x301200xx regs

	AM_RANGE(0x40000000, 0x40000003) AM_DEVREAD8("ymz774", ymz774_device, read, 0xffffffff)  AM_DEVWRITE8("ymz774", ymz774_device, write, 0xffffffff)

	// internal IGS036 - most of them is standard ATMEL peripherals followed by custom bits
	// AM_RANGE(0xfffa0000, 0xfffa00ff) TC (Timer Counter) not used, mentioned in disabled / unused code
	// AM_RANGE(0xffffec00, 0xffffec7f) SMC (Static Memory Controller)
	// AM_RANGE(0xffffee00, 0xffffee57) MATRIX (Bus Matrix)
	AM_RANGE(0xfffff000, 0xfffff14b) AM_DEVICE("arm_aic", arm_aic_device, regs_map)
	// AM_RANGE(0xfffff200, 0xfffff247) DBGU (Debug Unit)
	// AM_RANGE(0xfffff400, 0xfffff4af) PIO (Parallel Input Output Controller)
	AM_RANGE(0xfffff430, 0xfffff437) AM_WRITENOP // often
	// AM_RANGE(0xfffffd00, 0xfffffd0b) RSTC (Reset Controller)
	// AM_RANGE(0xfffffd20, 0xfffffd2f) RTTC (Real Time Timer)
	AM_RANGE(0xfffffd28, 0xfffffd2b) AM_READ(rtc_r)
	// AM_RANGE(0xfffffd40, 0xfffffd4b) WDTC (Watch Dog Timer)
	// custom IGS036 stuff starts here
	AM_RANGE(0xfffffa08, 0xfffffa0b) AM_WRITE(encryption_do_w) // after uploading encryption? table might actually send it or enable external ROM? when read bits0-1 called FUSE 0 and 1, must be 0
	AM_RANGE(0xfffffa0c, 0xfffffa0f) AM_READ(unk_startup_r) // written 0, then 0x1c, then expected to return (result&0x180)==0x180, then written 0x7c
	AM_RANGE(0xfffffc00, 0xfffffcff) AM_READWRITE8(encryption_r, encryption_w, 0xffffffff)
ADDRESS_MAP_END


static ADDRESS_MAP_START( pgm2_rom_map, AS_PROGRAM, 32, pgm2_state )
	AM_RANGE(0x10000000, 0x10ffffff) AM_ROM AM_REGION("user1", 0) // external ROM
	AM_IMPORT_FROM(pgm2_map)
ADDRESS_MAP_END

static ADDRESS_MAP_START( pgm2_ram_rom_map, AS_PROGRAM, 32, pgm2_state )
	AM_RANGE(0x10000000, 0x101fffff) AM_RAM AM_SHARE("romboard_ram") // we should also probably decrypt writes once the encryption is enabled, but the game never writes with it turned on anyway
	AM_RANGE(0x10200000, 0x103fffff) AM_ROM AM_REGION("user1", 0) // external ROM
	AM_IMPORT_FROM(pgm2_map)
ADDRESS_MAP_END

static ADDRESS_MAP_START( pgm2_module_rom_map, AS_PROGRAM, 32, pgm2_state )
	AM_RANGE(0x10000000, 0x107fffff) AM_WRITE16(module_rom_w, 0xffffffff)
	AM_RANGE(0x10000000, 0x1000000f) AM_READ16(module_rom_r, 0xffffffff)
	AM_RANGE(0xfffff430, 0xfffff433) AM_WRITE(pio_sodr_w)
	AM_RANGE(0xfffff434, 0xfffff437) AM_WRITE(pio_codr_w)
	AM_RANGE(0xfffff43c, 0xfffff43f) AM_READ(pio_pdsr_r)
	AM_IMPORT_FROM(pgm2_rom_map)
ADDRESS_MAP_END

static INPUT_PORTS_START( pgm2 )
	PORT_START("INPUTS0")
	PORT_BIT( 0x00000001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(1)
	PORT_BIT( 0x00000002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(1)
	PORT_BIT( 0x00000004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(1)
	PORT_BIT( 0x00000008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(1)
	PORT_BIT( 0x00000010, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x00000020, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x00000040, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT( 0x00000080, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(1)
	PORT_BIT( 0x00000100, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x00000200, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x00000400, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x00000800, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT( 0x00001000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x00002000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x00004000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x00008000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x00010000, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x00020000, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(2)
	PORT_BIT( 0x00040000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x00080000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x00100000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(3)
	PORT_BIT( 0x00200000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(3)
	PORT_BIT( 0x00400000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(3)
	PORT_BIT( 0x00800000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(3)
	PORT_BIT( 0x01000000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(3)
	PORT_BIT( 0x02000000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(3)
	PORT_BIT( 0x04000000, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(3)
	PORT_BIT( 0x08000000, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(3)
	PORT_BIT( 0x10000000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x20000000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x40000000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80000000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START("INPUTS1")
	PORT_BIT( 0x00000001, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(4)
	PORT_BIT( 0x00000002, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(4)
	PORT_BIT( 0x00000004, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(4)
	PORT_BIT( 0x00000008, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(4)
	PORT_BIT( 0x00000010, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(4)
	PORT_BIT( 0x00000020, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(4)
	PORT_BIT( 0x00000040, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(4)
	PORT_BIT( 0x00000080, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(4)
	PORT_BIT( 0x00000100, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x00000200, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x00000400, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x00000800, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x00001000, IP_ACTIVE_LOW, IPT_START3 )
	PORT_BIT( 0x00002000, IP_ACTIVE_LOW, IPT_START4 )
	PORT_BIT( 0x00004000, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x00008000, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x00010000, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x00020000, IP_ACTIVE_LOW, IPT_COIN4 )
	PORT_BIT( 0x00040000, IP_ACTIVE_LOW, IPT_SERVICE1 ) // test key p1+p2
	PORT_BIT( 0x00080000, IP_ACTIVE_LOW, IPT_SERVICE2 ) // test key p3+p4
	PORT_BIT( 0x00100000, IP_ACTIVE_LOW, IPT_SERVICE3 ) // service key p1+p2
	PORT_BIT( 0x00200000, IP_ACTIVE_LOW, IPT_SERVICE4 ) // service key p3+p4
	PORT_BIT( 0x00400000, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x00800000, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_SERVICE( 0x01000000, IP_ACTIVE_LOW ) PORT_DIPLOCATION("SW1:1")
	PORT_DIPNAME( 0x02000000, 0x02000000, "Music" )  PORT_DIPLOCATION("SW1:2")
	PORT_DIPSETTING(          0x00000000, DEF_STR( Off ) )
	PORT_DIPSETTING(          0x02000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x04000000, 0x04000000, "Voice" )  PORT_DIPLOCATION("SW1:3")
	PORT_DIPSETTING(          0x00000000, DEF_STR( Off ) )
	PORT_DIPSETTING(          0x04000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x08000000, 0x08000000, "Free" )  PORT_DIPLOCATION("SW1:4")
	PORT_DIPSETTING(          0x08000000, DEF_STR( Off ) )
	PORT_DIPSETTING(          0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x10000000, 0x10000000, "Stop" )  PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(          0x10000000, DEF_STR( Off ) )
	PORT_DIPSETTING(          0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x20000000, 0x20000000, DEF_STR( Unused ) )  PORT_DIPLOCATION("SW1:6")
	PORT_DIPSETTING(          0x20000000, DEF_STR( Off ) )
	PORT_DIPSETTING(          0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x40000000, 0x40000000, DEF_STR( Unused ) )  PORT_DIPLOCATION("SW1:7")
	PORT_DIPSETTING(          0x40000000, DEF_STR( Off ) )
	PORT_DIPSETTING(          0x00000000, DEF_STR( On ) )
	PORT_DIPNAME( 0x80000000, 0x80000000, "Debug" )  PORT_DIPLOCATION("SW1:8")
	PORT_DIPSETTING(          0x80000000, DEF_STR( Off ) )
	PORT_DIPSETTING(          0x00000000, DEF_STR( On ) )
INPUT_PORTS_END


WRITE_LINE_MEMBER(pgm2_state::irq)
{
//  printf("irq\n");
	if (state == ASSERT_LINE) m_maincpu->set_input_line(ARM7_IRQ_LINE, ASSERT_LINE);
	else m_maincpu->set_input_line(ARM7_IRQ_LINE, CLEAR_LINE);
}

void pgm2_state::machine_start()
{
	save_item(NAME(m_encryption_table));
	save_item(NAME(m_has_decrypted));
	save_item(NAME(m_has_decrypted_kov3_module));
	save_item(NAME(m_spritekey));
	save_item(NAME(m_realspritekey));
	save_item(NAME(m_mcu_regs));
	save_item(NAME(m_mcu_result0));
	save_item(NAME(m_mcu_result1));
	save_item(NAME(m_mcu_last_cmd));
	save_item(NAME(m_shareram));
	save_item(NAME(m_share_bank));
	save_item(NAME(pio_out_data));
	save_item(NAME(module_in_latch));
	save_item(NAME(module_sum_read));
	save_item(NAME(module_out_latch));
	save_item(NAME(module_prev_state));
	save_item(NAME(module_clk_cnt));
	save_item(NAME(module_rcv_buf));
	save_item(NAME(module_send_buf));

	machine().save().register_postload(save_prepost_delegate(FUNC(pgm2_state::postload), this));
}

void pgm2_state::machine_reset()
{
	m_spritekey = 0;
	m_realspritekey = 0;
	m_mcu_last_cmd = 0;
	m_share_bank = 0;

	// as the decryption is dynamic controlled by the program, restore the encrypted copy
	memcpy(memregion("user1")->base(), &m_encrypted_copy[0], memregion("user1")->bytes());

	m_has_decrypted = 0;
	m_has_decrypted_kov3_module = 0;

	pio_out_data = 0;
	module_prev_state = 0;
	module_sum_read = false;
	module_clk_cnt = 151; // this needed because of "false" clock pulse happen during gpio init
}

static const gfx_layout tiles8x8_layout =
{
	8,8,
	RGN_FRAC(1,1),
	4,
	{ 0, 1, 2, 3 },
	{ 4, 0, 12, 8, 20, 16, 28, 24 },
	{ 0*32, 1*32, 2*32, 3*32, 4*32, 5*32, 6*32, 7*32 },
	32*8
};

static const gfx_layout tiles32x32x8_layout =
{
	32,32,
	RGN_FRAC(1,1),
	7,
	{ 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8,
		16*8, 17*8, 18*8, 19*8, 20*8, 21*8, 22*8, 23*8, 24*8, 25*8, 26*8, 27*8, 28*8, 29*8, 30*8, 31*8 },
	{ 0*256, 1*256, 2*256, 3*256, 4*256, 5*256, 6*256, 7*256, 8*256, 9*256, 10*256, 11*256, 12*256, 13*256, 14*256, 15*256,
		16*256, 17*256, 18*256, 19*256, 20*256, 21*256, 22*256, 23*256, 24*256, 25*256, 26*256, 27*256, 28*256, 29*256, 30*256, 31*256
	},
	256*32
};

static GFXDECODE_START( pgm2_tx )
	GFXDECODE_ENTRY( "tiles", 0, tiles8x8_layout, 0, 0x800/4/0x10 )
GFXDECODE_END

static GFXDECODE_START( pgm2_bg )
	GFXDECODE_ENTRY( "bgtile", 0, tiles32x32x8_layout, 0, 0x2000/4/0x80 )
GFXDECODE_END

MACHINE_CONFIG_START(pgm2_state::pgm2)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", IGS036, 100000000) // ?? ARM based CPU, has internal ROM.
	MCFG_CPU_PROGRAM_MAP(pgm2_rom_map)

	MCFG_CPU_VBLANK_INT_DRIVER("screen", pgm2_state,  igs_interrupt)
	MCFG_TIMER_DRIVER_ADD("mcu_timer", pgm2_state, igs_interrupt2)

	MCFG_ARM_AIC_ADD("arm_aic")
	MCFG_IRQ_LINE_CB(WRITELINE(pgm2_state, irq))

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(64*8, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(0, 448-1, 0, 224-1)
	MCFG_SCREEN_UPDATE_DRIVER(pgm2_state, screen_update_pgm2)
	MCFG_SCREEN_VBLANK_CALLBACK(WRITELINE(pgm2_state, screen_vblank_pgm2))

	MCFG_GFXDECODE_ADD("gfxdecode2", "tx_palette", pgm2_tx)

	MCFG_GFXDECODE_ADD("gfxdecode3", "bg_palette", pgm2_bg)

	MCFG_PALETTE_ADD("sp_palette", 0x4000/4) // sprites
	MCFG_PALETTE_FORMAT(XRGB)

	MCFG_PALETTE_ADD("tx_palette", 0x800/4) // text
	MCFG_PALETTE_FORMAT(XRGB)

	MCFG_PALETTE_ADD("bg_palette", 0x2000/4) // bg
	MCFG_PALETTE_FORMAT(XRGB)

	MCFG_NVRAM_ADD_0FILL("sram")

	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")
	MCFG_YMZ774_ADD("ymz774", 16384000) // is clock correct ?
	MCFG_SOUND_ROUTE(0, "lspeaker", 1.0)
	MCFG_SOUND_ROUTE(1, "rspeaker", 1.0)

	MCFG_PGM2_MEMCARD_ADD("memcard_p1")
	MCFG_PGM2_MEMCARD_ADD("memcard_p2")
	MCFG_PGM2_MEMCARD_ADD("memcard_p3")
	MCFG_PGM2_MEMCARD_ADD("memcard_p4")
MACHINE_CONFIG_END

// not strictly needed as the video code supports changing on the fly, but makes recording easier etc.
MACHINE_CONFIG_DERIVED(pgm2_state::pgm2_lores, pgm2)
	MCFG_SCREEN_MODIFY("screen")
	MCFG_SCREEN_VISIBLE_AREA(0, 320-1, 0, 240-1)
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(pgm2_state::pgm2_hires, pgm2)
	MCFG_CPU_MODIFY("maincpu")
	MCFG_CPU_PROGRAM_MAP(pgm2_module_rom_map)
	MCFG_SCREEN_MODIFY("screen")
	MCFG_SCREEN_VISIBLE_AREA(0, 512-1, 0, 240-1)
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(pgm2_state::pgm2_ramrom, pgm2)
	MCFG_CPU_MODIFY("maincpu")
	MCFG_CPU_PROGRAM_MAP(pgm2_ram_rom_map)
MACHINE_CONFIG_END

/* using macros for the video / sound roms because the locations never change between sets, and
   we're going to have a LOT of clones to cover all the internal rom regions and external rom revision
   combinations, so it keeps things readable */

// Oriental Legend 2

#define ORLEG2_VIDEO_SOUND_ROMS \
	ROM_REGION( 0x200000, "tiles", ROMREGION_ERASEFF ) \
	ROM_LOAD( "ig-a_text.u4",            0x00000000, 0x0200000, CRC(fa444c32) SHA1(31e5e3efa92d52bf9ab97a0ece51e3b77f52ce8a) ) \
	\
	ROM_REGION( 0x1000000, "bgtile", 0 ) \
	ROM_LOAD32_WORD( "ig-a_bgl.u35",     0x00000000, 0x0800000, CRC(083a8315) SHA1(0dba25e132fbb12faa59ced648c27b881dc73478) ) \
	ROM_LOAD32_WORD( "ig-a_bgh.u36",     0x00000002, 0x0800000, CRC(e197221d) SHA1(5574b1e3da4b202db725be906dd868edc2fd4634) ) \
	\
	ROM_REGION( 0x2000000, "sprites_mask", 0 ) /* 1bpp sprite mask data (packed) */ \
	ROM_LOAD32_WORD( "ig-a_bml.u12",     0x00000000, 0x1000000, CRC(113a331c) SHA1(ee6b31bb2b052cc8799573de0d2f0a83f0ab4f6a) ) \
	ROM_LOAD32_WORD( "ig-a_bmh.u16",     0x00000002, 0x1000000, CRC(fbf411c8) SHA1(5089b5cc9bbf6496ef1367c6255e63e9ab895117) ) \
	\
	ROM_REGION( 0x4000000, "sprites_colour", 0 ) /* sprite colour data (6bpp data, 2 bits unused except for 4 bytes that are randomly 0xff - check dump?) */ \
	ROM_LOAD32_WORD( "ig-a_cgl.u18",     0x00000000, 0x2000000, CRC(43501fa6) SHA1(58ccce6d393964b771fec3f5c583e3ede57482a3) ) \
	ROM_LOAD32_WORD( "ig-a_cgh.u26",     0x00000002, 0x2000000, CRC(7051d020) SHA1(3d9b24c6fda4c9699bb9f00742e0888059b623e1) ) \
	\
	ROM_REGION( 0x1000000, "ymz774", ROMREGION_ERASEFF ) /* ymz774 */ \
	ROM_LOAD16_WORD_SWAP( "ig-a_sp.u2",  0x00000000, 0x1000000, CRC(8250688c) SHA1(d2488477afc528aeee96826065deba2bce4f0a7d) ) \
	\
	ROM_REGION( 0x10000, "sram", 0 ) \
	ROM_LOAD( "xyj2_nvram",            0x00000000, 0x10000, CRC(ccccc71c) SHA1(585b5ccbf89dd28d8532da785d7c8af12f31c6d6) )

/*
   External program revisions are CONFIRMED to be the same between regions, even if the label changes (localized game title + country specific extension code)

   Confirmed country codes used on labels
   FA = Oversea
   CN = China
   JP = Japan
   TW = Taiwan

*/

#define ORLEG2_PROGRAM_104(prefix, extension) \
	ROM_REGION( 0x1000000, "user1", 0 ) \
	ROM_LOAD( #prefix "_v104" #extension ".u7",  0x000000, 0x800000, CRC(7c24a4f5) SHA1(3cd9f9264ef2aad0869afdf096e88eb8d74b2570) )

#define ORLEG2_PROGRAM_103(prefix, extension) \
	ROM_REGION( 0x1000000, "user1", 0 ) \
	ROM_LOAD( #prefix "_v103" #extension ".u7",  0x000000, 0x800000, CRC(21c1fae8) SHA1(36eeb7a5e8dc8ee7c834f3ff1173c28cf6c2f1a3) )

#define ORLEG2_PROGRAM_101(prefix, extension) \
	ROM_REGION( 0x1000000, "user1", 0 ) \
	ROM_LOAD( #prefix "_v101" #extension ".u7",  0x000000, 0x800000, CRC(45805b53) SHA1(f2a8399c821b75fadc53e914f6f318707e70787c) )

/*
   Internal ROMs for CHINA, JAPAN and OVERSEA are confirmed to differ by just the region byte, other regions not yet verified.
   label is a localized version of the game title and the country code (see above)
   For OVERSEA this is "O/L2", but we omit the / due to naming rules
   For the CHINA version this uses the Chinese characters
*/

#define ORLEG2_INTERNAL_CHINA \
	ROM_REGION( 0x04000, "maincpu", 0 ) \
	ROM_LOAD( "xyj2_cn.igs036", 0x00000000, 0x0004000, CRC(bcce7641) SHA1(c3b5cf6e9f6eae09b6785314777a52b34c3c7657) ) \
	ROM_REGION( 0x108, "default_card", 0 ) \
	ROM_LOAD( "blank_orleg2_china_card.pg2", 0x000, 0x108, CRC(dc29556f) SHA1(2335cc7af25d4dd9763c6944d3f0eb50276de80a) )

#define ORLEG2_INTERNAL_OVERSEAS \
	ROM_REGION( 0x04000, "maincpu", 0 ) \
	ROM_LOAD( "ol2_fa.igs036", 0x00000000, 0x0004000, CRC(cc4d398a) SHA1(c50bcc81f02cd5aa8ad157d73209dc53bdedc023) )

#define ORLEG2_INTERNAL_JAPAN \
	ROM_REGION( 0x04000, "maincpu", 0 ) \
	ROM_LOAD( "ol2_a10.igs036", 0x00000000, 0x0004000, CRC(69375284) SHA1(a120c6a3d8d7898cc3ca508abea78e5e54090c66) )

ROM_START( orleg2 )
	ORLEG2_INTERNAL_OVERSEAS
	ORLEG2_PROGRAM_104(ol2,fa)
	ORLEG2_VIDEO_SOUND_ROMS
ROM_END

ROM_START( orleg2_103 )
	ORLEG2_INTERNAL_OVERSEAS
	ORLEG2_PROGRAM_103(ol2,fa)
	ORLEG2_VIDEO_SOUND_ROMS
ROM_END

ROM_START( orleg2_101 )
	ORLEG2_INTERNAL_OVERSEAS
	ORLEG2_PROGRAM_101(ol2,fa)
	ORLEG2_VIDEO_SOUND_ROMS
ROM_END

ROM_START( orleg2_104cn )
	ORLEG2_INTERNAL_CHINA
	ORLEG2_PROGRAM_104(xyj2,cn)
	ORLEG2_VIDEO_SOUND_ROMS
ROM_END

ROM_START( orleg2_103cn )
	ORLEG2_INTERNAL_CHINA
	ORLEG2_PROGRAM_103(xyj2,cn)
	ORLEG2_VIDEO_SOUND_ROMS
ROM_END

ROM_START( orleg2_101cn )
	ORLEG2_INTERNAL_CHINA
	ORLEG2_PROGRAM_101(xyj2,cn)
	ORLEG2_VIDEO_SOUND_ROMS
ROM_END

ROM_START( orleg2_104jp )
	ORLEG2_INTERNAL_JAPAN
	ORLEG2_PROGRAM_104(ol2,a10)
	ORLEG2_VIDEO_SOUND_ROMS
ROM_END

ROM_START( orleg2_103jp )
	ORLEG2_INTERNAL_JAPAN
	ORLEG2_PROGRAM_103(ol2,a10)
	ORLEG2_VIDEO_SOUND_ROMS
ROM_END

ROM_START( orleg2_101jp )
	ORLEG2_INTERNAL_JAPAN
	ORLEG2_PROGRAM_101(ol2,a10)
	ORLEG2_VIDEO_SOUND_ROMS
ROM_END

// Knights of Valour 2 New Legend

#define KOV2NL_VIDEO_SOUND_ROMS \
	ROM_REGION( 0x200000, "tiles", ROMREGION_ERASEFF ) \
	ROM_LOAD( "ig-a3_text.u4",           0x00000000, 0x0200000, CRC(214530ff) SHA1(4231a02054b0345392a077042b95779fd45d6c22) ) \
	\
	ROM_REGION( 0x1000000, "bgtile", 0 ) \
	ROM_LOAD32_WORD( "ig-a3_bgl.u35",    0x00000000, 0x0800000, CRC(2d46b1f6) SHA1(ea8c805eda6292e86a642e9633d8fee7054d10b1) ) \
	ROM_LOAD32_WORD( "ig-a3_bgh.u36",    0x00000002, 0x0800000, CRC(df710c36) SHA1(f826c3f496c4f17b46d18af1d8e02cac7b7027ac) ) \
	\
	ROM_REGION( 0x2000000, "sprites_mask", 0 ) /* 1bpp sprite mask data */ \
	ROM_LOAD32_WORD( "ig-a3_bml.u12",    0x00000000, 0x1000000, CRC(0bf63836) SHA1(b8e4f1951f8074b475b795bd7840c5a375b6f5ef) ) \
	ROM_LOAD32_WORD( "ig-a3_bmh.u16",    0x00000002, 0x1000000, CRC(4a378542) SHA1(5d06a8a8796285a786ebb690c34610f923ef5570) ) \
	\
	ROM_REGION( 0x4000000, "sprites_colour", 0 ) /* sprite colour data */ \
	ROM_LOAD32_WORD( "ig-a3_cgl.u18",    0x00000000, 0x2000000, CRC(8d923e1f) SHA1(14371cf385dd8857017d3111cd4710f4291b1ae2) ) \
	ROM_LOAD32_WORD( "ig-a3_cgh.u26",    0x00000002, 0x2000000, CRC(5b6fbf3f) SHA1(d1f52e230b91ee6cde939d7c2b74da7fd6527e73) ) \
	\
	ROM_REGION( 0x2000000, "ymz774", ROMREGION_ERASEFF ) /* ymz774 */ \
	ROM_LOAD16_WORD_SWAP( "ig-a3_sp.u37",            0x00000000, 0x2000000, CRC(45cdf422) SHA1(8005d284bcee73cff37a147fcd1c3e9f039a7203) ) \
	\
	ROM_REGION(0x10000, "sram", 0) \
	ROM_LOAD("gsyx_nvram", 0x00000000, 0x10000, CRC(22400c16) SHA1(f775a16299c30f2ce23d683161b910e06eff37c1) )

#define KOV2NL_PROGRAM_302(prefix, extension) \
	ROM_REGION( 0x1000000, "user1", 0 ) \
	ROM_LOAD( #prefix "_v302" #extension ".u7", 0x00000000, 0x0800000, CRC(b19cf540) SHA1(25da5804bbfd7ef2cdf5cc5aabaa803d18b98929) )

#define KOV2NL_PROGRAM_301(prefix, extension) \
	ROM_REGION( 0x1000000, "user1", 0 ) \
	ROM_LOAD( #prefix "_v301" #extension ".u7", 0x000000, 0x800000, CRC(c4595c2c) SHA1(09e379556ef76f81a63664f46d3f1415b315f384) )

#define KOV2NL_PROGRAM_300(prefix, extension) \
	ROM_REGION( 0x1000000, "user1", 0 ) \
	ROM_LOAD( #prefix "_v300" #extension ".u7", 0x000000, 0x800000, CRC(08da7552) SHA1(303b97d7694405474c8133a259303ccb49db48b1) )


// Region 0x00 - China
#define KOV2NL_INTERNAL_CHINA \
	ROM_REGION( 0x04000, "maincpu", 0 ) \
	ROM_LOAD( "gsyx_igs036_china.rom", 0x00000000, 0x0004000, CRC(e09fe4ce) SHA1(c0cac64ef8727cbe79d503ec4df66ddb6f2c925e) ) \
	ROM_REGION( 0x108, "default_card", 0 ) \
	ROM_LOAD( "blank_gsyx_china.pg2", 0x000, 0x108, CRC(02842ae8) SHA1(a6cda633b09a706039a79b73db2c258094826f85) )

// Region 0x01 - Taiwan  CRC(b3ca3124) SHA1(793d3bdc4bfccb892eb51c351c4ccd103ee9b7ce)
// uses cards with CRC(1155f01f) SHA1(60f7bed1461b362a3da687503cd72ed2d5e96f30) (same as Oversea, Korea)

// Region 0x02 - Japan CRC(46344f1a) SHA1(fbe846be4a39e8a4c41417858311faaaebf67cb9)
// uses cards with CRC(0d63cb64) SHA1(957cce2d47f3369bc4f98b1652ba8639c08fb9bd) (unique)

// Region 0x03 - Korea CRC(15619af0) SHA1(619e58e13c4d4351e8a4359a1df1eb9952326e84)
// uses cards with CRC(1155f01f) SHA1(60f7bed1461b362a3da687503cd72ed2d5e96f30) (same as Oversea, Taiwan)
// (incomplete / partial translation, shows Oversea disclaimer and corrupt text on some screens, so likely unreleased or needs newer mainprg)

// Region 0x04 - Hong Kong  CRC(76b9b527) SHA1(e77a7b59aca221b5d04dcd1ffc632114be7e5647)
// uses cards with CRC(02842ae8) SHA1(a6cda633b09a706039a79b73db2c258094826f85) (same as China)

// Region 0x05 - Overseas
#define KOV2NL_INTERNAL_OVERSEA \
	ROM_REGION( 0x04000, "maincpu", 0 ) \
	ROM_LOAD( "kov2nl_igs036_oversea.rom", 0x00000000, 0x0004000, CRC(25ec60cd) SHA1(7dd12d2bc642bfa79520676fe5de458ce7d08ef6) ) \
	ROM_REGION( 0x108, "default_card", 0 ) \
	ROM_LOAD( "blank_kov2nl_overseas_card.pg2", 0x000, 0x108, CRC(1155f01f) SHA1(60f7bed1461b362a3da687503cd72ed2d5e96f30) )


ROM_START( kov2nl )
	KOV2NL_INTERNAL_OVERSEA
	KOV2NL_PROGRAM_302(kov2nl, fa)
	KOV2NL_VIDEO_SOUND_ROMS
ROM_END

ROM_START( kov2nl_301 )
	KOV2NL_INTERNAL_OVERSEA
	KOV2NL_PROGRAM_301(kov2nl, fa)
	KOV2NL_VIDEO_SOUND_ROMS
ROM_END

ROM_START( kov2nl_300 )
	KOV2NL_INTERNAL_OVERSEA
	KOV2NL_PROGRAM_300(kov2nl, fa)
	KOV2NL_VIDEO_SOUND_ROMS
ROM_END


ROM_START( kov2nl_302cn )
	KOV2NL_INTERNAL_CHINA
	KOV2NL_PROGRAM_302(gsyx, cn)
	KOV2NL_VIDEO_SOUND_ROMS
ROM_END

ROM_START( kov2nl_301cn )
	KOV2NL_INTERNAL_CHINA
	KOV2NL_PROGRAM_301(gsyx, cn)
	KOV2NL_VIDEO_SOUND_ROMS
ROM_END

ROM_START( kov2nl_300cn )
	KOV2NL_INTERNAL_CHINA
	KOV2NL_PROGRAM_300(gsyx, cn)
	KOV2NL_VIDEO_SOUND_ROMS
ROM_END

// Dodonpachi Daioujou Tamashii

#define DDPDOJT_VIDEO_SOUND_ROMS \
	ROM_REGION( 0x200000, "tiles", ROMREGION_ERASEFF ) \
	ROM_LOAD( "ddpdoj_text.u1",          0x00000000, 0x0200000, CRC(f18141d1) SHA1(a16e0a76bc926a158bb92dfd35aca749c569ef50) ) \
	\
	ROM_REGION( 0x2000000, "bgtile", 0 ) \
	ROM_LOAD32_WORD( "ddpdoj_bgl.u23",   0x00000000, 0x1000000, CRC(ff65fdab) SHA1(abdd5ca43599a2daa722547a999119123dd9bb28) ) \
	ROM_LOAD32_WORD( "ddpdoj_bgh.u24",   0x00000002, 0x1000000, CRC(bb84d2a6) SHA1(a576a729831b5946287fa8f0d923016f43a9bedb) ) \
	\
	ROM_REGION( 0x1000000, "sprites_mask", 0 ) /* 1bpp sprite mask data */ \
	ROM_LOAD32_WORD( "ddpdoj_mapl0.u13", 0x00000000, 0x800000, CRC(bcfbb0fc) SHA1(9ec478eba9905913cf997bd9b46c70c1ad383630) ) \
	ROM_LOAD32_WORD( "ddpdoj_maph0.u15", 0x00000002, 0x800000, CRC(0cc75d4e) SHA1(6d1b5ef0fdebf1e84fa199b939ffa07b810b12c9) ) \
	\
	ROM_REGION( 0x2000000, "sprites_colour", 0 ) /* sprite colour data */ \
	ROM_LOAD32_WORD( "ddpdoj_spa0.u9",   0x00000000, 0x1000000, CRC(1232c1b4) SHA1(ecc1c549ae19d2f052a85fe4a993608aedf49a25) ) \
	ROM_LOAD32_WORD( "ddpdoj_spb0.u18",  0x00000002, 0x1000000, CRC(6a9e2cbf) SHA1(8e0a4ea90f5ef534820303d62f0873f8ac9f080e) ) \
	\
	ROM_REGION( 0x1000000, "ymz774", ROMREGION_ERASEFF ) /* ymz774 */ \
	ROM_LOAD16_WORD_SWAP( "ddpdoj_wave0.u12",        0x00000000, 0x1000000, CRC(2b71a324) SHA1(f69076cc561f40ca564d804bc7bd455066f8d77c) ) \
	\
	ROM_REGION( 0x10000, "sram", 0 ) \
	ROM_LOAD( "ddpdojt_sram",            0x00000000, 0x10000, CRC(af99e304) SHA1(e44fed22b902431298748eca84533f8685926afd) )

ROM_START( ddpdojt )
	ROM_REGION( 0x04000, "maincpu", 0 )
	ROM_LOAD( "ddpdoj_igs036_china.rom",       0x00000000, 0x0004000, CRC(5db91464) SHA1(723d8086285805bd815e62120dfa9a4269bcd932) )

	ROM_REGION( 0x0200000, "user1", 0 )
	ROM_LOAD( "ddpdoj_v201cn.u4",        0x00000000, 0x0200000, CRC(89e4b760) SHA1(9fad1309da31d12a413731b416a8bbfdb304ed9e) )

	DDPDOJT_VIDEO_SOUND_ROMS
ROM_END

// Knights of Valour 3

/*
   The Kov3 Program rom is a module consisting of a NOR flash and a FPGA, this provides an extra layer of encryption on top of the usual
   that is only unlocked when the correct sequence is recieved from the ARM MCU (IGS036)

   Newer gambling games use the same modules.
*/

#define KOV3_VIDEO_SOUND_ROMS \
	ROM_REGION( 0x200000, "tiles", ROMREGION_ERASEFF ) \
	ROM_LOAD( "kov3_text.u1",            0x00000000, 0x0200000, CRC(198b52d6) SHA1(e4502abe7ba01053d16c02114f0c88a3f52f6f40) ) \
	\
	ROM_REGION( 0x2000000, "bgtile", 0 ) \
	ROM_LOAD32_WORD( "kov3_bgl.u6",      0x00000000, 0x1000000, CRC(49a4c5bc) SHA1(26b7da91067bda196252520e9b4893361c2fc675) ) \
	ROM_LOAD32_WORD( "kov3_bgh.u7",      0x00000002, 0x1000000, CRC(adc1aff1) SHA1(b10490f0dbef9905cdb064168c529f0b5a2b28b8) ) \
	\
	ROM_REGION( 0x4000000, "sprites_mask", 0 ) /* 1bpp sprite mask data */ \
	ROM_LOAD32_WORD( "kov3_mapl0.u15",   0x00000000, 0x2000000, CRC(9e569bf7) SHA1(03d26e000e9d8e744546be9649628d2130f2ec4c) ) \
	ROM_LOAD32_WORD( "kov3_maph0.u16",   0x00000002, 0x2000000, CRC(6f200ad8) SHA1(cd12c136d4f5d424bd7daeeacd5c4127beb3d565) ) \
	\
	ROM_REGION( 0x8000000, "sprites_colour", 0 ) /* sprite colour data */ \
	ROM_LOAD32_WORD( "kov3_spa0.u17",    0x00000000, 0x4000000, CRC(3a1e58a9) SHA1(6ba251407c69ee62f7ea0baae91bc133acc70c6f) ) \
	ROM_LOAD32_WORD( "kov3_spb0.u10",    0x00000002, 0x4000000, CRC(90396065) SHA1(01bf9f69d77a792d5b39afbba70fbfa098e194f1) ) \
	\
	ROM_REGION( 0x4000000, "ymz774", ROMREGION_ERASEFF ) /* ymz774 */ \
	ROM_LOAD16_WORD_SWAP( "kov3_wave0.u13",              0x00000000, 0x4000000, CRC(aa639152) SHA1(2314c6bd05524525a31a2a4668a36a938b924ba4) ) \
	\
	ROM_REGION( 0x10000, "sram", 0 ) \
	ROM_LOAD( "kov3_sram",            0x00000000, 0x10000, CRC(d9608102) SHA1(dec5631642393f4ec76912c81fd60249bb45aa13) )

#define KOV3_INTERNAL_CHINA \
	ROM_REGION( 0x04000, "maincpu", 0 ) \
	ROM_LOAD( "kov3_igs036_china.rom", 0x00000000, 0x0004000, CRC(c7d33764) SHA1(5cd48f876e637d60391d39ac6e40bf243300cc75) ) \
	ROM_REGION( 0x108, "default_card", 0 ) \
	ROM_LOAD( "blank_kov3_china_card.pg2", 0x000, 0x108, CRC(bd5a968f) SHA1(b9045eb70e02afda7810431c592208053d863980) )


ROM_START( kov3 )
	KOV3_INTERNAL_CHINA

	ROM_REGION( 0x1000000, "user1", 0 )
	ROM_LOAD( "kov3_v104cn_raw.bin",         0x00000000, 0x0800000, CRC(1b5cbd24) SHA1(6471d4842a08f404420dea2bd1c8b88798c80fd5) )

	KOV3_VIDEO_SOUND_ROMS
ROM_END

ROM_START( kov3_102 )
	KOV3_INTERNAL_CHINA

	ROM_REGION( 0x1000000, "user1", 0 )
	ROM_LOAD( "kov3_v102cn_raw.bin",         0x00000000, 0x0800000, CRC(61d0dabd) SHA1(959b22ef4e342ca39c2386549ac7274f9d580ab8) )

	KOV3_VIDEO_SOUND_ROMS
ROM_END

ROM_START( kov3_101 )
	KOV3_INTERNAL_CHINA

	ROM_REGION( 0x1000000, "user1", 0 )
	ROM_LOAD( "kov3_v101.bin",         0x00000000, 0x0800000, BAD_DUMP CRC(d6664449) SHA1(64d912425f018c3531951019b33e909657724547) ) // dump was not raw, manually xored with fake value

	KOV3_VIDEO_SOUND_ROMS
ROM_END

ROM_START( kov3_100 )
	KOV3_INTERNAL_CHINA

	ROM_REGION( 0x1000000, "user1", 0 )
	ROM_LOAD( "kov3_v100cn_raw.bin",         0x00000000, 0x0800000, CRC(93bca924) SHA1(ecaf2c4676eb3d9f5e4fdbd9388be41e51afa0e4) )

	KOV3_VIDEO_SOUND_ROMS
ROM_END

/* King of Fighters '98: Ultimate Match HERO

device types were as follows

kof98umh_v100cn.u4  SAMSUNG K8Q2815UQB
ig-d3_text.u1       cFeon EN29LV160AB
all others:         SPANSION S99-50070

*/

#define KOF98UMH_VIDEO_SOUND_ROMS \
	ROM_REGION( 0x200000, "tiles", ROMREGION_ERASEFF ) \
	ROM_LOAD( "ig-d3_text.u1",          0x00000000, 0x0200000, CRC(9a0ea82e) SHA1(7844fd7e46c3fbb2164060f160da528254fd177e) ) \
	\
	ROM_REGION( 0x2000000, "bgtile", ROMREGION_ERASE00 ) \
	/* bgl/bgh unpopulated (no background tilemap) */ \
	\
	ROM_REGION( 0x08000000, "sprites_mask", 0 ) /* 1bpp sprite mask data */ \
	ROM_LOAD32_WORD( "ig-d3_mapl0.u13", 0x00000000, 0x4000000, CRC(5571d63e) SHA1(dad73797a35738013d82e3b8ca96fa001ec56f69) ) \
	ROM_LOAD32_WORD( "ig-d3_maph0.u15", 0x00000002, 0x4000000, CRC(0da7b1b8) SHA1(87741242bd827eca3788b490df6dcb65f7a89733) ) \
	\
	ROM_REGION( 0x20000000, "sprites_colour", 0 ) /* sprite colour data - some byte are 0x40 or even 0xff, but verified on 2 boards */ \
	ROM_LOAD32_WORD( "ig-d3_spa0.u9",   0x00000000, 0x4000000, CRC(cfef8f7d) SHA1(54f58d1b9eb7d2e4bbe13fbdfd98f5b14ce2086b) ) \
	ROM_LOAD32_WORD( "ig-d3_spb0.u18",  0x00000002, 0x4000000, CRC(f199d5c8) SHA1(91f5e8efd1f6a9e5aada51afdf5a8f52bac24185) ) \
	/* spa1/spb1 unpopulated */ \
	ROM_LOAD32_WORD( "ig-d3_spa2.u10",  0x10000000, 0x4000000, CRC(03bfd35c) SHA1(814998cd5ee01c9da775b73f7a0ba4216fe4970e) ) \
	ROM_LOAD32_WORD( "ig-d3_spb2.u20",  0x10000002, 0x4000000, CRC(9aaa840b) SHA1(3c6078d53bb5eca5c501540214287dd102102ea1) ) \
	/* spa3/spb3 unpopulated */ \
	\
	ROM_REGION( 0x08000000, "ymz774", ROMREGION_ERASEFF ) /* ymz774 */ \
	ROM_LOAD16_WORD_SWAP( "ig-d3_wave0.u12",        0x00000000, 0x4000000, CRC(edf2332d) SHA1(7e01c7e03e515814d7de117c265c3668d32842fa) ) \
	ROM_LOAD16_WORD_SWAP( "ig-d3_wave1.u11",        0x04000000, 0x4000000, CRC(62321b20) SHA1(a388c8a2489430fbe92fb26b3ef81c66ce97f318) ) \
	\
	ROM_REGION( 0x10000, "sram", 0 ) \
	ROM_LOAD( "kof98umh_sram",            0x00000000, 0x10000, CRC(60460ed9) SHA1(55cd8de37cee04ff7ad940fb52f8fb8db042c26e) )


ROM_START( kof98umh )
	ROM_REGION( 0x04000, "maincpu", 0 )
	ROM_LOAD( "kof98umh_internal_rom.bin",       0x00000000, 0x0004000, CRC(3ed2e50f) SHA1(35310045d375d9dda36c325e35257123a7b5b8c7) )

	ROM_REGION( 0x1000000, "user1", 0 )
	ROM_LOAD( "kof98umh_v100cn.u4",        0x00000000, 0x1000000, CRC(2ea91e3b) SHA1(5a586bb99cc4f1b02e0db462d5aff721512e0640) )

	KOF98UMH_VIDEO_SOUND_ROMS
ROM_END

static void iga_u16_decode(uint16_t *rom, int len, int ixor)
{
	int i;

	for (i = 1; i < len / 2; i+=2)
	{
		uint16_t x = ixor;

		if ( (i>>1) & 0x000001) x ^= 0x0010;
		if ( (i>>1) & 0x000002) x ^= 0x2004;
		if ( (i>>1) & 0x000004) x ^= 0x0801;
		if ( (i>>1) & 0x000008) x ^= 0x0300;
		if ( (i>>1) & 0x000010) x ^= 0x0080;
		if ( (i>>1) & 0x000020) x ^= 0x0020;
		if ( (i>>1) & 0x000040) x ^= 0x4008;
		if ( (i>>1) & 0x000080) x ^= 0x1002;
		if ( (i>>1) & 0x000100) x ^= 0x0400;
		if ( (i>>1) & 0x000200) x ^= 0x0040;
		if ( (i>>1) & 0x000400) x ^= 0x8000;

		rom[i] ^= x;
		rom[i] = bitswap<16>(rom[i], 8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7);
	}
}

static void iga_u12_decode(uint16_t* rom, int len, int ixor)
{
	int i;

	for (i = 0; i < len / 2; i+=2)
	{
		uint16_t x = ixor;

		if ( (i>>1) & 0x000001) x ^= 0x9004;
		if ( (i>>1) & 0x000002) x ^= 0x0028;
		if ( (i>>1) & 0x000004) x ^= 0x0182;
		if ( (i>>1) & 0x000008) x ^= 0x0010;
		if ( (i>>1) & 0x000010) x ^= 0x2040;
		if ( (i>>1) & 0x000020) x ^= 0x0801;
		if ( (i>>1) & 0x000040) x ^= 0x0000;
		if ( (i>>1) & 0x000080) x ^= 0x0000;
		if ( (i>>1) & 0x000100) x ^= 0x4000;
		if ( (i>>1) & 0x000200) x ^= 0x0600;
		if ( (i>>1) & 0x000400) x ^= 0x0000;

		rom[i] ^= x;
		rom[i] = bitswap<16>(rom[i], 8,9,10,11,12,13,14,15,0,1,2,3,4,5,6,7);
	}
}

static void sprite_colour_decode(uint16_t* rom, int len)
{
	int i;

	for (i = 0; i < len / 2; i++)
	{
		rom[i] = bitswap<16>(rom[i], 15, 14, /* unused - 6bpp */
								   13, 12, 11,
								   5, 4, 3,
								   7, 6, /* unused - 6bpp */
								   10, 9, 8,
								   2, 1, 0  );
	}
}

READ32_MEMBER(pgm2_state::orleg2_speedup_r)
{
	int pc = m_maincpu->pc();
	if ((pc == 0x1002faec) || (pc == 0x1002f9b8))
	{
		if ((m_mainram[0x20114 / 4] == 0x00) && (m_mainram[0x20118 / 4] == 0x00))
			m_maincpu->spin_until_interrupt();
	}
	/*else
	{
	    printf("pc is %08x\n", pc);
	}*/

	return m_mainram[0x20114 / 4];
}

READ32_MEMBER(pgm2_state::kov2nl_speedup_r)
{
	int pc = m_maincpu->pc();

	if ((pc == 0x10053a94) || (pc == 0x1005332c) || (pc == 0x1005327c))
	{
		if ((m_mainram[0x20470 / 4] == 0x00) && (m_mainram[0x20474 / 4] == 0x00))
			m_maincpu->spin_until_interrupt();
	}
	/*
	else
	{
	    printf("pc is %08x\n", pc);
	}
	*/

	return m_mainram[0x20470 / 4];
}

READ32_MEMBER(pgm2_state::kof98umh_speedup_r)
{
	int pc = m_maincpu->pc();

	if (pc == 0x100028f6)
	{
		if ((m_mainram[0x00060 / 4] == 0x00) && (m_mainram[0x00064 / 4] == 0x00))
			m_maincpu->spin_until_interrupt();
	}
	/*
	else
	{
	    printf("pc is %08x\n", pc);
	}
	*/

	return m_mainram[0x00060 / 4];
}

READ32_MEMBER(pgm2_state::kov3_speedup_r)
{
	int pc = m_maincpu->pc();

	if ((pc == 0x1000729a) || (pc == 0x1000729e))
	{
		if ((m_mainram[0x000b4 / 4] == 0x00) && (m_mainram[0x000b8 / 4] == 0x00))
			m_maincpu->spin_until_interrupt();
	}
	/*
	else
	{
	    printf("pc is %08x\n", pc);
	}
	*/

	return m_mainram[0x000b4 / 4];
}




READ32_MEMBER(pgm2_state::ddpdojt_speedup_r)
{
	int pc = m_maincpu->pc();

	if (pc == 0x10001a7e)
	{
		if ((m_mainram[0x00060 / 4] == 0x00) && (m_mainram[0x00064 / 4] == 0x00))
			m_maincpu->spin_until_interrupt();
	}
	/*
	else
	{
	printf("pc is %08x\n", pc);
	}
	*/

	return m_mainram[0x00060 / 4];
}

READ32_MEMBER(pgm2_state::ddpdojt_speedup2_r)
{
	int pc = m_maincpu->pc();

	if (pc == 0x1008fefe || pc == 0x1008fbe8)
	{
		if ((m_mainram[0x21e04 / 4] & 0x00ff0000) != 0) // not sure if this endian safe ?
			m_maincpu->spin_until_interrupt();
	}
	/*
	else
	{
	printf("pc is %08x\n", pc);
	}
	*/

	return m_mainram[0x21e04 / 4];
}


// for games with the internal ROMs fully dumped that provide the sprite key and program rom key at runtime
void pgm2_state::common_encryption_init()
{
	// store off a copy of the encrypted rom so we can restore it later when needed
	m_encrypted_copy.resize(memregion("user1")->bytes());
	memcpy(&m_encrypted_copy[0], memregion("user1")->base(), memregion("user1")->bytes());

	uint16_t *src = (uint16_t *)memregion("sprites_mask")->base();

	iga_u12_decode(src, memregion("sprites_mask")->bytes(), 0x0000);
	iga_u16_decode(src, memregion("sprites_mask")->bytes(), 0x0000);
	m_sprite_predecrypted = 0;

	src = (uint16_t *)memregion("sprites_colour")->base();
	sprite_colour_decode(src, memregion("sprites_colour")->bytes());

	m_has_decrypted = 0;
}

DRIVER_INIT_MEMBER(pgm2_state,orleg2)
{
	common_encryption_init();
	m_maincpu->space(AS_PROGRAM).install_read_handler(0x20020114, 0x20020117, read32_delegate(FUNC(pgm2_state::orleg2_speedup_r),this));
}

DRIVER_INIT_MEMBER(pgm2_state,kov2nl)
{
	common_encryption_init();
	m_maincpu->space(AS_PROGRAM).install_read_handler(0x20020470, 0x20020473, read32_delegate(FUNC(pgm2_state::kov2nl_speedup_r), this));
}

DRIVER_INIT_MEMBER(pgm2_state,ddpdojt)
{
	common_encryption_init();
	m_maincpu->space(AS_PROGRAM).install_read_handler(0x20000060, 0x20000063, read32_delegate(FUNC(pgm2_state::ddpdojt_speedup_r), this));
	m_maincpu->space(AS_PROGRAM).install_read_handler(0x20021e04, 0x20021e07, read32_delegate(FUNC(pgm2_state::ddpdojt_speedup2_r), this));
}

// currently we don't know how to derive address/data xor values from real keys, so we need both
static const kov3_module_key kov3_104_key = { { 0x40,0xac,0x30,0x00,0x47,0x49,0x00,0x00 } ,{ 0xeb,0x7d,0x8d,0x90,0x2c,0xf4,0x09,0x82 }, 0x18ec71, 0xb89d }; // fake zero-key
static const kov3_module_key kov3_102_key = { { 0x49,0xac,0xb0,0xec,0x47,0x49,0x95,0x38 } ,{ 0x09,0xbd,0xf1,0x31,0xe6,0xf0,0x65,0x2b }, 0x021d37, 0x81d0 };
static const kov3_module_key kov3_101_key = { { 0xc1,0x2c,0xc1,0xe5,0x3c,0xc1,0x59,0x9e } ,{ 0xf2,0xb2,0xf0,0x89,0x37,0xf2,0xc7,0x0b }, 0, 0xffff }; // real xor values is unknown
static const kov3_module_key kov3_100_key = { { 0x40,0xac,0x30,0x00,0x47,0x49,0x00,0x00 } ,{ 0x96,0xf0,0x91,0xe1,0xb3,0xf1,0xef,0x90 }, 0x3e8aa8, 0xc530 }; // fake zero-key

DRIVER_INIT_MEMBER(pgm2_state,kov3)
{
	common_encryption_init();
	m_maincpu->space(AS_PROGRAM).install_read_handler(0x200000b4, 0x200000b7, read32_delegate(FUNC(pgm2_state::kov3_speedup_r),this));
}

void pgm2_state::decrypt_kov3_module(uint32_t addrxor, uint16_t dataxor)
{
	uint16_t *src = (uint16_t *)memregion("user1")->base();
	uint32_t size = memregion("user1")->bytes();

	std::vector<uint16_t> buffer(size/2);

	for (int i = 0; i < size/2; i++)
		buffer[i] = src[i^addrxor]^dataxor;

	memcpy(src, &buffer[0], size);

	m_has_decrypted_kov3_module = 1;
}

DRIVER_INIT_MEMBER(pgm2_state, kov3_104)
{
	module_key = &kov3_104_key;
	DRIVER_INIT_CALL(kov3);
}

DRIVER_INIT_MEMBER(pgm2_state, kov3_102)
{
	module_key = &kov3_102_key;
	DRIVER_INIT_CALL(kov3);
}

DRIVER_INIT_MEMBER(pgm2_state, kov3_101)
{
	module_key = &kov3_101_key;
	DRIVER_INIT_CALL(kov3);
}

DRIVER_INIT_MEMBER(pgm2_state, kov3_100)
{
	module_key = &kov3_100_key;
	DRIVER_INIT_CALL(kov3);
}

DRIVER_INIT_MEMBER(pgm2_state,kof98umh)
{
	common_encryption_init();
	machine().device("maincpu")->memory().space(AS_PROGRAM).install_read_handler(0x20000060, 0x20000063, read32_delegate(FUNC(pgm2_state::kof98umh_speedup_r),this));
}





/* PGM2 */

// Oriental Legend 2 - should be a V102 and V100 too
GAME( 2007, orleg2,       0,         pgm2,    pgm2, pgm2_state,     orleg2,       ROT0, "IGS", "Oriental Legend 2 (V104, Oversea)", MACHINE_SUPPORTS_SAVE ) /* Overseas sets of OL2 do not use the card reader */
GAME( 2007, orleg2_103,   orleg2,    pgm2,    pgm2, pgm2_state,     orleg2,       ROT0, "IGS", "Oriental Legend 2 (V103, Oversea)", MACHINE_SUPPORTS_SAVE )
GAME( 2007, orleg2_101,   orleg2,    pgm2,    pgm2, pgm2_state,     orleg2,       ROT0, "IGS", "Oriental Legend 2 (V101, Oversea)", MACHINE_SUPPORTS_SAVE )

GAME( 2007, orleg2_104cn, orleg2,    pgm2,    pgm2, pgm2_state,     orleg2,       ROT0, "IGS", "Oriental Legend 2 (V104, China)", MACHINE_SUPPORTS_SAVE )
GAME( 2007, orleg2_103cn, orleg2,    pgm2,    pgm2, pgm2_state,     orleg2,       ROT0, "IGS", "Oriental Legend 2 (V103, China)", MACHINE_SUPPORTS_SAVE )
GAME( 2007, orleg2_101cn, orleg2,    pgm2,    pgm2, pgm2_state,     orleg2,       ROT0, "IGS", "Oriental Legend 2 (V101, China)", MACHINE_SUPPORTS_SAVE )

GAME( 2007, orleg2_104jp, orleg2,    pgm2,    pgm2, pgm2_state,     orleg2,       ROT0, "IGS", "Oriental Legend 2 (V104, Japan)", MACHINE_SUPPORTS_SAVE )
GAME( 2007, orleg2_103jp, orleg2,    pgm2,    pgm2, pgm2_state,     orleg2,       ROT0, "IGS", "Oriental Legend 2 (V103, Japan)", MACHINE_SUPPORTS_SAVE )
GAME( 2007, orleg2_101jp, orleg2,    pgm2,    pgm2, pgm2_state,     orleg2,       ROT0, "IGS", "Oriental Legend 2 (V101, Japan)", MACHINE_SUPPORTS_SAVE )

// Knights of Valour 2 New Legend
GAME( 2008, kov2nl,       0,         pgm2,    pgm2, pgm2_state,     kov2nl,       ROT0, "IGS", "Knights of Valour 2 New Legend (V302, Oversea)", MACHINE_SUPPORTS_SAVE )
GAME( 2008, kov2nl_301,   kov2nl,    pgm2,    pgm2, pgm2_state,     kov2nl,       ROT0, "IGS", "Knights of Valour 2 New Legend (V301, Oversea)", MACHINE_SUPPORTS_SAVE )
GAME( 2008, kov2nl_300,   kov2nl,    pgm2,    pgm2, pgm2_state,     kov2nl,       ROT0, "IGS", "Knights of Valour 2 New Legend (V300, Oversea)", MACHINE_SUPPORTS_SAVE )

GAME( 2008, kov2nl_302cn, kov2nl,    pgm2,    pgm2, pgm2_state,     kov2nl,       ROT0, "IGS", "Knights of Valour 2 New Legend (V302, China)", MACHINE_SUPPORTS_SAVE )
GAME( 2008, kov2nl_301cn, kov2nl,    pgm2,    pgm2, pgm2_state,     kov2nl,       ROT0, "IGS", "Knights of Valour 2 New Legend (V301, China)", MACHINE_SUPPORTS_SAVE )
GAME( 2008, kov2nl_300cn, kov2nl,    pgm2,    pgm2, pgm2_state,     kov2nl,       ROT0, "IGS", "Knights of Valour 2 New Legend (V300, China)", MACHINE_SUPPORTS_SAVE )


// Dodonpachi Daioujou Tamashii - should be a V200 too
GAME( 2010, ddpdojt,      0,    pgm2_ramrom,    pgm2, pgm2_state,     ddpdojt,    ROT270, "IGS / Cave", "DoDonPachi Dai-Ou-Jou Tamashii (V201, China)", MACHINE_SUPPORTS_SAVE )

// Knights of Valour 3 - should be a V103 and V101 too
GAME( 2011, kov3,         0,    pgm2_hires, pgm2, pgm2_state,     kov3_104,   ROT0, "IGS", "Knights of Valour 3 (V104, China, Hong Kong, Taiwan)", MACHINE_SUPPORTS_SAVE )
GAME( 2011, kov3_102,     kov3, pgm2_hires, pgm2, pgm2_state,     kov3_102,   ROT0, "IGS", "Knights of Valour 3 (V102, China, Hong Kong, Taiwan)", MACHINE_SUPPORTS_SAVE )
GAME( 2011, kov3_101,     kov3, pgm2_hires, pgm2, pgm2_state,     kov3_101,   ROT0, "IGS", "Knights of Valour 3 (V101, China, Hong Kong, Taiwan)", MACHINE_SUPPORTS_SAVE )
GAME( 2011, kov3_100,     kov3, pgm2_hires, pgm2, pgm2_state,     kov3_100,   ROT0, "IGS", "Knights of Valour 3 (V100, China, Hong Kong, Taiwan)", MACHINE_SUPPORTS_SAVE )

// King of Fighters '98: Ultimate Match Hero
GAME( 2009, kof98umh,     0,    pgm2_lores, pgm2, pgm2_state,  kof98umh,   ROT0, "IGS / SNK Playmore / New Channel", "The King of Fighters '98: Ultimate Match HERO (China, V100, 09-08-23)", MACHINE_SUPPORTS_SAVE )

// Jigsaw World Arena

// Puzzle of Ocha / Ochainu No Pazuru
