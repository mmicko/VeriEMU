// license:BSD-3-Clause
// copyright-holders:Robbbert
/*************************************************************************************************


    The Kaypro 2/83 computer - the very first Kaypro II - 2 full height floppy drives.
    Each disk was single sided, and could hold 191k. The computer had 2x pio
    and 1x sio. One of the sio ports communicated with the keyboard with a coiled
    telephone cord, complete with modular plug on each end. The keyboard carries
    its own Intel 87C51 processor and is an intelligent device.

    Kaypro 10 notes:
    - This machine comes with a 10MB hard drive, split into 2 5MB partitions. It also
      has one floppy drive. The drive letters change depending on what drive it was booted
      from. The boot drive is always A:.
      If booted from floppy:
      A: floppy
      B: HD partition 1
      C: HD partition 2
      If booted from HD (presumably partition 1)
      A: HD partition 1
      B: HD partition 2
      C: floppy

    ToDo:
    - Find original Kaycomp ROMs
    - Need dump of 87C51 cpu in a Kaypro II keyboard variant

    - Kaypro 2x, 4a: floppy not working "No operating system present on this disk"
    - Kaypro 10: Boots from floppy, but needs hard drive added.
    - "Univeral"-ROM 81-478A: Boots, but fails with CP/M "BDOS Error on A:", was working until MAME v0190
    - fix Kayplus 84 ROM screen corruption
    - Kaypro iip88, 484p88: works as a normal Kaypro 4, extra hardware not done
    - Kaypro Robie: has twin 2.6MB 5.25 floppy drives which we don't support, no software available
    - Handyman ROM is really an add-on board with 16K proprietory RAM

    - Hard Disk not emulated.
      The controller is a WD1002-HD0 (original version, for Winchester drives), HD is 10MB, e.g. Shugart 712

    - RTC type MM58167A to be added. Modem chips TMS99531, TMS99532 to be developed.

    - Once everything works, sort out parent and compat relationships.

**************************************************************************************************/

#include "emu.h"
#include "includes/kaypro.h"
#include "machine/kay_kbd.h"

#include "bus/rs232/rs232.h"
#include "machine/clock.h"
#include "machine/com8116.h"
#include "machine/z80sio.h"
#include "screen.h"
#include "softlist.h"
#include "speaker.h"


READ8_MEMBER( kaypro_state::kaypro484_87_r ) { return 0x7f; }    /* to bypass unemulated HD controller */

/***********************************************************

    Address Maps

************************************************************/

static ADDRESS_MAP_START( kaypro_map, AS_PROGRAM, 8, kaypro_state )
	AM_RANGE(0x0000, 0x2fff) AM_READ_BANK("bankr0") AM_WRITE_BANK("bankw0")
	AM_RANGE(0x3000, 0x3fff) AM_RAMBANK("bank3")
	AM_RANGE(0x4000, 0xffff) AM_RAM AM_REGION("rambank", 0x4000)
ADDRESS_MAP_END

static ADDRESS_MAP_START( kayproii_io, AS_IO, 8, kaypro_state )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x00, 0x03) AM_DEVWRITE("brg", com8116_device, stt_w)
	AM_RANGE(0x04, 0x07) AM_DEVREADWRITE("sio", z80sio_device, cd_ba_r, cd_ba_w)
	AM_RANGE(0x08, 0x0b) AM_DEVREADWRITE("z80pio_g", z80pio_device, read_alt, write_alt)
	AM_RANGE(0x0c, 0x0f) AM_DEVWRITE("brg", com8116_device, str_w)
	AM_RANGE(0x10, 0x13) AM_DEVREADWRITE("fdc", fd1793_device, read, write)
	AM_RANGE(0x1c, 0x1f) AM_DEVREADWRITE("z80pio_s", z80pio_device, read_alt, write_alt)
ADDRESS_MAP_END

static ADDRESS_MAP_START( kaypro484_io, AS_IO, 8, kaypro_state )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x00, 0x03) AM_DEVWRITE("brg", com8116_device, str_w)
	AM_RANGE(0x04, 0x07) AM_DEVREADWRITE("sio_1", z80sio_device, cd_ba_r, cd_ba_w)
	AM_RANGE(0x08, 0x0b) AM_DEVWRITE("brg", com8116_device, stt_w)
	AM_RANGE(0x0c, 0x0f) AM_DEVREADWRITE("sio_2", z80sio_device, ba_cd_r, ba_cd_w)
	AM_RANGE(0x10, 0x13) AM_DEVREADWRITE("fdc", fd1793_device, read, write)
	AM_RANGE(0x14, 0x17) AM_READWRITE(kaypro484_system_port_r,kaypro484_system_port_w)
	AM_RANGE(0x18, 0x1b) AM_DEVWRITE("cent_data_out", output_latch_device, write)
	AM_RANGE(0x1c, 0x1c) AM_READWRITE(kaypro484_status_r,kaypro484_index_w)
	AM_RANGE(0x1d, 0x1d) AM_DEVREAD("crtc", mc6845_device, register_r) AM_WRITE(kaypro484_register_w)
	AM_RANGE(0x1f, 0x1f) AM_READWRITE(kaypro484_videoram_r,kaypro484_videoram_w)

	/* The below are not emulated */
