// license:BSD-3-Clause
// copyright-holders:Robbbert
/********************************************************************************

Bull (Originally R2E) Micral 80-22G

2015-10-01 Skeleton [Robbbert]

http://www.ti99.com/exelvision/website/index.php?page=r2e-micral-8022-g

This expensive, futuristic-looking design featured a motherboard and slots,
much like an ancient pc. The known chip complement is:
Z80A, 4MHz; 64KB RAM, 2KB BIOS ROM, 256x4 prom (7611);
CRT8002, TMS9937 (=CRT5037), 4KB video RAM, 256x4 prom (7611);
2x 5.25 inch floppy drives, one ST506 5MB hard drive;
CDP6402 UART. Sound is a beeper.
The keyboard has a uPD780C (=Z80) and 1KB of ROM.

The FDC and HDC are unknown.
No manuals, schematic or circuit description have been found.

Commands must be in uppercase. Reboot to exit each command.
Bx[,x]: ??
Gxxxx : go (writes a jump @FFED then jumps to FFEB)
T     : test
*     : echo keystrokes
enter : ??

Using generic keyboard via the uart for now. It's unknown how the real keyboard
communicates with the main cpu.

FFF8/9 are used for sending instructions to the screen. FFF9 is command/status,
and FFF8 is data. The codes 0-D seem to be for the CRT5037, but the results don't
make much sense. Code A0 is to write a byte to the current cursor position, and
B0 is to get the status.

Screen needs:
- Scrolling
- Proper character generator
- To be properly understood
- According to the web, graphics are possible. A screenshot shows reverse video
  exists.

Other things...
- Beeper
- 2 floppy drives
- keyboard
- unknown ports

*********************************************************************************/

#include "emu.h"
#include "cpu/z80/z80.h"
#include "video/tms9927.h"
//#include "sound/beep.h"
#include "screen.h"
#include "speaker.h"
#include "machine/ay31015.h"
#include "bus/rs232/rs232.h"


class micral_state : public driver_device
{
public:
	micral_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		//, m_beep(*this, "beeper")
		, m_p_videoram(*this, "vram")
		, m_p_chargen(*this, "chargen")
		, m_uart(*this, "uart")
		, m_crtc(*this, "crtc")
	{ }

	DECLARE_DRIVER_INIT(micral);
	DECLARE_MACHINE_RESET(micral);
	DECLARE_READ8_MEMBER(keyin_r);
	DECLARE_READ8_MEMBER(status_r);
	DECLARE_READ8_MEMBER(unk_r);
	DECLARE_READ8_MEMBER(video_r);
	DECLARE_WRITE8_MEMBER(video_w);
	u32 screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);

	void micral(machine_config &config);
private:
	u16 s_curpos;
	u8 s_command;
	u8 s_data;
	required_device<cpu_device> m_maincpu;
	//required_device<beep_device> m_beep;
	required_region_ptr<u8> m_p_videoram;
	required_region_ptr<u8> m_p_chargen;
	required_device<ay31015_device> m_uart;
	required_device<crt5037_device> m_crtc;
};

READ8_MEMBER( micral_state::status_r )
{
	return m_uart->get_output_pin(AY31015_DAV) | 4;
}

READ8_MEMBER( micral_state::unk_r )
{
	return 0x96;
}

READ8_MEMBER( micral_state::keyin_r )
{
	m_uart->set_input_pin(AY31015_RDAV, 0);
	u8 result = m_uart->get_received_data();
	m_uart->set_input_pin(AY31015_RDAV, 1);
	return result;
}

READ8_MEMBER( micral_state::video_r )
{
	if (offset)
		return 0x07;
	else
		return m_p_videoram[s_curpos];
}

WRITE8_MEMBER( micral_state::video_w )
{
	if (offset)
	{
		s_command = data;
		if (s_command == 0x0c)
			s_curpos = (s_curpos & 0xff00) | s_data;
		else
		if (s_command == 0x0d)
			s_curpos = (s_curpos & 0xff) | ((s_data & 0x1f) << 8);
		else
		if (s_command == 0xa0)
			m_p_videoram[s_curpos] = s_data;

		//if (s_command < 0x10)
			//m_crtc->write(space, s_command, s_data);
	}
	else
	{
		s_data = data;
	}
}


static ADDRESS_MAP_START( mem_map, AS_PROGRAM, 8, micral_state )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0xf7ff) AM_RAM
	AM_RANGE(0xf800, 0xfeff) AM_ROM
	AM_RANGE(0xff00, 0xffef) AM_RAM
	AM_RANGE(0xfff6, 0xfff7) // AM_WRITENOP // unknown ports
	AM_RANGE(0xfff8, 0xfff9) AM_READWRITE(video_r, video_w)
	AM_RANGE(0xfffa, 0xfffa) AM_READ(keyin_r)
	AM_RANGE(0xfffb, 0xfffb) AM_READ(unk_r)
	AM_RANGE(0xfffc, 0xfffc) AM_READ(status_r)
	AM_RANGE(0xfffd, 0xffff) // more unknown ports
