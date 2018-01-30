// license:BSD-3-Clause
// copyright-holders:Nigel Barnes,Thomas Busse
/**********************************************************************

    Spectrum Currah MicroSpeech emulation

**********************************************************************/

#include "emu.h"
#include "uspeech.h"
#include "speaker.h"



//**************************************************************************
//  MACROS/CONSTANTS
//**************************************************************************

#define SP0256_TAG      "sp0256"


//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(SPECTRUM_USPEECH, spectrum_uspeech_device, "spectrum_uspeech", "Spectrum Currah \xC2\xB5Speech")


//-------------------------------------------------
//  ROM( uspeech )
//-------------------------------------------------

ROM_START( uspeech )
	ROM_REGION(0x0800, "rom", 0)
	ROM_LOAD("currah.rom", 0x0000, 0x0800, CRC(ce7cf52e) SHA1(90dbba5afbf07949df9cbdcb0a8ec0b106340422))

	ROM_REGION(0x10000, SP0256_TAG, 0)
	ROM_LOAD( "sp0256a-al2.bin", 0x1000, 0x0800, CRC(b504ac15) SHA1(e60fcb5fa16ff3f3b69d36c7a6e955744d3feafc) )
ROM_END


//-------------------------------------------------
//  rom_region - device-specific ROM region
//-------------------------------------------------

const tiny_rom_entry *spectrum_uspeech_device::device_rom_region() const
{
	return ROM_NAME( uspeech );
}


//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------

MACHINE_CONFIG_START(spectrum_uspeech_device::device_add_mconfig)
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD(SP0256_TAG, SP0256, XTAL(14'000'000) / 4)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.0)
MACHINE_CONFIG_END


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  spectrum_uspeech_device - constructor
//-------------------------------------------------

spectrum_uspeech_device::spectrum_uspeech_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, SPECTRUM_USPEECH, tag, owner, clock),
	device_spectrum_expansion_interface(mconfig, *this),
	m_nsp(*this, SP0256_TAG),
	m_rom(*this, "rom")
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void spectrum_uspeech_device::device_start()
{
	m_slot = dynamic_cast<spectrum_expansion_slot_device *>(owner());
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void spectrum_uspeech_device::device_reset()
{
	m_romcs = 0;
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

READ_LINE_MEMBER(spectrum_uspeech_device::romcs)
{
	return m_romcs;
}


READ8_MEMBER(spectrum_uspeech_device::mreq_r)
{
	uint8_t data;

	if (!machine().side_effect_disabled() && (offset == 0x38))
	{
		m_romcs = !m_romcs;
	}

	switch (offset)
	{
	case 0x1000:
		data = !m_nsp->lrq_r(); // (m_nsp->lrq_r() && (m_nsp->sby_r() != 0)) ? 0x00 : 0x01;
		break;
	default:
		data = m_rom->base()[offset & 0x7ff];
		break;
	}

	return data;
}

WRITE8_MEMBER(spectrum_uspeech_device::mreq_w)
{
	switch (offset)
	{
	case 0x1000:
		// allophone
		m_nsp->ald_w(space, 0, data & 0x3f);
		break;

	case 0x3000:
		// intonation low
		m_nsp->set_clock(3500000); // CK / 4 ??
		break;

	case 0x3001:
		// intonation high
		m_nsp->set_clock(3800000); // TODO: the exact frequency is unknown
		break;
	}
}