/*  AM_RANGE(0x20, 0x23) AM_DEVREADWRITE("z80pio", kaypro484_pio_r, kaypro484_pio_w) - for RTC and Modem
    AM_RANGE(0x24, 0x27) communicate with MM58167A RTC. Modem uses TMS99531 and TMS99532 chips.
    AM_RANGE(0x80, 0x80) Hard drive controller card I/O port - 10MB hard drive only fitted to the Kaypro 10
    AM_RANGE(0x81, 0x81) Hard Drive READ error register, WRITE precomp
    AM_RANGE(0x82, 0x82) Hard Drive Sector register count I/O
    AM_RANGE(0x83, 0x83) Hard Drive Sector register number I/O
    AM_RANGE(0x84, 0x84) Hard Drive Cylinder low register I/O
    AM_RANGE(0x85, 0x85) Hard Drive Cylinder high register I/O
    AM_RANGE(0x86, 0x86) Hard Drive Size / Drive / Head register I/O
    AM_RANGE(0x87, 0x87) Hard Drive READ status register, WRITE command register */
	AM_RANGE(0x20, 0x86) AM_NOP
	AM_RANGE(0x87, 0x87) AM_READ(kaypro484_87_r)
ADDRESS_MAP_END


static INPUT_PORTS_START(kaypro)
	// everything comes from the keyboard device
INPUT_PORTS_END


/***************************************************************

    F4 CHARACTER DISPLAYER

****************************************************************/
static const gfx_layout kayproii_charlayout =
{
	8, 8,                   /* 8 x 8 characters */
	256,                    /* 256 characters */
	1,                  /* 1 bits per pixel */
	{ 0 },                  /* no bitplanes */
	/* x offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8                 /* every char takes 8 bytes */
};

static const gfx_layout kaypro484_charlayout =
{
	8, 16,                  /* 8 x 16 characters */
	256,                    /* 256 characters */
	1,                  /* 1 bits per pixel */
	{ 0 },                  /* no bitplanes */
	/* x offsets */
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	/* y offsets */
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	8*16                    /* every char takes 16 bytes */
};

static GFXDECODE_START( kayproii )
	GFXDECODE_ENTRY( "chargen", 0x0000, kayproii_charlayout, 0, 1 )
GFXDECODE_END

static GFXDECODE_START( kaypro484 )
	GFXDECODE_ENTRY( "chargen", 0x0000, kaypro484_charlayout, 0, 1 )
GFXDECODE_END

/***************************************************************

    Interfaces

****************************************************************/

static const z80_daisy_config kayproii_daisy_chain[] =
{
	{ "sio" },          /* sio */
	{ "z80pio_s" },     /* System pio */
	{ "z80pio_g" },     /* General purpose pio */
	{ nullptr }
};

static const z80_daisy_config kaypro484_daisy_chain[] =
{
	{ "sio_1" },        /* sio for RS232C and keyboard */
	{ "sio_2" },        /* sio for serial printer and inbuilt modem */
	{ nullptr }
};


/***********************************************************

    Machine Driver

************************************************************/


static SLOT_INTERFACE_START( kaypro_floppies )
	SLOT_INTERFACE( "525ssdd", FLOPPY_525_SSDD )
	SLOT_INTERFACE( "525dd", FLOPPY_525_DD )
	SLOT_INTERFACE( "525qd", FLOPPY_525_QD )
SLOT_INTERFACE_END


