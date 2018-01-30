// license:BSD-3-Clause
// copyright-holders:Luca Elia,David Haywood,Angelo Salese
/***************************************************************************

    Imagetek I4100 / I4220 / I4300 device files

    device emulation by Angelo Salese, based off from original metro.cpp
    implementation by Luca Elia & David Haywood

    TODO:
    - interrupt enable/acknowledge/vector;
    - soundlatch delegate;
    - inputs for i4300;
    - hyprduel.cpp uses scanline attribute which crawls to unusable state
      with current video routines here;
    - CRT Controller, also understand why it needs so many writes before actual parameters;
    - Wrong color bars in service mode (e.g. balcube, toride2g). They use solid color tiles (80xx),
      but the right palette is not at 00-ff.
      Related to the unknown table in the RAM mapped just before the palette?
      Update: the colors should have a common bank of 0xb (so 0x8bxx), it's unknown why the values
      diverges, the blitter is responsible of that upload fwiw;
    - Some gfx problems in ladykill, 3kokushi, puzzli, gakusai, seem related to how we handle
      windows, wrapping, read-modify-write areas;
    - puzzli: emulate hblank irq and fix video routines here (water effect not emulated?);

============================================================================

                    driver by   Luca Elia (l.elia@tin.it)

                            [ 3 Scrolling Layers ]

        There is memory for a huge layer, but the actual tilemap
        is a smaller window (of fixed size) carved from anywhere
        inside that layer.

        Tile Size:                  8 x 8 x 4
        (later games can switch to  8 x 8 x 8, 16 x 16 x 4/8 at run time)

        Big Layer Size:         2048 x 2048 (8x8 tiles) or 4096 x 4096 (16x16 tiles)

        Tilemap Window Size:    512 x 256 (8x8 tiles) or 1024 x 512 (16x16 tiles)

        The tile codes in memory do not map directly to tiles. They
        are indexes into a table (with 0x200 entries) that defines
        a virtual set of tiles for the 3 layers. Each entry in that
        table adds 16 tiles to the set of available tiles, and decides
        their color code.

        Tile code with their msbit set are different as they mean:
        draw a tile filled with a single color (0-fff)


                            [ 512 Zooming Sprites ]

        The sprites are NOT tile based: the "tile" size can vary from
        8 to 64 (independently for width and height) with an 8 pixel
        granularity. The "tile" address is a multiple of 8x8 pixels.

        Each sprite can be shrinked to ~1/4 or enlarged to ~32x following
        an exponential curve of sizes (with one zoom value for both width
        and height)


***************************************************************************/

#include "emu.h"
#include "imagetek_i4100.h"



//**************************************************************************
//  GLOBAL VARIABLES
//**************************************************************************

// device type definition
DEFINE_DEVICE_TYPE(I4100, imagetek_i4100_device, "i4100", "Imagetek I4100 052 VDP")
DEFINE_DEVICE_TYPE(I4220, imagetek_i4220_device, "i4220", "Imagetek I4220 071 VDP")
DEFINE_DEVICE_TYPE(I4300, imagetek_i4300_device, "i4300", "Imagetek I4300 095 VDP")

DEVICE_ADDRESS_MAP_START( map, 16, imagetek_i4100_device)
	AM_RANGE(0x00000, 0x1ffff) AM_READWRITE(vram_0_r, vram_0_w) AM_SHARE("vram_0")
	AM_RANGE(0x20000, 0x3ffff) AM_READWRITE(vram_1_r, vram_1_w) AM_SHARE("vram_1")
	AM_RANGE(0x40000, 0x5ffff) AM_READWRITE(vram_2_r, vram_2_w) AM_SHARE("vram_2")
	AM_RANGE(0x60000, 0x6ffff) AM_READ(gfxrom_r)
	AM_RANGE(0x70000, 0x71fff) AM_READWRITE(scratchram_r, scratchram_w) AM_SHARE("scratchram") // unknown, maybe palette
	AM_RANGE(0x72000, 0x73fff) AM_DEVREADWRITE("palette", palette_device, read16, write16) AM_SHARE("palette")
	AM_RANGE(0x74000, 0x74fff) AM_READWRITE(spriteram_r, spriteram_w) AM_SHARE("spriteram")
	AM_RANGE(0x75000, 0x75fff) AM_READWRITE(rmw_vram_0_r, rmw_vram_0_w)
	AM_RANGE(0x76000, 0x76fff) AM_READWRITE(rmw_vram_1_r, rmw_vram_1_w)
	AM_RANGE(0x77000, 0x77fff) AM_READWRITE(rmw_vram_2_r, rmw_vram_2_w)
	AM_RANGE(0x78000, 0x787ff) AM_READWRITE(tiletable_r, tiletable_w) AM_SHARE("tiletable")
	// video registers
	AM_RANGE(0x78800, 0x78801) AM_READWRITE(sprite_count_r,      sprite_count_w)
	AM_RANGE(0x78802, 0x78803) AM_READWRITE(sprite_priority_r,   sprite_priority_w)
	AM_RANGE(0x78804, 0x78805) AM_READWRITE(sprite_yoffset_r,    sprite_yoffset_w)
	AM_RANGE(0x78806, 0x78807) AM_READWRITE(sprite_xoffset_r,    sprite_xoffset_w)
	AM_RANGE(0x78808, 0x78809) AM_READWRITE(sprite_color_code_r, sprite_color_code_w)
	AM_RANGE(0x78810, 0x78811) AM_READWRITE(layer_priority_r,    layer_priority_w)
	AM_RANGE(0x78812, 0x78813) AM_READWRITE(background_color_r,  background_color_w)

	AM_RANGE(0x78840, 0x7884d) AM_WRITE(blitter_w) AM_SHARE("blitter_regs")
	AM_RANGE(0x78850, 0x78851) AM_READWRITE(screen_yoffset_r,    screen_yoffset_w)
	AM_RANGE(0x78852, 0x78853) AM_READWRITE(screen_xoffset_r,    screen_xoffset_w)
	AM_RANGE(0x78860, 0x7886b) AM_READWRITE(window_r, window_w) AM_SHARE("windowregs")
	AM_RANGE(0x78870, 0x7887b) AM_READWRITE(scroll_r, scroll_w) AM_SHARE("scrollregs")

	AM_RANGE(0x78880, 0x78881) AM_WRITE(crtc_vert_w)
	AM_RANGE(0x78890, 0x78891) AM_WRITE(crtc_horz_w)
	AM_RANGE(0x788a0, 0x788a1) AM_WRITE(crtc_unlock_w)
	AM_RANGE(0x788aa, 0x788ab) AM_WRITE(rombank_w)
	AM_RANGE(0x788ac, 0x788ad) AM_WRITE(screen_ctrl_w)
