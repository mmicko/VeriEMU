// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
/**********************************************************************

    Slogger Pegasus 400 disk interface

**********************************************************************/


#include "emu.h"
#include "peg400.h"


//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(ELECTRON_PEG400, electron_peg400_device, "electron_peg400", "Slogger Pegasus 400 disk interface")


//-------------------------------------------------
//  MACHINE_DRIVER( peg400 )
//-------------------------------------------------

FLOPPY_FORMATS_MEMBER(electron_peg400_device::floppy_formats)
	FLOPPY_ACORN_SSD_FORMAT,
	FLOPPY_ACORN_DSD_FORMAT,
	FLOPPY_ACORN_ADFS_OLD_FORMAT
FLOPPY_FORMATS_END0

SLOT_INTERFACE_START(peg400_floppies)
	SLOT_INTERFACE("35dd",  FLOPPY_35_DD)
	SLOT_INTERFACE("525qd", FLOPPY_525_QD)
SLOT_INTERFACE_END

//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------

MACHINE_CONFIG_START(electron_peg400_device::device_add_mconfig)
	/* fdc */
	MCFG_WD1770_ADD("fdc", 16_MHz_XTAL / 2)
	MCFG_WD_FDC_DRQ_CALLBACK(WRITELINE(electron_peg400_device, fdc_drq_w))
	MCFG_FLOPPY_DRIVE_ADD("fdc:0", peg400_floppies, "525qd", electron_peg400_device::floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
	MCFG_FLOPPY_DRIVE_ADD("fdc:1", peg400_floppies, nullptr, electron_peg400_device::floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
MACHINE_CONFIG_END

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  electron_peg400_device - constructor
//-------------------------------------------------

electron_peg400_device::electron_peg400_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, ELECTRON_PEG400, tag, owner, clock)
	, device_electron_cart_interface(mconfig, *this)
	, m_fdc(*this, "fdc")
	, m_floppy0(*this, "fdc:0")
	, m_floppy1(*this, "fdc:1")
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void electron_peg400_device::device_start()
{
}

//-------------------------------------------------
//  read - cartridge data read
//-------------------------------------------------

uint8_t electron_peg400_device::read(address_space &space, offs_t offset, int infc, int infd, int romqa)
{
	uint8_t data = 0xff;

	if (infc)
	{
		switch (offset & 0xff)
		{
		case 0xc0:
			data = m_drive_control;
			break;
		case 0xc4:
		case 0xc5:
		case 0xc6:
		case 0xc7:
			data = m_fdc->read(space, offset & 0x03);
			break;
		}
	}

	if (!infc && !infd)
	{
		if (offset >= 0x0000 && offset < 0x4000)
		{
			data = m_rom[(offset & 0x3fff) + (romqa * 0x4000)];
		}

		if (romqa == 0 && offset >= 0x3800 && offset < 0x4000)
		{
			data = m_ram[offset & 0x07ff];
		}
	}

	return data;
}

//-------------------------------------------------
//  write - cartridge data write
//-------------------------------------------------

void electron_peg400_device::write(address_space &space, offs_t offset, uint8_t data, int infc, int infd, int romqa)
{
	if (infc)
	{
		switch (offset & 0xff)
		{
		case 0xc0:
			wd1770_control_w(space, 0, data);
			break;
		case 0xc4:
		case 0xc5:
		case 0xc6:
		case 0xc7:
			m_fdc->write(space, offset & 0x03, data);
			break;
		}
	}

	if (!infc && !infd)
	{
		if (offset >= 0x3800 && offset < 0x4000)
		{
			m_ram[offset & 0x07ff] = data;
		}
	}
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

WRITE8_MEMBER(electron_peg400_device::wd1770_control_w)
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

	// bit 4: send DRQ to NMI signal
	m_fdc_ie = BIT(data, 4);

	// bit 5: head load
}

WRITE_LINE_MEMBER(electron_peg400_device::fdc_drq_w)
{
	m_slot->nmi_w((state && m_fdc_ie) ? ASSERT_LINE : CLEAR_LINE);
}
