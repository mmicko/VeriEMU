// license:BSD-3-Clause
// copyright-holders:Robbbert
/******************************************************************************

    Proteus III computer.

    2015-10-02 Skeleton [Robbbert]

    Chips:
    6800 @ 894kHz
    6850 (TTY keyboard interface)
    6850 (Cassette interface)
    6820 (PIA for Keyboard and video
    6844 DMA
    MC14411 baud rate generator
    CRT96364 CRTC @1008kHz

    There's an undumped 74S287 prom (M24) in the video section.
    It converts ascii control codes into the crtc control codes

    Schematic has lots of errors and omissions.

    Like many systems of the time, the cassette is grossly over-complicated,
    using 12 chips for a standard Kansas-city interface. Speed = 600 baud.

    To use the serial keyboard, type in the command: PORT#1
    and to go back to the parallel keyboard type in: PORT#0

    The Basic seems rather buggy and does odd things from time to time.
    bios 0 doesn't seem to have any way to backspace and type over
    bios 0 is the only one to have the EDIT command, although no idea how
    to use it.
    bios 2 is from a compatible system called "Micro Systemes 1", from the
    same company.

    To Do:
    - Cassette (coded but too unreliable to be considered working)
    - Add support for k7 cassette files.
    - Need software
    - Need missing PROM, so that all the CRTC controls can be emulated
    - Keyboard may have its own CPU etc, but no info available.
    - Missing buttons: BYE, PANIC, SPEED, HERE-IS. Could be more.
    - Should be able to type in some low-res graphics from the keyboard, not implemented.

******************************************************************************/

#include "emu.h"

#include "cpu/m6800/m6800.h"
#include "imagedev/cassette.h"
#include "machine/6821pia.h"
#include "machine/6850acia.h"
#include "machine/mc14411.h"
#include "machine/clock.h"
#include "machine/keyboard.h"
#include "machine/timer.h"
#include "sound/wave.h"

#include "bus/rs232/rs232.h"

#include "screen.h"
#include "speaker.h"


class proteus3_state : public driver_device
{
public:
	proteus3_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_p_videoram(*this, "vram")
		, m_p_chargen(*this, "chargen")
		, m_pia(*this, "pia")
		, m_brg(*this, "brg")
		, m_acia1(*this, "acia1")
		, m_acia2(*this, "acia2")
		, m_cass(*this, "cassette")
		, m_serial(*this, "SERIAL")
	{ }

	DECLARE_WRITE_LINE_MEMBER(ca2_w);
	DECLARE_WRITE8_MEMBER(video_w);
	void kbd_put(u8 data);
	DECLARE_WRITE_LINE_MEMBER(acia1_txdata_w);
	DECLARE_WRITE_LINE_MEMBER(acia1_clock_w);
	TIMER_DEVICE_CALLBACK_MEMBER(timer_c);
	TIMER_DEVICE_CALLBACK_MEMBER(timer_p);
	uint32_t screen_update_proteus3(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);