ADDRESS_MAP_END

// same as above but with moved video registers (now at 0x797**)
DEVICE_ADDRESS_MAP_START( v2_map, 16, imagetek_i4220_device)
	AM_RANGE(0x00000, 0x1ffff) AM_READWRITE(vram_0_r, vram_0_w) AM_SHARE("vram_0")
	AM_RANGE(0x20000, 0x3ffff) AM_READWRITE(vram_1_r, vram_1_w) AM_SHARE("vram_1")
	AM_RANGE(0x40000, 0x5ffff) AM_READWRITE(vram_2_r, vram_2_w) AM_SHARE("vram_2")
	AM_RANGE(0x60000, 0x6ffff) AM_READ(gfxrom_r)
	AM_RANGE(0x70000, 0x71fff) AM_READWRITE(scratchram_r, scratchram_w) AM_SHARE("scratchram") // unknown, maybe palette
	AM_RANGE(0x72000, 0x73fff) AM_DEVREADWRITE("palette", palette_device, read16, write16) AM_SHARE("palette")
	AM_RANGE(0x74000, 0x74fff) AM_READWRITE(spriteram_r, spriteram_w) AM_SHARE("spriteram")
	AM_RANGE(0x75000, 0x75fff) AM_READWRITE(rmw_vram_0_r, rmw_vram_0_w)
	AM_RANGE(0x76000, 0x76fff) AM_READWRITE(rmw_vram_1_r, rmw_vram_1_w)
	AM_RANGE(0x77000, 0x77fff) AM_READWRITE(rmw_vram_2_r, rmw_vram_2_w)
	AM_RANGE(0x78000, 0x787ff) AM_READWRITE(tiletable_r, tiletable_w) AM_SHARE("tiletable")

	AM_RANGE(0x78840, 0x7884d) AM_WRITE(blitter_w) AM_SHARE("blitter_regs")
	AM_RANGE(0x78850, 0x78851) AM_READWRITE(screen_yoffset_r,    screen_yoffset_w)
	AM_RANGE(0x78852, 0x78853) AM_READWRITE(screen_xoffset_r,    screen_xoffset_w)
	AM_RANGE(0x78860, 0x7886b) AM_READWRITE(window_r, window_w) AM_SHARE("windowregs")
	AM_RANGE(0x78870, 0x7887b) AM_READWRITE(scroll_r, scroll_w) AM_SHARE("scrollregs")

	AM_RANGE(0x78880, 0x78881) AM_WRITE(crtc_vert_w)
	AM_RANGE(0x78890, 0x78891) AM_WRITE(crtc_horz_w)
	AM_RANGE(0x788a0, 0x788a1) AM_WRITE(crtc_unlock_w)
	AM_RANGE(0x788aa, 0x788ab) AM_WRITE(rombank_w)
	AM_RANGE(0x788ac, 0x788ad) AM_WRITE(screen_ctrl_w)

	// video registers
	AM_RANGE(0x79700, 0x79701) AM_READWRITE(sprite_count_r,      sprite_count_w)
	AM_RANGE(0x79702, 0x79703) AM_READWRITE(sprite_priority_r,   sprite_priority_w)
	AM_RANGE(0x79704, 0x79705) AM_READWRITE(sprite_yoffset_r,    sprite_yoffset_w)
	AM_RANGE(0x79706, 0x79707) AM_READWRITE(sprite_xoffset_r,    sprite_xoffset_w)
	AM_RANGE(0x79708, 0x79709) AM_READWRITE(sprite_color_code_r, sprite_color_code_w)
	AM_RANGE(0x79710, 0x79711) AM_READWRITE(layer_priority_r,    layer_priority_w)
	AM_RANGE(0x79712, 0x79713) AM_READWRITE(background_color_r,  background_color_w)
	// repeated here in Puzzlet compatibility mode
	AM_RANGE(0x78800, 0x78801) AM_READWRITE(sprite_count_r,      sprite_count_w)
	// ... this one breaks Blazing Tornado tho
//  AM_RANGE(0x78802, 0x78803) AM_READWRITE(sprite_priority_r,   sprite_priority_w)
	AM_RANGE(0x78804, 0x78805) AM_READWRITE(sprite_yoffset_r,    sprite_yoffset_w)
	AM_RANGE(0x78806, 0x78807) AM_READWRITE(sprite_xoffset_r,    sprite_xoffset_w)
	AM_RANGE(0x78808, 0x78809) AM_READWRITE(sprite_color_code_r, sprite_color_code_w)
	AM_RANGE(0x78810, 0x78811) AM_READWRITE(layer_priority_r,    layer_priority_w)
	AM_RANGE(0x78812, 0x78813) AM_READWRITE(background_color_r,  background_color_w)
ADDRESS_MAP_END

