// license:BSD-3-Clause
// copyright-holders:hap
// thanks-to:Berger
/******************************************************************************

    Novag generic F8 based chess computer driver

    TODO:
    - ccdelta1 doesn't work, goes bonkers when you press Enter. CPU core bug?
    - hardware is similar to MK I, the drivers can be merged in theory.
      But I prefer my source code to be licensed BSD3, mk1.cpp is GPL2.

******************************************************************************

Delta-1:
- 3850PK CPU at ~2MHz, 3853PK memory interface
- 4KB ROM(2332A), 256 bytes RAM(2*2111A-4)
- 4-digit 7seg panel, no sound, no chessboard

******************************************************************************/

#include "emu.h"
#include "includes/novagbase.h"

#include "cpu/f8/f8.h"
#include "machine/f3853.h"

// internal artwork
#include "novag_delta1.lh"


class novagf8_state : public novagbase_state
{
public:
	novagf8_state(const machine_config &mconfig, device_type type, const char *tag)
		: novagbase_state(mconfig, type, tag)
	{ }

	u8 m_io[2]; // F8 CPU I/O ports
	F3853_INTERRUPT_REQ_CB(f3853_interrupt);

	// Delta-1
	DECLARE_INPUT_CHANGED_MEMBER(reset_button);
	DECLARE_WRITE8_MEMBER(delta1_io0_w);
	DECLARE_WRITE8_MEMBER(delta1_io1_w);
	DECLARE_READ8_MEMBER(delta1_io0_r);
	DECLARE_READ8_MEMBER(delta1_io1_r);
	void delta1(machine_config &config);

protected:
	virtual void machine_start() override;
};

void novagf8_state::machine_start()
{
	novagbase_state::machine_start();

	// zerofill
	memset(m_io, 0, sizeof(m_io));

	// register for savestates
	save_item(NAME(m_io));
}



// Devices, I/O

/******************************************************************************
    Delta-1
******************************************************************************/

// CPU I/O ports

WRITE8_MEMBER(novagf8_state::delta1_io0_w)
{
	m_io[0] = data;

	// IO00-02: MC14028B A-C (IO03: GND)
	// MC14028B Q3-Q7: input mux
	// MC14028B Q4-Q7: digit select through 75492
	u16 sel = 1 << (~data & 7);
	m_inp_mux = sel >> 3 & 0x1f;

	// update display here
	set_display_segmask(0xf, 0x7f);
	display_matrix(7, 4, m_led_data, sel >> 4);
}

READ8_MEMBER(novagf8_state::delta1_io0_r)
{
	// IO04-07: multiplexed inputs
	return read_inputs(5) << 4 | m_io[0];
}

WRITE8_MEMBER(novagf8_state::delta1_io1_w)
{
	m_io[1] = data;

	// IO17: segment commons, active low (always 0?)
	// IO10-16: digit segments A-G
	data = (data & 0x80) ? 0 : (data & 0x7f);
	m_led_data = bitswap<7>(data, 0,1,2,3,4,5,6);
}

READ8_MEMBER(novagf8_state::delta1_io1_r)
{
	// unused?
	return m_io[1];
}



/******************************************************************************
    Address Maps
******************************************************************************/

// Delta-1

static ADDRESS_MAP_START( delta1_map, AS_PROGRAM, 8, novagf8_state )
	ADDRESS_MAP_GLOBAL_MASK(0x3fff)
	AM_RANGE(0x0000, 0x0fff) AM_MIRROR(0x1000) AM_ROM // _A13
	AM_RANGE(0x2000, 0x20ff) AM_MIRROR(0x1f00) AM_RAM // A13
ADDRESS_MAP_END

static ADDRESS_MAP_START( delta1_io, AS_IO, 8, novagf8_state )
	AM_RANGE(0x0, 0x0) AM_READWRITE(delta1_io0_r, delta1_io0_w )
	AM_RANGE(0x1, 0x1) AM_READWRITE(delta1_io1_r, delta1_io1_w )
	AM_RANGE(0xc, 0xf) AM_DEVREADWRITE("f3853", f3853_device, read, write )
