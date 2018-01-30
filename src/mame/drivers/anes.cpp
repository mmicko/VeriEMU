// license:BSD-3-Clause
// copyright-holders:Ivan Vangelista

/*
TON PUU MAHJONG (Japan) by ANES

TODO:
- ROM banking;
- blitter, 8bpp with hardcoded palette & writes to ROM area!?
- inputs;

- 1x Z0840008PSC Z80 CPU
- 1x 16.000 XTAL near the Z80
- 1x YM2413 sound chip
- 1x 3.579545 XTAL near the YM2413
- 1x Xilinx XC7354 CPLD
- 2x ISSI IS61C64AH 8k x8 SRAM
- 1x HM6265LK-70
- 1x unknown 160 pin device labeled "ANES ORIGINAL SEAL NO. A199."
- 4x bank of 8 dip-switches

Sanma - 3nin-uchi Mahjong is another ANES game confirmed running on the same hardware.
*/

#include "emu.h"
#include "screen.h"
#include "speaker.h"
#include "cpu/z80/z80.h"
#include "sound/ym2413.h"

class anes_state : public driver_device
{
public:
	anes_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
	{
	}

	DECLARE_WRITE8_MEMBER(vram_offset_w);
	DECLARE_WRITE8_MEMBER(blit_trigger_w);

	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);

	void anes(machine_config &config);
protected:
	virtual void machine_start() override;

private:
	uint8_t m_vram_offset[3];
};

uint32_t anes_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	return 0;
}

WRITE8_MEMBER(anes_state::vram_offset_w)
{
	m_vram_offset[offset] = data;
}

WRITE8_MEMBER(anes_state::blit_trigger_w)
{
	/*
	 operation is:
	 checks for bit 4 to be high in port $16
	 writes either 0 or 3 to port $04
	 writes a 0 to port $00
	 writes an offset to ports $0c / $0d / $0e
	 writes 1 to port $0b, writes to program space, writes 2 to port $0b, writes to program space
	 writes a mode to port $0b, writes to trigger port $0a
	 */

	//printf("%02x%02x%02x\n",m_vram_offset[0],m_vram_offset[1],m_vram_offset[2]);
}