// more changes around, namely the screen offsets being reversed here
DEVICE_ADDRESS_MAP_START( v3_map, 16, imagetek_i4300_device)
	AM_RANGE(0x00000, 0x1ffff) AM_READWRITE(vram_0_r, vram_0_w) AM_SHARE("vram_0")
	AM_RANGE(0x20000, 0x3ffff) AM_READWRITE(vram_1_r, vram_1_w) AM_SHARE("vram_1")
	AM_RANGE(0x40000, 0x5ffff) AM_READWRITE(vram_2_r, vram_2_w) AM_SHARE("vram_2")
	AM_RANGE(0x60000, 0x6ffff) AM_READ(gfxrom_r)
	AM_RANGE(0x70000, 0x71fff) AM_READWRITE(scratchram_r, scratchram_w) AM_SHARE("scratchram") // unknown, maybe palette
	AM_RANGE(0x72000, 0x73fff) AM_DEVREADWRITE("palette", palette_device, read16, write16) AM_SHARE("palette")
	AM_RANGE(0x74000, 0x74fff) AM_READWRITE(spriteram_r, spriteram_w) AM_SHARE("spriteram")
	AM_RANGE(0x75000, 0x75fff) AM_READWRITE(rmw_vram_0_r, rmw_vram_0_w)
	AM_RANGE(0x76000, 0x76fff) AM_READWRITE(rmw_vram_1_r, rmw_vram_1_w)
	AM_RANGE(0x77000, 0x77fff) AM_READWRITE(rmw_vram_2_r, rmw_vram_2_w)
	AM_RANGE(0x78000, 0x787ff) AM_READWRITE(tiletable_r, tiletable_w) AM_SHARE("tiletable")
	AM_RANGE(0x78808, 0x78809) AM_READWRITE(screen_xoffset_r,    screen_xoffset_w)
	AM_RANGE(0x7880a, 0x7880b) AM_READWRITE(screen_yoffset_r,    screen_yoffset_w)
	AM_RANGE(0x7880e, 0x7880f) AM_WRITE(screen_ctrl_w) // TODO: can be read back here (gakusai)

	AM_RANGE(0x78800, 0x78801) AM_WRITE(crtc_unlock_w)
	AM_RANGE(0x78802, 0x78803) AM_WRITE(crtc_horz_w)
	AM_RANGE(0x78804, 0x78805) AM_WRITE(crtc_vert_w)

	AM_RANGE(0x78840, 0x7884d) AM_WRITE(blitter_w) AM_SHARE("blitter_regs")
	AM_RANGE(0x78850, 0x7885b) AM_READWRITE(scroll_r, scroll_w) AM_SHARE("scrollregs")
	AM_RANGE(0x78860, 0x7886b) AM_READWRITE(window_r, window_w) AM_SHARE("windowregs")
	AM_RANGE(0x78870, 0x78871) AM_WRITE(rombank_w)

	// video registers
	AM_RANGE(0x79700, 0x79701) AM_READWRITE(sprite_count_r,      sprite_count_w)
	AM_RANGE(0x79702, 0x79703) AM_READWRITE(sprite_priority_r,   sprite_priority_w)
	AM_RANGE(0x79704, 0x79705) AM_READWRITE(sprite_yoffset_r,    sprite_yoffset_w)
	AM_RANGE(0x79706, 0x79707) AM_READWRITE(sprite_xoffset_r,    sprite_xoffset_w)
	AM_RANGE(0x79708, 0x79709) AM_READWRITE(sprite_color_code_r, sprite_color_code_w)
	AM_RANGE(0x79710, 0x79711) AM_READWRITE(layer_priority_r,    layer_priority_w)
	AM_RANGE(0x79712, 0x79713) AM_READWRITE(background_color_r,  background_color_w)
ADDRESS_MAP_END


//**************************************************************************
//  LIVE DEVICE
//**************************************************************************

//-------------------------------------------------
//  i4100_device - constructor
//-------------------------------------------------


imagetek_i4100_device::imagetek_i4100_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock, bool has_ext_tiles)
	: device_t(mconfig, type, tag, owner, clock)
	, device_video_interface(mconfig, *this)
	, m_vram_0(*this, "vram_0")
	, m_vram_1(*this, "vram_1")
	, m_vram_2(*this, "vram_2")
	, m_scratchram(*this, "scratchram")
	, m_blitter_regs(*this, "blitter_regs")
	, m_spriteram(*this, "spriteram")
	, m_tiletable(*this, "tiletable")
	, m_window(*this, "windowregs")
	, m_scroll(*this, "scrollregs")
	, m_palette(*this, "palette")
	, m_gfxdecode(*this, finder_base::DUMMY_TAG)
	, m_gfxrom(*this, "^gfx1")
	, m_blit_irq_cb(*this)
	, m_support_8bpp( has_ext_tiles )
	, m_support_16x16( has_ext_tiles )
	, m_tilemap_scrolldx{0, 0, 0}
	, m_tilemap_scrolldy{0, 0, 0}
{
}

imagetek_i4100_device::imagetek_i4100_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: imagetek_i4100_device(mconfig, I4100, tag, owner, clock, false)
{
}


imagetek_i4220_device::imagetek_i4220_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: imagetek_i4100_device(mconfig, I4220, tag, owner, clock, true)
{
}

imagetek_i4300_device::imagetek_i4300_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: imagetek_i4100_device(mconfig, I4300, tag, owner, clock, true)
{
}

//-------------------------------------------------
//  device_add_mconfig - device-specific machine
//  configuration addiitons
//-------------------------------------------------

MACHINE_CONFIG_START(imagetek_i4100_device::device_add_mconfig)
	MCFG_PALETTE_ADD("palette", 0x1000)
	MCFG_PALETTE_FORMAT(GGGGGRRRRRBBBBBx)
MACHINE_CONFIG_END


//-------------------------------------------------
//  static_set_gfxdecode_tag: Set the tag of the
//  gfx decoder
//-------------------------------------------------

void imagetek_i4100_device::static_set_gfxdecode_tag(device_t &device, const char *tag)
{
	downcast<imagetek_i4100_device &>(device).m_gfxdecode.set_tag(tag);
}


void imagetek_i4100_device::static_set_tmap_xoffsets(device_t &device, int x1, int x2, int x3)
{
	downcast<imagetek_i4100_device &>(device).m_tilemap_scrolldx[0] = x1;
	downcast<imagetek_i4100_device &>(device).m_tilemap_scrolldx[1] = x2;
	downcast<imagetek_i4100_device &>(device).m_tilemap_scrolldx[2] = x3;
}


void imagetek_i4100_device::static_set_tmap_yoffsets(device_t &device, int y1, int y2, int y3)
{
	downcast<imagetek_i4100_device &>(device).m_tilemap_scrolldy[0] = y1;
	downcast<imagetek_i4100_device &>(device).m_tilemap_scrolldy[1] = y2;
	downcast<imagetek_i4100_device &>(device).m_tilemap_scrolldy[2] = y3;
}


//-------------------------------------------------
//  device_start - device-specific startup
//-------------------------------------------------

void imagetek_i4100_device::expand_gfx1()
{
	// TODO: remove from device_reset (otherwise you get broken sprites in i4220+ games because gfx rom isn't yet inited!)
	if(m_inited_hack == true)
		return;

	m_inited_hack = true;
	uint32_t length   =   m_gfxrom_size * 2;

	m_expanded_gfx1 = std::make_unique<uint8_t[]>(length);

	for (int i = 0; i < length; i += 2)
	{
		uint8_t src = m_gfxrom[i / 2];

		m_expanded_gfx1[i + 0] = src & 0xf;
		m_expanded_gfx1[i + 1] = src >> 4;
	}
}