MACHINE_CONFIG_START(kaypro_state::kayproii)
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", Z80, 20_MHz_XTAL / 8)
	MCFG_CPU_PROGRAM_MAP(kaypro_map)
	MCFG_CPU_IO_MAP(kayproii_io)
	MCFG_Z80_DAISY_CHAIN(kayproii_daisy_chain)

	MCFG_MACHINE_START_OVERRIDE(kaypro_state, kayproii )
	MCFG_MACHINE_RESET_OVERRIDE(kaypro_state, kaypro )

	/* video hardware */
	MCFG_SCREEN_ADD_MONOCHROME("screen", RASTER, rgb_t::green())
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(80*7, 24*10)
	MCFG_SCREEN_VISIBLE_AREA(0,80*7-1,0,24*10-1)
	MCFG_VIDEO_START_OVERRIDE(kaypro_state, kaypro )
	MCFG_SCREEN_UPDATE_DRIVER(kaypro_state, screen_update_kayproii)
	MCFG_SCREEN_PALETTE("palette")

	MCFG_GFXDECODE_ADD("gfxdecode", "palette", kayproii)
	MCFG_PALETTE_ADD_MONOCHROME("palette")

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("beeper", BEEP, 950) /* piezo-device needs to be measured */
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)

	/* devices */
	MCFG_QUICKLOAD_ADD("quickload", kaypro_state, kaypro, "com,cpm", 3)

	MCFG_DEVICE_ADD("kbd", KAYPRO_10_KEYBOARD, 0)
	MCFG_KAYPRO10KBD_RXD_CB(DEVWRITELINE("sio", z80sio_device, rxb_w))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("sio", z80sio_device, syncb_w))

	MCFG_CENTRONICS_ADD("centronics", centronics_devices, "printer")
	MCFG_CENTRONICS_BUSY_HANDLER(WRITELINE(kaypro_state, write_centronics_busy))

	MCFG_CENTRONICS_OUTPUT_LATCH_ADD("cent_data_out", "centronics")

	MCFG_RS232_PORT_ADD("serial", default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("sio", z80sio_device, rxa_w))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("sio", z80sio_device, synca_w))
	MCFG_RS232_CTS_HANDLER(DEVWRITELINE("sio", z80sio_device, ctsa_w))
	MCFG_RS232_DCD_HANDLER(DEVWRITELINE("sio", z80sio_device, dcda_w))

	MCFG_DEVICE_ADD("brg", COM8116, XTAL(5'068'800)) // WD1943, SMC8116
	MCFG_COM8116_FR_HANDLER(DEVWRITELINE("sio", z80sio_device, rxca_w))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("sio", z80sio_device, txca_w))
	MCFG_COM8116_FT_HANDLER(DEVWRITELINE("sio", z80sio_device, rxtxcb_w))

	MCFG_DEVICE_ADD("z80pio_g", Z80PIO, 20_MHz_XTAL / 8)
	MCFG_Z80PIO_OUT_INT_CB(INPUTLINE("maincpu", INPUT_LINE_IRQ0))
	MCFG_Z80PIO_OUT_PA_CB(DEVWRITE8("cent_data_out", output_latch_device, write))

	MCFG_DEVICE_ADD("z80pio_s", Z80PIO, 20_MHz_XTAL / 8)
	MCFG_Z80PIO_OUT_INT_CB(INPUTLINE("maincpu", INPUT_LINE_IRQ0))
	MCFG_Z80PIO_IN_PA_CB(READ8(kaypro_state, pio_system_r))
	MCFG_Z80PIO_OUT_PA_CB(WRITE8(kaypro_state, kayproii_pio_system_w))

	MCFG_DEVICE_ADD("sio", Z80SIO, 20_MHz_XTAL / 8)
	MCFG_Z80SIO_OUT_INT_CB(INPUTLINE("maincpu", INPUT_LINE_IRQ0))
	MCFG_Z80SIO_OUT_TXDA_CB(DEVWRITELINE("serial", rs232_port_device, write_txd))
	MCFG_Z80SIO_OUT_RTSA_CB(DEVWRITELINE("serial", rs232_port_device, write_rts))
	MCFG_Z80SIO_OUT_DTRA_CB(DEVWRITELINE("serial", rs232_port_device, write_dtr))
	MCFG_Z80SIO_OUT_TXDB_CB(DEVWRITELINE("kbd", kaypro_10_keyboard_device, txd_w))

	MCFG_FD1793_ADD("fdc", 20_MHz_XTAL / 20)
	MCFG_WD_FDC_INTRQ_CALLBACK(WRITELINE(kaypro_state, fdc_intrq_w))
	MCFG_WD_FDC_DRQ_CALLBACK(WRITELINE(kaypro_state, fdc_drq_w))
	MCFG_WD_FDC_FORCE_READY
	MCFG_FLOPPY_DRIVE_ADD("fdc:0", kaypro_floppies, "525ssdd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
	MCFG_FLOPPY_DRIVE_ADD("fdc:1", kaypro_floppies, "525ssdd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
	MCFG_SOFTWARE_LIST_ADD("flop_list","kayproii")
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(kaypro_state::kayproiv, kayproii)
	MCFG_DEVICE_REMOVE("z80pio_s")
	MCFG_DEVICE_ADD("z80pio_s", Z80PIO, 2500000)
	MCFG_Z80PIO_OUT_INT_CB(INPUTLINE("maincpu", INPUT_LINE_IRQ0))
	MCFG_Z80PIO_IN_PA_CB(READ8(kaypro_state, pio_system_r))
	MCFG_Z80PIO_OUT_PA_CB(WRITE8(kaypro_state, kayproiv_pio_system_w))
	MCFG_DEVICE_REMOVE("fdc:0")
	MCFG_DEVICE_REMOVE("fdc:1")
	MCFG_FLOPPY_DRIVE_ADD("fdc:0", kaypro_floppies, "525dd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
	MCFG_FLOPPY_DRIVE_ADD("fdc:1", kaypro_floppies, "525dd", floppy_image_device::default_floppy_formats)
MACHINE_CONFIG_END

MACHINE_CONFIG_START(kaypro_state::kaypro484)
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", Z80, 16_MHz_XTAL / 4)
	MCFG_CPU_PROGRAM_MAP(kaypro_map)
	MCFG_CPU_IO_MAP(kaypro484_io)
	MCFG_Z80_DAISY_CHAIN(kaypro484_daisy_chain)

	MCFG_MACHINE_RESET_OVERRIDE(kaypro_state, kaypro )

	/* video hardware */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_REFRESH_RATE(60)
	MCFG_SCREEN_VBLANK_TIME(ATTOSECONDS_IN_USEC(0))
	MCFG_SCREEN_SIZE(80*8, 25*16)
	MCFG_SCREEN_VISIBLE_AREA(0,80*8-1,0,25*16-1)
	MCFG_VIDEO_START_OVERRIDE(kaypro_state, kaypro )
	MCFG_SCREEN_UPDATE_DRIVER(kaypro_state, screen_update_kaypro484)

	MCFG_GFXDECODE_ADD("gfxdecode", "palette", kaypro484)
	MCFG_PALETTE_ADD("palette", 3)
	MCFG_PALETTE_INIT_OWNER(kaypro_state, kaypro)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("beeper", BEEP, 950) /* piezo-device needs to be measured */
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)

	/* devices */
	MCFG_MC6845_ADD("crtc", MC6845, "screen", 2000000) /* comes out of ULA - needs to be measured */
	MCFG_MC6845_SHOW_BORDER_AREA(false)
	MCFG_MC6845_CHAR_WIDTH(7)
	MCFG_MC6845_UPDATE_ROW_CB(kaypro_state, kaypro484_update_row)

	MCFG_QUICKLOAD_ADD("quickload", kaypro_state, kaypro, "com,cpm", 3)

	MCFG_DEVICE_ADD("kbd", KAYPRO_10_KEYBOARD, 0)
	MCFG_KAYPRO10KBD_RXD_CB(DEVWRITELINE("sio_1", z80sio_device, rxb_w))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("sio_1", z80sio_device, syncb_w))

	MCFG_CLOCK_ADD("kbdtxrxc", 4800)
	MCFG_CLOCK_SIGNAL_HANDLER(DEVWRITELINE("sio_1", z80sio_device, rxtxcb_w))

	MCFG_CENTRONICS_ADD("centronics", centronics_devices, "printer")
	MCFG_CENTRONICS_BUSY_HANDLER(WRITELINE(kaypro_state, write_centronics_busy))

	MCFG_CENTRONICS_OUTPUT_LATCH_ADD("cent_data_out", "centronics")

	MCFG_RS232_PORT_ADD("modem", default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("sio_1", z80sio_device, rxa_w))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("sio_1", z80sio_device, synca_w))
	MCFG_RS232_CTS_HANDLER(DEVWRITELINE("sio_1", z80sio_device, ctsa_w))
	MCFG_RS232_DCD_HANDLER(DEVWRITELINE("sio_1", z80sio_device, dcda_w))

	MCFG_RS232_PORT_ADD("serprn", default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("sio_2", z80sio_device, rxa_w))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("sio_2", z80sio_device, synca_w))
	MCFG_RS232_CTS_HANDLER(DEVWRITELINE("sio_2", z80sio_device, ctsa_w))

	MCFG_DEVICE_ADD("sio_1", Z80SIO, 16_MHz_XTAL / 4)
	MCFG_Z80SIO_OUT_INT_CB(INPUTLINE("maincpu", INPUT_LINE_IRQ0)) // FIXME: use a combiner
	MCFG_Z80SIO_OUT_TXDA_CB(DEVWRITELINE("modem", rs232_port_device, write_txd))
	MCFG_Z80SIO_OUT_RTSA_CB(DEVWRITELINE("modem", rs232_port_device, write_rts))
	MCFG_Z80SIO_OUT_DTRA_CB(DEVWRITELINE("modem", rs232_port_device, write_dtr))
	MCFG_Z80SIO_OUT_TXDB_CB(DEVWRITELINE("kbd", kaypro_10_keyboard_device, txd_w))

	MCFG_DEVICE_ADD("sio_2", Z80SIO, 16_MHz_XTAL / 4)   /* extra sio for modem and printer */
	MCFG_Z80SIO_OUT_INT_CB(INPUTLINE("maincpu", INPUT_LINE_IRQ0)) // FIXME: use a combiner
	MCFG_Z80SIO_OUT_TXDA_CB(DEVWRITELINE("serprn", rs232_port_device, write_txd))

	MCFG_DEVICE_ADD("brg", COM8116, XTAL(5'068'800)) // WD1943, SMC8116
	MCFG_COM8116_FR_HANDLER(DEVWRITELINE("sio_1", z80sio_device, rxca_w))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("sio_1", z80sio_device, txca_w))
	MCFG_COM8116_FT_HANDLER(DEVWRITELINE("sio_2", z80sio_device, rxca_w))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("sio_2", z80sio_device, txca_w))

	MCFG_FD1793_ADD("fdc", 16_MHz_XTAL / 16)
	MCFG_WD_FDC_INTRQ_CALLBACK(WRITELINE(kaypro_state, fdc_intrq_w))
	MCFG_WD_FDC_DRQ_CALLBACK(WRITELINE(kaypro_state, fdc_drq_w))
	MCFG_WD_FDC_FORCE_READY
	MCFG_FLOPPY_DRIVE_ADD("fdc:0", kaypro_floppies, "525dd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
	MCFG_FLOPPY_DRIVE_ADD("fdc:1", kaypro_floppies, "525dd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(kaypro_state::kaypro10, kaypro484)
	MCFG_DEVICE_REMOVE("fdc:1")  // only has 1 floppy drive
	// need to add hard drive & controller
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(kaypro_state::kaypronew2, kaypro484)
	MCFG_DEVICE_REMOVE("fdc:1")  // only has 1 floppy drive
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(kaypro_state::kaypro284, kaypro484)
	MCFG_DEVICE_REMOVE("fdc:0")
	MCFG_DEVICE_REMOVE("fdc:1")
	MCFG_FLOPPY_DRIVE_ADD("fdc:0", kaypro_floppies, "525ssdd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
	MCFG_FLOPPY_DRIVE_ADD("fdc:1", kaypro_floppies, "525ssdd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_SOUND(true)
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(kaypro_state::omni2, kayproiv)
	MCFG_SCREEN_MODIFY("screen")
	MCFG_SCREEN_UPDATE_DRIVER(kaypro_state, screen_update_omni2)
MACHINE_CONFIG_END

DRIVER_INIT_MEMBER( kaypro_state, kaypro )
{
	uint8_t *main = memregion("roms")->base();
	uint8_t *ram = memregion("rambank")->base();

	membank("bankr0")->configure_entry(1, &main[0x0000]);
	membank("bankr0")->configure_entry(0, &ram[0x0000]);
	membank("bank3")->configure_entry(1, &main[0x3000]);
	membank("bank3")->configure_entry(0, &ram[0x3000]);
	membank("bankw0")->configure_entry(0, &ram[0x0000]);
}


/***********************************************************

    Game driver

************************************************************/

/* The detested bios "universal rom" is part number 81-478 */

// Kaypro II
ROM_START(kayproii)
	/* The original board could take a 2716 or 2732 */
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "149", "81-149 for Kaypro Bd. 81-110")
	ROMX_LOAD("81-149.u47",   0x0000, 0x0800, CRC(28264bc1) SHA1(a12afb11a538fc0217e569bc29633d5270dfa51b), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "149b", "81-149B for Kaypro Bd. 81-110")
	ROMX_LOAD("81-149b.u47",  0x0000, 0x0800, CRC(c008549e) SHA1(b9346a16f5f9ffb6bb0eb1766c348b74056485a8), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "149c", "81-149C for Kaypro Bd. 81-110")
	ROMX_LOAD("81-149c.u47",  0x0000, 0x0800, CRC(1272aa65) SHA1(027fee2f5f17ba71a4738f00188e132e326536ff), ROM_BIOS(3) )
	ROM_SYSTEM_BIOS( 3, "232", "81-232 for Kaypro Bd. 81-240")
	ROMX_LOAD("81-232.u47",   0x0000, 0x1000, CRC(4918fb91) SHA1(cd9f45cc3546bcaad7254b92c5d501c40e2ef0b2), ROM_BIOS(4) )
	ROM_SYSTEM_BIOS( 4, "roadrunner", "Highland Microkit Roadrunner 1.5")
	ROMX_LOAD("kaypro_ii_roadrunner_1_5.bin",  0x0000, 0x1000, CRC(ca11357d) SHA1(8e8a6d6e0d31d1051db9a24601f12a3b4639b3bb), ROM_BIOS(5) ) // does not boot here but originally comes from a II
	ROM_SYSTEM_BIOS( 5, "turbo", "Advent Turbo ROM")
	ROMX_LOAD("trom34_3.rom",  0x0000, 0x1000, CRC(908a4c0e) SHA1(6e220479715a812d9116b0927a9ff2792f82b2a7), ROM_BIOS(6) )
	ROM_SYSTEM_BIOS( 6, "kplus83", "MICROCode Consulting KayPLUS 83")
	ROMX_LOAD("kplus83.rom",  0x0000, 0x2000, CRC(5e9b817d) SHA1(26ea875ee3659a964cbded4ed0c82a3af42db64b), ROM_BIOS(7) )
	ROM_SYSTEM_BIOS( 7, "pro8v3", "MicroCornucopia_Pro8_V3.3")
	ROMX_LOAD("pro8-3.rom",  0x0000, 0x1000, CRC(f2d4c598) SHA1(269b2fddeb98db3e5eba2056ff250dff72b0561e), ROM_BIOS(8) )
	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x0800, "chargen", ROMREGION_INVERT)
	ROM_LOAD("81-146.u43",   0x0000, 0x0800, CRC(4cc7d206) SHA1(5cb880083b94bd8220aac1f87d537db7cfeb9013) )
ROM_END

// Kaypro IV (or 4'83)
ROM_START(kayproiv)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "232", "81-232 for Kaypro Bd. 81-240")
	ROMX_LOAD("81-232.u47",   0x0000, 0x1000, CRC(4918fb91) SHA1(cd9f45cc3546bcaad7254b92c5d501c40e2ef0b2), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "roadrunner", "Highland Microkit Roadrunner 1.5")
	ROMX_LOAD("kaypro_ii_roadrunner_1_5.bin",  0x0000, 0x1000, CRC(ca11357d) SHA1(8e8a6d6e0d31d1051db9a24601f12a3b4639b3bb), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "turbo", "Advent Turbo ROM")
	ROMX_LOAD("trom34_3.rom",  0x0000, 0x1000, CRC(908a4c0e) SHA1(6e220479715a812d9116b0927a9ff2792f82b2a7), ROM_BIOS(3) )
	ROM_SYSTEM_BIOS( 3, "kplus83", "MICROCode Consulting KayPLUS 83")
	ROMX_LOAD("kplus83.rom",  0x0000, 0x2000, CRC(5e9b817d) SHA1(26ea875ee3659a964cbded4ed0c82a3af42db64b), ROM_BIOS(4) )
	ROM_SYSTEM_BIOS( 4, "pro8v3", "MicroCornucopia_Pro8_V3.3")
	ROMX_LOAD("pro8-3.rom",  0x0000, 0x1000, CRC(f2d4c598) SHA1(269b2fddeb98db3e5eba2056ff250dff72b0561e), ROM_BIOS(5) )
	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x0800, "chargen", ROMREGION_INVERT)
	ROM_LOAD("81-146.u43",   0x0000, 0x0800, CRC(4cc7d206) SHA1(5cb880083b94bd8220aac1f87d537db7cfeb9013) )
