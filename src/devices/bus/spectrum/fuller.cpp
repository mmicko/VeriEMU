// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
/**********************************************************************

    Fuller Box Games Unit

**********************************************************************/

#include "emu.h"
#include "fuller.h"
#include "speaker.h"

//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(SPECTRUM_FULLER, spectrum_fuller_device, "spectrum_fuller", "Fuller Box")


//-------------------------------------------------
//  INPUT_PORTS( fuller )
//-------------------------------------------------

static INPUT_PORTS_START( fuller )
	PORT_START("JOY")
	PORT_BIT(0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP)    PORT_8WAY
	PORT_BIT(0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN)  PORT_8WAY
	PORT_BIT(0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT)  PORT_8WAY
	PORT_BIT(0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT) PORT_8WAY
	PORT_BIT(0x80, IP_ACTIVE_LOW, IPT_BUTTON1)
INPUT_PORTS_END


//-------------------------------------------------
//  input_ports - device-specific input ports
//-------------------------------------------------

ioport_constructor spectrum_fuller_device::device_input_ports() const
{
	return INPUT_PORTS_NAME( fuller );
}

//-------------------------------------------------
//  device_add_mconfig - add device configuration
//-------------------------------------------------

MACHINE_CONFIG_START(spectrum_fuller_device::device_add_mconfig)
	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("ay8912", AY8912, XTAL(3'579'545) / 2) // unverified clock
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)

	/* passthru */
	MCFG_SPECTRUM_PASSTHRU_EXPANSION_SLOT_ADD()
MACHINE_CONFIG_END

//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  spectrum_fuller_device - constructor
//-------------------------------------------------

spectrum_fuller_device::spectrum_fuller_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, SPECTRUM_FULLER, tag, owner, clock)
	, device_spectrum_expansion_interface(mconfig, *this)
	, m_exp(*this, "exp")
	, m_psg(*this, "ay8912")
	, m_joy(*this, "JOY")
{
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void spectrum_fuller_device::device_start()
{
	address_space& spaceio = machine().device("maincpu")->memory().space(AS_IO);
	m_slot = dynamic_cast<spectrum_expansion_slot_device *>(owner());

	spaceio.install_write_handler(0x3f, 0x3f, 0, 0xff00, 0, write8_delegate(FUNC(ay8910_device::address_w), (ay8910_device*)m_psg));
	spaceio.install_readwrite_handler(0x5f, 0x5f, 0, 0xff00, 0, read8_delegate(FUNC(ay8910_device::data_r), (ay8910_device*)m_psg), write8_delegate(FUNC(ay8910_device::data_w), (ay8910_device*)m_psg));
	spaceio.install_read_handler(0x7f, 0x7f, 0, 0xff00, 0, read8_delegate(FUNC(spectrum_fuller_device::joystick_r), this));
}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void spectrum_fuller_device::device_reset()
{
}


//**************************************************************************
//  IMPLEMENTATION
//**************************************************************************

READ8_MEMBER(spectrum_fuller_device::joystick_r)
{
	return m_joy->read() | (0xff ^ 0x8f);
}

READ_LINE_MEMBER(spectrum_fuller_device::romcs)
{
	return m_exp->romcs();
}

READ8_MEMBER(spectrum_fuller_device::mreq_r)
{
	return m_exp->mreq_r(space, offset);
}

WRITE8_MEMBER(spectrum_fuller_device::mreq_w)
{
	if (m_exp->romcs())
		m_exp->mreq_w(space, offset, data);
}

READ8_MEMBER(spectrum_fuller_device::port_fe_r)
{
	uint8_t data = 0xff;

	if (m_exp->romcs())
		data &= m_exp->port_fe_r(space, offset);

	return data;
}