void imagetek_i4100_device::device_start()
{
	if(!m_gfxdecode->started())
		throw device_missing_dependencies();

	m_inited_hack = false;
	save_item(NAME(m_rombank));
	save_item(NAME(m_crtc_unlock));
	save_item(NAME(m_sprite_count));
	save_item(NAME(m_sprite_priority));
	save_item(NAME(m_sprite_color_code));
	save_item(NAME(m_sprite_xoffset));
	save_item(NAME(m_sprite_yoffset));
	save_item(NAME(m_screen_xoffset));
	save_item(NAME(m_screen_yoffset));
	save_item(NAME(m_layer_priority));
	save_item(NAME(m_background_color));
//  save_item(NAME(m_window));
//  save_item(NAME(m_scroll));
	save_item(NAME(m_screen_blank));
	save_item(NAME(m_screen_flip));

//  memory_region *devregion =  machine().root_device().memregion(":gfx1");
//  m_gfxrom = devregion->base();
	if (m_gfxrom == nullptr)
		fatalerror("Imagetek i4100 %s: \"gfx1\" memory base not found",this->tag());

	m_gfxrom_size = m_gfxrom.bytes();

	m_blit_irq_cb.resolve_safe();
	m_blit_done_timer = timer_alloc(TIMER_BLIT_END);

}


//-------------------------------------------------
//  device_reset - device-specific reset
//-------------------------------------------------

void imagetek_i4100_device::device_reset()
{
	expand_gfx1();
}


void imagetek_i4100_device::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	switch (id)
	{
		case TIMER_BLIT_END:
			m_blit_irq_cb(ASSERT_LINE);
			break;
	}
}

//**************************************************************************
//  READ/WRITE HANDLERS
//**************************************************************************

READ16_MEMBER(imagetek_i4100_device::vram_0_r){ return m_vram_0[offset]; }
READ16_MEMBER(imagetek_i4100_device::vram_1_r){ return m_vram_1[offset]; }
READ16_MEMBER(imagetek_i4100_device::vram_2_r){ return m_vram_2[offset]; }
WRITE16_MEMBER(imagetek_i4100_device::vram_0_w){ COMBINE_DATA(&m_vram_0[offset]); }
WRITE16_MEMBER(imagetek_i4100_device::vram_1_w){ COMBINE_DATA(&m_vram_1[offset]); }
WRITE16_MEMBER(imagetek_i4100_device::vram_2_w){ COMBINE_DATA(&m_vram_2[offset]); }

/* This game uses almost only the blitter to write to the tilemaps.
   The CPU can only access a "window" of 512x256 pixels in the upper
   left corner of the big tilemap */
// TODO: Puzzlet, Sankokushi & Lady Killer contradicts with aformentioned description (more like RMW?)

#define RMW_OFFS( _x_ ) ((_x_) & (0x3f)) + (((_x_) & ~(0x3f)) * (0x100 / 0x40))

READ16_MEMBER(imagetek_i4100_device::rmw_vram_0_r){ return m_vram_0[RMW_OFFS(offset)]; }
READ16_MEMBER(imagetek_i4100_device::rmw_vram_1_r){ return m_vram_1[RMW_OFFS(offset)]; }
READ16_MEMBER(imagetek_i4100_device::rmw_vram_2_r){ return m_vram_2[RMW_OFFS(offset)]; }
WRITE16_MEMBER(imagetek_i4100_device::rmw_vram_0_w){ COMBINE_DATA(&m_vram_0[RMW_OFFS(offset)]); }
WRITE16_MEMBER(imagetek_i4100_device::rmw_vram_1_w){ COMBINE_DATA(&m_vram_1[RMW_OFFS(offset)]); }
WRITE16_MEMBER(imagetek_i4100_device::rmw_vram_2_w){ COMBINE_DATA(&m_vram_2[RMW_OFFS(offset)]); }

#undef RMW_OFFS

READ16_MEMBER(imagetek_i4100_device::scratchram_r ) { return m_scratchram[offset]; }
WRITE16_MEMBER(imagetek_i4100_device::scratchram_w ) { COMBINE_DATA(&m_scratchram[offset]); }
READ16_MEMBER(imagetek_i4100_device::spriteram_r ) { return m_spriteram[offset]; }
WRITE16_MEMBER(imagetek_i4100_device::spriteram_w ) { COMBINE_DATA(&m_spriteram[offset]); }
READ16_MEMBER(imagetek_i4100_device::tiletable_r ) { return m_tiletable[offset]; }
WRITE16_MEMBER(imagetek_i4100_device::tiletable_w ) { COMBINE_DATA(&m_tiletable[offset]); }

// video registers
/*************************************************************
 *
 * 0.w  ---- ---- ---- ----     Number Of Sprites To Draw
 *
 ************************************************************/
READ16_MEMBER(imagetek_i4100_device::sprite_count_r) { return m_sprite_count; }
WRITE16_MEMBER(imagetek_i4100_device::sprite_count_w) { COMBINE_DATA(&m_sprite_count); }

/*************************************************************
 *
 * 2.w  f--- ---- ---- ----     Disable Sprites Layer Priority
 *      -edc ---- ---- ----
 *      ---- ba-- ---- ----     Sprites Masked Layer
 *      ---- --98 ---- ----     Sprites Priority
 *      ---- ---- 765- ----
 *      ---- ---- ---4 3210     Sprites Masked Number
 *
 *************************************************************/
READ16_MEMBER(imagetek_i4100_device::sprite_priority_r) { return m_sprite_priority; }
WRITE16_MEMBER(imagetek_i4100_device::sprite_priority_w) { COMBINE_DATA(&m_sprite_priority); }

/*************************************************************
 *
 * 4.w  ---- ---- ---- ----     Sprites Y Offset
 * 6.w  ---- ---- ---- ----     Sprites X Offset
 *
 ************************************************************/
READ16_MEMBER(imagetek_i4100_device::sprite_xoffset_r) { return m_sprite_xoffset; }
WRITE16_MEMBER(imagetek_i4100_device::sprite_xoffset_w) { COMBINE_DATA(&m_sprite_xoffset); }
READ16_MEMBER(imagetek_i4100_device::sprite_yoffset_r) { return m_sprite_yoffset; }
WRITE16_MEMBER(imagetek_i4100_device::sprite_yoffset_w) { COMBINE_DATA(&m_sprite_yoffset); }

/*************************************************************
 *
 * 8.w  ---- ---- ---- ----     Sprites Color Codes Start
 *
 ************************************************************/