ROM_END

// Kaypro 10, '83 model, hard disk cable connector in the middle of the mainboard, no space for modem or RTC
ROM_START(kaypro10)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "188", "V1.9 for Kaypro Bd. 81-180")
	ROMX_LOAD("81-188.u42",   0x0000, 0x1000, CRC(6cbd6aa0) SHA1(47004f8c6e17407e4f8d613c9520f9316716d9e2), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "x", "V1.7")
	ROMX_LOAD("x.bin",        0x0000, 0x0fff, BAD_DUMP CRC(01e2e7b2) SHA1(fc2f8dc8a077d0c89a74463328efa1c444662d88), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "turbo", "Advent Turbo ROM")
	ROMX_LOAD("trom34.rom",   0x0000, 0x2000, CRC(0ec6d39a) SHA1(8c2a92b8642e144452c28300bf50a00a11a060cd), ROM_BIOS(3) )
	ROM_SYSTEM_BIOS( 3, "kplus83", "MICROCode Consulting KayPLUS 83")
	ROMX_LOAD("kplus83.rom",  0x0000, 0x2000, CRC(5e9b817d) SHA1(26ea875ee3659a964cbded4ed0c82a3af42db64b), ROM_BIOS(4) )
	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-187.u31",   0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

// Kaypro 10, '84 model, hard disk cable on the right hand side of the mainboard
ROM_START(kaypro1084)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "302", "V1.9E for Kaypro Bd. 81-181")
	ROMX_LOAD("81-302.u42",   0x0000, 0x1000, CRC(3f9bee20) SHA1(b29114a199e70afe46511119b77a662e97b093a0), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "1.9ee", "V1.9ee")
	ROMX_LOAD("rom19ee.bin",  0x0000, 0x0fee, BAD_DUMP CRC(c3515bd0) SHA1(48a0a43c164e4d3e75e8e916498421ef616943cf), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "277", "V1.9E(F)")
	ROMX_LOAD("81-277.u42",   0x0000, 0x1000, CRC(e4e1831f) SHA1(1de31ed532a461ace7a4abad1f6647eeddceb3e7), ROM_BIOS(3) )
	ROM_SYSTEM_BIOS( 3, "478", "V2.01 for Kaypro Bd. 81-582 (universal)")
	ROMX_LOAD("81-478.u42",   0x0000, 0x2000, CRC(de618380) SHA1(c8d6312e6eeb62a53e741f1ff3b878bdcb7b5aaa), ROM_BIOS(4) )
	ROM_SYSTEM_BIOS( 4, "turbo", "Advent Turbo ROM")
	ROMX_LOAD("trom34.rom",   0x0000, 0x2000, CRC(0ec6d39a) SHA1(8c2a92b8642e144452c28300bf50a00a11a060cd), ROM_BIOS(5) )
	ROM_SYSTEM_BIOS( 5, "kplus", "MICROCode Consulting KayPLUS 84")
	ROMX_LOAD("kplus84.rom",   0x0000, 0x2000, CRC(4551905a) SHA1(48f0964edfad05b214810ae5595638245c30e5c0), ROM_BIOS(6) )

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-187.u31",   0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

