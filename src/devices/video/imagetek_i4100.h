// license:BSD-3-Clause
// copyright-holders:Luca Elia,David Haywood,Angelo Salese
/***************************************************************************

    Imagetek I4100 / I4220 / I4300 device files

***************************************************************************/

#ifndef MAME_VIDEO_I4100_H
#define MAME_VIDEO_I4100_H

#pragma once

#include "screen.h"


//**************************************************************************
//  INTERFACE CONFIGURATION MACROS
//**************************************************************************

#define MCFG_I4100_GFXDECODE(gfxtag) \
	imagetek_i4100_device::static_set_gfxdecode_tag(*device, ("^" gfxtag));

#define MCFG_I4100_BLITTER_END_CALLBACK(_devcb) \
	devcb = &imagetek_i4100_device::static_set_blitter_irq_callback(*device, DEVCB_##_devcb);

#define MCFG_I4100_TILEMAP_XOFFSETS(_a, _b, _c) \
	imagetek_i4100_device::static_set_tmap_xoffsets(*device, _a, _b, _c);

#define MCFG_I4100_TILEMAP_YOFFSETS(_a, _b, _c) \
	imagetek_i4100_device::static_set_tmap_yoffsets(*device, _a, _b, _c);


//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> i4100_device

class imagetek_i4100_device : public device_t,
							  public device_video_interface
{
public:
	// construction/destruction
	imagetek_i4100_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	DECLARE_ADDRESS_MAP(map, 16);

	static void static_set_gfxdecode_tag(device_t &device, const char *tag);
	static void static_set_tmap_xoffsets(device_t &device, int x1, int x2, int x3);
	static void static_set_tmap_yoffsets(device_t &device, int y1, int y2, int y3);

	template <class Object> static devcb_base &static_set_blitter_irq_callback(device_t &device, Object &&cb) { return downcast<imagetek_i4100_device &>(device).m_blit_irq_cb.set_callback(std::forward<Object>(cb)); }


	// I/O operations
	DECLARE_READ16_MEMBER( vram_0_r );
	DECLARE_READ16_MEMBER( vram_1_r );
	DECLARE_READ16_MEMBER( vram_2_r );
	DECLARE_WRITE16_MEMBER( vram_0_w );
	DECLARE_WRITE16_MEMBER( vram_1_w );
	DECLARE_WRITE16_MEMBER( vram_2_w );
	DECLARE_READ16_MEMBER( rmw_vram_0_r );
	DECLARE_READ16_MEMBER( rmw_vram_1_r );
	DECLARE_READ16_MEMBER( rmw_vram_2_r );
	DECLARE_WRITE16_MEMBER( rmw_vram_0_w );
	DECLARE_WRITE16_MEMBER( rmw_vram_1_w );
	DECLARE_WRITE16_MEMBER( rmw_vram_2_w );
	DECLARE_READ16_MEMBER( scratchram_r );
	DECLARE_WRITE16_MEMBER( scratchram_w );
	DECLARE_READ16_MEMBER( spriteram_r );
	DECLARE_WRITE16_MEMBER( spriteram_w );
	DECLARE_READ16_MEMBER( tiletable_r );
	DECLARE_WRITE16_MEMBER( tiletable_w );
	DECLARE_READ16_MEMBER( sprite_count_r );
	DECLARE_WRITE16_MEMBER( sprite_count_w );
	DECLARE_READ16_MEMBER( sprite_priority_r );
	DECLARE_WRITE16_MEMBER( sprite_priority_w );
	DECLARE_READ16_MEMBER( sprite_xoffset_r );
	DECLARE_WRITE16_MEMBER( sprite_xoffset_w );
	DECLARE_READ16_MEMBER( sprite_yoffset_r );
	DECLARE_WRITE16_MEMBER( sprite_yoffset_w );
	DECLARE_READ16_MEMBER( sprite_color_code_r );
	DECLARE_WRITE16_MEMBER( sprite_color_code_w );
	DECLARE_READ16_MEMBER( layer_priority_r );
	DECLARE_WRITE16_MEMBER( layer_priority_w );
	DECLARE_READ16_MEMBER( background_color_r );
	DECLARE_WRITE16_MEMBER( background_color_w );

	DECLARE_READ16_MEMBER( screen_xoffset_r );
	DECLARE_WRITE16_MEMBER( screen_xoffset_w );
	DECLARE_READ16_MEMBER( screen_yoffset_r );
	DECLARE_WRITE16_MEMBER( screen_yoffset_w );

	DECLARE_READ16_MEMBER( window_r );
	DECLARE_WRITE16_MEMBER( window_w );
	DECLARE_READ16_MEMBER( scroll_r );
	DECLARE_WRITE16_MEMBER( scroll_w );


	DECLARE_READ16_MEMBER( gfxrom_r );
	DECLARE_WRITE16_MEMBER( crtc_vert_w );
	DECLARE_WRITE16_MEMBER( crtc_horz_w );
	DECLARE_WRITE16_MEMBER( crtc_unlock_w );
	DECLARE_WRITE16_MEMBER( blitter_w );
	DECLARE_WRITE16_MEMBER( screen_ctrl_w );
	DECLARE_WRITE16_MEMBER( rombank_w );


	uint32_t screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);

protected:
	imagetek_i4100_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, bool has_ext_tiles);

	// device-level overrides
	//virtual void device_validity_check(validity_checker &valid) const override;
	virtual void device_add_mconfig(machine_config &config) override;
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr) override;

	required_shared_ptr<uint16_t> m_vram_0;
	required_shared_ptr<uint16_t> m_vram_1;
	required_shared_ptr<uint16_t> m_vram_2;
	required_shared_ptr<uint16_t> m_scratchram;
	required_shared_ptr<uint16_t> m_blitter_regs;
	required_shared_ptr<uint16_t> m_spriteram;
	required_shared_ptr<uint16_t> m_tiletable;
	required_shared_ptr<uint16_t> m_window;
	required_shared_ptr<uint16_t> m_scroll;

	required_device<palette_device> m_palette;
	required_device<gfxdecode_device> m_gfxdecode;
	required_region_ptr<uint8_t> m_gfxrom;

	std::unique_ptr<uint8_t[]>      m_expanded_gfx1;

	devcb_write_line m_blit_irq_cb;

	uint16_t m_rombank;
	size_t m_gfxrom_size;
	bool m_crtc_unlock;
	uint16_t m_sprite_count;
	uint16_t m_sprite_priority;
	uint16_t m_sprite_xoffset,m_sprite_yoffset;
	uint16_t m_sprite_color_code;
	uint8_t m_layer_priority[3];
	uint16_t m_background_color;
	uint16_t m_screen_xoffset,m_screen_yoffset;
	bool m_layer_tile_select[3];
	bool m_screen_blank;
	bool m_screen_flip;
	const bool m_support_8bpp, m_support_16x16;
	int  m_tilemap_scrolldx[3];
	int  m_tilemap_scrolldy[3];

	void blt_write( address_space &space, const int tmap, const offs_t offs, const uint16_t data, const uint16_t mask );

	enum
	{
		TIMER_BLIT_END = 1
	};

	emu_timer *m_blit_done_timer;

	void draw_layers( screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect, int pri );
	inline uint8_t get_tile_pix( uint16_t code, uint8_t x, uint8_t y, bool big, uint16_t *pix );
	void draw_tilemap( screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect, uint32_t flags, uint32_t pcode,
					int sx, int sy, int wx, int wy, bool big, uint16_t *tilemapram, int layer );
	void draw_sprites( screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect );
	void expand_gfx1();

