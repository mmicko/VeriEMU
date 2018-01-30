// license:BSD-3-Clause
// copyright-holders:Bryan McPhail
/*************************************************************************

    deco_ace.h

    Data East 99 "ACE" Chip Emulation

    Original source (from deco32.cpp) by Bryan McPhail, Splited by cam900.

**************************************************************************/
#ifndef MAME_VIDEO_DECO_ACE_H
#define MAME_VIDEO_DECO_ACE_H

#pragma once


/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/


class deco_ace_device : public device_t,
						public device_video_interface
{
public:
	deco_ace_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// static configuration
	static void static_set_palette_tag(device_t &device, const char *tag);

	DECLARE_READ32_MEMBER( buffered_palette_r );
	DECLARE_READ16_MEMBER( buffered_palette16_r );
	DECLARE_READ16_MEMBER( ace_r );
	DECLARE_WRITE32_MEMBER( buffered_palette_w );
	DECLARE_WRITE16_MEMBER( buffered_palette16_w );
	DECLARE_WRITE16_MEMBER( ace_w );
	void palette_update();
	void set_palette_effect_max(uint32_t val);
	uint16_t get_aceram(uint8_t val);
	uint8_t get_alpha(uint8_t val);
	DECLARE_WRITE16_MEMBER( palette_dma_w );

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_post_load() override;

private:
	// internal state
	uint32_t m_palette_effect_min;
	uint32_t m_palette_effect_max;
	required_device<palette_device> m_palette;
	std::unique_ptr<uint32_t[]> m_paletteram;
	std::unique_ptr<uint32_t[]> m_paletteram_buffered;
	std::unique_ptr<uint16_t[]> m_ace_ram;
};

DECLARE_DEVICE_TYPE(DECO_ACE, deco_ace_device)



/***************************************************************************
    DEVICE CONFIGURATION MACROS
***************************************************************************/

#define MCFG_DECO_ACE_ADD(_tag) \
	MCFG_DEVICE_ADD(_tag, DECO_ACE, 0)

#define MCFG_DECO_ACE_PALETTE(_palette_tag) \
	deco_ace_device::static_set_palette_tag(*device, "^" _palette_tag);

#endif // MAME_VIDEO_DECO_ACE_H
