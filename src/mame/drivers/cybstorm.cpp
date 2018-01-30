// license:BSD-3-Clause
// copyright-holders:Aaron Giles, Phil Bennett
/***************************************************************************

    Atari Cyberstorm hardware

    driver by Aaron Giles and Phil Bennett

    Games supported:
        * Cyberstorm (prototype) (1993)

    Known bugs:
        * STAIN effect not 100% correct

***************************************************************************/


#include "emu.h"
#include "includes/cybstorm.h"
#include "cpu/m68000/m68000.h"
#include "machine/eeprompar.h"
#include "machine/watchdog.h"
#include "speaker.h"


/*************************************
 *
 *  Initialization
 *
 *************************************/

void cybstorm_state::update_interrupts()
{
	m_maincpu->set_input_line(4, m_scanline_int_state ? ASSERT_LINE : CLEAR_LINE);
	m_maincpu->set_input_line(6, m_sound_int_state ? ASSERT_LINE : CLEAR_LINE);
}


MACHINE_START_MEMBER(cybstorm_state,cybstorm)
{
	atarigen_state::machine_start();
	save_item(NAME(m_latch_data));
	save_item(NAME(m_alpha_tile_bank));
}

MACHINE_RESET_MEMBER(cybstorm_state,cybstorm)
{
	atarigen_state::machine_reset();
}



/*************************************
 *
 *  I/O handling
 *
 *************************************/

READ32_MEMBER(cybstorm_state::special_port1_r)
{
	return ioport("9F0010")->read();
}


WRITE32_MEMBER(cybstorm_state::latch_w)
{
	uint32_t oldword = m_latch_data;
	COMBINE_DATA(&m_latch_data);

	/* bit 4 is connected to the /RESET pin on the 6502 */
	m_jsa->soundcpu().set_input_line(INPUT_LINE_RESET, m_latch_data & 0x00100000 ? CLEAR_LINE : ASSERT_LINE);

	/* alpha bank is selected by the upper 4 bits */
	if ((oldword ^ m_latch_data) & 0x00e00000)
	{
		m_screen->update_partial(m_screen->vpos());
		m_vad->alpha().mark_all_dirty();
		m_alpha_tile_bank = (m_latch_data >> 21) & 7;
	}
}


/*************************************
 *
 *  Main CPU memory handlers
 *
 *************************************/


/*************************************
 *
 *  Memory maps
 *
 *************************************/

static ADDRESS_MAP_START( main_map, AS_PROGRAM, 32, cybstorm_state )
	AM_RANGE(0x000000, 0x1fffff) AM_ROM
	AM_RANGE(0x200000, 0x20ffff) AM_RAM_DEVWRITE("palette", palette_device, write32) AM_SHARE("palette")
	AM_RANGE(0x3effc0, 0x3effff) AM_DEVREADWRITE16("vad", atari_vad_device, control_read, control_write, 0xffffffff)
	AM_RANGE(0x3f0000, 0x3fffff) AM_DEVICE16("vadbank", address_map_bank_device, amap16, 0xffffffff)
	AM_RANGE(0x9f0000, 0x9f0003) AM_READ_PORT("9F0000")
	AM_RANGE(0x9f0010, 0x9f0013) AM_READ(special_port1_r)
	AM_RANGE(0x9f0030, 0x9f0033) AM_DEVREAD8("jsa", atari_jsa_iii_device, main_response_r, 0x00ff0000)
	AM_RANGE(0x9f0040, 0x9f0043) AM_DEVWRITE8("jsa", atari_jsa_iii_device, main_command_w, 0x00ff0000)
	AM_RANGE(0x9f0050, 0x9f0053) AM_WRITE(latch_w)
	AM_RANGE(0xfb0000, 0xfb0003) AM_DEVWRITE("watchdog", watchdog_timer_device, reset32_w)
	AM_RANGE(0xfc0000, 0xfc0003) AM_DEVWRITE("eeprom", eeprom_parallel_28xx_device, unlock_write32)
	AM_RANGE(0xfd0000, 0xfd0fff) AM_DEVREADWRITE8("eeprom", eeprom_parallel_28xx_device, read, write, 0xff00ff00)
	AM_RANGE(0xfe0000, 0xffffff) AM_RAM
ADDRESS_MAP_END