ROM_START(kaypro484) // later renamed in 2X (or 2X MTC to signify the inclusion of Modem and RTC in comparison with the "old" 2X)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "292a", "81-292a for Kaypro Bd. 81-184")
	ROMX_LOAD("81-292a.u34",  0x0000, 0x1000, CRC(241f27a5) SHA1(82711289d19e9b165e35324da010466d225e503a), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "turbo", "Advent Turbo ROM")
	ROMX_LOAD("trom34.rom",   0x0000, 0x2000, CRC(0ec6d39a) SHA1(8c2a92b8642e144452c28300bf50a00a11a060cd), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "kplus", "MICROCode Consulting KayPLUS 84")
	ROMX_LOAD("kplus84.rom",   0x0000, 0x2000, CRC(4551905a) SHA1(48f0964edfad05b214810ae5595638245c30e5c0), ROM_BIOS(3) )
	ROM_SYSTEM_BIOS( 3, "pro884", "MicroCornucopia pro884 SuperMax 2.7")
	ROMX_LOAD("pro884mx.rom",   0x0000, 0x2000, CRC(febc6f51) SHA1(1f009aa9b7c9a3eddd0ee6ea7321a1c47c3e9807), ROM_BIOS(4) )
	ROM_SYSTEM_BIOS( 4, "pro8v5", "MicroCornucopia Pro8 V5")
	ROMX_LOAD("pro884v5.rom",  0x0000, 0x2000, CRC(fe0051b1) SHA1(cac429154d40e21174ae05ceb0017b62473cdebd), ROM_BIOS(5) )

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-235.u9",    0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

 // Kaypro 2'84, like the 4'84, but two single sided drives, no RTC, no modem
