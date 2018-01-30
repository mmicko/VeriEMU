// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria, Couriersud
// thanks-to: Marc Lafontaine
/***************************************************************************

  video.c

  Functions to emulate the video hardware of the machine.

***************************************************************************/

#include "emu.h"
#include "video/resnet.h"
#include "includes/popeye.h"

#define USE_NEW_COLOR (1)

// Only enable USE_INTERLACE if you can ensure the game is rendered at an
// integer multiple of it's original resolution
#define USE_INTERLACE (0)


/***************************************************************************

  Convert the color PROMs into a more useable format.

  Popeye has four color PROMS:
  - 32x8 char palette
  - 32x8 background palette
  - two 256x4 sprite palette

  The char and sprite PROMs are connected to the RGB output this way:

  bit 7 -- 220 ohm resistor  -- BLUE (inverted)
        -- 470 ohm resistor  -- BLUE (inverted)
        -- 220 ohm resistor  -- GREEN (inverted)
        -- 470 ohm resistor  -- GREEN (inverted)
        -- 1  kohm resistor  -- GREEN (inverted)
        -- 220 ohm resistor  -- RED (inverted)
        -- 470 ohm resistor  -- RED (inverted)
  bit 0 -- 1  kohm resistor  -- RED (inverted)

  The background PROM is connected to the RGB output this way:

  bit 7 -- 470 ohm resistor  -- BLUE (inverted)
        -- 680 ohm resistor  -- BLUE (inverted)  (1300 ohm in Sky Skipper)
        -- 470 ohm resistor  -- GREEN (inverted)
        -- 680 ohm resistor  -- GREEN (inverted)
        -- 1.2kohm resistor  -- GREEN (inverted)
        -- 470 ohm resistor  -- RED (inverted)
        -- 680 ohm resistor  -- RED (inverted)
  bit 0 -- 1.2kohm resistor  -- RED (inverted)

  The bootleg is the same, but the outputs are not inverted.

***************************************************************************/

static const res_net_decode_info popeye_7051_decode_info =
{
	1,      /*  one prom 5 lines */
	0,      /*  start at 0 */
	15,     /*  end at 15 (banked) */
	/*  R,   G,   B,  */
	{   0,   0,   0 },      /*  offsets */
	{   0,   3,   6 },      /*  shifts */
	{0x07,0x07,0x03 }       /*  masks */
};

static const res_net_decode_info popeye_7052_decode_info =
{
	1,      /*  two 4 bit proms */
	0,      /*  start at 0 */
	31,     /*  end at 31 (banked) */
	/*  R,   G,   B */
	{   0,   0,   0},       /*  offsets */
	{   0,   3,   6},       /*  shifts */
	{0x07,0x07,0x03}        /*  masks */
};

static const res_net_info popeye_7051_txt_net_info =
{
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_MB7051 | RES_NET_MONITOR_SANYO_EZV20,
	{
		{ RES_NET_AMP_DARLINGTON, 470, 0, 3, { 1000, 470, 220 } },
		{ RES_NET_AMP_DARLINGTON, 470, 0, 3, { 1000, 470, 220 } },
		{ RES_NET_AMP_DARLINGTON, 680, 0, 2, {  470, 220,   0 } }  /*  popeye */
	}
};

static const res_net_info popeye_7051_bck_net_info =
{
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_MB7051 | RES_NET_MONITOR_SANYO_EZV20,
	{
		{ RES_NET_AMP_DARLINGTON, 470, 0, 3, { 1200, 680, 470 } },
		{ RES_NET_AMP_DARLINGTON, 470, 0, 3, { 1200, 680, 470 } },
		{ RES_NET_AMP_DARLINGTON, 680, 0, 2, {  680, 470,   0 } }  /*  popeye */
	}
};


static const res_net_info popeye_7052_obj_net_info =
{
	RES_NET_VCC_5V | RES_NET_VBIAS_5V | RES_NET_VIN_MB7052 |  RES_NET_MONITOR_SANYO_EZV20,
	{
		{ RES_NET_AMP_DARLINGTON, 470, 0, 3, { 1000, 470, 220 } },
		{ RES_NET_AMP_DARLINGTON, 470, 0, 3, { 1000, 470, 220 } },
		{ RES_NET_AMP_DARLINGTON, 680, 0, 2, {  470, 220,   0 } }  /*  popeye */
	}
};


PALETTE_INIT_MEMBER(popeye_state, tpp1)
{
	/* Two of the PROM address pins are tied together */
	for (int i = 0; i < 0x20; i++)
	{
		int color = (i & 0xf) | ((i & 0x8) << 1);
		m_color_prom[i + 0x20] = m_color_prom[color + 0x20];
	}

	m_last_palette = -1;

	update_palette();
}