static ADDRESS_MAP_START( vadbank_map, AS_PROGRAM, 16, cybstorm_state )
	AM_RANGE(0x000000, 0x001fff) AM_RAM_DEVWRITE("vad", atari_vad_device, playfield2_latched_msb_w) AM_SHARE("vad:playfield2")
	AM_RANGE(0x002000, 0x003fff) AM_RAM_DEVWRITE("vad", atari_vad_device, playfield_latched_lsb_w) AM_SHARE("vad:playfield")
	AM_RANGE(0x004000, 0x005fff) AM_RAM_DEVWRITE("vad", atari_vad_device, playfield_upper_w) AM_SHARE("vad:playfield_ext")
	AM_RANGE(0x006000, 0x007fff) AM_RAM AM_SHARE("vad:mob")
	AM_RANGE(0x008000, 0x008eff) AM_DEVWRITE("vad", atari_vad_device, alpha_w) AM_SHARE("vad:alpha")
	AM_RANGE(0x008f00, 0x008f7f) AM_RAM AM_SHARE("vad:eof")
	AM_RANGE(0x008f80, 0x008fff) AM_RAM AM_SHARE("vad:mob:slip")
	AM_RANGE(0x009000, 0x00ffff) AM_RAM
ADDRESS_MAP_END



/*************************************
 *
 *  Port definitions
 *
 *************************************/