// A 2048 x 2048 virtual tilemap
	static constexpr uint32_t BIG_NX = (0x100);
	static constexpr uint32_t BIG_NY = (0x100);

// A smaller 512 x 256 window defines the actual tilemap
	static constexpr uint32_t WIN_NX = (0x40);
	static constexpr uint32_t WIN_NY = (0x20);

	bool m_inited_hack;
};

class imagetek_i4220_device : public imagetek_i4100_device
{
public:
	// construction/destruction
	imagetek_i4220_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	DECLARE_ADDRESS_MAP(v2_map, 16);

	// needed by Blazing Tornado / Grand Striker 2 for mixing with PSAC
	// (it's unknown how the chip enables external sync)
	uint16_t get_background_pen() { return m_background_color; };

protected:

};

class imagetek_i4300_device : public imagetek_i4100_device
{
public:
	// construction/destruction
	imagetek_i4300_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	DECLARE_ADDRESS_MAP(v3_map, 16);


protected:

};

// device type definition
DECLARE_DEVICE_TYPE(I4100, imagetek_i4100_device)
DECLARE_DEVICE_TYPE(I4220, imagetek_i4220_device)
DECLARE_DEVICE_TYPE(I4300, imagetek_i4300_device)



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************


#endif // MAME_VIDEO_I4100_H
