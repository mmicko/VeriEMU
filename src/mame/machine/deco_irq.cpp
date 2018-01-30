// license: BSD-3-Clause
// copyright-holders: Dirk Best
/***************************************************************************

    Data East IRQ Controller

    TODO:
    - Lightgun support is only used by Locked 'n Loaded and works badly

***************************************************************************/

#include "emu.h"
#include "deco_irq.h"


//**************************************************************************
//  DEVICE DEFINITIONS
//**************************************************************************

DEFINE_DEVICE_TYPE(DECO_IRQ, deco_irq_device, "deco_irq", "Data East IRQ Controller")


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  deco_irq_device - constructor
//-------------------------------------------------

deco_irq_device::deco_irq_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock) :
	device_t(mconfig, DECO_IRQ, tag, owner, clock),
	m_screen(*this, finder_base::DUMMY_TAG),
	m_scanline_timer(nullptr),
	m_lightgun1_cb(*this), m_lightgun2_cb(*this),
	m_lightgun_irq_cb(*this),
	m_raster1_irq_cb(*this), m_raster2_irq_cb(*this),
	m_vblank_irq_cb(*this),
	m_lightgun_irq(false), m_raster_irq(false), m_vblank_irq(false),
	m_raster_irq_target(RASTER1_IRQ), m_raster_irq_masked(true),
	m_raster_irq_scanline(0),
	m_lightgun_latch(0)
{
}

//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void deco_irq_device::device_start()
{
	// make sure our screen is started
	if (!m_screen->started())
		throw device_missing_dependencies();

	// resolve callbacks
	m_lightgun1_cb.resolve_safe(0);
	m_lightgun2_cb.resolve_safe(0);
	m_lightgun_irq_cb.resolve_safe();
	m_raster1_irq_cb.resolve_safe();
	m_raster2_irq_cb.resolve_safe();
	m_vblank_irq_cb.resolve_safe();

	// allocate scanline timer and start it
	m_scanline_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(deco_irq_device::scanline_callback), this));
	m_scanline_timer->adjust(m_screen->time_until_pos(0));

	// register for save states
	save_item(NAME(m_lightgun_irq));
	save_item(NAME(m_raster_irq));
	save_item(NAME(m_vblank_irq));
	save_item(NAME(m_raster_irq_target));
	save_item(NAME(m_raster_irq_masked));
	save_item(NAME(m_raster_irq_scanline));
	save_item(NAME(m_lightgun_latch));
}

//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void deco_irq_device::device_reset()
{
}

//-------------------------------------------------
//  set_screen_tag - set screen we are attached to
//-------------------------------------------------

void deco_irq_device::set_screen_tag(device_t &device, const char *tag)
{
	deco_irq_device &dev = dynamic_cast<deco_irq_device &>(device);
	dev.m_screen.set_tag(tag);
}

//-------------------------------------------------
//  scanline_callback - called on each scanline
//-------------------------------------------------

TIMER_CALLBACK_MEMBER( deco_irq_device::scanline_callback )
{
	const rectangle visible = m_screen->visible_area();
	uint8_t y = m_screen->vpos();

	// raster irq?
	if (m_raster_irq_scanline > 0 && m_raster_irq_scanline < 240 && y == (m_raster_irq_scanline - 1))
	{
		if (!m_raster_irq_masked)
		{
			m_raster_irq = true;

			switch (m_raster_irq_target)
			{
			case RASTER1_IRQ: m_raster1_irq_cb(ASSERT_LINE); break;
			case RASTER2_IRQ: m_raster2_irq_cb(ASSERT_LINE); break;
			}
		}
	}

	// lightgun?
	if (m_lightgun_latch >= visible.min_y && m_lightgun_latch <= visible.max_y && y == m_lightgun_latch)
	{
		m_lightgun_irq = true;
		m_lightgun_irq_cb(ASSERT_LINE);
	}

	// vblank-in?
	if (y == (visible.max_y + 1))
	{
		m_vblank_irq = true;
		m_vblank_irq_cb(ASSERT_LINE);
	}

	// wait for next line
	m_scanline_timer->adjust(m_screen->time_until_pos(y + 1));
}


//**************************************************************************
//  INTERFACE
//**************************************************************************

DEVICE_ADDRESS_MAP_START(map, 8, deco_irq_device)
	AM_RANGE(0x0, 0x0) AM_WRITE(control_w)
	AM_RANGE(0x1, 0x1) AM_READWRITE(scanline_r, scanline_w)
	AM_RANGE(0x2, 0x2) AM_READWRITE(raster_irq_ack_r, vblank_irq_ack_w)
	AM_RANGE(0x3, 0x3) AM_READ(status_r)
ADDRESS_MAP_END

WRITE8_MEMBER( deco_irq_device::control_w )
{
	// 765-----  unused?
	// ---4----  raster irq target
	// ----3---  unused?
	// -----2--  unknown
	// ------1-  raster irq mask
	// -------0  unused?

	m_raster_irq_target = BIT(data, 4);
	m_raster_irq_masked = bool(BIT(data, 1));

	if (m_raster_irq_masked)
		raster_irq_ack_r(space, 0);
}

READ8_MEMBER( deco_irq_device::scanline_r )
{
	return m_raster_irq_scanline;
}

WRITE8_MEMBER( deco_irq_device::scanline_w )
{
	m_raster_irq_scanline = data;
}

READ8_MEMBER( deco_irq_device::raster_irq_ack_r )
{
	m_raster_irq = false;
	m_raster1_irq_cb(CLEAR_LINE);
	m_raster2_irq_cb(CLEAR_LINE);

	return 0xff;
}

WRITE8_MEMBER( deco_irq_device::vblank_irq_ack_w )
{
	m_vblank_irq = false;
	m_vblank_irq_cb(CLEAR_LINE);
}

READ8_MEMBER( deco_irq_device::status_r )
{
	uint8_t data = 0;

	// 7-------  unknown
	// -6------  lightgun irq
	// --5-----  raster irq
	// ---4----  vblank irq
	// ----32--  unknown
	// ------1-  vblank
	// -------0  hblank?

	data |= 1 << 7;
	data |= (m_lightgun_irq ? 1 : 0) << 6;
	data |= (m_raster_irq ? 1 : 0) << 5;
	data |= (m_vblank_irq ? 1 : 0) << 4;
	data |= 0 << 3;
	data |= 0 << 2;
	data |= m_screen->vblank() << 1;
//  data |= (m_screen->hblank() & m_screen->vblank()) << 0;
	data |= m_screen->hblank() << 0;

	return data;
}

WRITE_LINE_MEMBER( deco_irq_device::lightgun1_trigger_w )
{
	if (state)
		m_lightgun_latch = m_lightgun1_cb() / 2;
}

WRITE_LINE_MEMBER( deco_irq_device::lightgun2_trigger_w )
{
	if (state)
		m_lightgun_latch = m_lightgun2_cb() / 2;
}

WRITE_LINE_MEMBER( deco_irq_device::lightgun_irq_ack_w )
{
	m_lightgun_irq = false;
	m_lightgun_irq_cb(CLEAR_LINE);
}