READ16_MEMBER(imagetek_i4100_device::sprite_color_code_r) { return m_sprite_color_code; }
WRITE16_MEMBER(imagetek_i4100_device::sprite_color_code_w) { COMBINE_DATA(&m_sprite_color_code); }

/*************************************************************
 *
 * 10.w ---- ---- --54 ----     Layer 2 Priority (3 backmost, 0 frontmost)
 *      ---- ---- ---- 32--     Layer 1 Priority
 *      ---- ---- ---- --10     Layer 0 Priority
 *
 ************************************************************/

READ16_MEMBER(imagetek_i4100_device::layer_priority_r)
{
	return (m_layer_priority[2]<<4) | (m_layer_priority[1]<<2) | m_layer_priority[0];
}

WRITE16_MEMBER(imagetek_i4100_device::layer_priority_w)
{
	m_layer_priority[2] = (data >> 4) & 3;
	m_layer_priority[1] = (data >> 2) & 3;
	m_layer_priority[0] = (data >> 0) & 3;
	if((data >> 6) != 0)
		logerror("%s warning: layer_priority_w write with %04x %04x\n",this->tag(),data,mem_mask);
}

/*************************************************************
 *
 * 12.w ---- cccc cccc cccc     Background Color
 *
 ************************************************************/

READ16_MEMBER(imagetek_i4100_device::background_color_r)
{
	return m_background_color;
}

WRITE16_MEMBER(imagetek_i4100_device::background_color_w)
{
	COMBINE_DATA(&m_background_color);

	m_background_color &= 0x0fff;
	if(data & 0xf000)
		logerror("%s warning: background_color_w write with %04x %04x\n",this->tag(),data,mem_mask);
}

/***************************************************************************
 *
 *  0.w                                 Sprite Y center point
 *  2.w                                 Sprite X center point
 *
 * Appears to apply only for sprites, maybe they applies to tilemaps too under
 * certain conditions
 *
 ***************************************************************************/
READ16_MEMBER(imagetek_i4100_device::screen_xoffset_r) { return m_screen_xoffset; }
WRITE16_MEMBER(imagetek_i4100_device::screen_xoffset_w) { COMBINE_DATA(&m_screen_xoffset); }
READ16_MEMBER(imagetek_i4100_device::screen_yoffset_r) { return m_screen_yoffset; }
WRITE16_MEMBER(imagetek_i4100_device::screen_yoffset_w) { COMBINE_DATA(&m_screen_yoffset); }

READ16_MEMBER(imagetek_i4100_device::window_r) { return m_window[offset]; }
WRITE16_MEMBER(imagetek_i4100_device::window_w) { COMBINE_DATA(&m_window[offset]); }
READ16_MEMBER(imagetek_i4100_device::scroll_r) { return m_scroll[offset]; }
WRITE16_MEMBER(imagetek_i4100_device::scroll_w) { COMBINE_DATA(&m_scroll[offset]); }

/****************************************************
 *
 * Screen Control Register:
 *
 * f--- ---- ---- ----     ?
 * -edc b--- ---- ----
 * ---- -a98 ---- ----     ? Leds (see gakusai attract)
 * ---- ---- 765- ----     16x16 Tiles  (Layer 2-1-0)
 * ---- ---- ---4 32--
 * ---- ---- ---- --1-     Blank Screen
 * ---- ---- ---- ---0     Flip  Screen
 *
 ****************************************************/
WRITE16_MEMBER(imagetek_i4100_device::screen_ctrl_w)
{
	m_layer_tile_select[2] = BIT(data,7);
	m_layer_tile_select[1] = BIT(data,6);
	m_layer_tile_select[0] = BIT(data,5);

	// TODO: some of these must be externalized
	m_screen_blank = BIT(data,1);
	m_screen_flip = BIT(data,0);

	if(data & 0xff1c)
		logerror("%s warning: screen_ctrl_w write with %04x %04x\n",this->tag(),data,mem_mask);

}


/*
    The main CPU has access to the ROMs that hold the graphics through
    a banked window of 64k. Those ROMs also usually store the tables for
    the virtual tiles set. The tile codes to be written to the tilemap
    memory to render the backgrounds are also stored here, in a format
    that the blitter can readily use (which is a form of compression)
*/

READ16_MEMBER(imagetek_i4100_device::gfxrom_r)
{
	offset = offset * 2 + 0x10000 * (m_rombank);

	if (offset < m_gfxrom_size)
		return ((m_gfxrom[offset + 0] << 8) + m_gfxrom[offset + 1]);
	else
		return 0xffff;
}

WRITE16_MEMBER( imagetek_i4100_device::rombank_w )
{
	COMBINE_DATA(&m_rombank);
}

WRITE16_MEMBER( imagetek_i4100_device::crtc_horz_w )
{
	if(m_crtc_unlock == true)
	{
		//logerror("%s CRTC horizontal %04x %04x\n",this->tag(),data,mem_mask);
	}
}

WRITE16_MEMBER( imagetek_i4100_device::crtc_vert_w )
{
	if(m_crtc_unlock == true)
	{
		//logerror("%s CRTC vertical %04x %04x\n",this->tag(),data,mem_mask);
	}
}

WRITE16_MEMBER( imagetek_i4100_device::crtc_unlock_w )
{
	m_crtc_unlock = BIT(data,0);
	if(data & ~1)
		logerror("%s warning: unlock register write with %04x %04x\n",this->tag(),data,mem_mask);
}

/***************************************************************************


                                    Blitter

    [ Registers ]

        Offset:     Value:

        0.l         Destination Tilemap      (1,2,3)
        4.l         Blitter Data Address     (byte offset into the gfx ROMs)
        8.l         Destination Address << 7 (byte offset into the tilemap)

        The Blitter reads a byte and looks at the most significative
        bits for the opcode, while the remaining bits define a value
        (usually how many bytes to write). The opcode byte may be
        followed by a number of other bytes:

            76------            Opcode
            --543210            N
            (at most N+1 bytes follow)


        The blitter is designed to write every other byte (e.g. it
        writes a byte and skips the next). Hence 2 blits are needed
        to fill a tilemap (first even, then odd addresses)

    [ Opcodes ]

            0       Copy the following N+1 bytes. If the whole byte
                    is $00: stop and generate an IRQ

            1       Fill N+1 bytes with a sequence, starting with
                    the  value in the following byte

            2       Fill N+1 bytes with the value in the following
                    byte

            3       Skip N+1 bytes. If the whole byte is $C0:
                    skip to the next row of the tilemap (+0x200 bytes)
                    but preserve the column passed at the start of the
                    blit (destination address % 0x200)


***************************************************************************/

