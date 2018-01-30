// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
/**********************************************************************

    ALA13 - Acorn Plus 3

    http://chrisacorns.computinghistory.org.uk/8bit_Upgrades/Acorn_Plus3.html

    The Acorn Plus 3 was a hardware module that connected independently
    of the Plus 1 and provided a double-density 3.5" disc drive connected
    through a WD1770 drive controller and an ADFS ROM. There were two
    versions of the Plus 3 produced: A Single-sided and a Double-sided
    drive version.

**********************************************************************/


#include "emu.h"
#include "plus3.h"


//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(ELECTRON_PLUS3, electron_plus3_device, "electron_plus3", "Acorn Plus 3 Disc Expansion")


//-------------------------------------------------
//  MACHINE_DRIVER( plus3 )
//-------------------------------------------------

FLOPPY_FORMATS_MEMBER(electron_plus3_device::floppy_formats)
	FLOPPY_ACORN_SSD_FORMAT,
	FLOPPY_ACORN_DSD_FORMAT,
	FLOPPY_ACORN_ADFS_OLD_FORMAT
FLOPPY_FORMATS_END0

SLOT_INTERFACE_START(electron_floppies)
	SLOT_INTERFACE("35dd",    FLOPPY_35_DD)
	SLOT_INTERFACE("525qd",   FLOPPY_525_QD)
SLOT_INTERFACE_END


ROM_START( plus3 )
	// Bank 4 Disc
	ROM_REGION(0x4000, "exp_rom", 0)
	ROM_DEFAULT_BIOS("adfs100")
	// ADFS
	ROM_SYSTEM_BIOS(0, "adfs100", "Acorn ADFS 1.00")
	ROMX_LOAD("adfs.rom", 0x0000, 0x4000, CRC(3289bdc6) SHA1(e7c7a1094d50a3579751df2007269067c8ff6812), ROM_BIOS(1))
	ROM_SYSTEM_BIOS(1, "adfs113", "PRES ADFS 1.13")
	ROMX_LOAD("pres_adfs_113.rom", 0x0000, 0x4000, CRC(f06ca04a) SHA1(3c8221d63457c552aa2c9a502db632ce1dea66b4), ROM_BIOS(2))
	ROM_SYSTEM_BIOS(2, "adfs115", "PRES ADFS 1.15")
	ROMX_LOAD("pres_adfs_115.rom", 0x0000, 0x4000, CRC(8f81edc3) SHA1(32007425058a7b0f8bd5c17b3c22552aa3a03eca), ROM_BIOS(3))
	// DFS
	ROM_SYSTEM_BIOS(3, "dfs200", "Advanced 1770 DFS 2.00")
	ROMX_LOAD("acp_dfs1770_200.rom", 0x0000, 0x4000, CRC(5a3a13c7) SHA1(d5dad7ab5a0237c44d0426cd85a8ec86545747e0), ROM_BIOS(4))
	ROM_SYSTEM_BIOS(4, "dfs210", "Advanced 1770 DFS 2.10")
	ROMX_LOAD("acp_dfs1770_210.rom", 0x0000, 0x4000, CRC(b0661992) SHA1(c62f1290f689788dad6a2df30ace083eb827cffe), ROM_BIOS(5))
ROM_END

//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------

MACHINE_CONFIG_START(electron_plus3_device::device_add_mconfig)
	/* fdc */
	MCFG_WD1770_ADD("fdc", 16_MHz_XTAL / 2)
	MCFG_FLOPPY_DRIVE_ADD_FIXED("fdc:0", electron_floppies, "35dd", floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
	MCFG_FLOPPY_DRIVE_ADD("fdc:1", electron_floppies, nullptr, floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)


	/* pass-through */
	MCFG_ELECTRON_PASSTHRU_EXPANSION_SLOT_ADD(nullptr)
MACHINE_CONFIG_END

const tiny_rom_entry *electron_plus3_device::device_rom_region() const
{
	return ROM_NAME( plus3 );
}

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  electron_plus3_device - constructor
//-------------------------------------------------

electron_plus3_device::electron_plus3_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, ELECTRON_PLUS3, tag, owner, clock)
	, device_electron_expansion_interface(mconfig, *this)
	, m_exp(*this, "exp")
	, m_exp_rom(*this, "exp_rom")
	, m_fdc(*this, "fdc")
	, m_floppy0(*this, "fdc:0")
	, m_floppy1(*this, "fdc:1")
	, m_romsel(0)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void electron_plus3_device::device_start()
{
}

//-------------------------------------------------
//  expbus_r - expansion data read
//-------------------------------------------------

uint8_t electron_plus3_device::expbus_r(address_space &space, offs_t offset, uint8_t data)
{
	if (offset >= 0x8000 && offset < 0xc000)
	{
		if (m_romsel == 4)
		{
			data = m_exp_rom->base()[offset & 0x3fff];
		}
	}
	else if (offset == 0xfcc0)
	{
		data = 0xff;
	}
	else if (offset >= 0xfcc4 && offset < 0xfcc8)
	{
		data = m_fdc->read(space, offset & 0x03);
	}

	data &= m_exp->expbus_r(space, offset, data);

	return data;
}

//-------------------------------------------------
//  expbus_w - expansion data write
//-------------------------------------------------

void electron_plus3_device::expbus_w(address_space &space, offs_t offset, uint8_t data)
{
	m_exp->expbus_w(space, offset, data);

	if (offset == 0xfcc0)
	{
		wd1770_status_w(space, offset, data);
	}
	else if (offset >= 0xfcc4 && offset < 0xfcc8)
	{
		m_fdc->write(space, offset & 0x03, data);
	}
	else if (offset == 0xfe05)
	{
		m_romsel = data & 0x0f;
	}
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

WRITE8_MEMBER(electron_plus3_device::wd1770_status_w)
{
	floppy_image_device *floppy = nullptr;

	m_drive_control = data;

	// bit 0, 1: drive select
	if (BIT(data, 0)) floppy = m_floppy0->get_device();
	if (BIT(data, 1)) floppy = m_floppy1->get_device();
	m_fdc->set_floppy(floppy);

	// bit 2: side select
	if (floppy)
		floppy->ss_w(BIT(data, 2));

	// bit 3: density
	m_fdc->dden_w(BIT(data, 3));

	// bit 5: reset
	if (!BIT(data, 5)) m_fdc->soft_reset();
}