static INPUT_PORTS_START( cybstorm )
	PORT_START("9F0000")
	PORT_BIT( 0x0000000f, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_BIT( 0x00000010, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x00000020, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x00000040, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x00000080, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(2)

	PORT_BIT( 0x00000100, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x00000200, IP_ACTIVE_LOW, IPT_BUTTON5 ) PORT_PLAYER(2)
	PORT_BIT( 0x00000400, IP_ACTIVE_LOW, IPT_BUTTON6 ) PORT_PLAYER(2)
	PORT_BIT( 0x00000800, IP_ACTIVE_LOW, IPT_BUTTON7 ) PORT_PLAYER(2)
	PORT_BIT( 0x00001000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(2)
	PORT_BIT( 0x00002000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(2)
	PORT_BIT( 0x00004000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(2)
	PORT_BIT( 0x00008000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(2)
	PORT_BIT( 0x000f0000, IP_ACTIVE_LOW, IPT_UNUSED )


	PORT_BIT( 0x00100000, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x00200000, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x00400000, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT( 0x00800000, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(1)

	PORT_BIT( 0x01000000, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x02000000, IP_ACTIVE_LOW, IPT_BUTTON5 ) PORT_PLAYER(1)
	PORT_BIT( 0x04000000, IP_ACTIVE_LOW, IPT_BUTTON6 ) PORT_PLAYER(1)
	PORT_BIT( 0x08000000, IP_ACTIVE_LOW, IPT_BUTTON7 ) PORT_PLAYER(1)
	PORT_BIT( 0x10000000, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_PLAYER(1)
	PORT_BIT( 0x20000000, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_PLAYER(1)
	PORT_BIT( 0x40000000, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_PLAYER(1)
	PORT_BIT( 0x80000000, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_PLAYER(1)

	PORT_START("9F0010")
	PORT_BIT( 0x0000ffff, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x00010000, IP_ACTIVE_LOW, IPT_CUSTOM ) PORT_HBLANK("screen")
	PORT_BIT( 0x00020000, IP_ACTIVE_LOW, IPT_SPECIAL )
	PORT_BIT( 0x00040000, IP_ACTIVE_LOW, IPT_VOLUME_DOWN )
	PORT_BIT( 0x00080000, IP_ACTIVE_LOW, IPT_VOLUME_UP )
	PORT_BIT( 0x00100000, IP_ACTIVE_LOW, IPT_CUSTOM ) PORT_ATARI_JSA_SOUND_TO_MAIN_READY("jsa")
	PORT_BIT( 0x00200000, IP_ACTIVE_LOW, IPT_CUSTOM ) PORT_ATARI_JSA_MAIN_TO_SOUND_READY("jsa")
	PORT_SERVICE( 0x00400000, IP_ACTIVE_LOW )
	PORT_BIT( 0x00800000, IP_ACTIVE_LOW, IPT_CUSTOM ) PORT_VBLANK("screen")
	PORT_BIT( 0xff000000, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END


/*************************************
 *
 *  Graphics definitions
 *
 *************************************/

static const gfx_layout anlayout =
{
	8,8,
	RGN_FRAC(1,1),
	2,
	{ 0, 4 },
	{ 0, 1, 2, 3, 8, 9, 10, 11 },
	{ 0*16, 1*16, 2*16, 3*16, 4*16, 5*16, 6*16, 7*16 },
	8*16
};


static const gfx_layout pflayout =
{
	8,8,
	RGN_FRAC(1,8),
	8,
	{ RGN_FRAC(7,8), RGN_FRAC(6,8), RGN_FRAC(5,8), RGN_FRAC(4,8), RGN_FRAC(3,8), RGN_FRAC(2,8), RGN_FRAC(1,8), RGN_FRAC(0,8) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};


static const gfx_layout molayout =
{
	8,8,
	RGN_FRAC(1,6),
	6,
	{ RGN_FRAC(5,6), RGN_FRAC(4,6), RGN_FRAC(3,6), RGN_FRAC(2,6), RGN_FRAC(1,6), RGN_FRAC(0,6) },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8
};


static GFXDECODE_START( cybstorm )
	GFXDECODE_ENTRY( "gfx2", 0, pflayout,     0, 16 )       /* sprites & playfield */
	GFXDECODE_ENTRY( "gfx3", 0, molayout,  4096, 64 )       /* sprites & playfield */
	GFXDECODE_ENTRY( "gfx1", 0, anlayout, 16384, 64 )       /* characters 8x8 */
GFXDECODE_END



/*************************************
 *
 *  Machine driver
 *
 *************************************/

MACHINE_CONFIG_START(cybstorm_state::round2)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", M68EC020, ATARI_CLOCK_14MHz)
	MCFG_CPU_PROGRAM_MAP(main_map)
	MCFG_DEVICE_VBLANK_INT_DRIVER("screen", atarigen_state, video_int_gen)

	MCFG_MACHINE_RESET_OVERRIDE(cybstorm_state,cybstorm)

	MCFG_EEPROM_2816_ADD("eeprom")
	MCFG_EEPROM_28XX_LOCK_AFTER_WRITE(true)

	MCFG_WATCHDOG_ADD("watchdog")

	/* video hardware */
	MCFG_ATARI_VAD_ADD("vad", "screen", WRITELINE(atarigen_state, scanline_int_write_line))
	MCFG_ATARI_VAD_PLAYFIELD(cybstorm_state, "gfxdecode", get_playfield_tile_info)
	MCFG_ATARI_VAD_PLAYFIELD2(cybstorm_state, "gfxdecode", get_playfield2_tile_info)
	MCFG_ATARI_VAD_ALPHA(cybstorm_state, "gfxdecode", get_alpha_tile_info)
	MCFG_ATARI_VAD_MOB(cybstorm_state::s_mob_config, "gfxdecode")

	MCFG_DEVICE_ADD("vadbank", ADDRESS_MAP_BANK, 0)
	MCFG_DEVICE_PROGRAM_MAP(vadbank_map)
	MCFG_ADDRESS_MAP_BANK_ENDIANNESS(ENDIANNESS_BIG)
	MCFG_ADDRESS_MAP_BANK_DATA_WIDTH(16)
	MCFG_ADDRESS_MAP_BANK_STRIDE(0x90000)

	MCFG_GFXDECODE_ADD("gfxdecode", "palette", cybstorm)
	MCFG_PALETTE_ADD("palette", 32768)
	MCFG_PALETTE_FORMAT(IRRRRRGGGGGBBBBB)

	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_PALETTE("palette")
	MCFG_SCREEN_VIDEO_ATTRIBUTES(VIDEO_UPDATE_BEFORE_VBLANK)
	/* note: these parameters are from published specs, not derived */
	/* the board uses an SOS-2 chip to generate video signals */
	MCFG_SCREEN_RAW_PARAMS(ATARI_CLOCK_14MHz/2, 456, 0, 336, 262, 0, 240)

	MCFG_SCREEN_UPDATE_DRIVER(cybstorm_state, screen_update_cybstorm)
	MCFG_VIDEO_START_OVERRIDE(cybstorm_state, cybstorm)
MACHINE_CONFIG_END


MACHINE_CONFIG_DERIVED(cybstorm_state::cybstorm, round2)
	/* sound hardware */
	MCFG_SPEAKER_STANDARD_STEREO("lspeaker", "rspeaker")

	MCFG_ATARI_JSA_IIIS_ADD("jsa", WRITELINE(atarigen_state, sound_int_write_line))
	MCFG_ATARI_JSA_TEST_PORT("9F0010", 22)
	MCFG_SOUND_ROUTE(0, "lspeaker", 1.0)
	MCFG_SOUND_ROUTE(1, "rspeaker", 1.0)
MACHINE_CONFIG_END



/*************************************
 *
 *  ROM definition(s)
 *
 *************************************/

ROM_START( cybstorm )
	ROM_REGION( 0x200000, "maincpu", 0 )    /* 6*128k for 68020 code */
	ROM_LOAD32_BYTE( "st_11.22.prog.6a",    0x000000, 0x080000, CRC(8b112ee9) SHA1(cd8367c47c653b8a1ba236c354f009f4297d521d) )
	ROM_LOAD32_BYTE( "st_11.22.prog.8a",    0x000001, 0x080000, CRC(36b7cec9) SHA1(c9c2ba6df1fc849200e0c66a7cbc292e8b0b22f3) )
	ROM_LOAD32_BYTE( "st_11.22.prog2.13a",  0x000002, 0x080000, CRC(1318f2c5) SHA1(929fbe96621852a10b7072490e1e554cdb2f20d8) )
	ROM_LOAD32_BYTE( "st_11.22.prog.16a",   0x000003, 0x080000, CRC(4ae586a8) SHA1(daa803ed38f6582677b397e744dd8f5f60cfb508) )

	ROM_REGION( 0x14000, "jsa:cpu", 0 ) /* 64k + 16k for 6502 code */
	ROM_LOAD( "st_11.22.6502",     0x010000, 0x004000, CRC(947421b2) SHA1(72b2b66122e779135f1f5af794e4d8513ccbbef6) )
	ROM_CONTINUE(             0x004000, 0x00c000 )

	ROM_REGION( 0x20000, "gfx1", 0 )
	ROM_LOAD( "st_11.22.csalph.6c",   0x000000, 0x020000, CRC(bafa4bbe) SHA1(c033a952fab6eb3a06c44ba7f48e58b20fe144f0) )

	ROM_REGION( 0x400000, "gfx2", ROMREGION_INVERT )
	ROM_LOAD( "st_11.22.pf0", 0x000000, 0x080000, CRC(0cf5874c) SHA1(1d739a3fc42baa5556aa22e051c873db9357396f) )
	ROM_LOAD( "st_11.22.pf1", 0x080000, 0x080000, CRC(ee0a6a81) SHA1(7c36ccbcd51497ea8a872ddf7dabe2ceb0895408) )
	ROM_LOAD( "st_11.22.pf2", 0x100000, 0x080000, CRC(03791514) SHA1(0688b55015f8d86ee92497cb7fcdfbdbfbc492a2) )
	ROM_LOAD( "st_11.22.pf3", 0x180000, 0x080000, CRC(8daf8d8f) SHA1(bfe90c789df5952f2e55c6cceebbf1285ea8d18e) )
	ROM_LOAD( "st_11.22.pf4", 0x200000, 0x080000, CRC(c0f759ab) SHA1(6acbc7c12669c89efeec9218eba03523748e4bf2) )
	ROM_LOAD( "st_11.22.pf5", 0x280000, 0x080000, CRC(921a080e) SHA1(7d3ef110569bacacfa269fab63777bf7ffb4c68e) )
	ROM_LOAD( "st_11.22.pf6", 0x300000, 0x080000, CRC(58b3c0d9) SHA1(226ff2e948c5bb0ca150700a2f3426492fce79f7) )
	ROM_LOAD( "st_11.22.pf7", 0x380000, 0x080000, CRC(f84b27ca) SHA1(a7812e18e15fad9992a59b0ebd177cb848a743bb) )

	ROM_REGION( 0xc00000, "gfx3", ROMREGION_INVERT )
	ROM_LOAD( "st_11.22.mo00",0x000000, 0x080000, CRC(216ffdb9) SHA1(7e6418da1419d82e67bef9ae314781708ed62a76) )
	ROM_LOAD( "st_11.22.mo01",0x200000, 0x080000, CRC(af15908b) SHA1(9dc8dbf0288a084891bdd646cfb7b8c97b89cf2e) )
	ROM_LOAD( "st_11.22.mo02",0x400000, 0x080000, CRC(fc066982) SHA1(bbf258ff23619234cb31b4afab4eac1681cdeae0) )
	ROM_LOAD( "st_11.22.mo03",0x600000, 0x080000, CRC(95c85715) SHA1(585cdb3cadfcd205e7dfcf846230b404093cd018) )
	ROM_LOAD( "st_11.22.mo04",0x800000, 0x080000, CRC(f53cebc8) SHA1(91280f8bf9f2fbb977f3971324134ffd8eec11b9) )
	ROM_LOAD( "st_11.22.mo05",0xa00000, 0x080000, CRC(6c696989) SHA1(1a688252faa85a380fd639950068b28de0b50cdf) )
	ROM_LOAD( "st_11.22.mo10",0x080000, 0x080000, CRC(a65b00da) SHA1(12a482e58207ac6fc2f7a47da1162a38ea902a96) )
	ROM_LOAD( "st_11.22.mo11",0x280000, 0x080000, CRC(11da3f44) SHA1(18f228524e2a00655f4e965208f99d892741d7cb) )
	ROM_LOAD( "st_11.22.mo12",0x480000, 0x080000, CRC(44257e7d) SHA1(307f350908b5f8d53495368e19a02e1042d9cb03) )
	ROM_LOAD( "st_11.22.mo13",0x680000, 0x080000, CRC(8ec4cc3e) SHA1(20e74ce2aa60cd2eb67a39b1bd8cf37454db4776) )
	ROM_LOAD( "st_11.22.mo14",0x880000, 0x080000, CRC(8f144f42) SHA1(6a35cf399775aaae50eeb35812fcf1343d9f9e1a) )
	ROM_LOAD( "st_11.22.mo15",0xa80000, 0x080000, CRC(3de4035a) SHA1(ecba9b464eb8b37a85a1c81ee4d7935a11646ed9) )
	ROM_LOAD( "st_11.22.mo20",0x100000, 0x080000, CRC(6f79ef90) SHA1(e323a7c35dac021c6b32870938dd54e865014078) )
	ROM_LOAD( "st_11.22.mo21",0x300000, 0x080000, CRC(69726b74) SHA1(31ba5ac1584ba6b63ec4a2189d531319db716690) )
	ROM_LOAD( "st_11.22.mo22",0x500000, 0x080000, CRC(5323d1f4) SHA1(65e81316467a3e5413ef480ba278a0a70e5c2f51) )
	ROM_LOAD( "st_11.22.mo23",0x700000, 0x080000, CRC(2387c947) SHA1(26d0f1ee83c5e84df4d143d6d3fd422f34d0d9af) )
	ROM_LOAD( "st_11.22.mo24",0x900000, 0x080000, CRC(2f133ccf) SHA1(a280879de200eb10e495e8ca804515c0e7d02eb9) )
	ROM_LOAD( "st_11.22.mo25",0xb00000, 0x080000, CRC(0746b2c5) SHA1(5414c3e600352cfb88d521ed3252941893afe438) )
	ROM_LOAD( "st_11.22.mo30",0x180000, 0x080000, CRC(7c0f2a9b) SHA1(82a405203356643529017201797b37efb7d1b227) )
	ROM_LOAD( "st_11.22.mo31",0x380000, 0x080000, CRC(8c21a84c) SHA1(7e84afb2b72af32af731df25fa1dd4d5f4dde7ba) )
	ROM_LOAD( "st_11.22.mo32",0x580000, 0x080000, CRC(a7382479) SHA1(c694e4ca4111252c3b23e274695ed235dd8a92e2) )
	ROM_LOAD( "st_11.22.mo33",0x780000, 0x080000, CRC(eac63276) SHA1(d72e85c46ce609b6275280c8cd73ed93aa6eddc3) )
	ROM_LOAD( "st_11.22.mo34",0x980000, 0x080000, CRC(b8b0c8b6) SHA1(f47218a4d94aa151964687a6e4c02f2b3065fdd3) )
	ROM_LOAD( "st_11.22.mo35",0xb80000, 0x080000, CRC(f0b9cf9d) SHA1(7ce30b05c1ee02346e8f568f36274b46d1ed99c4) )

	ROM_REGION( 0x100000, "jsa:oki1", 0 )   /* 1MB for ADPCM */
	ROM_LOAD( "st_11.22.5a",    0x000000, 0x080000, CRC(d469692c) SHA1(b7d94c042cf9f28ea65d44f5305d56459562d209) )

	ROM_REGION( 0x100000, "jsa:oki2", 0 )    /* 1MB for ADPCM */
	ROM_LOAD( "st_11.22.5a",    0x000000, 0x080000, CRC(d469692c) SHA1(b7d94c042cf9f28ea65d44f5305d56459562d209) )
ROM_END



/*************************************
 *
 *  Driver initialization
 *
 *************************************/

DRIVER_INIT_MEMBER(cybstorm_state, cybstorm)
{

}



/*************************************
 *
 *  Game driver(s)
 *
 *************************************/

GAME( 1993, cybstorm, 0, cybstorm, cybstorm, cybstorm_state, cybstorm, ROT0, "Atari Games", "Cyberstorm (prototype)", MACHINE_SUPPORTS_SAVE )