PALETTE_INIT_MEMBER(popeye_state,popeyebl)
{
	/* Two of the PROM address pins are tied together */
	for (int i = 0; i < 0x20; i++)
	{
		int color = (i & 0xf) | ((i & 0x8) << 1);
		m_color_prom[i + 0x20] = m_color_prom[color + 0x20];
	}

	m_last_palette = -1;

	update_palette();
}

PALETTE_INIT_MEMBER(popeye_state, tnx1)
{
	/* Two of the PROM address pins are tied together and one is not connected... */
	for (int i = 0;i < 0x100;i++)
	{
		int color = (i & 0x3f) | ((i & 0x20) << 1);
		m_color_prom_spr[i] = m_color_prom_spr[color];
	}

	m_last_palette = -1;

	update_palette();
}

void popeye_state::update_palette()
{
	if ((*m_palettebank ^ m_last_palette) & 0x08)
	{
		uint8_t *color_prom = m_color_prom + 16 * ((*m_palettebank & 0x08) >> 3);

#if USE_NEW_COLOR
		std::vector<rgb_t> rgb;

		compute_res_net_all(rgb, color_prom, popeye_7051_decode_info, popeye_7051_bck_net_info);
		m_palette->set_pen_colors(0, rgb);
#else
		for (int i = 0; i < 16; i++)
		{
			/* red component */
			int bit0 = (~color_prom[i] >> 0) & 0x01;
			int bit1 = (~color_prom[i] >> 1) & 0x01;
			int bit2 = (~color_prom[i] >> 2) & 0x01;
			int r = 0x1c * bit0 + 0x31 * bit1 + 0x47 * bit2;
			/* green component */
			bit0 = (~color_prom[i] >> 3) & 0x01;
			bit1 = (~color_prom[i] >> 4) & 0x01;
			bit2 = (~color_prom[i] >> 5) & 0x01;
			int g = 0x1c * bit0 + 0x31 * bit1 + 0x47 * bit2;
			/* blue component */
			bit0 = 0;
			bit1 = (~color_prom[i] >> 6) & 0x01;
			bit2 = (~color_prom[i] >> 7) & 0x01;
			//if (m_bitmap_type == TYPE_TNX1)
			//{
			//  /* Sky Skipper has different weights */
			//  bit0 = bit1;
			//  bit1 = 0;
			//}
			int b = 0x1c * bit0 + 0x31 * bit1 + 0x47 * bit2;

			m_palette->set_pen_color(i, rgb_t(r, g, b));
		}
#endif
	}

	if ((*m_palettebank ^ m_last_palette) & 0x08)
	{
		uint8_t *color_prom = m_color_prom + 32 + 16 * ((*m_palettebank & 0x08) >> 3);

		/* characters */
#if USE_NEW_COLOR
		for (int i = 0; i < 16; i++)
		{
			int r = compute_res_net((color_prom[i] >> 0) & 0x07, 0, popeye_7051_txt_net_info);
			int g = compute_res_net((color_prom[i] >> 3) & 0x07, 1, popeye_7051_txt_net_info);
			int b = compute_res_net((color_prom[i] >> 6) & 0x03, 2, popeye_7051_txt_net_info);
			m_palette->set_pen_color(16 + (2 * i) + 0, rgb_t(0, 0, 0));
			m_palette->set_pen_color(16 + (2 * i) + 1, rgb_t(r, g, b));
		}
#else
		for (int i = 0; i < 16; i++)
		{
			/* red component */
			int bit0 = (~color_prom[i] >> 0) & 0x01;
			int bit1 = (~color_prom[i] >> 1) & 0x01;
			int bit2 = (~color_prom[i] >> 2) & 0x01;
			int r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
			/* green component */
			bit0 = (~color_prom[i] >> 3) & 0x01;
			bit1 = (~color_prom[i] >> 4) & 0x01;
			bit2 = (~color_prom[i] >> 5) & 0x01;
			int g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
			/* blue component */
			bit0 = 0;
			bit1 = (~color_prom[i] >> 6) & 0x01;
			bit2 = (~color_prom[i] >> 7) & 0x01;
			int b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

			m_palette->set_pen_color(16 + (2 * i) + 1, rgb_t(r, g, b));
		}
#endif
	}

	if ((*m_palettebank ^ m_last_palette) & 0x07)
	{
		uint8_t *color_prom = m_color_prom_spr + 32 * (*m_palettebank & 0x07);

#if USE_NEW_COLOR
		/* sprites */
		std::vector<rgb_t> rgb;
		compute_res_net_all(rgb, color_prom, popeye_7052_decode_info, popeye_7052_obj_net_info);
		m_palette->set_pen_colors(48, rgb);
#else
		for (int i = 0; i < 32; i++)
		{
			int bit0, bit1, bit2, r, g, b;

			/* red component */
			bit0 = (~color_prom[i] >> 0) & 0x01;
			bit1 = (~color_prom[i] >> 1) & 0x01;
			bit2 = (~color_prom[i] >> 2) & 0x01;
			r = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
			/* green component */
			bit0 = (~color_prom[i] >> 3) & 0x01;
			bit1 = (~color_prom[i] >> 4) & 0x01;
			bit2 = (~color_prom[i] >> 5) & 0x01;
			g = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;
			/* blue component */
			bit0 = 0;
			bit1 = (~color_prom[i] >> 6) & 0x01;
			bit2 = (~color_prom[i] >> 7) & 0x01;
			b = 0x21 * bit0 + 0x47 * bit1 + 0x97 * bit2;

			m_palette->set_pen_color(48 + i, rgb_t(r, g, b));
		}
#endif
	}

	m_last_palette = *m_palettebank;
}