ROM_START(kaypro284)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "292a", "81-292a for Kaypro Bd. 81-184")
	ROMX_LOAD("81-292a.u34",  0x0000, 0x1000, CRC(241f27a5) SHA1(82711289d19e9b165e35324da010466d225e503a), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "turbo", "Advent Turbo ROM")
	ROMX_LOAD("trom34.rom",   0x0000, 0x2000, CRC(0ec6d39a) SHA1(8c2a92b8642e144452c28300bf50a00a11a060cd), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "kplus", "MICROCode Consulting KayPLUS 84")
	ROMX_LOAD("kplus84.rom",   0x0000, 0x2000, CRC(4551905a) SHA1(48f0964edfad05b214810ae5595638245c30e5c0), ROM_BIOS(3) )
	ROM_SYSTEM_BIOS( 3, "pro884", "MicroCornucopia pro884 SuperMax 2.7")
	ROMX_LOAD("pro884mx.rom",   0x0000, 0x2000, CRC(febc6f51) SHA1(1f009aa9b7c9a3eddd0ee6ea7321a1c47c3e9807), ROM_BIOS(4) )
	ROM_SYSTEM_BIOS( 4, "pro8v5", "MicroCornucopia Pro8 V5")
	ROMX_LOAD("pro884v5.rom",  0x0000, 0x2000, CRC(fe0051b1) SHA1(cac429154d40e21174ae05ceb0017b62473cdebd), ROM_BIOS(5) )

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-235.u9",    0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