void imagetek_i4100_device::blt_write( address_space &space, const int tmap, const offs_t offs, const uint16_t data, const uint16_t mask )
{
	switch(tmap)
	{
		case 1: vram_0_w(space, offs, data, mask);    break;
		case 2: vram_1_w(space, offs, data, mask);    break;
		case 3: vram_2_w(space, offs, data, mask);    break;
	}
//  logerror("%s : Blitter %X] %04X <- %04X & %04X\n", machine().describe_context(), tmap, offs, data, mask);
}

/***************************************************************************


                                    Blitter

    [ Registers ]

        Offset:     Value:

        0.l         Destination Tilemap      (1,2,3)
        4.l         Blitter Data Address     (byte offset into the gfx ROMs)
        8.l         Destination Address << 7 (byte offset into the tilemap)

        The Blitter reads a byte and looks at the most significative
        bits for the opcode, while the remaining bits define a value
        (usually how many bytes to write). The opcode byte may be
        followed by a number of other bytes:

            76------            Opcode
            --543210            N
            (at most N+1 bytes follow)


        The blitter is designed to write every other byte (e.g. it
        writes a byte and skips the next). Hence 2 blits are needed
        to fill a tilemap (first even, then odd addresses)

    [ Opcodes ]

            0       Copy the following N+1 bytes. If the whole byte
                    is $00: stop and generate an IRQ

            1       Fill N+1 bytes with a sequence, starting with
                    the  value in the following byte

            2       Fill N+1 bytes with the value in the following
                    byte

            3       Skip N+1 bytes. If the whole byte is $C0:
                    skip to the next row of the tilemap (+0x200 bytes)
                    but preserve the column passed at the start of the
                    blit (destination address % 0x200)


***************************************************************************/


// TODO: clean this up
WRITE16_MEMBER( imagetek_i4100_device::blitter_w )
{
	COMBINE_DATA(&m_blitter_regs[offset]);

	if (offset == 0x0c / 2)
	{
		//uint8_t *src     = memregion("gfx1")->base();

		uint32_t tmap     = (m_blitter_regs[0x00 / 2] << 16) + m_blitter_regs[0x02 / 2];
		uint32_t src_offs = (m_blitter_regs[0x04 / 2] << 16) + m_blitter_regs[0x06 / 2];
		uint32_t dst_offs = (m_blitter_regs[0x08 / 2] << 16) + m_blitter_regs[0x0a / 2];

		int shift   = (dst_offs & 0x80) ? 0 : 8;
		uint16_t mask = (dst_offs & 0x80) ? 0x00ff : 0xff00;

//      logerror("%s Blitter regs %08X, %08X, %08X\n", machine().describe_context(), tmap, src_offs, dst_offs);

		dst_offs >>= 7 + 1;
		switch (tmap)
		{
			case 1:
			case 2:
			case 3:
				break;
			default:
				logerror("%s Blitter unknown destination: %08X\n", machine().describe_context(), tmap);
				return;
		}

		while (1)
		{
			uint16_t b1, b2, count;

			src_offs %= m_gfxrom_size;
			b1 = m_gfxrom[src_offs];
//          logerror("%s Blitter opcode %02X at %06X\n", machine().describe_context(), b1, src_offs);
			src_offs++;

			count = ((~b1) & 0x3f) + 1;

			switch ((b1 & 0xc0) >> 6)
			{
			case 0:
				/* Stop and Generate an IRQ. We can't generate it now
				       both because it's unlikely that the blitter is so
				       fast and because some games (e.g. lastfort) need to
				       complete the blitter irq service routine before doing
				       another blit. */
				if (b1 == 0)
				{
					m_blit_done_timer->adjust(attotime::from_usec(500));
					return;
				}

				/* Copy */
				while (count--)
				{
					src_offs %= m_gfxrom_size;
					b2 = m_gfxrom[src_offs] << shift;
					src_offs++;

					dst_offs &= 0xffff;
					blt_write(space, tmap, dst_offs, b2, mask);
					dst_offs = ((dst_offs + 1) & (0x100 - 1)) | (dst_offs & (~(0x100 - 1)));
				}
				break;

			case 1:
				/* Fill with an increasing value */
				src_offs %= m_gfxrom_size;
				b2 = m_gfxrom[src_offs];
				src_offs++;

				while (count--)
				{
					dst_offs &= 0xffff;
					blt_write(space, tmap, dst_offs, b2 << shift, mask);
					dst_offs = ((dst_offs + 1) & (0x100 - 1)) | (dst_offs & (~(0x100 - 1)));
					b2++;
				}
				break;

			case 2:
				/* Fill with a fixed value */
				src_offs %= m_gfxrom_size;
				b2 = m_gfxrom[src_offs] << shift;
				src_offs++;

				while (count--)
				{
					dst_offs &= 0xffff;
					blt_write(space, tmap, dst_offs, b2, mask);
					dst_offs = ((dst_offs + 1) & (0x100 - 1)) | (dst_offs & (~(0x100 - 1)));
				}
				break;

			case 3:
				/* Skip to the next line ?? */
				if (b1 == 0xc0)
				{
					dst_offs +=   0x100;
					dst_offs &= ~(0x100 - 1);
					dst_offs |=  (0x100 - 1) & (m_blitter_regs[0x0a / 2] >> (7 + 1));
				}
				else
				{
					dst_offs += count;
				}
				break;

			default:
				//logerror("%s Blitter unknown opcode %02X at %06X\n",machine().describe_context(),b1,src_offs-1);
				return;
			}

		}
	}
}



/***************************************************************************
 *
 * Screen Drawing
 *
 ***************************************************************************/

 /***************************************************************************


                                Sprites Drawing


        Offset:     Bits:                   Value:

        0.w         fedc b--- ---- ----     Priority (0 = Max)
                    ---- -a98 7654 3210     X

        2.w         fedc ba-- ---- ----     Zoom (Both X & Y)
                    ---- --98 7654 3210     Y

        4.w         f--- ---- ---- ----     Flip X
                    -e-- ---- ---- ----     Flip Y
                    --dc b--- ---- ----     Size X *
                    ---- -a98 ---- ----     Size Y *
                    ---- ---- 7654 ----     Color
                    ---- ---- ---- 3210     Code High Bits **

        6.w                                 Code Low Bits  **

*  8 pixel increments
** 8x8 pixel increments

***************************************************************************/