ADDRESS_MAP_END

static ADDRESS_MAP_START( mem_kbd, AS_PROGRAM, 8, micral_state )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x03ff) AM_ROM
	AM_RANGE(0x8000, 0x8000) AM_RAM // byte returned to main cpu after receiving irq
	AM_RANGE(0x8001, 0x8001) AM_READ_PORT("X0")
	AM_RANGE(0x8002, 0x8002) AM_READ_PORT("X1")
	AM_RANGE(0x8004, 0x8004) AM_READ_PORT("X2")
	AM_RANGE(0x8008, 0x8008) AM_READ_PORT("X3")
	AM_RANGE(0x8010, 0x8010) AM_READ_PORT("X4")
	AM_RANGE(0x8020, 0x8020) AM_READ_PORT("X5")
	AM_RANGE(0x8040, 0x8040) AM_READ_PORT("X6")
	AM_RANGE(0x8080, 0x8080) AM_READ_PORT("X7")
	AM_RANGE(0x8100, 0x8100) AM_READ_PORT("X8")
	AM_RANGE(0x8200, 0x8200) AM_READ_PORT("X9")
	AM_RANGE(0x8400, 0x8400) AM_READ_PORT("X10")
	AM_RANGE(0x8800, 0x8800) AM_READ_PORT("X11")
	AM_RANGE(0x9000, 0x9000) AM_READ_PORT("X12")
	AM_RANGE(0xa000, 0xa000) AM_READ_PORT("X13")
ADDRESS_MAP_END

static ADDRESS_MAP_START( io_kbd, AS_IO, 8, micral_state )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x00, 0x00) AM_READ_PORT("X14")
ADDRESS_MAP_END

/* Input ports */
static INPUT_PORTS_START( micral )
	PORT_START("X0")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_ESC)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_SPACE)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 01
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 03
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00

	PORT_START("X1")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_1) // 2A
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_3) // 22
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_5) // 28
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_7) // 94
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_9) // 90
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 29

	PORT_START("X2")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 3E, 3C
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_A)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_E)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_T)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_U)

	PORT_START("X3")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_O)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // '^'
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_S)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_F)

	PORT_START("X4")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_H)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_K)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_M)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_CLOSEBRACE) // 5B
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // OB
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_W)

	PORT_START("X5")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_C)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_B)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_COMMA) // 3F
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // ':','/'
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 08
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 06

	PORT_START("X6")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_ENTER)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_TAB)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 02
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00

	PORT_START("X7")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_2) // 91
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_4) // 27
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_6) // '-'
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_8) // '_'
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_0) // 8E
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_EQUALS) // '+'

	PORT_START("X8")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Z)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_R)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Y)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_I)

	PORT_START("X9")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_P)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // '@', '#'
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_Q)
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_D)
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_G)

	PORT_START("X10")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_J)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_L)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 9C, '%'
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 05
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 7F
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_X)

	PORT_START("X11")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_V)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_N)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // ';','.'
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // '!','&'
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 0A
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00

	PORT_START("X12")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 95,FE
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 97,FC
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 9D,'$'
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 96,'\'
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 99,84
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 9A,92

	PORT_START("X13")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) // 00

	PORT_START("X14")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_RCONTROL) // ?? don't look for a new keypress
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_NAME("SHIFT") PORT_CODE(KEYCODE_LSHIFT) PORT_CODE(KEYCODE_RSHIFT) PORT_CHAR(UCHAR_SHIFT_1)
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_KEYBOARD ) PORT_CODE(KEYCODE_LCONTROL) // ??
INPUT_PORTS_END

uint32_t micral_state::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	uint8_t y,ra,chr,gfx;
	uint16_t sy=0,ma=0,x;

	for (y = 0; y < 24; y++)
	{
		for (ra = 0; ra < 10; ra++)
		{
			uint16_t *p = &bitmap.pix16(sy++);

			for (x = 0; x < 80; x++)
			{
				gfx = 0;
				if (ra < 9)
				{
					chr = m_p_videoram[x+ma];
					gfx = m_p_chargen[(chr<<4) | ra ];
					if (((s_curpos & 0xff)==x) && ((s_curpos >> 8)==y))
						gfx ^= 0xff;
				}
				/* Display a scanline of a character */
				*p++ = BIT(gfx, 7);
				*p++ = BIT(gfx, 6);
				*p++ = BIT(gfx, 5);
				*p++ = BIT(gfx, 4);
				*p++ = BIT(gfx, 3);
				*p++ = BIT(gfx, 2);
				*p++ = BIT(gfx, 1);
				*p++ = BIT(gfx, 0);
			}
		}
		ma+=256;
	}
	return 0;
}