ADDRESS_MAP_END



/******************************************************************************
    Input Ports
******************************************************************************/

INPUT_CHANGED_MEMBER(novagf8_state::reset_button)
{
	// wired directly to CPU reset pin
	m_maincpu->set_input_line(INPUT_LINE_RESET, newval ? ASSERT_LINE : CLEAR_LINE);
}

static INPUT_PORTS_START( delta1 )
	PORT_START("IN.0")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_Y) PORT_NAME("Time Set")
	PORT_BIT(0x0d, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START("IN.1")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_CODE(KEYCODE_A) PORT_NAME("A 1 / White King")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_CODE(KEYCODE_B) PORT_NAME("B 2 / White Queen")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_CODE(KEYCODE_C) PORT_NAME("C 3 / White Rook")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_CODE(KEYCODE_D) PORT_NAME("D 4")

	PORT_START("IN.2")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_CODE(KEYCODE_E) PORT_NAME("E 5 / White Bishop")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_CODE(KEYCODE_F) PORT_NAME("F 6 / White Knight")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_CODE(KEYCODE_G) PORT_NAME("G 7 / White Pawn")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_CODE(KEYCODE_H) PORT_NAME("H 8")

	PORT_START("IN.3")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_NAME("9 / Black King")
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_NAME("0 / Black Queen")
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_T) PORT_NAME("Time Display / Black Rook")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_DEL) PORT_CODE(KEYCODE_BACKSPACE) PORT_NAME("CE") // clear entry

	PORT_START("IN.4")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_U) PORT_NAME("FP / Black Bishop") // find position
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_I) PORT_NAME("EP / Black Knight") // enter position
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_O) PORT_NAME("Color / Black Pawn")
	PORT_BIT(0x08, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_NAME("Enter")

	PORT_START("RESET") // not on matrix
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_KEYPAD) PORT_CODE(KEYCODE_N) PORT_CHANGED_MEMBER(DEVICE_SELF, novagf8_state, reset_button, nullptr) PORT_NAME("New Game")
INPUT_PORTS_END



/******************************************************************************
    Machine Drivers
******************************************************************************/

F3853_INTERRUPT_REQ_CB(novagf8_state::f3853_interrupt)
{
	m_maincpu->set_input_line_vector(F8_INPUT_LINE_INT_REQ, addr);
	m_maincpu->set_input_line(F8_INPUT_LINE_INT_REQ, level ? ASSERT_LINE : CLEAR_LINE);
}

MACHINE_CONFIG_START(novagf8_state::delta1)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", F8, 2000000) // LC circuit, measured 2MHz
	MCFG_CPU_PROGRAM_MAP(delta1_map)
	MCFG_CPU_IO_MAP(delta1_io)

	MCFG_DEVICE_ADD("f3853", F3853, 2000000)
	MCFG_F3853_EXT_INPUT_CB(novagf8_state, f3853_interrupt)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", novagbase_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_novag_delta1)
MACHINE_CONFIG_END



/******************************************************************************
    ROM Definitions
******************************************************************************/

ROM_START( ccdelta1 )
	ROM_REGION( 0x10000, "maincpu", 0 )
	ROM_LOAD("ma_winke_y1d", 0x0000, 0x1000, CRC(ddc04aca) SHA1(bbf334c82bc89b2f131f5a50f0a617bc3bc4c329) ) // 2332a
ROM_END



/******************************************************************************
    Drivers
******************************************************************************/

//    YEAR  NAME      PARENT  CMP MACHINE  INPUT    STATE          INIT  COMPANY, FULLNAME, FLAGS
CONS( 1979, ccdelta1, 0,       0, delta1,  delta1,  novagf8_state, 0,    "Novag", "Chess Champion: Delta-1", MACHINE_SUPPORTS_SAVE | MACHINE_NO_SOUND_HW | MACHINE_NOT_WORKING )