WRITE8_MEMBER(popeye_state::tnx1_bitmap_w)
{
	// TODO: confirm the memory layout
	bool lsn = (data & 0x80) == 0;
	if (lsn)
	{
		m_bitmapram[offset] = (m_bitmapram[offset] & 0xf0) | (data & 0xf);
	}
	else
	{
		m_bitmapram[offset] = (m_bitmapram[offset] & 0x0f) | (data << 4);
	}
}

WRITE8_MEMBER(popeye_state::tpp2_bitmap_w)
{
	// TODO: confirm the memory layout
	bool lsn = (offset & 0x40) == 0;
	offset = (offset & 0x3f) | ((offset & ~0x7f) >> 1);
	if (lsn)
	{
		m_bitmapram[offset] = (m_bitmapram[offset] & 0xf0) | (data & 0xf);
	}
	else
	{
		m_bitmapram[offset] = (m_bitmapram[offset] & 0x0f) | (data << 4);
	}
}

WRITE8_MEMBER(popeye_state::popeye_videoram_w)
{
	m_videoram[offset] = data;
	m_fg_tilemap->mark_tile_dirty(offset);
}

WRITE8_MEMBER(popeye_state::popeye_colorram_w)
{
	m_colorram[offset] = data;
	m_fg_tilemap->mark_tile_dirty(offset);
}

TILE_GET_INFO_MEMBER(popeye_state::get_fg_tile_info)
{
	int code = m_videoram[tile_index];
	int color = m_colorram[tile_index] & 0x0f;

	SET_TILE_INFO_MEMBER(0, code, color, 0);
}

void popeye_state::video_start()
{
	m_sprite_bitmap = std::make_unique<bitmap_ind16>(512, 512);

	m_fg_tilemap = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(FUNC(popeye_state::get_fg_tile_info),this), TILEMAP_SCAN_ROWS, 16, 16, 32, 32);
	m_fg_tilemap->set_transparent_pen(0);

	m_field = 0;

	save_item(NAME(m_field));
	save_item(NAME(m_last_palette));
	save_item(NAME(*m_sprite_bitmap));
	save_item(NAME(m_bitmapram));
}

VIDEO_START_MEMBER(popeye_state,tpp1)
{
	m_sprite_bitmap = std::make_unique<bitmap_ind16>(512, 512);

	m_fg_tilemap = &machine().tilemap().create(*m_gfxdecode, tilemap_get_info_delegate(FUNC(popeye_state::get_fg_tile_info),this), TILEMAP_SCAN_ROWS, 16, 16, 32, 32);
	m_fg_tilemap->set_transparent_pen(0);

	m_field = 0;

	save_item(NAME(m_field));
	save_item(NAME(m_last_palette));
	save_item(NAME(*m_sprite_bitmap));
	save_item(NAME(m_bitmapram));
}