	// Clocks
	void write_acia_clocks(int id, int state);
	DECLARE_WRITE_LINE_MEMBER (write_f1_clock){ write_acia_clocks(mc14411_device::TIMER_F1, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f2_clock){ write_acia_clocks(mc14411_device::TIMER_F2, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f3_clock){ write_acia_clocks(mc14411_device::TIMER_F3, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f4_clock){ write_acia_clocks(mc14411_device::TIMER_F4, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f5_clock){ write_acia_clocks(mc14411_device::TIMER_F5, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f6_clock){ write_acia_clocks(mc14411_device::TIMER_F6, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f7_clock){ write_acia_clocks(mc14411_device::TIMER_F7, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f8_clock){ write_acia_clocks(mc14411_device::TIMER_F8, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f9_clock){ write_acia_clocks(mc14411_device::TIMER_F9, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f10_clock){ write_acia_clocks(mc14411_device::TIMER_F10, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f11_clock){ write_acia_clocks(mc14411_device::TIMER_F11, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f12_clock){ write_acia_clocks(mc14411_device::TIMER_F12, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f13_clock){ write_acia_clocks(mc14411_device::TIMER_F13, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f14_clock){ write_acia_clocks(mc14411_device::TIMER_F14, state); }
	DECLARE_WRITE_LINE_MEMBER (write_f15_clock){ write_acia_clocks(mc14411_device::TIMER_F15, state); }

	void proteus3(machine_config &config);
private:
	uint8_t m_video_data;
	uint8_t m_flashcnt;
	uint16_t m_curs_pos;
	uint8_t m_cass_data[4];
	bool m_cass_state;
	bool m_cassold;
	uint8_t m_clockcnt;
	virtual void machine_reset() override;
	required_device<cpu_device> m_maincpu;
	required_region_ptr<u8> m_p_videoram;
	required_region_ptr<u8> m_p_chargen;
	required_device<pia6821_device> m_pia;
	required_device<mc14411_device> m_brg;
	required_device<acia6850_device> m_acia1; // cassette uart
	required_device<acia6850_device> m_acia2; // tty keyboard uart
	required_device<cassette_image_device> m_cass;

	// hardware configuration and things that need rewiring
	required_ioport             m_serial;
};




/******************************************************************************
 Address Maps
******************************************************************************/

static ADDRESS_MAP_START(proteus3_mem, AS_PROGRAM, 8, proteus3_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x7fff) AM_RAM
	AM_RANGE(0x8004, 0x8007) AM_DEVREADWRITE("pia", pia6821_device, read, write)
	AM_RANGE(0x8008, 0x8009) AM_DEVREADWRITE("acia1", acia6850_device, read, write) // cassette
	AM_RANGE(0x8010, 0x8011) AM_DEVREADWRITE("acia2", acia6850_device, read, write) // serial keyboard (never writes data)
	AM_RANGE(0xc000, 0xffff) AM_ROM
ADDRESS_MAP_END


/******************************************************************************
 Input Ports
******************************************************************************/

static INPUT_PORTS_START(proteus3)
	PORT_START("SERIAL")
	PORT_CONFNAME(0x0F , 0x00 , "Serial Baud Rate") // F1-F16 pins on MC14411 in X16
	PORT_CONFSETTING(mc14411_device::TIMER_F1,  "153600")
	PORT_CONFSETTING(mc14411_device::TIMER_F2,  "115200")
	PORT_CONFSETTING(mc14411_device::TIMER_F3,  "76800")
	PORT_CONFSETTING(mc14411_device::TIMER_F4,  "57600")
	PORT_CONFSETTING(mc14411_device::TIMER_F5,  "38400")
	PORT_CONFSETTING(mc14411_device::TIMER_F6,  "28800")
	PORT_CONFSETTING(mc14411_device::TIMER_F7,  "19200")
	PORT_CONFSETTING(mc14411_device::TIMER_F8,  "9600")
	PORT_CONFSETTING(mc14411_device::TIMER_F9,  "4800")
	PORT_CONFSETTING(mc14411_device::TIMER_F10, "3200")
	PORT_CONFSETTING(mc14411_device::TIMER_F11, "2400")
	PORT_CONFSETTING(mc14411_device::TIMER_F12, "2153.3")
	PORT_CONFSETTING(mc14411_device::TIMER_F13, "1758.8")
	PORT_CONFSETTING(mc14411_device::TIMER_F14, "1200")
	PORT_CONFSETTING(mc14411_device::TIMER_F15, "921.6")
	PORT_CONFSETTING(mc14411_device::TIMER_F16, "1.8432")
INPUT_PORTS_END

void proteus3_state::kbd_put(u8 data)
{
	if (data == 0x08)
		data = 0x0f; // take care of backspace (bios 1 and 2)
	m_pia->portb_w(data);
	m_pia->cb1_w(1);
	m_pia->cb1_w(0);
}

void proteus3_state::write_acia_clocks(int id, int state)
{
	if (id == m_serial->read()) // Configurable serial port
	{
		m_acia2->write_txc(state);
		m_acia2->write_rxc(state);
	}
	if (id == mc14411_device::TIMER_F8) // Fixed bitrate for the cassette interface
	{
		acia1_clock_w(state);
	}
}

/******************************************************************************
 Cassette
******************************************************************************/
TIMER_DEVICE_CALLBACK_MEMBER( proteus3_state::timer_c )
{
	m_cass_data[3]++;

	if (m_cass_state != m_cassold)
	{
		m_cass_data[3] = 0;
		m_cassold = m_cass_state;
	}

	if (m_cass_state)
		m_cass->output(BIT(m_cass_data[3], 0) ? -1.0 : +1.0); // 2400Hz
	else
		m_cass->output(BIT(m_cass_data[3], 1) ? -1.0 : +1.0); // 1200Hz
}

TIMER_DEVICE_CALLBACK_MEMBER( proteus3_state::timer_p )
{
	/* cassette - turn 1200/2400Hz to a bit */
	m_cass_data[1]++;
	uint8_t cass_ws = (m_cass->input() > +0.01) ? 1 : 0;

	if (cass_ws != m_cass_data[0])
	{
		m_cass_data[0] = cass_ws;
		m_acia1->write_rxd((m_cass_data[1] < 12) ? 1 : 0);
		m_cass_data[1] = 0;
	}
}

WRITE_LINE_MEMBER( proteus3_state::acia1_txdata_w )
{
	m_cass_state = state;
}

WRITE_LINE_MEMBER( proteus3_state::acia1_clock_w )
{
	m_clockcnt++;
	m_acia1->write_txc(BIT(m_clockcnt, 0));  // divide by 16 selected in the acia
	if ((m_clockcnt & 0x0f) == 0x04)
		m_acia1->write_rxc(BIT(m_clockcnt, 4));  // divide by 1 selected
}


/******************************************************************************
 Video
******************************************************************************/
WRITE8_MEMBER( proteus3_state::video_w )
{
	m_video_data = data;
}

WRITE_LINE_MEMBER( proteus3_state::ca2_w )
{
	if (state)
	{
		switch(m_video_data)
		{
			case 0x0a: // Line Feed
				if (m_curs_pos > 959) // on bottom line?
				{
					memmove(m_p_videoram, m_p_videoram+64, 960); // scroll
					memset(m_p_videoram+960, 0x20, 64); // blank bottom line
				}
				else
					m_curs_pos += 64;
				break;
			case 0x0d: // Carriage Return
				m_curs_pos &= 0x3c0;
				break;
			case 0x0c: // CLS
				m_curs_pos = 0; // home cursor
				memset(m_p_videoram, 0x20, 1024); // clear screen
				break;
			case 0x0f: // Cursor Left
				if (m_curs_pos)
					m_curs_pos--;
				break;
			case 0x7f: // Erase character under cursor
				m_p_videoram[m_curs_pos] = 0x20;
				break;
			default: // If a displayable character, show it
				if ((m_video_data > 0x1f) && (m_video_data < 0x7f))
				{
					m_p_videoram[m_curs_pos] = m_video_data;
					m_curs_pos++;
					if (m_curs_pos > 1023) // have we run off the bottom?
					{
						m_curs_pos -= 64;
						memmove(m_p_videoram, m_p_videoram+64, 960); // scroll
						memset(m_p_videoram+960, 0x20, 64); // blank bottom line
					}
				}
		}
	}
}

uint32_t proteus3_state::screen_update_proteus3(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	uint8_t y,ra,chr,gfx;
	uint16_t sy=0,ma=0,x;
	m_flashcnt++;

	for(y = 0; y < 16; y++ )
	{
		for (ra = 0; ra < 12; ra++)
		{
			uint16_t *p = &bitmap.pix16(sy++);

			for (x = ma; x < ma + 64; x++)
			{
				gfx = 0;
				if (ra < 8)
				{
					chr = m_p_videoram[x]; // get char in videoram
					gfx = m_p_chargen[(chr<<3) | ra]; // get dot pattern in chargen
				}
				else
				if ((ra == 9) && (m_curs_pos == x) && BIT(m_flashcnt, 4))
					gfx = 0xff;

				/* Display a scanline of a character */
				*p++ = BIT(gfx, 0);
				*p++ = BIT(gfx, 1);
				*p++ = BIT(gfx, 2);
				*p++ = BIT(gfx, 3);
				*p++ = BIT(gfx, 4);
				*p++ = BIT(gfx, 5);
				*p++ = BIT(gfx, 6);
				*p++ = BIT(gfx, 7);
			}
		}
		ma+=64;
	}
	return 0;
}


/* F4 Character Displayer */
static const gfx_layout charlayout =
{
	8, 8,                  /* 8 x 8 characters */
	128,                    /* 128 characters */
	1,                  /* 1 bits per pixel */
	{ 0 },                  /* no bitplanes */
	/* x offsets */
	{ 7, 6, 5, 4, 3, 2, 1, 0 },
	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8                    /* every char takes 8 bytes */
};

static GFXDECODE_START( proteus3 )
	GFXDECODE_ENTRY( "chargen", 0, charlayout, 0, 1 )
GFXDECODE_END


void proteus3_state::machine_reset()
{
	m_curs_pos = 0;
	m_cass_data[0] = m_cass_data[1] = m_cass_data[2] = m_cass_data[3] = 0;
	m_cass_state = 1;
	m_cassold = 1;
	m_acia1->write_rxd(1);

	// Set up the BRG divider. RSA is a jumper setting and RSB is always set High
	m_brg->rsa_w( CLEAR_LINE );
	m_brg->rsb_w( ASSERT_LINE );

	// Disable all configured timers, only enabling the used ones
	m_brg->timer_disable_all();
	m_brg->timer_enable((mc14411_device::timer_id) m_serial->read(), true); // Serial port
	m_brg->timer_enable( mc14411_device::TIMER_F8, true); // Cassette interface
}


/******************************************************************************
 Machine Drivers
******************************************************************************/

MACHINE_CONFIG_START(proteus3_state::proteus3)
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M6800, XTAL(3'579'545))  /* Divided by 4 internally */
	MCFG_CPU_PROGRAM_MAP(proteus3_mem)

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(200))
	MCFG_SCREEN_SIZE(64*8, 16*12)
	MCFG_SCREEN_VISIBLE_AREA(0, 64*8-1, 0, 16*12-1)
	MCFG_SCREEN_UPDATE_DRIVER(proteus3_state, screen_update_proteus3)
	MCFG_SCREEN_PALETTE("palette")
	MCFG_GFXDECODE_ADD("gfxdecode", "palette", proteus3)
	MCFG_PALETTE_ADD_MONOCHROME("palette")

	/* Devices */
	MCFG_DEVICE_ADD("pia", PIA6821, 0)
	MCFG_PIA_WRITEPA_HANDLER(WRITE8(proteus3_state, video_w))
	MCFG_PIA_CA2_HANDLER(WRITELINE(proteus3_state, ca2_w))
	MCFG_PIA_IRQB_HANDLER(INPUTLINE("maincpu", M6800_IRQ_LINE))
	MCFG_DEVICE_ADD("keyboard", GENERIC_KEYBOARD, 0)
	MCFG_GENERIC_KEYBOARD_CB(PUT(proteus3_state, kbd_put))

	/* cassette */
	MCFG_DEVICE_ADD ("acia1", ACIA6850, 0)
	MCFG_ACIA6850_TXD_HANDLER(WRITELINE(proteus3_state, acia1_txdata_w))
	MCFG_CASSETTE_ADD("cassette")
	MCFG_CASSETTE_DEFAULT_STATE(CASSETTE_PLAY | CASSETTE_MOTOR_ENABLED | CASSETTE_SPEAKER_ENABLED)
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_WAVE_ADD(WAVE_TAG, "cassette")
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("timer_c", proteus3_state, timer_c, attotime::from_hz(4800))
	MCFG_TIMER_DRIVER_ADD_PERIODIC("timer_p", proteus3_state, timer_p, attotime::from_hz(40000))

	// optional tty keyboard
	MCFG_DEVICE_ADD ("acia2", ACIA6850, 0)
	MCFG_ACIA6850_TXD_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_txd))
	MCFG_ACIA6850_RTS_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_rts))
	MCFG_RS232_PORT_ADD("rs232", default_rs232_devices, "keyboard")
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("acia2", acia6850_device, write_rxd))
	MCFG_RS232_CTS_HANDLER(DEVWRITELINE("acia2", acia6850_device, write_cts))

	/* Bit Rate Generator */
	MCFG_MC14411_ADD ("brg", XTAL(1'843'200)) // crystal needs verification but is the likely one
	MCFG_MC14411_F1_CB(WRITELINE (proteus3_state, write_f1_clock))
	MCFG_MC14411_F2_CB(WRITELINE (proteus3_state, write_f2_clock))
	MCFG_MC14411_F3_CB(WRITELINE (proteus3_state, write_f3_clock))
	MCFG_MC14411_F4_CB(WRITELINE (proteus3_state, write_f4_clock))
	MCFG_MC14411_F5_CB(WRITELINE (proteus3_state, write_f5_clock))
	MCFG_MC14411_F6_CB(WRITELINE (proteus3_state, write_f6_clock))
	MCFG_MC14411_F7_CB(WRITELINE (proteus3_state, write_f7_clock))
	MCFG_MC14411_F8_CB(WRITELINE (proteus3_state, write_f8_clock))
	MCFG_MC14411_F9_CB(WRITELINE (proteus3_state, write_f9_clock))
	MCFG_MC14411_F10_CB(WRITELINE (proteus3_state, write_f10_clock))
	MCFG_MC14411_F11_CB(WRITELINE (proteus3_state, write_f11_clock))
	MCFG_MC14411_F12_CB(WRITELINE (proteus3_state, write_f12_clock))
	MCFG_MC14411_F13_CB(WRITELINE (proteus3_state, write_f13_clock))
	MCFG_MC14411_F14_CB(WRITELINE (proteus3_state, write_f14_clock))
	MCFG_MC14411_F15_CB(WRITELINE (proteus3_state, write_f15_clock))
MACHINE_CONFIG_END



/******************************************************************************
 ROM Definitions
******************************************************************************/

ROM_START(proteus3)
	ROM_REGION(0x10000, "maincpu", 0)
	ROM_SYSTEM_BIOS( 0, "14k", "14k BASIC")
	ROMX_LOAD( "bas1.bin",     0xc800, 0x0800, CRC(016bf2d6) SHA1(89605dbede3b6fd101ee0548e5c545a0824fcfd3), ROM_BIOS(1) )
	ROMX_LOAD( "bas2.bin",     0xd000, 0x0800, CRC(39d3e543) SHA1(dd0fe220e3c2a48ce84936301311cbe9f1597ca7), ROM_BIOS(1) )
	ROMX_LOAD( "bas3.bin",     0xd800, 0x0800, CRC(3a41617d) SHA1(175406f4732389e226bc50d27ada39e6ea48de34), ROM_BIOS(1) )
	ROMX_LOAD( "bas4.bin",     0xe000, 0x0800, CRC(ee9d77ee) SHA1(f7e60a1ab88a3accc8ffdc545657c071934d09d2), ROM_BIOS(1) )
	ROMX_LOAD( "bas5.bin",     0xe800, 0x0800, CRC(bd81bb34) SHA1(6325735e5750a9536e63b67048f74711fae1fa42), ROM_BIOS(1) )
	ROMX_LOAD( "bas6.bin",     0xf000, 0x0800, CRC(60cd006b) SHA1(28354f78490da1eb5116cbbc43eaca0670f7f398), ROM_BIOS(1) )
	ROMX_LOAD( "bas7.bin",     0xf800, 0x0800, CRC(84c3dc22) SHA1(8fddba61b5f0270ca2daef32ab5edfd60300c776), ROM_BIOS(1) )
	ROM_FILL( 0xc000, 1, 0x00 )  // if c000 isn't 0 it assumes a rom is there and jumps to it

	ROM_SYSTEM_BIOS( 1, "8k", "8k BASIC")
	ROMX_LOAD( "proteus3_basic8k.m0", 0xe000, 0x2000, CRC(7d9111c2) SHA1(3c032c9c7f87d22a1a9819b3b812be84404d2ad2), ROM_BIOS(2) )
	ROM_RELOAD( 0xc000, 0x2000 )

	ROM_SYSTEM_BIOS( 2, "8kms", "8k Micro-Systemes BASIC")
	ROMX_LOAD( "ms1_basic8k.bin", 0xe000, 0x2000, CRC(b5476e28) SHA1(c8c2366d549b2645c740be4ab4237e05c3cab4a9), ROM_BIOS(3) )
	ROM_RELOAD( 0xc000, 0x2000 )

	ROM_REGION(0x400, "chargen", 0)
	ROM_LOAD( "proteus3_font.m25",   0x0200, 0x0100, CRC(6a3a30a5) SHA1(ab39bf09722928483e497b87ac2dbd870828893b) )
	ROM_CONTINUE( 0x100, 0x100 )
	ROM_CONTINUE( 0x300, 0x100 )
	ROM_CONTINUE( 0x000, 0x100 )

	ROM_REGION(0x400, "vram", ROMREGION_ERASE00)

	ROM_REGION(0x0800, "user1", 0) // roms not used yet
	// Proteus III - pbug F800-FFFF, expects RAM at F000-F7FF
	ROM_LOAD( "proteus3_pbug.bin", 0x0000, 0x0800, CRC(1118694d) SHA1(2dfc08d405e8f2936f5b0bd1c4007995151abbba) )
ROM_END


/******************************************************************************
 Drivers
******************************************************************************/

//    YEAR  NAME      PARENT  COMPAT  MACHINE   INPUT     CLASS           INIT  COMPANY                  FULLNAME       FLAGS
COMP( 1978, proteus3, 0,      0,      proteus3, proteus3, proteus3_state, 0,    "Proteus International", "Proteus III", MACHINE_NOT_WORKING | MACHINE_NO_SOUND_HW)
