// license:BSD-3-Clause
// copyright-holders:Robbbert
/***************************************************************************

Powertran Cortex

2012-04-20 Skeleton driver.

ftp://ftp.whtech.com/Powertran Cortex/
http://www.powertrancortex.com/index.html

Uses Texas Instruments parts and similar to other TI computers.
It was designed by TI engineers, so it may perhaps be a clone
of another TI or the Geneve.

Chips:
TMS9995   - CPU
TMS9929   - Video
TMS9911   - DMA to floppy (unemulated device)
TMS9909   - Floppy Disk Controller (unemulated device)
TMS9902   - UART (x2) (device not usable with rs232.h)
AY-5-2376 - Keyboard controller

All input to be in uppercase. Note that "lowercase" is just smaller uppercase,
and is not acceptable as input.

There's no option in BASIC to produce sound. It will beep if an invalid key
(usually a control key) is pressed.

To clear the screen press Ctrl L.

ToDo:
- Unemulated devices
- Cassette
- Keyboard to use AY device
- Memory mapping unit (74LS610)
- Various CRU I/O

Note that the MAME implementation of CRU addresses is not the same as real
hardware. For writing, MAME uses the correct address (R12/2 + offset), with
the bit (0 or 1), being in 'data'. However, for reading, 8 CRU bits are
packed into a single address-byte (CRU 0 = bit 0, etc). So the address is
(R12/2 + offset) >> 3.

****************************************************************************/


#include "emu.h"
#include "cpu/tms9900/tms9995.h"
#include "machine/74259.h"
#include "video/tms9928a.h"
//#include "machine/tms9902.h"
#include "machine/keyboard.h"
#include "sound/beep.h"
#include "speaker.h"

class cortex_state : public driver_device
{
public:
	cortex_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_beep(*this, "beeper")
		, m_io_dsw(*this, "DSW")
		{ }

	void kbd_put(u8 data);
	DECLARE_WRITE_LINE_MEMBER(keyboard_ack_w);
	DECLARE_WRITE_LINE_MEMBER(romsw_w);
	DECLARE_WRITE_LINE_MEMBER(vdp_int_w);
	DECLARE_READ8_MEMBER(pio_r);
	DECLARE_READ8_MEMBER(keyboard_r);
	DECLARE_DRIVER_INIT(init);

	void cortex(machine_config &config);
private:
	bool m_kbd_ack;
	bool m_vdp_int;
	uint8_t m_term_data;
	virtual void machine_reset() override;
	required_device<tms9995_device> m_maincpu;
	required_device<beep_device>    m_beep;
	required_ioport m_io_dsw;
};

static ADDRESS_MAP_START( mem_map, AS_PROGRAM, 8, cortex_state )
	AM_RANGE(0x0000, 0x7fff) AM_READ_BANK("bankr0") AM_WRITE_BANK("bankw0")
	AM_RANGE(0x8000, 0xefff) AM_RAM
	AM_RANGE(0xf100, 0xf11f) AM_RAM // memory mapping unit
	AM_RANGE(0xf120, 0xf120) AM_DEVREADWRITE("crtc", tms9928a_device, vram_read, vram_write)
	AM_RANGE(0xf121, 0xf121) AM_DEVREADWRITE("crtc", tms9928a_device, register_read, register_write)
	//AM_RANGE(0xf140, 0xf147) // fdc tms9909
ADDRESS_MAP_END

