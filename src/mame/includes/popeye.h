#include "machine/eepromser.h"

// license:BSD-3-Clause
// copyright-holders:Nicola Salmoria, Couriersud
// thanks-to: Marc Lafontaine
class popeye_state : public driver_device
{
public:
	popeye_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag),
		m_background_pos(*this, "background_pos"),
		m_palettebank(*this, "palettebank"),
		m_spriteram(*this, "spriteram"),
		m_videoram(*this, "videoram"),
		m_colorram(*this, "colorram"),
		m_color_prom(*this, "proms"),
		m_color_prom_spr(*this, "sprpal"),
		m_maincpu(*this, "maincpu"),
		m_gfxdecode(*this, "gfxdecode"),
		m_palette(*this, "palette") { }

	uint8_t m_prot0;
	uint8_t m_prot1;
	uint8_t m_prot_shift;
	uint8_t m_dswbit;
	required_shared_ptr<uint8_t> m_background_pos;
	required_shared_ptr<uint8_t> m_palettebank;
	required_shared_ptr<uint8_t> m_spriteram;
	required_shared_ptr<uint8_t> m_videoram;
	required_shared_ptr<uint8_t> m_colorram;
	required_region_ptr<uint8_t> m_color_prom;
	required_region_ptr<uint8_t> m_color_prom_spr;

	uint8_t m_bitmapram[0x1000];
	std::unique_ptr<bitmap_ind16> m_sprite_bitmap;
	tilemap_t *m_fg_tilemap;
	uint8_t m_last_palette;
	int   m_field;

	DECLARE_READ8_MEMBER(protection_r);
	DECLARE_WRITE8_MEMBER(protection_w);
	DECLARE_WRITE8_MEMBER(popeye_videoram_w);
	DECLARE_WRITE8_MEMBER(popeye_colorram_w);
	DECLARE_WRITE8_MEMBER(tpp2_bitmap_w);
	DECLARE_WRITE8_MEMBER(tnx1_bitmap_w);
	DECLARE_WRITE8_MEMBER(popeye_portB_w);
	DECLARE_CUSTOM_INPUT_MEMBER(dsw1_read);
	DECLARE_DRIVER_INIT(tnx1);
	DECLARE_DRIVER_INIT(tpp2);
	TILE_GET_INFO_MEMBER(get_fg_tile_info);
	virtual void driver_start() override;
	virtual void video_start() override;
	DECLARE_PALETTE_INIT(tpp1);
	DECLARE_VIDEO_START(tpp1);
	DECLARE_PALETTE_INIT(popeyebl);
	DECLARE_PALETTE_INIT(tnx1);
	uint32_t screen_update_tnx1(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	uint32_t screen_update_tpp1(screen_device &screen, bitmap_ind16 &bitmap, const rectangle &cliprect);
	INTERRUPT_GEN_MEMBER(popeye_interrupt);
	DECLARE_CUSTOM_INPUT_MEMBER( pop_field_r );
	void update_palette();
	void draw_sprites(bitmap_ind16 &bitmap, const rectangle &cliprect);
	void draw_field(bitmap_ind16 &bitmap, const rectangle &cliprect);

	required_device<cpu_device> m_maincpu;
	required_device<gfxdecode_device> m_gfxdecode;
	required_device<palette_device> m_palette;
	void tpp2(machine_config &config);
	void tpp1(machine_config &config);
	void tnx1(machine_config &config);
	void popeyebl(machine_config &config);
};