static ADDRESS_MAP_START( prg_map, AS_PROGRAM, 8, anes_state )
	AM_RANGE(0x0000, 0xefff) AM_ROM
	AM_RANGE(0xf000, 0xffff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( io_map, AS_IO, 8, anes_state )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	ADDRESS_MAP_UNMAP_HIGH

	AM_RANGE(0x07, 0x07) AM_WRITENOP // mux write
	AM_RANGE(0x08, 0x09) AM_DEVWRITE("ym", ym2413_device, write)
	AM_RANGE(0x0a, 0x0a) AM_WRITE(blit_trigger_w)
//  AM_RANGE(0x0b, 0x0b) AM_WRITE(blit_mode_w)
	AM_RANGE(0x0c, 0x0e) AM_WRITE(vram_offset_w)
	AM_RANGE(0x11, 0x11) AM_READ_PORT("DSW1")
	AM_RANGE(0x12, 0x12) AM_READ_PORT("DSW2")
	AM_RANGE(0x13, 0x13) AM_READ_PORT("DSW3")
	AM_RANGE(0x14, 0x15) AM_READNOP // mux read
	AM_RANGE(0x16, 0x16) AM_READ_PORT("IN0") AM_WRITENOP
//  AM_RANGE(0xfe, 0xfe) banking? unknown ROM range
ADDRESS_MAP_END


static INPUT_PORTS_START( anes )
	PORT_START("IN0")
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_UNKNOWN ) // blitter busy status
	PORT_BIT( 0xef, IP_ACTIVE_LOW, IPT_UNKNOWN ) // used, coin?

	PORT_START("DSW1")
	PORT_DIPUNKNOWN_DIPLOC(0x01, 0x01, "SW1:1")
	PORT_DIPUNKNOWN_DIPLOC(0x02, 0x02, "SW1:2")
	PORT_DIPUNKNOWN_DIPLOC(0x04, 0x04, "SW1:3")
	PORT_DIPUNKNOWN_DIPLOC(0x08, 0x08, "SW1:4")
	PORT_DIPUNKNOWN_DIPLOC(0x10, 0x10, "SW1:5")
	PORT_DIPUNKNOWN_DIPLOC(0x20, 0x20, "SW1:6")
	PORT_DIPUNKNOWN_DIPLOC(0x40, 0x40, "SW1:7")
	PORT_DIPUNKNOWN_DIPLOC(0x80, 0x80, "SW1:8")

	PORT_START("DSW2")
	PORT_DIPUNKNOWN_DIPLOC(0x01, 0x01, "SW2:1")
	PORT_DIPUNKNOWN_DIPLOC(0x02, 0x02, "SW2:2")
	PORT_DIPUNKNOWN_DIPLOC(0x04, 0x04, "SW2:3")
	PORT_DIPUNKNOWN_DIPLOC(0x08, 0x08, "SW2:4")
	PORT_DIPUNKNOWN_DIPLOC(0x10, 0x10, "SW2:5")
	PORT_DIPUNKNOWN_DIPLOC(0x20, 0x20, "SW2:6")
	PORT_DIPUNKNOWN_DIPLOC(0x40, 0x40, "SW2:7")
	PORT_DIPUNKNOWN_DIPLOC(0x80, 0x80, "SW2:8")

	PORT_START("DSW3")
	PORT_DIPUNKNOWN_DIPLOC(0x01, 0x01, "SW3:1")
	PORT_DIPUNKNOWN_DIPLOC(0x02, 0x02, "SW3:2")
	PORT_DIPUNKNOWN_DIPLOC(0x04, 0x04, "SW3:3")
	PORT_DIPUNKNOWN_DIPLOC(0x08, 0x08, "SW3:4")
	PORT_DIPUNKNOWN_DIPLOC(0x10, 0x10, "SW3:5")
	PORT_DIPUNKNOWN_DIPLOC(0x20, 0x20, "SW3:6")
	PORT_DIPUNKNOWN_DIPLOC(0x40, 0x40, "SW3:7")
	PORT_DIPUNKNOWN_DIPLOC(0x80, 0x80, "SW3:8")

	PORT_START("DSW4")
	PORT_DIPUNKNOWN_DIPLOC(0x01, 0x01, "SW4:1")
	PORT_DIPUNKNOWN_DIPLOC(0x02, 0x02, "SW4:2")
	PORT_DIPUNKNOWN_DIPLOC(0x04, 0x04, "SW4:3")
	PORT_DIPUNKNOWN_DIPLOC(0x08, 0x08, "SW4:4")
	PORT_DIPUNKNOWN_DIPLOC(0x10, 0x10, "SW4:5")
	PORT_DIPUNKNOWN_DIPLOC(0x20, 0x20, "SW4:6")
	PORT_DIPUNKNOWN_DIPLOC(0x40, 0x40, "SW4:7")
	PORT_DIPUNKNOWN_DIPLOC(0x80, 0x80, "SW4:8")
INPUT_PORTS_END


void anes_state::machine_start()
{
}



MACHINE_CONFIG_START(anes_state::anes)
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", Z80, XTAL(16'000'000) / 2) // Z0840008PSC
	MCFG_CPU_PROGRAM_MAP(prg_map)
	MCFG_CPU_IO_MAP(io_map)
	MCFG_CPU_VBLANK_INT_DRIVER("screen", anes_state, irq0_line_hold)

	// all wrong
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(64*8, 32*8)
	MCFG_SCREEN_VISIBLE_AREA(0*8, 64*8-1, 0*8, 32*8-1)
	MCFG_SCREEN_UPDATE_DRIVER(anes_state, screen_update)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_PALETTE_ADD("palette", 0x100)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("ym", YM2413, XTAL(3'579'545))
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.30)
MACHINE_CONFIG_END


ROM_START( tonpuu )
	ROM_REGION(0x20000, "maincpu", 0)
	ROM_LOAD( "201.u32", 0x00000, 0x20000, CRC(ace857bb) SHA1(3f65976883c0c514abf73eeed9223ca52a2be410) ) // 27C010

	ROM_REGION(0x100000, "gfx1", 0)
	ROM_LOAD( "202.u33", 0x00000, 0x80000, CRC(4d62a358) SHA1(6edff8e031272cd5a466d9767454093870a0f90a) ) // 27C4001
	ROM_LOAD( "203.u34", 0x80000, 0x80000, CRC(a6068528) SHA1(c988bd1fc2f91befa9d0d39995ba98ef86b5d854) ) // 27C4001
ROM_END


GAME( 200?, tonpuu,  0, anes, anes, anes_state, 0, ROT0, "ANES", "Ton Puu Mahjong [BET] (Japan)", MACHINE_NOT_WORKING | MACHINE_IMPERFECT_GRAPHICS )
