// license:BSD-3-Clause
// copyright-holders:David Haywood
/******************************************************************************************************************************

has a sticker marked
Part # 04-0008
Main PCB  w/ Dark-Slide
Serial# : 0115

2x Z80



There were several different designs for this, it's possible they used
different speech roms etc.

ToDo:
- work out how the cpus communicate (there's an interrupt handler in main cpu)
- clear up the speech
- inputs
- mechanical matters (camera, printer, etc)

At the moment it simply outputs all the speech strings, one after the other, then stops.

*****************************************************************************************************************************/

#include "emu.h"
#include "cpu/z80/z80.h"
#include "machine/clock.h"
#include "sound/dac.h"
#include "sound/volt_reg.h"
#include "speaker.h"

class instantm_state : public driver_device
{
public:
	instantm_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
	{ }

	DECLARE_READ8_MEMBER(port01_r);
	DECLARE_WRITE8_MEMBER(port01_w);
	DECLARE_WRITE_LINE_MEMBER(clock_w);

	void instantm(machine_config &config);
private:
	u8 m_port01;
	bool m_clock_en;
	virtual void machine_start() override;
	virtual void machine_reset() override;
	required_device<cpu_device> m_maincpu;
};

// return instruction from main cpu
READ8_MEMBER( instantm_state::port01_r )
{
	return m_port01;
}

// tell maincpu the speech is done
WRITE8_MEMBER( instantm_state::port01_w )
{
	// bump to next bit of speech for now
	if ((m_port01 & 15) < 15)
		m_port01++;
	else
		m_clock_en = false;
}

// clock out the speech bytes
WRITE_LINE_MEMBER( instantm_state::clock_w )
{
	if (m_clock_en)
		m_port01 ^= 0x80;
}



static ADDRESS_MAP_START( main_map, AS_PROGRAM, 8, instantm_state )
	AM_RANGE(0x0000, 0x1fff) AM_ROM
	AM_RANGE(0x4000, 0x47ff) AM_RAM
	AM_RANGE(0x8000, 0x8000) //AM_WRITE
	AM_RANGE(0xc000, 0xc000) //AM_WRITE
	AM_RANGE(0xc400, 0xc400) //AM_WRITE
	AM_RANGE(0xc800, 0xc800) //AM_WRITE
	AM_RANGE(0xcc00, 0xcc00) //AM_WRITE
	AM_RANGE(0xec00, 0xec00) //AM_READ
	AM_RANGE(0xf000, 0xf000) //AM_READ
	AM_RANGE(0xf400, 0xf400) //AM_READ
	AM_RANGE(0xfc00, 0xfc00) //AM_READ
ADDRESS_MAP_END

// doesn't use ram
static ADDRESS_MAP_START( sub_map, AS_PROGRAM, 8, instantm_state )
	AM_RANGE(0x0000, 0xffff) AM_ROM
	AM_RANGE(0x0000, 0x0000) AM_DEVWRITE("dac", dac_byte_interface, write)
ADDRESS_MAP_END

static ADDRESS_MAP_START( sub_io, AS_PROGRAM, 8, instantm_state )
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0x01, 0x01) AM_READWRITE(port01_r,port01_w)
ADDRESS_MAP_END

static INPUT_PORTS_START( instantm )
INPUT_PORTS_END


void instantm_state::machine_start()
{
}

void instantm_state::machine_reset()
{
	m_port01 = 0xf0; // bit 4 low sends subcpu to a test mode
	m_clock_en = true;
}

// OSC1 = XTAL(3'579'545)

MACHINE_CONFIG_START(instantm_state::instantm)
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", Z80, XTAL(3'579'545))
	MCFG_CPU_PROGRAM_MAP(main_map)

	MCFG_CPU_ADD("subcpu", Z80, XTAL(3'579'545))
	MCFG_CPU_PROGRAM_MAP(sub_map)
	MCFG_CPU_IO_MAP(sub_io)

	// all guesswork
	MCFG_DEVICE_ADD("voice_clock", CLOCK, 24000)
	MCFG_CLOCK_SIGNAL_HANDLER(WRITELINE(instantm_state, clock_w))

	MCFG_SPEAKER_STANDARD_MONO("speaker")
	MCFG_SOUND_ADD("dac", MC1408, 0) MCFG_SOUND_ROUTE(ALL_OUTPUTS, "speaker", 0.5)
	MCFG_DEVICE_ADD("vref", VOLTAGE_REGULATOR, 0) MCFG_VOLTAGE_REGULATOR_OUTPUT(5.0)
	MCFG_SOUND_ROUTE_EX(0, "dac", 1.0, DAC_VREF_POS_INPUT) MCFG_SOUND_ROUTE_EX(0, "dac", -1.0, DAC_VREF_NEG_INPUT)
MACHINE_CONFIG_END



ROM_START( instantm )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD( "System5.3Beta.u16", 0x00000, 0x02000, CRC(a1701f4b) SHA1(fa5b0234bd2b666e478aa41129479bb6cec2bcf5) )

	ROM_REGION( 0x10000, "subcpu", 0 )
	ROM_LOAD( "SpeechUS10.u20", 0x00000, 0x10000, CRC(1797bcee) SHA1(c6fb7fbe8592dfae3ba44b49b5ce447206515b77) )
ROM_END


GAME( 199?, instantm,  0,    instantm, instantm, instantm_state,  0, ROT0, "Capcom / Polaroid", "Polaroid Instant Memories", MACHINE_IS_SKELETON_MECHANICAL )
