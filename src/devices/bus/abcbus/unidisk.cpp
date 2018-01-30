// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    MyAB UNI DISK floppy disk controller emulation

*********************************************************************/

#include "emu.h"
#include "unidisk.h"



//**************************************************************************
//  MACROS / CONSTANTS
//**************************************************************************

#define TMS9995_TAG "maincpu"


//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(UNIDISK, unidisk_t, "unidisk", "MyAB UNI DISK")


//-------------------------------------------------
//  ROM( unidisk )
//-------------------------------------------------

ROM_START( unidisk )
	ROM_REGION( 0x1000, TMS9995_TAG, 0 )
	ROM_SYSTEM_BIOS( 0, "5d", "5\" D PROM" )
	ROMX_LOAD("unidisk5d.bin", 0x0000, 0x1000, CRC(569dd60c) SHA1(47b810bcb5a063ffb3034fd7138dc5e15d243676), ROM_BIOS(1))
	ROM_SYSTEM_BIOS( 1, "5h", "5\" H PROM" )
	ROMX_LOAD("unidisk5h.bin", 0x0000, 0x1000, CRC(5079ad85) SHA1(42bb91318f13929c3a440de3fa1f0491a0b90863), ROM_BIOS(2))
	ROM_SYSTEM_BIOS( 2, "8", "8\" PROM" )
	ROMX_LOAD("unidisk8.bin", 0x0000, 0x1000, CRC(d04e6a43) SHA1(8db504d46ff0355c72bd58fd536abeb17425c532), ROM_BIOS(3))
ROM_END


//-------------------------------------------------
//  rom_region - device-specific ROM region
//-------------------------------------------------

const tiny_rom_entry *unidisk_t::device_rom_region() const
{
	return ROM_NAME( unidisk );
}


//-------------------------------------------------
//  ADDRESS_MAP( unidisk_mem )
//-------------------------------------------------

static ADDRESS_MAP_START( unidisk_mem, AS_PROGRAM, 8, unidisk_t )
	AM_RANGE(0x0000, 0x0fff) AM_ROM AM_REGION(TMS9995_TAG, 0)
ADDRESS_MAP_END


//-------------------------------------------------
//  ADDRESS_MAP( unidisk_io )
//-------------------------------------------------

static ADDRESS_MAP_START( unidisk_io, AS_IO, 8, unidisk_t )
ADDRESS_MAP_END


//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------

MACHINE_CONFIG_START(unidisk_t::device_add_mconfig)
	MCFG_TMS99xx_ADD(TMS9995_TAG, TMS9995, 12000000, unidisk_mem, unidisk_io)
MACHINE_CONFIG_END


//-------------------------------------------------
//  INPUT_PORTS( unidisk )
//-------------------------------------------------

INPUT_PORTS_START( unidisk )
INPUT_PORTS_END


//-------------------------------------------------
//  input_ports - device-specific input ports
//-------------------------------------------------

ioport_constructor unidisk_t::device_input_ports() const
{
	return INPUT_PORTS_NAME( unidisk );
}



//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  unidisk_t - constructor
//-------------------------------------------------

unidisk_t::unidisk_t(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, UNIDISK, tag, owner, clock),
	device_abcbus_card_interface(mconfig, *this),
	m_maincpu(*this, TMS9995_TAG)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void unidisk_t::device_start()
{
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void unidisk_t::device_reset()
{
	m_cs = false;
}



//**************************************************************************
//  ABC BUS INTERFACE
//**************************************************************************

//-------------------------------------------------
//  abcbus_cs -
//-------------------------------------------------

void unidisk_t::abcbus_cs(uint8_t data)
{
}


//-------------------------------------------------
//  abcbus_stat -
//-------------------------------------------------

uint8_t unidisk_t::abcbus_stat()
{
	uint8_t data = 0xff;

	if (m_cs)
	{
	}

	return data;
}


//-------------------------------------------------
//  abcbus_inp -
//-------------------------------------------------

uint8_t unidisk_t::abcbus_inp()
{
	uint8_t data = 0xff;

	if (m_cs)
	{
	}

	return data;
}


//-------------------------------------------------
//  abcbus_out -
//-------------------------------------------------

void unidisk_t::abcbus_out(uint8_t data)
{
	if (!m_cs) return;
}


//-------------------------------------------------
//  abcbus_c1 -
//-------------------------------------------------

void unidisk_t::abcbus_c1(uint8_t data)
{
	if (m_cs)
	{
	}
}


//-------------------------------------------------
//  abcbus_c3 -
//-------------------------------------------------

void unidisk_t::abcbus_c3(uint8_t data)
{
	if (m_cs)
	{
		m_maincpu->reset();
	}
}