void imagetek_i4100_device::draw_sprites( screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect )
{
	uint8_t *base_gfx4 = m_expanded_gfx1.get();
	uint8_t *base_gfx8 = m_gfxrom;
	uint32_t gfx_size = m_gfxrom_size;

	int max_x = (m_screen_xoffset+1)*2;
	int max_y = (m_screen_yoffset+1)*2;
	int m_sprite_xoffs = m_sprite_xoffset - (m_screen_xoffset+1);
	int m_sprite_yoffs = m_sprite_yoffset - (m_screen_yoffset+1);

	int max_sprites = m_spriteram.bytes() / 8;
	int sprites     = m_sprite_count % max_sprites;

	int color_start = (m_sprite_color_code & 0x0f) << 4;

	int i, j, pri;
	static const int primask[4] = { 0x0000, 0xff00, 0xff00 | 0xf0f0, 0xff00 | 0xf0f0 | 0xcccc };

	uint16_t *src;
	int inc;

	if (sprites == 0)
		return;

	for (i = 0; i < 0x20; i++)
	{
		if (!(m_sprite_priority & 0x8000))
		{
			src = m_spriteram + (sprites - 1) * (8 / 2);
			inc = -(8 / 2);
		} else {
			src = m_spriteram;
			inc = (8 / 2);
		}

		for (j = 0; j < sprites; j++)
		{
			int x, y, attr, code, color, flipx, flipy, zoom, curr_pri, width, height;

			// Exponential zoom table extracted from daitoride
			static const int zoomtable[0x40] =
			{   0xAAC,0x800,0x668,0x554,0x494,0x400,0x390,0x334,
				0x2E8,0x2AC,0x278,0x248,0x224,0x200,0x1E0,0x1C8,
				0x1B0,0x198,0x188,0x174,0x164,0x154,0x148,0x13C,
				0x130,0x124,0x11C,0x110,0x108,0x100,0x0F8,0x0F0,
				0x0EC,0x0E4,0x0DC,0x0D8,0x0D4,0x0CC,0x0C8,0x0C4,
				0x0C0,0x0BC,0x0B8,0x0B4,0x0B0,0x0AC,0x0A8,0x0A4,
				0x0A0,0x09C,0x098,0x094,0x090,0x08C,0x088,0x080,
				0x078,0x070,0x068,0x060,0x058,0x050,0x048,0x040 };

			x = src[0];
			curr_pri = (x & 0xf800) >> 11;

			if ((curr_pri == 0x1f) || (curr_pri != i))
			{
				src += inc;
				continue;
			}

			pri = (m_sprite_priority & 0x0300) >> 8;

			if (!(m_sprite_priority & 0x8000))
			{
				if (curr_pri > (m_sprite_priority & 0x1f))
					pri = (m_sprite_priority & 0x0c00) >> 10;
			}

			y     = src[1];
			attr  = src[2];
			code  = src[3];

			flipx =  attr & 0x8000;
			flipy =  attr & 0x4000;
			color = (attr & 0xf0) >> 4;

			zoom = zoomtable[(y & 0xfc00) >> 10] << (16 - 8);

			x = (x & 0x07ff) - m_sprite_xoffs;
			y = (y & 0x03ff) - m_sprite_yoffs;

			width  = (((attr >> 11) & 0x7) + 1) * 8;
			height = (((attr >>  8) & 0x7) + 1) * 8;

			uint32_t gfxstart = (8 * 8 * 4 / 8) * (((attr & 0x000f) << 16) + code);

			if (m_screen_flip)
			{
				flipx = !flipx;     x = max_x - x - width;
				flipy = !flipy;     y = max_y - y - height;
			}

			if (m_support_8bpp == true && color == 0xf)  /* 8bpp */
			{
				/* Bounds checking */
				if ((gfxstart + width * height - 1) >= gfx_size)
					continue;

				gfx_element gfx(m_palette, base_gfx8 + gfxstart, width, height, width, m_palette->entries(), 0, 256);

				gfx.prio_zoom_transpen(bitmap,cliprect,
								0,
								color_start >> 4,
								flipx, flipy,
								x, y,
								zoom, zoom,
								screen.priority(),primask[pri], 255);
			}
			else
			{
				/* Bounds checking */
				if ((gfxstart + width / 2 * height - 1) >= gfx_size)
					continue;

				gfx_element gfx(m_palette, base_gfx4 + 2 * gfxstart, width, height, width, m_palette->entries(),0, 16);

				gfx.prio_zoom_transpen(bitmap,cliprect,
								0,
								color + color_start,
								flipx, flipy,
								x, y,
								zoom, zoom,
								screen.priority(),primask[pri], 15);
			}

			src += inc;
		}
	}
}


 inline uint8_t imagetek_i4100_device::get_tile_pix( uint16_t code, uint8_t x, uint8_t y, bool big, uint16_t *pix )
{
	int table_index;
	uint32_t tile;

	// Use code as an index into the tiles set table
	table_index = ((code & 0x1ff0) >> 4) * 2;
	tile = (m_tiletable[table_index + 0] << 16) + m_tiletable[table_index + 1];

	if (code & 0x8000) // Special: draw a tile of a single color (i.e. not from the gfx ROMs)
	{
		*pix = code & 0x0fff;

		if ((*pix & 0xf) != 0xf)
			return 1;
		else
			return 0;
	}
	else if (((tile & 0x00f00000) == 0x00f00000) && (m_support_8bpp == true)) /* draw tile as 8bpp (e.g. balcube bg) */
	{
		gfx_element *gfx1 = m_gfxdecode->gfx(big?3:1);
		uint32_t tile2 = big ? ((tile & 0xfffff) + 8*(code & 0xf)) :
								((tile & 0xfffff) + 2*(code & 0xf));
		const uint8_t* data;
		uint8_t flipxy = (code & 0x6000) >> 13;

		if (tile2 < gfx1->elements())
			data = gfx1->get_data(tile2);
		else
		{
			*pix = 0;
			return 0;
		}

		switch (flipxy)
		{
			default:
			case 0x0: *pix = data[(y              * (big?16:8)) + x];              break;
			case 0x1: *pix = data[(((big?15:7)-y) * (big?16:8)) + x];              break;
			case 0x2: *pix = data[(y              * (big?16:8)) + ((big?15:7)-x)]; break;
			case 0x3: *pix = data[(((big?15:7)-y) * (big?16:8)) + ((big?15:7)-x)]; break;
		}

		*pix |= ((tile & 0x0f000000) >> 24) * 0x100;

		if ((*pix & 0xff) != 0xff)
			return 1;
		else
			return 0;
	}
	else
	{
		gfx_element *gfx1 = m_gfxdecode->gfx(big?2:0);
		uint32_t tile2 = big ? ((tile & 0xfffff) + 4*(code & 0xf)) :
								((tile & 0xfffff) +   (code & 0xf));
		const uint8_t* data;
		uint8_t flipxy = (code & 0x6000) >> 13;

		if (tile2 < gfx1->elements())
			data = gfx1->get_data(tile2);
		else
		{
			*pix = 0;
			return 0;
		}

		switch (flipxy)
		{
			default:
			case 0x0: *pix = data[(y              * (big?16:8)) + x];             break;
			case 0x1: *pix = data[(((big?15:7)-y) * (big?16:8)) + x];             break;
			case 0x2: *pix = data[(y              * (big?16:8)) + ((big?15:7)-x)]; break;
			case 0x3: *pix = data[(((big?15:7)-y) * (big?16:8)) + ((big?15:7)-x)]; break;
		}

		*pix |= (((tile & 0x0ff00000) >> 20)) * 0x10;

		if ((*pix & 0xf) != 0xf)
			return 1;
		else
			return 0;
	}
}