DRIVER_INIT_MEMBER( micral_state, micral )
{
	//uint8_t *main = memregion("maincpu")->base();

	//membank("bankr0")->configure_entry(1, &main[0xf800]);
	//membank("bankr0")->configure_entry(0, &main[0x10000]);
	//membank("bankw0")->configure_entry(0, &main[0xf800]);
}

MACHINE_RESET_MEMBER( micral_state, micral )
{
	//membank("bankr0")->set_entry(0); // point at rom
	//membank("bankw0")->set_entry(0); // always write to ram
	m_maincpu->set_state_int(Z80_PC, 0xf800);

	// no idea if these are hard-coded, or programmable
	m_uart->set_input_pin(AY31015_XR, 0);
	m_uart->set_input_pin(AY31015_XR, 1);
	m_uart->set_input_pin(AY31015_SWE, 0);
	m_uart->set_input_pin(AY31015_NP, 1);
	m_uart->set_input_pin(AY31015_TSB, 0);
	m_uart->set_input_pin(AY31015_NB1, 1);
	m_uart->set_input_pin(AY31015_NB2, 1);
	m_uart->set_input_pin(AY31015_EPS, 1);
	m_uart->set_input_pin(AY31015_CS, 1);
	m_uart->set_input_pin(AY31015_CS, 0);
}

MACHINE_CONFIG_START(micral_state::micral)
	// basic machine hardware
	MCFG_CPU_ADD( "maincpu", Z80, XTAL(4'000'000) )
	MCFG_CPU_PROGRAM_MAP(mem_map)
	// no i/o ports on main cpu
	MCFG_CPU_ADD( "keyboard", Z80, XTAL(1'000'000) ) // freq unknown
	MCFG_CPU_PROGRAM_MAP(mem_kbd)
	MCFG_CPU_IO_MAP(io_kbd)

	MCFG_MACHINE_RESET_OVERRIDE(micral_state, micral)

	// video hardware
	MCFG_SCREEN_ADD_MONOCHROME("screen", RASTER, rgb_t::green())
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(250))
	MCFG_SCREEN_UPDATE_DRIVER(micral_state, screen_update)
	MCFG_SCREEN_SIZE(640, 240)
	MCFG_SCREEN_VISIBLE_AREA(0, 639, 0, 239)
	MCFG_SCREEN_PALETTE("palette")
	MCFG_PALETTE_ADD_MONOCHROME("palette")
	//MCFG_GFXDECODE_ADD("gfxdecode", "palette", micral)

	MCFG_DEVICE_ADD("crtc", CRT5037, 4000000)  // xtal freq unknown
	MCFG_TMS9927_CHAR_WIDTH(8)  // unknown
	//MCFG_TMS9927_VSYN_CALLBACK(DEVWRITELINE(TMS5501_TAG, tms5501_device, sens_w))
	MCFG_VIDEO_SET_SCREEN("screen")

	/* sound hardware */
	//MCFG_SPEAKER_STANDARD_MONO("mono")
	//MCFG_SOUND_ADD("beeper", BEEP, 2000)
	//MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.50)

	MCFG_DEVICE_ADD("uart", AY51013, 0) // CDP6402
	MCFG_AY51013_TX_CLOCK(153600)
	MCFG_AY51013_RX_CLOCK(153600)
	MCFG_AY51013_READ_SI_CB(DEVREADLINE("rs232", rs232_port_device, rxd_r))
	MCFG_AY51013_WRITE_SO_CB(DEVWRITELINE("rs232", rs232_port_device, write_txd))
	MCFG_RS232_PORT_ADD("rs232", default_rs232_devices, "keyboard")
MACHINE_CONFIG_END

ROM_START( micral )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "8022g.rom", 0xf800, 0x0800, CRC(882732a9) SHA1(3f37b82c450a54aedec209bd46fcbcf131c86313) )

	ROM_REGION( 0x400, "keyboard", 0 )
	ROM_LOAD( "2010221.rom", 0x000, 0x400, CRC(65123378) SHA1(401f0a648b78bf1662a1cd2546e83ba8e3cb7a42) )

	ROM_REGION( 0x2000, "vram", ROMREGION_ERASEFF )

	// Using the chargen from 'c10' for now.
	ROM_REGION( 0x2000, "chargen", 0 )
	ROM_LOAD( "c10_char.bin", 0x0000, 0x2000, BAD_DUMP CRC(cb530b6f) SHA1(95590bbb433db9c4317f535723b29516b9b9fcbf))
ROM_END

/* Driver */

//    YEAR  NAME    PARENT  COMPAT  MACHINE    INPUT    CLASS          INIT     COMPANY     FULLNAME         FLAGS
COMP( 1981, micral, 0,      0,      micral,    micral,  micral_state,  micral,  "Bull R2E", "Micral 80-22G", MACHINE_IS_SKELETON )
