// license:BSD-3-Clause
// copyright-holders:
/***********************************************************************************************************************************

2017-11-20 Skeleton

Milwaukee Computer MC-1000 series (MC-1000/1100/1200/1300/1400) all the same except for disk options.

Chips: SY6502, 2x 6821, 2x MC6850P, 6852, INS8253
Other: 2x 7-position rotary "dips" to select baud rates on each 6850 (19.2K, 9600, 4800, 2400, 1200, 600, 300).

************************************************************************************************************************************/

#include "emu.h"
#include "cpu/m6502/m6502.h"
#include "machine/6821pia.h"
#include "machine/6850acia.h"
#include "machine/pit8253.h"
#include "machine/mc6852.h"
#include "machine/clock.h"
#include "bus/rs232/rs232.h"

class milwaukee_state : public driver_device
{
public:
	milwaukee_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		//, m_p_chargen(*this, "chargen")
	{ }

		void milwaukee(machine_config &config);
private:
	required_device<cpu_device> m_maincpu;
	//required_region_ptr<u8> m_p_chargen;
};

static ADDRESS_MAP_START( mem_map, AS_PROGRAM, 8, milwaukee_state )
	AM_RANGE(0x0000, 0xf7ff) AM_RAM
	//AM_RANGE(0xf800, 0xf87f) expansion i/o
	AM_RANGE(0xf880, 0xf881) AM_DEVREADWRITE("acia1", acia6850_device, read, write) // terminal
	AM_RANGE(0xf882, 0xf883) AM_DEVREADWRITE("acia2", acia6850_device, read, write) // remote
	AM_RANGE(0xf884, 0xf887) AM_DEVREADWRITE("pia1", pia6821_device, read, write) // centronics
	AM_RANGE(0xf888, 0xf88b) AM_DEVREADWRITE("pit", pit8253_device, read, write)
	AM_RANGE(0xf88c, 0xf88f) AM_DEVREADWRITE("pia2", pia6821_device, read, write) // disk controller
	AM_RANGE(0xf890, 0xf891) AM_DEVREADWRITE("ssda", mc6852_device, read, write)
	AM_RANGE(0xf898, 0xffff) AM_ROM AM_REGION("roms", 0x0098)
ADDRESS_MAP_END

static INPUT_PORTS_START( milwaukee )
INPUT_PORTS_END

MACHINE_CONFIG_START(milwaukee_state::milwaukee)
	MCFG_CPU_ADD("maincpu", M6502, XTAL(16'000'000) / 16)
	MCFG_CPU_PROGRAM_MAP(mem_map)

	MCFG_DEVICE_ADD("pit", PIT8253, 0)
	MCFG_PIT8253_CLK0(XTAL(16'000'000) / 16 / 4) // 250 kHz
	MCFG_PIT8253_OUT0_HANDLER(DEVWRITELINE("pit", pit8253_device, write_gate0)) MCFG_DEVCB_INVERT
	MCFG_PIT8253_CLK1(XTAL(16'000'000) / 2 / 13 / 2048 / 5) // 60.09 Hz?
	MCFG_PIT8253_OUT1_HANDLER(DEVWRITELINE("pit", pit8253_device, write_clk2)) MCFG_DEVCB_INVERT

	MCFG_DEVICE_ADD("pia1", PIA6821, 0)
	MCFG_DEVICE_ADD("pia2", PIA6821, 0)
	MCFG_DEVICE_ADD("acia2", ACIA6850, 0)
	MCFG_DEVICE_ADD("ssda", MC6852, 0)

	MCFG_DEVICE_ADD("acia_clock", CLOCK, XTAL(16'000'000) / 2 / 13 / 4)
	MCFG_CLOCK_SIGNAL_HANDLER(DEVWRITELINE("acia1", acia6850_device, write_txc))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("acia1", acia6850_device, write_rxc))

	MCFG_DEVICE_ADD("acia1", ACIA6850, 0)
	MCFG_ACIA6850_TXD_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_txd))
	MCFG_ACIA6850_RTS_HANDLER(DEVWRITELINE("rs232", rs232_port_device, write_rts))

	MCFG_RS232_PORT_ADD("rs232", default_rs232_devices, "terminal")
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("acia1", acia6850_device, write_rxd))
	MCFG_RS232_CTS_HANDLER(DEVWRITELINE("acia1", acia6850_device, write_cts))
MACHINE_CONFIG_END

ROM_START( mc1200 )
	ROM_REGION( 0x0c00, "roms", 0 )
	ROM_LOAD( "mfm_6-29-82_10_sector.u15", 0x0000, 0x0800, CRC(40b0af66) SHA1(c988e1f90c9abb93171c4e40a6585ce9cc3fd495) )
	ROM_LOAD( "2758.u14", 0x0800, 0x0400, CRC(b20e2345) SHA1(da498cc0c746897a85d6f2d1a5bd70a726c1e4ef) ) // big white sticker, but nothing on it
ROM_END

COMP( 1980, mc1200, 0, 0, milwaukee, milwaukee, milwaukee_state, 0, "Milwaukee Computers", "MC-1200", MACHINE_IS_SKELETON )