// Kaypro 2X, a 4'84 without modem and RTC, later the 4'84 is renamed 2X, this fully decked out variant is called 2X MTC
ROM_START(kaypro2x)
	ROM_REGION(0x8000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "292a", "81-292a for Kaypro Bd. 81-184")
	ROMX_LOAD("81-292a.u34",  0x0000, 0x1000, CRC(241f27a5) SHA1(82711289d19e9b165e35324da010466d225e503a), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "292", "V2.00 (early universal)" )
	ROMX_LOAD("81-292.u34",   0x0000, 0x2000, CRC(5eb69aec) SHA1(525f955ca002976e2e30ac7ee37e4a54f279fe96), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "478", "V2.01 for Kaypro Bd. 81-580 (universal)")
	ROMX_LOAD("81-478.u42",   0x0000, 0x2000, CRC(de618380) SHA1(c8d6312e6eeb62a53e741f1ff3b878bdcb7b5aaa), ROM_BIOS(3) )
	ROM_SYSTEM_BIOS( 3, "turbo", "Advent Turbo ROM")
	ROMX_LOAD("trom34.rom",   0x0000, 0x2000, CRC(0ec6d39a) SHA1(8c2a92b8642e144452c28300bf50a00a11a060cd), ROM_BIOS(4) )
	ROM_SYSTEM_BIOS( 4, "kplus", "MICROCode Consulting KayPLUS 84")
	ROMX_LOAD("kplus84.rom",   0x0000, 0x2000, CRC(4551905a) SHA1(48f0964edfad05b214810ae5595638245c30e5c0), ROM_BIOS(5) )
	ROM_SYSTEM_BIOS( 5, "pro884", "MicroCornucopia pro884 SuperMax 2.7")
	ROMX_LOAD("pro884mx.rom",   0x0000, 0x2000, CRC(febc6f51) SHA1(1f009aa9b7c9a3eddd0ee6ea7321a1c47c3e9807), ROM_BIOS(6) )
	ROM_SYSTEM_BIOS( 6, "pro8v5", "MicroCornucopia Pro8 V5")
	ROMX_LOAD("pro884v5.rom",  0x0000, 0x2000, CRC(fe0051b1) SHA1(cac429154d40e21174ae05ceb0017b62473cdebd), ROM_BIOS(7) )
	ROM_SYSTEM_BIOS( 7, "handyman", "Hitech Research Handyman")                                                             // http://content.thetechnickel.com/misc/kaypro-handyman/kaypro-4-plus-88-06.jpg
	ROMX_LOAD( "handyman.bin", 0x0000, 0x8000, CRC(f020d82c) SHA1(576a6608270d4ec7cf814c9de46ecf4e2869d30a), ROM_BIOS(8) )  // fits any classic Kaypro, needs its own 16K RAM

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-235.u9",    0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

// Kaypro II'84 plus 88, the "KAYPRO-88" board has 128k or 256k of its own ram on it, it's a factory installed SWP CoPower 88
ROM_START(kayproiip88)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "232", "81-232 for Kaypro Bd. 81-240")
	ROMX_LOAD("81-232.u47",   0x0000, 0x1000, CRC(4918fb91) SHA1(cd9f45cc3546bcaad7254b92c5d501c40e2ef0b2), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "kplus83", "MICROCode Consulting KayPLUS 83")
	ROMX_LOAD("kplus83.rom",  0x0000, 0x2000, CRC(5e9b817d) SHA1(26ea875ee3659a964cbded4ed0c82a3af42db64b), ROM_BIOS(2) )
	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x0800, "chargen", ROMREGION_INVERT)
	ROM_LOAD("81-146.u43",   0x0000, 0x0800, CRC(4cc7d206) SHA1(5cb880083b94bd8220aac1f87d537db7cfeb9013) )

	ROM_REGION(0x1000, "8088cpu",0)
	ROM_LOAD("81-356.u29",   0x0000, 0x1000, CRC(948556db) SHA1(6e779866d099cc0dc8c6369bdfb37a923ac448a4) )
ROM_END

// Kaypro 4'84 plus 88, the "KAYPRO-88" board has 128k or 256k of its own ram on it, it's a factory installed SWP CoPower 88
ROM_START(kaypro484p88)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "292a", "292A")
	ROMX_LOAD("81-292a.u34",  0x0000, 0x1000, CRC(241f27a5) SHA1(82711289d19e9b165e35324da010466d225e503a), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "kplus", "MICROCode Consulting KayPLUS 84")
	ROMX_LOAD("kplus84.rom",   0x0000, 0x2000, CRC(4551905a) SHA1(48f0964edfad05b214810ae5595638245c30e5c0), ROM_BIOS(2) )

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-235.u9",    0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

// Kaypro New 2, no modem, no RTC, single DS/DD disk drive
ROM_START(kaypronew2)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "478", "V2.01 for Kaypro Bd. 81-294 (universal")
	ROMX_LOAD("81-478.u42",   0x0000, 0x2000, CRC(de618380) SHA1(c8d6312e6eeb62a53e741f1ff3b878bdcb7b5aaa), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "kplus", "MICROCode Consulting KayPLUS 84")
	ROMX_LOAD("kplus84.rom",   0x0000, 0x2000, CRC(4551905a) SHA1(48f0964edfad05b214810ae5595638245c30e5c0), ROM_BIOS(2) )

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-235.u9",    0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

// "Desktop" PC with two high density, 2.8MB floppy disk drivers over the monitor
ROM_START(robie)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "326", "V1.7R")
	ROMX_LOAD("81-326.u34",   0x0000, 0x2000, CRC(7f0c3f68) SHA1(54b088a1b2200f9df4b9b347bbefb0115f3a4976), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "u", "V1.4")
	ROMX_LOAD("robie_u.u34",  0x0000, 0x2000, CRC(da7248b5) SHA1(1dc053b3e44ead47255cc166b7b4b0adaeb3dd3d), ROM_BIOS(2) ) // rom number unknown

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-235.u9",    0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

