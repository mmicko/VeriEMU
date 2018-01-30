// license:BSD-3-Clause
// copyright-holders:AJR
/***********************************************************************************************************************************

2017-11-02 Skeleton

Transitional Technology Inc. SCSI host adapter, possibly part of the QTx (Q-Bus) series or UTx (Unibus) series.
Model number not known, zipfile was named "TTI_10012000.zip"

Chips: NCR 53C90A, Motorola MC68901P, Fujitsu 8464A-10L (8KB static ram), Xicor X24C44P (16x16 serial NOVRAM), and 14 undumped
Lattice PLDs.

Other: LED, 20MHz crystal. Next to the MC68901P is another chip just as large (48 pin DIL), with a huge "MFG. UNDER LICENSE FROM
       DIGITAL EQUIPMENT CORP." sticker covering all details. Assumed to be a Motorola MC68008 CPU.

************************************************************************************************************************************/

#include "emu.h"
#include "bus/rs232/rs232.h"
#include "cpu/m68000/m68000.h"
#include "machine/74259.h"
#include "machine/eepromser.h"
#include "machine/mc68901.h"

class tti_state : public driver_device
{
public:
	tti_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_mfp(*this, "mfp")
	{ }

	IRQ_CALLBACK_MEMBER(intack);

	void tti(machine_config &config);
protected:
	required_device<cpu_device> m_maincpu;
	required_device<mc68901_device> m_mfp;
};

IRQ_CALLBACK_MEMBER(tti_state::intack)
{
	return m_mfp->get_vector();
}

static INPUT_PORTS_START( tti )
INPUT_PORTS_END

static ADDRESS_MAP_START( prg_map, AS_PROGRAM, 8, tti_state )
	AM_RANGE(0x00000, 0x07fff) AM_ROM AM_REGION("maincpu", 0)
	AM_RANGE(0x7e000, 0x7ffff) AM_RAM
	AM_RANGE(0x80000, 0x80017) AM_DEVREADWRITE("mfp", mc68901_device, read, write)
	AM_RANGE(0x80070, 0x80077) AM_DEVWRITE("bitlatch", ls259_device, write_d0)
ADDRESS_MAP_END

MACHINE_CONFIG_START(tti_state::tti)
	MCFG_DEVICE_ADD("maincpu", M68008, XTAL(20'000'000) / 2) // guess
	MCFG_CPU_PROGRAM_MAP(prg_map)
	MCFG_CPU_IRQ_ACKNOWLEDGE_DRIVER(tti_state, intack)

	MCFG_DEVICE_ADD("mfp", MC68901, 0)
	MCFG_MC68901_TIMER_CLOCK(XTAL(20'000'000) / 2) // guess
	MCFG_MC68901_RX_CLOCK(9600) // for testing (FIXME: actually 16x)
	MCFG_MC68901_TX_CLOCK(9600) // for testing (FIXME: actually 16x)
	MCFG_MC68901_OUT_SO_CB(DEVWRITELINE("rs232", rs232_port_device, write_txd))
	MCFG_MC68901_OUT_IRQ_CB(INPUTLINE("maincpu", M68K_IRQ_5))

	MCFG_RS232_PORT_ADD("rs232", default_rs232_devices, "terminal")
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("mfp", mc68901_device, write_rx))

	MCFG_EEPROM_SERIAL_X24C44_ADD("novram")
	MCFG_EEPROM_SERIAL_DO_CALLBACK(DEVWRITELINE("mfp", mc68901_device, i0_w))

	MCFG_DEVICE_ADD("bitlatch", LS259, 0) // U17
	MCFG_ADDRESSABLE_LATCH_Q0_OUT_CB(DEVWRITELINE("novram", eeprom_serial_x24c44_device, di_write))
	MCFG_ADDRESSABLE_LATCH_Q1_OUT_CB(DEVWRITELINE("novram", eeprom_serial_x24c44_device, clk_write))
	MCFG_ADDRESSABLE_LATCH_Q2_OUT_CB(DEVWRITELINE("novram", eeprom_serial_x24c44_device, cs_write))
MACHINE_CONFIG_END

ROM_START( tti )
	ROM_REGION( 0x8000, "maincpu", 0 )
	ROM_LOAD( "tti_10012000_rev2.3.bin", 0x0000, 0x8000, CRC(95a5bce8) SHA1(46d7c99e37ca5598aec2062dfd9759853a237c14) )
	ROM_LOAD( "tti_10012000_rev1.7.bin", 0x0000, 0x8000, CRC(6660c059) SHA1(05d97009b5b8034dda520f655c73c474da97f822) )
ROM_END

COMP( 1989, tti, 0, 0, tti, tti, tti_state, 0, "Transitional Technology Inc", "unknown TTI SCSI host adapter", MACHINE_IS_SKELETON )
