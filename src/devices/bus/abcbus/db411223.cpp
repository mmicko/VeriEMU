// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    Databoard 4112-23 floppy disk controller emulation

*********************************************************************/

#include "emu.h"
#include "db411223.h"



//**************************************************************************
//  MACROS / CONSTANTS
//**************************************************************************

#define Z80_TAG "maincpu"


//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(DATABOARD_4112_23, databoard_4112_23_t, "db411223", "Databoard 4112-23")


//-------------------------------------------------
//  ROM( databoard_4112_23 )
//-------------------------------------------------

ROM_START( databoard_4112_23 )
	ROM_REGION( 0x2000, Z80_TAG, 0 )
	ROM_LOAD("fpy_int_4112-23_teac_fd55.bin", 0x0000, 0x2000, CRC(9175ceb8) SHA1(95c150d3152df318abd9267915d5669d2ec33895))
ROM_END


//-------------------------------------------------
//  rom_region - device-specific ROM region
//-------------------------------------------------

const tiny_rom_entry *databoard_4112_23_t::device_rom_region() const
{
	return ROM_NAME( databoard_4112_23 );
}


//-------------------------------------------------
//  ADDRESS_MAP( databoard_4112_23_mem )
//-------------------------------------------------

static ADDRESS_MAP_START( databoard_4112_23_mem, AS_PROGRAM, 8, databoard_4112_23_t )
	AM_RANGE(0x0000, 0x1fff) AM_ROM AM_REGION(Z80_TAG, 0)
ADDRESS_MAP_END


//-------------------------------------------------
//  ADDRESS_MAP( databoard_4112_23_io )
//-------------------------------------------------

static ADDRESS_MAP_START( databoard_4112_23_io, AS_IO, 8, databoard_4112_23_t )
ADDRESS_MAP_END


//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------

MACHINE_CONFIG_START(databoard_4112_23_t::device_add_mconfig)
	MCFG_CPU_ADD(Z80_TAG, Z80, 4000000)
	MCFG_CPU_PROGRAM_MAP(databoard_4112_23_mem)
	MCFG_CPU_IO_MAP(databoard_4112_23_io)
MACHINE_CONFIG_END


//-------------------------------------------------
//  INPUT_PORTS( databoard_4112_23 )
//-------------------------------------------------

INPUT_PORTS_START( databoard_4112_23 )
INPUT_PORTS_END


//-------------------------------------------------
//  input_ports - device-specific input ports
//-------------------------------------------------

ioport_constructor databoard_4112_23_t::device_input_ports() const
{
	return INPUT_PORTS_NAME( databoard_4112_23 );
}



//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  databoard_4112_23_t - constructor
//-------------------------------------------------

databoard_4112_23_t::databoard_4112_23_t(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, DATABOARD_4112_23, tag, owner, clock),
	device_abcbus_card_interface(mconfig, *this),
	m_maincpu(*this, Z80_TAG)
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void databoard_4112_23_t::device_start()
{
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void databoard_4112_23_t::device_reset()
{
	m_cs = false;
}



//**************************************************************************
//  ABC BUS INTERFACE
//**************************************************************************

//-------------------------------------------------
//  abcbus_cs -
//-------------------------------------------------

void databoard_4112_23_t::abcbus_cs(uint8_t data)
{
}


//-------------------------------------------------
//  abcbus_stat -
//-------------------------------------------------

uint8_t databoard_4112_23_t::abcbus_stat()
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

uint8_t databoard_4112_23_t::abcbus_inp()
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

void databoard_4112_23_t::abcbus_out(uint8_t data)
{
	if (!m_cs) return;
}


//-------------------------------------------------
//  abcbus_c1 -
//-------------------------------------------------

void databoard_4112_23_t::abcbus_c1(uint8_t data)
{
	if (m_cs)
	{
	}
}


//-------------------------------------------------
//  abcbus_c3 -
//-------------------------------------------------

void databoard_4112_23_t::abcbus_c3(uint8_t data)
{
	if (m_cs)
	{
		m_maincpu->reset();
	}
}