/***************************************************************************

                        Tilemaps: Tiles Set & Window

    Each entry in the Tiles Set RAM uses 2 words to specify a starting
    tile code and a color code. This adds 16 consecutive tiles with
    that color code to the set of available tiles.

        Offset:     Bits:                   Value:

        0.w         fedc ---- ---- ----
                    ---- ba98 7654 ----     Color Code*
                    ---- ---- ---- 3210     Code High Bits

        2.w                                 Code Low Bits

* 00-ff, but on later chips supporting it, xf means 256 color tile and palette x

***************************************************************************/

void imagetek_i4100_device::draw_tilemap( screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect, uint32_t flags, uint32_t pcode,
							int sx, int sy, int wx, int wy, bool big, uint16_t *tilemapram, int layer )
{
	int y;

	bitmap_ind8 &priority_bitmap = screen.priority();

	int width  = big ? 4096 : 2048;
	int height = big ? 4096 : 2048;

	int scrwidth  = bitmap.width();
	int scrheight = bitmap.height();

	int windowwidth  = width >> 2;
	int windowheight = height >> 3;

	sx += m_tilemap_scrolldx[layer] * (m_screen_flip ? 1 : -1);
	sy += m_tilemap_scrolldy[layer] * (m_screen_flip ? 1 : -1);

	for (y = 0; y < scrheight; y++)
	{
		int scrolly = (sy+y-wy)&(windowheight-1);
		int x;
		uint16_t *dst;
		uint8_t *priority_baseaddr;
		int srcline = (wy+scrolly)&(height-1);
		int srctilerow = srcline >> (big ? 4 : 3);

		if (!m_screen_flip)
		{
			dst = &bitmap.pix16(y);
			priority_baseaddr = &priority_bitmap.pix8(y);

			for (x = 0; x < scrwidth; x++)
			{
				int scrollx = (sx+x-wx)&(windowwidth-1);
				int srccol = (wx+scrollx)&(width-1);
				int srctilecol = srccol >> (big ? 4 : 3);
				int tileoffs = srctilecol + srctilerow * BIG_NX;

				uint16_t dat = 0;

				uint16_t tile = tilemapram[tileoffs];
				uint8_t draw = get_tile_pix(tile, big ? (srccol&0xf) : (srccol&0x7), big ? (srcline&0xf) : (srcline&0x7), big, &dat);

				if (draw)
				{
					dst[x] = dat;
					priority_baseaddr[x] = (priority_baseaddr[x] & (pcode >> 8)) | pcode;
				}
			}
		}
		else // flipped case
		{
			dst = &bitmap.pix16(scrheight-y-1);
			priority_baseaddr = &priority_bitmap.pix8(scrheight-y-1);

			for (x = 0; x < scrwidth; x++)
			{
				int scrollx = (sx+x-wx)&(windowwidth-1);
				int srccol = (wx+scrollx)&(width-1);
				int srctilecol = srccol >> (big ? 4 : 3);
				int tileoffs = srctilecol + srctilerow * BIG_NX;

				uint16_t dat = 0;

				uint16_t tile = tilemapram[tileoffs];
				uint8_t draw = get_tile_pix(tile, big ? (srccol&0xf) : (srccol&0x7), big ? (srcline&0xf) : (srcline&0x7), big, &dat);

				if (draw)
				{
					dst[scrwidth-x-1] = dat;
					priority_baseaddr[scrwidth-x-1] = (priority_baseaddr[scrwidth-x-1] & (pcode >> 8)) | pcode;
				}
			}
		}
	}
}


void imagetek_i4100_device::draw_layers( screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect, int pri )
{
	// Draw all the layers with priority == pri
	for (int layer = 2; layer >= 0; layer--)
	{
		if (pri == m_layer_priority[layer])
		{
			// Scroll and Window values
			uint16_t sy = m_scroll[layer * 2 + 0]; uint16_t sx = m_scroll[layer * 2 + 1];
			uint16_t wy = m_window[layer * 2 + 0]; uint16_t wx = m_window[layer * 2 + 1];

			uint16_t *tilemapram = nullptr;

			switch (layer)
			{
				case 0: tilemapram = m_vram_0;   break;
				case 1: tilemapram = m_vram_1;   break;
				case 2: tilemapram = m_vram_2;   break;
			}

			bool big = (m_support_16x16 && m_layer_tile_select[layer]) == 1;

			draw_tilemap(screen, bitmap, cliprect, 0, 1 << (3 - pri), sx, sy, wx, wy, big, tilemapram, layer);
		}
	}
}


uint32_t imagetek_i4100_device::screen_update(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	screen.priority().fill(0, cliprect);

	bitmap.fill(m_background_color, cliprect);

	if (m_screen_blank == true)
		return 0;

	for (int pri = 3; pri >= 0; pri--)
		draw_layers(screen, bitmap, cliprect, pri);

	draw_sprites(screen, bitmap, cliprect);

	return 0;
}