static ADDRESS_MAP_START( io_map, AS_IO, 8, cortex_state )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x0007) AM_MIRROR(0x18) AM_DEVWRITE("control", ls259_device, write_d0)
	AM_RANGE(0x0000, 0x0000) AM_READ(pio_r)
	AM_RANGE(0x0001, 0x0001) AM_READ(keyboard_r)
	//AM_RANGE(0x0040, 0x005f) AM_DEVWRITE("uart1", tms9902_device, cruwrite) // RS232 (r12 = 80-bf)
	//AM_RANGE(0x0008, 0x000b) AM_DEVREAD("uart1", tms9902_device, cruread) // RS232
	//AM_RANGE(0x00c0, 0x00df) AM_DEVWRITE("uart2", tms9902_device, cruwrite) // Cassette (r12 = 180-1bf)
	//AM_RANGE(0x0018, 0x001b) AM_DEVREAD("uart2", tms9902_device, cruread) // Cassette
	//AM_RANGE(0x00e0, 0x00ff) AM_WRITE("dma", tms9911_device, write) // r12 = 1c0-1fe
	//AM_RANGE(0x001c, 0x001f) AM_READ("dma", tms9911_device, read) // if reading is needed
	//AM_RANGE(0x0400, 0x0407) AM_WRITE(cent_data_w) // r12 = 800-80e
	//AM_RANGE(0x0408, 0x0408) AM_WRITE(cent_strobe_w) // r12 = 810
	//AM_RANGE(0x0081, 0x0081) AM_READ(cent_stat_r) // CRU 409 (r12 = 812)
ADDRESS_MAP_END

/* Input ports */
static INPUT_PORTS_START( cortex )
	PORT_START("DSW")
	PORT_DIPNAME( 0x04, 0x00, "DISK SIZE")
	PORT_DIPSETTING(    0x04, "20cm")
	PORT_DIPSETTING(    0x00, "13cm")
	PORT_DIPNAME( 0x08, 0x08, "DISK DENSITY")
	PORT_DIPSETTING(    0x08, "Double")
	PORT_DIPSETTING(    0x00, "Single")
INPUT_PORTS_END

READ8_MEMBER( cortex_state::pio_r )
{
	return (m_kbd_ack ? 0x20 : 0) | (m_vdp_int ? 0x40 : 0) | m_io_dsw->read() | 0x93;
}

READ8_MEMBER( cortex_state::keyboard_r )
{
	return m_term_data;
}

WRITE_LINE_MEMBER( cortex_state::keyboard_ack_w )
{
	if (!state)
	{
		m_maincpu->set_input_line(INT_9995_INT4, CLEAR_LINE);
		m_kbd_ack = 1;
	}
}

WRITE_LINE_MEMBER( cortex_state::romsw_w )
{
	membank("bankr0")->set_entry(state ? 0 : 1);
}

WRITE_LINE_MEMBER( cortex_state::vdp_int_w )
{
	m_vdp_int = state ? 0 : 1;  // change polarity to match mame
}

void cortex_state::kbd_put(u8 data)
{
	m_term_data = data;
	m_kbd_ack = 0;
	m_maincpu->set_input_line(INT_9995_INT4, ASSERT_LINE);
}

void cortex_state::machine_reset()
{
	m_kbd_ack = 1;
	m_vdp_int = 0;
	m_beep->set_state(0);
	membank("bankr0")->set_entry(1); // point at rom
	membank("bankw0")->set_entry(0); // always write to ram
	m_maincpu->ready_line(ASSERT_LINE);
	m_maincpu->reset_line(ASSERT_LINE);
}

DRIVER_INIT_MEMBER( cortex_state, init )
{
	uint8_t *main = memregion("maincpu")->base();

	membank("bankr0")->configure_entry(1, &main[0x10000]);
	membank("bankr0")->configure_entry(0, &main[0x00000]);
	membank("bankw0")->configure_entry(0, &main[0x00000]);
}

