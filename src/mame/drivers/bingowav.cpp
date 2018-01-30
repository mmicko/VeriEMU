// license:BSD-3-Clause
// copyright-holders:AJR
/***************************************************************************

    Taito Bingo Wave

    This is a skeleton driver for what appears to be a satellite betting
    game similar to Sega's Caribbean Boule.

    "MAIN PCB"
    - Toshiba TMP68303F-16 (IC26)
    - Mitsubishi M66220SP (IC19)
    - Unpopulated space for 68000 (IC36)
    - TEL TE7750 (IC38, IC47)
    - Taito TC0140SYT (IC68)
    - Zilog Z80-A (IC74)
    - Yamaha YM2610 (IC77)
    - ??MHz XTAL (OSC1)

    "TERMINAL PCB"
    - Toshiba TMP68303F-16 (IC35)
    - Taito TC0650FDA (IC26)
    - Unpopulated space for 68000-16 (IC41)
    - TEL TE7750 (IC42)
    - Taito TCO470LIN (IC33)
    - Taito TC0600OBT (IC11)
    - Yamaha YM2149F (IC53)
    - 36MHz XTAL (OSC1)
    - 32MHz XTAL (OSC2)
    - DIP switches (1x8)
    - JAMMA edge connector

    "CONTROL PCB"
    - Taito PC060HA (IC2)
    - Zilog Z80 (IC13)
    - 16MHz XTAL (OSC1)
    - DIP switches (1x8)

***************************************************************************/

#include "emu.h"
#include "speaker.h"
#include "cpu/m68000/m68000.h"
#include "cpu/z80/z80.h"
#include "machine/te7750.h"
#include "machine/tmp68301.h"
#include "sound/2610intf.h"
#include "audio/taitosnd.h"

class bingowav_state : public driver_device
{
public:
	bingowav_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
	{ }

	void bingowav(machine_config &config);
protected:
	virtual void machine_start() override;

private:
	required_device<cpu_device> m_maincpu;
};


void bingowav_state::machine_start()
{
}