// Kaypro 4X, a Robie in the standard portable Kaypro enclosure
ROM_START(kaypro4x)
	ROM_REGION(0x4000, "roms",0)
	ROM_LOAD("81-326.u34",   0x0000, 0x2000, CRC(7f0c3f68) SHA1(54b088a1b2200f9df4b9b347bbefb0115f3a4976) )

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-235.u9",    0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

// Kaypro 1, equivalent to "old" 2X, before 4'84 became 2X
ROM_START(kaypro1)
	ROM_REGION(0x4000, "roms",0)
	ROM_SYSTEM_BIOS( 0, "478", "V2.01 for Kaypro Bd. 81-294 (universal")
	ROMX_LOAD("81-478.u42",   0x0000, 0x2000, CRC(de618380) SHA1(c8d6312e6eeb62a53e741f1ff3b878bdcb7b5aaa), ROM_BIOS(1) )
	ROM_SYSTEM_BIOS( 1, "turbo", "Advent Turbo ROM")
	ROMX_LOAD("trom34.rom",   0x0000, 0x2000, CRC(0ec6d39a) SHA1(8c2a92b8642e144452c28300bf50a00a11a060cd), ROM_BIOS(2) )
	ROM_SYSTEM_BIOS( 2, "kplus", "MICROCode Consulting KayPLUS 84")
	ROMX_LOAD("kplus84.rom",   0x0000, 0x2000, CRC(4551905a) SHA1(48f0964edfad05b214810ae5595638245c30e5c0), ROM_BIOS(3) )

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen",0)
	ROM_LOAD("81-235.u9",    0x0000, 0x1000, CRC(5f72da5b) SHA1(8a597000cce1a7e184abfb7bebcb564c6bf24fb7) )
ROM_END

// Omni II logic analyzer
ROM_START(omni2)
	ROM_REGION(0x4000, "roms",0)
	ROM_LOAD("omni2.u47",    0x0000, 0x1000, CRC(2883f9e0) SHA1(d98c784e62853582d298bf7ca84c75872847ac9b) )

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x0800, "chargen", ROMREGION_INVERT)
	ROM_LOAD("omni2.u43",    0x0000, 0x0800, CRC(049b3381) SHA1(46f1d4f038747ba9048b075dc617361be088f82a) )
ROM_END

// Omni 4 logic analyzer
ROM_START(omni4)
	ROM_REGION(0x4000, "roms",0)
	ROM_LOAD("omni4.u34",    0x0000, 0x2000, CRC(f24e8521) SHA1(374f2e2b791a807f103744a22c9c8f3af55f1033) )

	ROM_REGION(0x10000, "rambank", ROMREGION_ERASEFF)

	ROM_REGION(0x1000, "chargen", 0)
	ROM_LOAD("omni4.u9",    0x0000, 0x1000, CRC(579665a6) SHA1(261fcdc5a44821de9484340cbe429110400140b4) )
ROM_END


/*    YEAR  NAME         PARENT     COMPAT  MACHINE    INPUT   CLASS         INIT    COMPANY                FULLNAME */
COMP( 1982, kayproii,    0,         0,      kayproii,  kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro II - 2/83" , 0 )
COMP( 1983, kayproiv,    kayproii,  0,      kayproiv,  kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro IV - 4/83" , 0 ) // model 81-004
COMP( 1983, kaypro10,    0,         0,      kaypro10,  kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro 10 - 1983", 0 )
COMP( 1983, kayproiip88, kayproii,  0,      kayproii,  kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro 4 plus88 - 4/83" , MACHINE_NOT_WORKING ) // model 81-004 with an added 8088 daughterboard and rom
COMP( 1984, kaypro484,   0,         0,      kaypro484, kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro 4/84" , MACHINE_NOT_WORKING ) // model 81-015
COMP( 1984, kaypro284,   kaypro484, 0,      kaypro284, kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro 2/84" , MACHINE_NOT_WORKING ) // model 81-015
COMP( 1984, kaypro484p88,kaypro484, 0,      kaypro484, kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro 4/84 plus88", MACHINE_NOT_WORKING ) // model 81-015 with an added 8088 daughterboard and rom
COMP( 1984, kaypro1084,  kaypro10,  0,      kaypro10,  kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro 10" , MACHINE_NOT_WORKING ) // model 81-005
COMP( 1984, robie,       0,         0,      kaypro484, kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro Robie" , MACHINE_NOT_WORKING )
COMP( 1985, kaypro2x,    kaypro484, 0,      kaypro484, kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro 2x" , MACHINE_NOT_WORKING ) // model 81-025
COMP( 1985, kaypronew2,  0,         0,      kaypronew2,kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro New 2", MACHINE_NOT_WORKING )
COMP( 1985, kaypro4x,    robie,     0,      kaypro484, kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro 4x" , MACHINE_NOT_WORKING )
COMP( 1986, kaypro1,     kaypro484, 0,      kaypro484, kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Kaypro 1", MACHINE_NOT_WORKING )
COMP( 198?, omni2,       kayproii,  0,      omni2,     kaypro, kaypro_state, kaypro, "Non Linear Systems",  "Omni II Logic Analyzer" , 0 )
COMP( 198?, omni4,       kaypro484, 0,      kaypro484, kaypro, kaypro_state, kaypro, "Omni Logic Inc.",     "Omni 4 Logic Analyzer" , MACHINE_NOT_WORKING )