void popeye_state::draw_sprites(bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	m_sprite_bitmap->fill(0, cliprect);

	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		struct attribute_memory
		{
			int row;
			int sx;
			uint8_t color;
			uint16_t code;
			int flipx;
			int flipy;
		} attributes[64] = { 0 };

		for (int offs = 0; offs < m_spriteram.bytes(); offs += 4)
		{
			int sy = 0x200 - (m_spriteram[offs + 1] * 2);
			int row = y - sy;
			if (flip_screen())
			{
				sy ^= 0x1ff;
				row = sy - y;
			}

			if (row >= 0 && row < 16)
			{
				/*
				* offs+3:
				* bit 7 ?
				* bit 6 ?
				* bit 5 ?
				* bit 4 MSB of sprite code
				* bit 3 vertical flip
				* bit 2 sprite bank
				* bit 1 \ color (with bit 2 as well)
				* bit 0 /
				*/

				struct attribute_memory *a = &attributes[m_spriteram[offs] >> 2];
				a->sx = m_spriteram[offs] * 2;
				a->row = row;
				a->code = ((m_spriteram[offs + 2] & 0x7f)
					+ ((m_spriteram[offs + 3] & 0x10) << 3)
					+ ((m_spriteram[offs + 3] & 0x04) << 6)) ^ 0x1ff;
				a->color = (m_spriteram[offs + 3] & 0x07);
				a->flipx = (m_spriteram[offs + 2] & 0x80) ? 0xf : 0;
				a->flipy = (m_spriteram[offs + 3] & 0x08) ? 0xf : 0;
			}
		}

		for (int i = 0; i < 64; i++)
		{
			struct attribute_memory *a = &attributes[i];
			if (a->color != 0)
			{
				gfx_element *gfx = m_gfxdecode->gfx(1);
				const pen_t *pal = &m_palette->pen(gfx->colorbase() + gfx->granularity() * (a->color % gfx->colors()));
				const uint8_t *source_base = gfx->get_data(a->code % gfx->elements());
				const uint8_t *source = source_base + (a->row ^ a->flipy) * gfx->rowbytes();

				for (int x = 0; x < 16; x++)
				{
					int px = a->sx + x - 8;
					if (px >= 0 && px < 512)
					{
						if (flip_screen())
							px ^= 0x1ff;

						uint16_t p = source[x ^ a->flipx];
						if (p) p = pal[p];

						m_sprite_bitmap->pix(y, px) = p;
					}
				}
			}
		}
	}

	copybitmap_trans(bitmap, *m_sprite_bitmap, 0, 0, 0, 0, cliprect, 0);
}

void popeye_state::draw_field(bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	int x;
	int y;

	for (y=(cliprect.min_y & ~1) + m_field; y<=cliprect.max_y; y += 2)
		for (x=cliprect.min_x; x<=cliprect.max_x; x++)
			bitmap.pix(y, x) = 0;
}

uint32_t popeye_state::screen_update_tnx1(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	update_palette();

	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		int sy = y;
		if (flip_screen())
			sy ^= 0x1ff;

		sy -= 0x200 - (2 * m_background_pos[1]);

		for (int x = cliprect.min_x; x <= cliprect.max_x; x++)
		{
			if (sy < 0)
				bitmap.pix16(y, x) = m_bitmapram[0x3f] & 0xf; // TODO: find out exactly where the data is fetched from
			else
			{
				// TODO: confirm the memory layout
				int sx = x + (2 * (m_background_pos[0] | ((m_background_pos[2] & 1) << 8))) + 0x70;
				int shift = (sx & 0x200) / 0x80;

				bitmap.pix16(y, x) = (m_bitmapram[((sx / 8) & 0x3f) + ((sy / 8) * 0x40)] >> shift) & 0xf;
			}
		}
	}

	draw_sprites(bitmap, cliprect);
	m_fg_tilemap->draw(screen, bitmap, cliprect, 0, 0);
#if USE_INTERLACE
	draw_field(bitmap, cliprect);
#endif
	return 0;
}

uint32_t popeye_state::screen_update_tpp1(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect)
{
	update_palette();

	for (int y = cliprect.min_y; y <= cliprect.max_y; y++)
	{
		int sy = y;
		if (flip_screen())
			sy ^= 0x1ff;

		sy -= 0x200 - (2 * m_background_pos[1]);

		for (int x = cliprect.min_x; x <= cliprect.max_x; x++)
		{
			if (sy < 0)
				bitmap.pix16(y, x) = m_bitmapram[0x3f] & 0xf; // TODO: find out exactly where the data is fetched from
			else
			{
				// TODO: confirm the memory layout
				int sx = x + (2 * m_background_pos[0]) + 0x70;
				int shift = (sy & 4);

				bitmap.pix16(y, x) = (m_bitmapram[((sx / 8) & 0x3f) + ((sy / 8) * 0x40)] >> shift) & 0xf;
			}
		}
	}

	draw_sprites(bitmap, cliprect);
	m_fg_tilemap->draw(screen, bitmap, cliprect, 0, 0);
#if USE_INTERLACE
	draw_field(bitmap, cliprect);
#endif
	return 0;
}