MACHINE_CONFIG_START(cortex_state::cortex)
	/* basic machine hardware */
	/* TMS9995 CPU @ 12.0 MHz */
	// Standard variant, no overflow int
	// No lines connected yet
	MCFG_TMS99xx_ADD("maincpu", TMS9995, XTAL(12'000'000), mem_map, io_map)

	MCFG_DEVICE_ADD("control", LS259, 0) // IC64
	//MCFG_ADDRESSABLE_LATCH_Q0_OUT_CB(WRITELINE(cortex_state, basic_led_w))
	MCFG_ADDRESSABLE_LATCH_Q1_OUT_CB(WRITELINE(cortex_state, keyboard_ack_w))
	//MCFG_ADDRESSABLE_LATCH_Q2_OUT_CB(WRITELINE(cortex_state, ebus_int_ack_w))
	//MCFG_ADDRESSABLE_LATCH_Q3_OUT_CB(WRITELINE(cortex_state, ebus_to_en_w))
	//MCFG_ADDRESSABLE_LATCH_Q4_OUT_CB(WRITELINE(cortex_state, disk_size_w))
	MCFG_ADDRESSABLE_LATCH_Q5_OUT_CB(WRITELINE(cortex_state, romsw_w))
	MCFG_ADDRESSABLE_LATCH_Q6_OUT_CB(DEVWRITELINE("beeper", beep_device, set_state))

	/* video hardware */
	MCFG_DEVICE_ADD( "crtc", TMS9929A, XTAL(10'738'635) / 2 )
	MCFG_TMS9928A_OUT_INT_LINE_CB(INPUTLINE("maincpu", INT_9995_INT1))
	MCFG_DEVCB_CHAIN_OUTPUT(WRITELINE(cortex_state, vdp_int_w))
	MCFG_TMS9928A_VRAM_SIZE(0x4000)
	MCFG_TMS9928A_SCREEN_ADD_PAL( "screen" )
	MCFG_SCREEN_UPDATE_DEVICE( "crtc", tms9928a_device, screen_update )

	MCFG_DEVICE_ADD("keyboard", GENERIC_KEYBOARD, 0)
	MCFG_GENERIC_KEYBOARD_CB(PUT(cortex_state, kbd_put))

	//MCFG_DEVICE_ADD("uart1", TMS9902, XTAL(12'000'000) / 4)
	//MCFG_DEVICE_ADD("uart2", TMS9902, XTAL(12'000'000) / 4)

	/* Sound */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("beeper", BEEP, 950) // guess
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.05)
MACHINE_CONFIG_END

/* ROM definition */
ROM_START( cortex )
	ROM_REGION( 0x18000, "maincpu", ROMREGION_ERASEFF )
	ROM_SYSTEM_BIOS(0, "basic", "Cortex Bios")
	ROMX_LOAD( "cortex.ic47", 0x10000, 0x2000, CRC(bdb8c7bd) SHA1(340829dcb7a65f2e830fd5aff82a312e3ed7918f), ROM_BIOS(1))
	ROMX_LOAD( "cortex.ic46", 0x12000, 0x2000, CRC(4de459ea) SHA1(00a42fe556d4ffe1f85b2ce369f544b07fbd06d9), ROM_BIOS(1))
	ROMX_LOAD( "cortex.ic45", 0x14000, 0x2000, CRC(b0c9b6e8) SHA1(4e20c3f0b7546b803da4805cd3b8616f96c3d923), ROM_BIOS(1))
	ROM_SYSTEM_BIOS(1, "forth", "FIG-Forth")
	ROMX_LOAD( "forth.ic47",  0x10000, 0x2000, CRC(999034be) SHA1(0dcc7404c38aa0ae913101eb0aa98da82104b5d4), ROM_BIOS(2))
	ROMX_LOAD( "forth.ic46",  0x12000, 0x2000, CRC(8eca54cc) SHA1(0f1680e941ef60bb9bde9a4b843b78f30dff3202), ROM_BIOS(2))
ROM_END

/* Driver */

//    YEAR  NAME    PARENT  COMPAT   MACHINE    INPUT   STATE         INIT  COMPANY                  FULLNAME  FLAGS
COMP( 1982, cortex, 0,      0,       cortex,    cortex, cortex_state, init, "Powertran Cybernetics", "Cortex", MACHINE_NOT_WORKING )