static ADDRESS_MAP_START( bingowav_main_map, AS_PROGRAM, 16, bingowav_state )
	AM_RANGE(0x000000, 0x03ffff) AM_ROM
	AM_RANGE(0x100000, 0x10ffff) AM_RAM
	AM_RANGE(0x120000, 0x12001f) AM_DEVREADWRITE8("mainioh", te7750_device, read, write, 0xff00)
	AM_RANGE(0x120000, 0x12001f) AM_DEVREADWRITE8("mainiol", te7750_device, read, write, 0x00ff)
	AM_RANGE(0x140000, 0x140001) AM_READNOP
	AM_RANGE(0x150000, 0x150001) AM_DEVWRITE8("tc0140syt", tc0140syt_device, master_port_w, 0x00ff)
	AM_RANGE(0x150002, 0x150003) AM_DEVREADWRITE8("tc0140syt", tc0140syt_device, master_comm_r, master_comm_w, 0x00ff)
	AM_RANGE(0xfffc00, 0xffffff) AM_DEVREADWRITE("maintmp", tmp68301_device, regs_r, regs_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START( bingowav_audio_map, AS_PROGRAM, 8, bingowav_state )
	AM_RANGE(0x0000, 0xbfff) AM_ROM
	AM_RANGE(0xc000, 0xdfff) AM_RAM
	AM_RANGE(0xe000, 0xe003) AM_DEVREADWRITE("ymsnd", ym2610_device, read, write)
	AM_RANGE(0xe200, 0xe200) AM_DEVWRITE("tc0140syt", tc0140syt_device, slave_port_w)
	AM_RANGE(0xe201, 0xe201) AM_DEVREADWRITE("tc0140syt", tc0140syt_device, slave_comm_r, slave_comm_w)
	AM_RANGE(0xf200, 0xf200) AM_WRITENOP
ADDRESS_MAP_END

static ADDRESS_MAP_START( bingowav_drive_map, AS_PROGRAM, 16, bingowav_state )
	AM_RANGE(0x000000, 0x01ffff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( bingowav_control_map, AS_PROGRAM, 8, bingowav_state )
	AM_RANGE(0x0000, 0x3fff) AM_ROM
	AM_RANGE(0xc000, 0xdfff) AM_RAM
	AM_RANGE(0xf000, 0xf000) AM_WRITENOP
ADDRESS_MAP_END


static INPUT_PORTS_START( bingowav )
INPUT_PORTS_END


MACHINE_CONFIG_START(bingowav_state::bingowav)
	MCFG_CPU_ADD("maincpu", M68000, 12000000) // actually TMP63803F-16
	MCFG_CPU_PROGRAM_MAP(bingowav_main_map)

	MCFG_DEVICE_ADD("maintmp", TMP68301, 0) // wrong
	MCFG_TMP68301_CPU("maincpu")

	MCFG_DEVICE_ADD("mainioh", TE7750, 0)
	MCFG_TE7750_IOS_CB(CONSTANT(5))

	MCFG_DEVICE_ADD("mainiol", TE7750, 0)
	MCFG_TE7750_IOS_CB(CONSTANT(4))

	MCFG_CPU_ADD("audiocpu", Z80, 4000000)
	MCFG_CPU_PROGRAM_MAP(bingowav_audio_map)

	MCFG_SPEAKER_STANDARD_MONO("mono")

	MCFG_SOUND_ADD("ymsnd", YM2610, 8000000)
	MCFG_YM2610_IRQ_HANDLER(INPUTLINE("audiocpu", 0))
	MCFG_SOUND_ROUTE(0, "mono", 0.25)
	MCFG_SOUND_ROUTE(1, "mono", 1.0)
	MCFG_SOUND_ROUTE(2, "mono", 1.0)

	MCFG_DEVICE_ADD("tc0140syt", TC0140SYT, 0)
	MCFG_TC0140SYT_MASTER_CPU("maincpu")
	MCFG_TC0140SYT_SLAVE_CPU("audiocpu")

	MCFG_CPU_ADD("termcpu", M68000, 12000000) // actually TMP63803F-16
	MCFG_CPU_PROGRAM_MAP(bingowav_drive_map)
	MCFG_DEVICE_DISABLE()

	MCFG_CPU_ADD("ctrlcpu", Z80, XTAL(16'000'000) / 4)
	MCFG_CPU_PROGRAM_MAP(bingowav_control_map)
MACHINE_CONFIG_END


ROM_START( bingowav )
	// Program and graphics ROMs from the "Terminal PCB" are currently missing
	ROM_REGION( 0x40000, "maincpu", 0 )
	ROM_LOAD16_BYTE( "prg-h.ic35", 0x00000, 0x20000, CRC(0ae8c738) SHA1(c040c4f46089a07ef3c4d5067068ea3cd1546e4e) )
	ROM_LOAD16_BYTE( "prg-l.ic42", 0x00001, 0x20000, CRC(a7742e54) SHA1(2ab75c51ced0bb37b50f5077b611cde1770e2411) )

	ROM_REGION( 0x10000, "audiocpu", 0 )
	ROM_LOAD( "ic66", 0x00000, 0x10000, CRC(9fa09b9a) SHA1(e333b11129141649a18906120805842c2dcccd05) )

	ROM_REGION( 0x80000, "ymsnd", 0 )
	ROM_LOAD( "ic69", 0x00000, 0x80000, CRC(5d86d5bd) SHA1(81ebb63f984690e63e45c8b1fe4251fac8d5cf04) )

	ROM_REGION( 0x40000, "termcpu", 0 )
	ROM_LOAD16_BYTE( "prg-h.ic25", 0x00000, 0x20000, NO_DUMP )
	ROM_LOAD16_BYTE( "prg-l.ic20", 0x00001, 0x20000, NO_DUMP )

	ROM_REGION( 0x20000, "ctrlcpu", 0 )
	ROM_LOAD( "ic1", 0x00000, 0x20000, CRC(d257bbaf) SHA1(9d1414594dc1c1fca256ab5b6039e733bff1414a) )
ROM_END


GAME( 1994, bingowav, 0, bingowav, bingowav, bingowav_state, 0, ROT0, "Taito", "Bingo Wave", MACHINE_IS_SKELETON )
