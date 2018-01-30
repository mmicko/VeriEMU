// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
/**********************************************************************

    Currah MicroSlot

**********************************************************************/

#include "emu.h"
#include "uslot.h"

//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(SPECTRUM_USLOT, spectrum_uslot_device, "spectrum_uslot", "Spectrum Currah \xC2\xB5Slot")


//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------

MACHINE_CONFIG_START(spectrum_uslot_device::device_add_mconfig)
	/* passthru */
	MCFG_SPECTRUM_EXPANSION_SLOT_ADD("exp1", spectrum_expansion_devices, nullptr)
	MCFG_SPECTRUM_EXPANSION_SLOT_IRQ_HANDLER(DEVWRITELINE(DEVICE_SELF_OWNER, spectrum_expansion_slot_device, irq_w))
	MCFG_SPECTRUM_EXPANSION_SLOT_NMI_HANDLER(DEVWRITELINE(DEVICE_SELF_OWNER, spectrum_expansion_slot_device, nmi_w))

	MCFG_SPECTRUM_EXPANSION_SLOT_ADD("exp2", spectrum_expansion_devices, nullptr)
	MCFG_SPECTRUM_EXPANSION_SLOT_IRQ_HANDLER(DEVWRITELINE(DEVICE_SELF_OWNER, spectrum_expansion_slot_device, irq_w))
	MCFG_SPECTRUM_EXPANSION_SLOT_NMI_HANDLER(DEVWRITELINE(DEVICE_SELF_OWNER, spectrum_expansion_slot_device, nmi_w))
MACHINE_CONFIG_END

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  spectrum_uslot_device - constructor
//-------------------------------------------------

spectrum_uslot_device::spectrum_uslot_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SPECTRUM_USLOT, tag, owner, clock)
	, device_spectrum_expansion_interface(mconfig, *this)
	, m_exp1(*this, "exp1")
	, m_exp2(*this, "exp2")
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void spectrum_uslot_device::device_start()
{
	m_slot = dynamic_cast<spectrum_expansion_slot_device *>(owner());
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void spectrum_uslot_device::device_reset()
{
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

READ_LINE_MEMBER(spectrum_uslot_device::romcs)
{
	return m_exp1->romcs() | m_exp2->romcs();
}


READ8_MEMBER(spectrum_uslot_device::mreq_r)
{
	uint8_t temp;
	uint8_t data = 0xff;

	temp = m_exp1->mreq_r(space, offset);
	if (m_exp1->romcs())
		data &= temp;

	temp = m_exp2->mreq_r(space, offset);
	if (m_exp2->romcs())
		data &= temp;

	return data;
}

WRITE8_MEMBER(spectrum_uslot_device::mreq_w)
{
	if (m_exp1->romcs())
		m_exp1->mreq_w(space, offset, data);

	if (m_exp2->romcs())
		m_exp2->mreq_w(space, offset, data);
}

READ8_MEMBER(spectrum_uslot_device::port_fe_r)
{
	uint8_t data = 0xff;

	if (m_exp1->romcs())
		data &= m_exp1->port_fe_r(space, offset);

	if (m_exp2->romcs())
		data &= m_exp2->port_fe_r(space, offset);

	return data;
}
