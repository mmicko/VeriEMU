// license:BSD-3-Clause
// copyright-holders:hap, Sean Riddle
// thanks-to:Igor, ChoccyHobNob, RColtrane
/***************************************************************************

  Sharp SM5xx family handhelds.
  List of child drivers:
  - rzone: Tiger R-Zone

  The LCD screen graphics are provided internally with an SVG file.
  MAME external artwork is recommended for the backgrounds inlays.

  TODO:
  - improve/redo SVGs of: gnw_mmouse, gnw_egg, exospace
  - confirm gnw_mmouse/gnw_egg rom (dumped from Soviet clone, but pretty confident that it's same)
  - identify lcd segments for tgaiden

***************************************************************************/

#include "emu.h"
#include "includes/hh_sm510.h"

#include "cpu/sm510/sm510.h"
#include "cpu/sm510/sm500.h"
#include "rendlay.h"
#include "screen.h"
#include "speaker.h"

// internal artwork
#include "gnw_dualv.lh"
#include "gnw_dualh.lh"
//#include "hh_sm510_test.lh" // common test-layout - use external artwork
//#include "hh_sm500_test.lh" // "


// machine start/reset

void hh_sm510_state::machine_start()
{
	// resolve handlers
	m_out_x.resolve();

	// zerofill
	m_inp_mux = 0;
	/* m_inp_lines = 0; */ // not here
	/* m_inp_fixed = -1; */ // not here
	m_s = 0;
	m_r = 0;
	m_display_x_len = 0;
	m_display_y_len = 0;
	m_display_z_len = 0;
	memset(m_display_state, 0, sizeof(m_display_state));
	memset(m_display_decay, 0, sizeof(m_display_decay));

	// register for savestates
	save_item(NAME(m_inp_mux));
	save_item(NAME(m_inp_lines));
	save_item(NAME(m_inp_fixed));
	save_item(NAME(m_s));
	save_item(NAME(m_r));
	save_item(NAME(m_display_x_len));
	save_item(NAME(m_display_y_len));
	save_item(NAME(m_display_z_len));
	save_item(NAME(m_display_state));
	save_item(NAME(m_display_decay));
}

void hh_sm510_state::machine_reset()
{
}



/***************************************************************************

  Helper Functions

***************************************************************************/

// lcd panel - on lcd handhelds, usually not a generic x/y screen device
// deflicker here, especially needed for SM500/SM5A with the active shift register

TIMER_DEVICE_CALLBACK_MEMBER(hh_sm510_state::display_decay_tick)
{
	u8 z_mask = (1 << m_display_z_len) - 1;
	u8 zx_len = 1 << (m_display_x_len + m_display_z_len);

	for (int zx = 0; zx < zx_len; zx++)
	{
		for (int y = 0; y < m_display_y_len; y++)
		{
			// delay lcd segment on/off state
			if (m_display_state[zx] >> y & 1)
			{
				if (m_display_decay[y][zx] < (2 * m_display_wait - 1))
					m_display_decay[y][zx]++;
			}
			else if (m_display_decay[y][zx] > 0)
				m_display_decay[y][zx]--;
			u8 active_state = (m_display_decay[y][zx] < m_display_wait) ? 0 : 1;

			// SM510 series: output to x.y.z, where:
			// x = group a/b/bs/c (0/1/2/3)
			// y = segment 1-16 (0-15)
			// z = common H1-H4 (0-3)

			// SM500 series: output to x.y.z, where:
			// x = O group (0-*)
			// y = O segment 1-4 (0-3)
			// z = common H1/H2 (0/1)
			m_out_x[zx >> m_display_z_len][y][zx & z_mask] = active_state;
		}
	}
}

void hh_sm510_state::set_display_size(u8 x, u8 y, u8 z)
{
	// x = groups(in bits)
	// y = number of segments per group
	// z = commons(in bits)
	m_display_x_len = x;
	m_display_y_len = y;
	m_display_z_len = z;
}

WRITE16_MEMBER(hh_sm510_state::sm510_lcd_segment_w)
{
	set_display_size(2, 16, 2);
	m_display_state[offset] = data;
}

WRITE8_MEMBER(hh_sm510_state::sm500_lcd_segment_w)
{
	set_display_size(4, 4, 1);
	m_display_state[offset] = data;
}


// generic input handlers - usually S output is input mux, and K input for buttons

u8 hh_sm510_state::read_inputs(int columns, int fixed)
{
	u8 ret = 0;

	// read selected input rows
	for (int i = 0; i < columns; i++)
		if (m_inp_mux >> i & 1)
			ret |= m_inp_matrix[i]->read();

	if (fixed >= 0)
		ret |= m_inp_matrix[fixed]->read();

	return ret;
}

void hh_sm510_state::update_k_line()
{
	// this is necessary because the MCU can wake up on K input activity
	m_maincpu->set_input_line(SM510_INPUT_LINE_K, input_r(machine().dummy_space(), 0, 0xff) ? ASSERT_LINE : CLEAR_LINE);
}

INPUT_CHANGED_MEMBER(hh_sm510_state::input_changed)
{
	update_k_line();
}

WRITE8_MEMBER(hh_sm510_state::input_w)
{
	m_inp_mux = data;
	update_k_line();
}

READ8_MEMBER(hh_sm510_state::input_r)
{
	return read_inputs(m_inp_lines, m_inp_fixed);
}

INPUT_CHANGED_MEMBER(hh_sm510_state::acl_button)
{
	// ACL button is directly tied to MCU ACL pin
	m_maincpu->set_input_line(SM510_INPUT_LINE_ACL, newval ? ASSERT_LINE : CLEAR_LINE);
}


// other generic output handlers

WRITE8_MEMBER(hh_sm510_state::piezo_r1_w)
{
	// R1 to piezo (SM511 R pin is melody output)
	m_speaker->level_w(data & 1);
}

WRITE8_MEMBER(hh_sm510_state::piezo_r2_w)
{
	// R2 to piezo
	m_speaker->level_w(data >> 1 & 1);
}

WRITE8_MEMBER(hh_sm510_state::piezo_input_w)
{
	// R1 to piezo, other to input mux
	piezo_r1_w(space, 0, data & 1);
	input_w(space, 0, data >> 1);
}





/***************************************************************************

  Minidrivers (subclass, I/O, Inputs, Machine Config)

***************************************************************************/

/***************************************************************************

  Konami Double Dribble
  * Sharp SM510 under epoxy (die label CMS54C, KMS584)
  * lcd screen with custom segments, 1-bit sound

  BTANB: At the basket, the ball goes missing sometimes for 1 frame, or
  may show 2 balls at the same time. It's the same on the real handheld.
  Another BTANB? If a period is over at the same time a defender on the
  2nd column grabs the ball, his arm won't be erased until it's redrawn.

***************************************************************************/

class kdribble_state : public hh_sm510_state
{
public:
	kdribble_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void kdribble(machine_config &config);
};

// config

static INPUT_PORTS_START( kdribble )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Level Select")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(kdribble_state::kdribble)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(2) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1524, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1524-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami Top Gun
  * PCB label BH003
  * Sharp SM510 under epoxy (die label CMS54C, KMS598)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class ktopgun_state : public hh_sm510_state
{
public:
	ktopgun_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void ktopgun(machine_config &config);
};

// config

static INPUT_PORTS_START( ktopgun )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(ktopgun_state::ktopgun)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(2) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1515, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1515-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami Contra
  * PCB label BH002
  * Sharp SM511 under epoxy (die label KMS73B, KMS773)
  * lcd screen with custom segments, 1-bit sound

  Contra handheld is titled simply "C" in the USA.

***************************************************************************/

class kcontra_state : public hh_sm510_state
{
public:
	kcontra_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void kcontra(machine_config &config);
};

// config

static INPUT_PORTS_START( kcontra )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(kcontra_state::kcontra)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1505, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1505-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami Teenage Mutant Ninja Turtles
  * PCB label BH005
  * Sharp SM511 under epoxy (die label KMS73B, KMS774)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class ktmnt_state : public hh_sm510_state
{
public:
	ktmnt_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void ktmnt(machine_config &config);
};

// config

static INPUT_PORTS_START( ktmnt )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(ktmnt_state::ktmnt)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1505, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1505-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami Gradius
  * PCB label BH004
  * Sharp SM511 under epoxy (die label KMS73B, KMS774)
  * lcd screen with custom segments, 1-bit sound

  Known in Japan as Nemesis.

***************************************************************************/

class kgradius_state : public hh_sm510_state
{
public:
	kgradius_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 2;
	}

	void kgradius(machine_config &config);
};

// config

static INPUT_PORTS_START( kgradius )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(kgradius_state::kgradius)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1420, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1420-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami Lone Ranger
  * PCB label BH009
  * Sharp SM511 under epoxy (die label KMS73B, KMS781)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class kloneran_state : public hh_sm510_state
{
public:
	kloneran_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 2;
	}

	void kloneran(machine_config &config);
};

// config

static INPUT_PORTS_START( kloneran )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(kloneran_state::kloneran)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1497, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1497-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami Blades of Steel
  * PCB label BH011
  * Sharp SM511 under epoxy (die label KMS73B, KMS782)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class kblades_state : public hh_sm510_state
{
public:
	kblades_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void kblades(machine_config &config);
};

// config

static INPUT_PORTS_START( kblades )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(kblades_state::kblades)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1516, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1516-1, 0, 1080-1)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami NFL Football
  * Sharp SM511 under epoxy (die label KMS73B, KMS786)
  * lcd screen with custom segments, 1-bit sound

  This is the 1989 version. It was rereleased in 1992, assumed to be the same
  game underneath.

***************************************************************************/

class knfl_state : public hh_sm510_state
{
public:
	knfl_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void knfl(machine_config &config);
};

// config

static INPUT_PORTS_START( knfl )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(knfl_state::knfl)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1449, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1449-1, 0, 1080-1)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami The Adventures of Bayou Billy
  * Sharp SM511 under epoxy (die label KMS73B, KMS788)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class kbilly_state : public hh_sm510_state
{
public:
	kbilly_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void kbilly(machine_config &config);
};

// config

static INPUT_PORTS_START( kbilly )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(kbilly_state::kbilly)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1490, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1490-1, 0, 1080-1)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami Bucky O'Hare
  * Sharp SM511 under epoxy (die label KMS73B, N58)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class kbucky_state : public hh_sm510_state
{
public:
	kbucky_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 2;
	}

	void kbucky(machine_config &config);
};

// config

static INPUT_PORTS_START( kbucky )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(kbucky_state::kbucky)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1490, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1490-1, 0, 1080-1)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Konami Garfield
  * Sharp SM511 under epoxy (die label KMS73B, N62)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class kgarfld_state : public hh_sm510_state
{
public:
	kgarfld_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void kgarfld(machine_config &config);
};

// config

static INPUT_PORTS_START( kgarfld )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("IN.2") // S3
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("All Clear")
INPUT_PORTS_END

MACHINE_CONFIG_START(kgarfld_state::kgarfld)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1500, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1500-1, 0, 1080-1)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Nintendo Game & Watch: Mickey Mouse (model MC-25), Egg (model EG-26)
  * Sharp SM5A label ?
  * lcd screen with custom segments, 1-bit sound

  MC-25 and EG-26 are the same game, it's assumed that the latter was for
  regions where Nintendo wasn't able to license from Disney.

  In 1984, Elektronika(USSR) released a clone: Nu, pogodi! This was followed
  by several other titles that were the same under the hood, only differing
  in graphics. They also made a slightly modified version, adding a new game
  mode (by pressing A+B) where the player/CPU roles are reversed. This version
  is known as Razvedciki kosmosa (export version: Explorers of Space).

***************************************************************************/

class gnw_mmouse_state : public hh_sm510_state
{
public:
	gnw_mmouse_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void exospace(machine_config &config);
	void nupogodi(machine_config &config);
	void gnw_egg(machine_config &config);
	void gnw_mmouse(machine_config &config);
};

// config

static INPUT_PORTS_START( gnw_mmouse )
	PORT_START("IN.0") // R2
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // R3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_16WAY
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_16WAY
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_16WAY
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_16WAY

	PORT_START("IN.2") // R4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Time")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game B")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game A")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SERVICE2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Alarm")

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")

	PORT_START("BA") // MCU BA(alpha) pin pulled to GND, only works after power-on
	PORT_CONFNAME( 0x01, 0x01, "Infinite Lives (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

static INPUT_PORTS_START( exospace )
	PORT_INCLUDE( gnw_mmouse )

	PORT_MODIFY("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

MACHINE_CONFIG_START(gnw_mmouse_state::gnw_mmouse)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM5A, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT) // ?
	MCFG_SM500_WRITE_O_CB(WRITE8(hh_sm510_state, sm500_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_input_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1711, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1711-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(gnw_mmouse_state::gnw_egg, gnw_mmouse)

	/* video hardware */
	MCFG_SCREEN_MODIFY("screen")
	MCFG_SCREEN_SIZE(1694, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1694-1, 0, 1080-1)
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(gnw_mmouse_state::nupogodi, gnw_mmouse)

	/* basic machine hardware */
	MCFG_CPU_REPLACE("maincpu", KB1013VK12, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM500_WRITE_O_CB(WRITE8(hh_sm510_state, sm500_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_input_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))

	/* video hardware */
	MCFG_SCREEN_MODIFY("screen")
	MCFG_SCREEN_SIZE(1715, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1715-1, 0, 1080-1)
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(gnw_mmouse_state::exospace, nupogodi)

	/* video hardware */
	MCFG_SCREEN_MODIFY("screen")
	MCFG_SCREEN_SIZE(1756, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1756-1, 0, 1080-1)
MACHINE_CONFIG_END





/***************************************************************************

  Nintendo Game & Watch: Mickey & Donald (model DM-53)
  * PCB label DM-53
  * Sharp SM510 label DM-53 52ZC (die label CMS54C, CMS565)
  * vertical dual lcd screens with custom segments, 1-bit sound

***************************************************************************/

class gnw_mickdon_state : public hh_sm510_state
{
public:
	gnw_mickdon_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 2;
	}

	void gnw_mickdon(machine_config &config);
};

// config

static INPUT_PORTS_START( gnw_mickdon )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Time")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game B")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game A")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SERVICE2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Alarm")

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")

	PORT_START("B") // MCU B(beta) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "Infinite Lives (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

MACHINE_CONFIG_START(gnw_mickdon_state::gnw_mickdon)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(2) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r2_w))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen_top", "svg_top")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1920/2, 1281/2)
	MCFG_SCREEN_VISIBLE_AREA(0, 1920/2-1, 0, 1281/2-1)

	MCFG_SCREEN_SVG_ADD("screen_bottom", "svg_bottom")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1920/2, 1236/2)
	MCFG_SCREEN_VISIBLE_AREA(0, 1920/2-1, 0, 1236/2-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_gnw_dualv)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Nintendo Game & Watch: Donkey Kong II (model JR-55)
  * PCB label JR-55
  * Sharp SM510 label JR-55 53YC (die label CMS54C, KMS560)
  * vertical dual lcd screens with custom segments, 1-bit sound

***************************************************************************/

class gnw_dkong2_state : public hh_sm510_state
{
public:
	gnw_dkong2_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void gnw_dkong2(machine_config &config);
};

// config

static INPUT_PORTS_START( gnw_dkong2 )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Jump

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Time")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game B")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game A")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SERVICE2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Alarm")

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")

	PORT_START("B") // MCU B(beta) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "Invincibility (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

MACHINE_CONFIG_START(gnw_dkong2_state::gnw_dkong2)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(2) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen_top", "svg_top")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1920/2, 1241/2)
	MCFG_SCREEN_VISIBLE_AREA(0, 1920/2-1, 0, 1241/2-1)

	MCFG_SCREEN_SVG_ADD("screen_bottom", "svg_bottom")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1920/2, 1237/2)
	MCFG_SCREEN_VISIBLE_AREA(0, 1920/2-1, 0, 1237/2-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_gnw_dualv)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Nintendo Game & Watch: Mario Bros. (model MW-56)
  * PCB label MW-56-M
  * Sharp SM510 label MW-56 533C (no decap)
  * horizontal dual lcd screens with custom segments, 1-bit sound

***************************************************************************/

class gnw_mario_state : public hh_sm510_state
{
public:
	gnw_mario_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 2;
	}

	void gnw_mario(machine_config &config);
};

// config

static INPUT_PORTS_START( gnw_mario )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Time")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game B")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game A")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SERVICE2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Alarm")

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")

	PORT_START("BA") // MCU BA(alpha) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "Increase Score (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )

	PORT_START("B") // MCU B(beta) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "Infinite Lives (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

MACHINE_CONFIG_START(gnw_mario_state::gnw_mario)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(2) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen_left", "svg_left")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(2258/2, 1440/2)
	MCFG_SCREEN_VISIBLE_AREA(0, 2258/2-1, 0, 1440/2-1)

	MCFG_SCREEN_SVG_ADD("screen_right", "svg_right")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(2261/2, 1440/2)
	MCFG_SCREEN_VISIBLE_AREA(0, 2261/2-1, 0, 1440/2-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_gnw_dualh)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Nintendo Game & Watch: Donkey Kong Jr. (model DJ-101)
  * Sharp SM510 label DJ-101 52ZA (no decap)
  * lcd screen with custom segments, 1-bit sound

  This is the new wide screen version, there's also a tabletop version that
  plays more like the arcade game.

***************************************************************************/

class gnw_dkjr_state : public hh_sm510_state
{
public:
	gnw_dkjr_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void gnw_dkjr(machine_config &config);
};

// config

static INPUT_PORTS_START( gnw_dkjr )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Jump

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Time")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game B")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game A")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SERVICE2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Alarm")

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")

	PORT_START("BA") // MCU BA(alpha) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "Increase Score (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )

	PORT_START("B") // MCU B(beta) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "Invincibility (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

MACHINE_CONFIG_START(gnw_dkjr_state::gnw_dkjr)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(2) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1647, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1647-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Nintendo Game & Watch: Mario's Cement Factory (model ML-102)
  * Sharp SM510 label ML-102 298D (die label CMS54C, KMS577)
  * lcd screen with custom segments, 1-bit sound

  This is the new wide screen version, there's also a tabletop version.

***************************************************************************/

class gnw_mariocm_state : public hh_sm510_state
{
public:
	gnw_mariocm_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 2;
	}

	void gnw_mariocm(machine_config &config);
};

// config

static INPUT_PORTS_START( gnw_mariocm )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Open
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_16WAY
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_16WAY
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Time")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game B")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game A")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SERVICE2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Alarm")

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")

	PORT_START("BA") // MCU BA(alpha) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "Increase Score (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )

	PORT_START("B") // MCU B(beta) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "Infinite Lives (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

MACHINE_CONFIG_START(gnw_mariocm_state::gnw_mariocm)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(2) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1647, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1647-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Nintendo Game & Watch: Super Mario Bros. (model: see below)
  * Sharp SM511 label YM-105 9024B (new wide screen version) (die label ?)
  * lcd screen with custom segments, 1-bit sound

  First released in 1986 on Crystal Screen (model YM-801), rereleased on
  New Wide Screen in 1988 (model YM-105). It was also a prize in a Nintendo
  game contest in 1987 (model YM-901-S). In YM-801, Mario looks like the
  ones in ML-102 and MW-56. In YM-901-S and YM-105 he looks more detailed.
  Until further proof, it's assumed that the ROM is the same for each model.

***************************************************************************/

class gnw_smb_state : public hh_sm510_state
{
public:
	gnw_smb_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 3;
	}

	void gnw_smb(machine_config &config);
};

// config

static INPUT_PORTS_START( gnw_smb )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Time")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SERVICE2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Alarm")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Jump
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")

	PORT_START("B") // MCU B(beta) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "Infinite Lives (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

MACHINE_CONFIG_START(gnw_smb_state::gnw_smb)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1677, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1677-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Nintendo Game & Watch: Boxing (model BX-301)
  * Sharp SM511 label BX-301 287C (die label KMS73B, KMS744)
  * wide lcd screen with custom segments, 1-bit sound

  Also known as Punch-Out!! in the USA.

***************************************************************************/

class gnw_boxing_state : public hh_sm510_state
{
public:
	gnw_boxing_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 7;
	}

	void gnw_boxing(machine_config &config);
};

// config

static INPUT_PORTS_START( gnw_boxing )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_PLAYER(2)

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x03, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_PLAYER(2)

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x03, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_PLAYER(2)

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // S7
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Time")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_START2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game B")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_START1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game A")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_SERVICE2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Alarm")

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")

	PORT_START("BA") // MCU BA(alpha) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "P2 Decrease Health (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )

	PORT_START("B") // MCU B(beta) pin pulled to GND
	PORT_CONFNAME( 0x01, 0x01, "P1 Infinite Health (Cheat)")
	PORT_CONFSETTING(    0x01, DEF_STR( Off ) )
	PORT_CONFSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

MACHINE_CONFIG_START(gnw_boxing_state::gnw_boxing)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1920, 524)
	MCFG_SCREEN_VISIBLE_AREA(0, 1920-1, 0, 524-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Gauntlet (model 7-778), Robin Hood (model 7-861)
  * Sharp SM510 under epoxy (die label CMS54C, KMS583)
  * lcd screen with custom segments, 1-bit sound

  known releases (Gauntlet):
  - World: Gauntlet
  - Japan: Gauntlet (published by Sega)
  - UK: Gauntlet (published by Grandstand)

  Robin Hood is the same ROM, different LCD.

***************************************************************************/

class tgaunt_state : public hh_sm510_state
{
public:
	tgaunt_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void trobhood(machine_config &config);
	void tgaunt(machine_config &config);
};

// config

static INPUT_PORTS_START( tgaunt )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pick")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0a, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Bomb")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Attack")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Key")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

static INPUT_PORTS_START( trobhood )
	PORT_INCLUDE( tgaunt )

	PORT_MODIFY("IN.3")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Money")
INPUT_PORTS_END

MACHINE_CONFIG_START(tgaunt_state::tgaunt)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1425, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1425-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(tgaunt_state::trobhood, tgaunt)

	/* video hardware */
	MCFG_SCREEN_MODIFY("screen")
	MCFG_SCREEN_SIZE(1468, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1468-1, 0, 1080-1)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Double Dragon (model 7-780)
  * Sharp SM510 under epoxy (die label CMS54C, KMS570, 593)
  * lcd screen with custom segments, 1-bit sound

  BTANB: On the baddie in the background throwing dynamite, the sparks
  above his head are the same segment as the body, not the arm.

***************************************************************************/

class tddragon_state : public hh_sm510_state
{
public:
	tddragon_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 5;
		m_inp_fixed = 5;
	}

	void tddragon(machine_config &config);
};

// config

static INPUT_PORTS_START( tddragon )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Jump
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Sway
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch/Pick")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Status")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tddragon_state::tddragon)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1467, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1467-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Karnov (model 7-783)
  * Sharp SM510 under epoxy (die label CMS54C, KMS582)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tkarnov_state : public hh_sm510_state
{
public:
	tkarnov_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tkarnov(machine_config &config);
};

// config

static INPUT_PORTS_START( tkarnov )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Fire-Ball")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Boomerang")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Shield")
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Max Score")
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tkarnov_state::tkarnov)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1477, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1477-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Vindicators (model 7-786)
  * Sharp SM510 under epoxy (die label CMS54C, KMS595)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tvindictr_state : public hh_sm510_state
{
public:
	tvindictr_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tvindictr(machine_config &config);
};

// config

static INPUT_PORTS_START( tvindictr )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Gun Right")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Fire")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Gun Left")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Max Score")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tvindictr_state::tvindictr)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1459, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1459-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Ninja Gaiden (model 7-787)
  * Sharp SM510 under epoxy (die label M82)
  * lcd screen with custom segments, 1 led, 1-bit sound

***************************************************************************/

class tgaiden_state : public hh_sm510_state
{
public:
	tgaiden_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 5;
		m_inp_fixed = 5;
	}

	DECLARE_WRITE8_MEMBER(write_r);
	void tgaiden(machine_config &config);
};

// handlers

WRITE8_MEMBER(tgaiden_state::write_r)
{
	// R1: speaker out
	piezo_r1_w(space, 0, data & 1);

	// R2: led
	output().set_lamp_value(0, data >> 1 & 1);
}

// config

static INPUT_PORTS_START( tgaiden )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Jump")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Attack/Pick")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tgaiden_state::tgaiden)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(tgaiden_state, write_r))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1920, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1920-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Batman (model 7-799)
  * Sharp SM510 under epoxy (die label CMS54C, KMS597)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tbatman_state : public hh_sm510_state
{
public:
	tbatman_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 5;
		m_inp_fixed = 5;
	}

	void tbatman(machine_config &config);
};

// config

static INPUT_PORTS_START( tbatman )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pick")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Attack")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Max Score")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tbatman_state::tbatman)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1442, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1442-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Space Harrier II (model 7-814)
  * Sharp SM510 under epoxy (die label M91)
  * lcd screen with custom segments, 1-bit sound

  known releases:
  - World: Space Harrier II
  - Japan: Space Harrier (published by Sega)

***************************************************************************/

class tsharr2_state : public hh_sm510_state
{
public:
	tsharr2_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 5;
		m_inp_fixed = 5;
	}

	void tsharr2(machine_config &config);
};

// config

static INPUT_PORTS_START( tsharr2 )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Attack
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Max Score")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tsharr2_state::tsharr2)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1493, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1493-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Golden Axe (model 7-817)
  * Sharp SM510 under epoxy (die label M94)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tgoldnaxe_state : public hh_sm510_state
{
public:
	tgoldnaxe_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tgoldnaxe(machine_config &config);
};

// config

static INPUT_PORTS_START( tgoldnaxe )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Magic")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Attack Left")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Attack Right")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tgoldnaxe_state::tgoldnaxe)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1456, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1456-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Robocop 2 (model 7-830)
  * Sharp SM510 under epoxy (die label M96)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class trobocop2_state : public hh_sm510_state
{
public:
	trobocop2_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void trobocop2(machine_config &config);
};

// config

static INPUT_PORTS_START( trobocop2 )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Rescue
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // /Pick
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Shoot Right")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Shoot Up")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Shoot Left")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(trobocop2_state::trobocop2)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1487, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1487-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Altered Beast (model 7-831)
  * Sharp SM510 under epoxy (die label M88)
  * lcd screen with custom segments, 1-bit sound

  known releases:
  - World: Altered Beast
  - Japan: Juuouki (published by Sega)

***************************************************************************/

class taltbeast_state : public hh_sm510_state
{
public:
	taltbeast_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void taltbeast(machine_config &config);
};

// config

static INPUT_PORTS_START( taltbeast )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch Right")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick/Attack")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch Left")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(taltbeast_state::taltbeast)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1455, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1455-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Swamp Thing (model 7-851)
  * Sharp SM510 under epoxy (die label MB0)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tswampt_state : public hh_sm510_state
{
public:
	tswampt_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 5;
		m_inp_fixed = 5;
	}

	void tswampt(machine_config &config);
};

// config

static INPUT_PORTS_START( tswampt )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch/Fire")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Throw")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tswampt_state::tswampt)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1450, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1450-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger MC Hammer: U Can't Touch This (model 7-863)
  * Sharp SM511 under epoxy (die label N63)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tmchammer_state : public hh_sm510_state
{
public:
	tmchammer_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tmchammer(machine_config &config);
};

// config

static INPUT_PORTS_START( tmchammer )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Leg Footwork Left")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Arm Up")
	PORT_BIT( 0x0a, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Leg Footwork Right")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Leg Leaps Up/Jump")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICKRIGHT_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Leg Leaps Down")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Arm Splits Right")
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Arm Splits Left")
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICKLEFT_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Arm Down")
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Mode")
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tmchammer_state::tmchammer)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1471, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1471-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Battletoads (model 7-868)
  * Sharp SM510 under epoxy (die label MB3)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tbtoads_state : public hh_sm510_state
{
public:
	tbtoads_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tbtoads(machine_config &config);
};

// config

static INPUT_PORTS_START( tbtoads )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Attack Right")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Select")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Attack Left")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Score")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tbtoads_state::tbtoads)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1454, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1454-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Sonic The Hedgehog (model 78-513)
  * Sharp SM511 under epoxy (die label KMS73B, N71)
  * lcd screen with custom segments, 2-bit sound

***************************************************************************/

class tsonic_state : public hh_sm510_state
{
public:
	tsonic_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 5;
		m_inp_fixed = 5;
	}

	void update_speaker();
	DECLARE_WRITE8_MEMBER(write_r);
	DECLARE_WRITE8_MEMBER(write_s);
	void tsonic(machine_config &config);
};

// handlers

void tsonic_state::update_speaker()
{
	m_speaker->level_w((m_s << 1 & 2) | (m_r & 1));
}

WRITE8_MEMBER(tsonic_state::write_r)
{
	// R1: to speaker, via 120K resistor
	m_r = data;
	update_speaker();
}

WRITE8_MEMBER(tsonic_state::write_s)
{
	// S1: to speaker, via 39K resistor
	m_s = data;
	update_speaker();

	// other: input mux
	hh_sm510_state::input_w(space, 0, data >> 1);
}

// config

static INPUT_PORTS_START( tsonic )
	PORT_START("IN.0") // S2
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Jump Up
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Down
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Super Sonic Spin
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

static const s16 tsonic_speaker_levels[] = { 0, 0x7fff/3*1, 0x7fff/3*2, 0x7fff };

MACHINE_CONFIG_START(tsonic_state::tsonic)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(tsonic_state, write_s))
	MCFG_SM510_WRITE_R_CB(WRITE8(tsonic_state, write_r))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1517, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1517-1, 0, 1080-1)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SPEAKER_LEVELS(4, tsonic_speaker_levels)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger The Incredible Crash Dummies (model 78-516)
  * Sharp SM510 under epoxy (die label ME0)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tdummies_state : public hh_sm510_state
{
public:
	tdummies_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 5;
		m_inp_fixed = 5;
	}

	void tdummies(machine_config &config);
};

// config

static INPUT_PORTS_START( tdummies )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Crash
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Brake
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Accelerate")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pick")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tdummies_state::tdummies)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1441, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1441-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Street Fighter II (model 78-522)
  * Sharp SM510 under epoxy (die label ME1)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tsfight2_state : public hh_sm510_state
{
public:
	tsfight2_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tsfight2(machine_config &config);
};

// config

static INPUT_PORTS_START( tsfight2 )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Jump
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Down
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick Right")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch/Special Move")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick Left")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tsfight2_state::tsfight2)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1444, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1444-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Super Double Dragon (model 78-528)
  * Sharp SM510 under epoxy (die label MF5)
  * lcd screen with custom segments, 1-bit sound

  BTANB: The player char right arm muscle is part of the right kick segment.
  They probably meant to use it for the right punch segment, but this is
  how it shows on the LCD.

***************************************************************************/

class tsddragon_state : public hh_sm510_state
{
public:
	tsddragon_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tsddragon(machine_config &config);
};

// config

static INPUT_PORTS_START( tsddragon )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick Left")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Jump
	PORT_BIT( 0x0a, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Down/Pick
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Special Technique")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick Right")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Select")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tsddragon_state::tsddragon)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1503, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1503-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Nightmare Before Christmas (model 78-537)
  * Sharp SM510 under epoxy (die label MG0)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tnmarebc_state : public hh_sm510_state
{
public:
	tnmarebc_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 5;
		m_inp_fixed = 5;
	}

	virtual DECLARE_WRITE8_MEMBER(input_w) override;
	void tnmarebc(machine_config &config);
};

// handlers

WRITE8_MEMBER(tnmarebc_state::input_w)
{
	// S5 and S6 tied together
	hh_sm510_state::input_w(space, 0, (data & 0x1f) | (data >> 1 & 0x10));
}

// config

static INPUT_PORTS_START( tnmarebc )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Jump
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // Action
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5/S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tnmarebc_state::tnmarebc)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(tnmarebc_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1456, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1456-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Mortal Kombat (model 78-553)
  * Sharp SM510 under epoxy (die label MG6)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tmkombat_state : public hh_sm510_state
{
public:
	tmkombat_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tmkombat(machine_config &config);
};

// config

static INPUT_PORTS_START( tmkombat )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch High")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0a, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick High")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch Low")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick Low")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Select")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tmkombat_state::tmkombat)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1468, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1468-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger The Shadow (model 78-559)
  * Sharp SM510 under epoxy (die label MJ5)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tshadow_state : public hh_sm510_state
{
public:
	tshadow_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tshadow(machine_config &config);
};

// config

static INPUT_PORTS_START( tshadow )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Shadow")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tshadow_state::tshadow)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1484, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1484-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Skeleton Warriors - The Dark Crusade (model 78-569)
  * Sharp SM510 under epoxy (die label MK0)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tskelwarr_state : public hh_sm510_state
{
public:
	tskelwarr_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tskelwarr(machine_config &);
};

// config

static INPUT_PORTS_START( tskelwarr )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Attack Right")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pick/Call")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Attack Left")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tskelwarr_state::tskelwarr)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1444, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1444-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Batman Forever - Double Dose of Doom (model 78-572)
  * Sharp SM510 under epoxy (die label MK3)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tbatfor_state : public hh_sm510_state
{
public:
	tbatfor_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tbatfor(machine_config &config);
};

// config

static INPUT_PORTS_START( tbatfor )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Grappling Gun")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0a, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Help")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Batarang")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Thruster")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tbatfor_state::tbatfor)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1493, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1493-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Judge Dredd (model 78-581)
  * Sharp SM510 under epoxy (die label MK5)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tjdredd_state : public hh_sm510_state
{
public:
	tjdredd_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tjdredd(machine_config &config);
};

// config

static INPUT_PORTS_START( tjdredd )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Kick")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0a, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pick/Call")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Punch")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Fire")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tjdredd_state::tjdredd)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1444, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1444-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Apollo 13 (model 78-591)
  * Sharp SM510 under epoxy (die label 10 07)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tapollo13_state : public hh_sm510_state
{
public:
	tapollo13_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 6;
		m_inp_fixed = 6;
	}

	void tapollo13(machine_config &config);
};

// config

static INPUT_PORTS_START( tapollo13 )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Thruster Left")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0a, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON3 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Parachute")
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Thruster Right")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON4 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Capture")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tapollo13_state::tapollo13)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1467, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1467-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tiger Space Jam (model 78-621)
  * Sharp SM510 under epoxy (die label KMS10, 23)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tsjam_state : public hh_sm510_state
{
public:
	tsjam_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 5;
		m_inp_fixed = 5;
	}

	void tsjam(machine_config &config);
};

// config

static INPUT_PORTS_START( tsjam )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0b, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x09, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0d, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Shoot/Block")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON2 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Tune/Steal")
	PORT_BIT( 0x0c, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SELECT ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Pause")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // GND!
	PORT_BIT( 0x07, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_START ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Power On/Start")

	PORT_START("BA")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_VOLUME_DOWN ) PORT_NAME("Sound")

	PORT_START("B")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_POWER_OFF )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_SERVICE1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")
INPUT_PORTS_END

MACHINE_CONFIG_START(tsjam_state::tsjam)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL) // no external XTAL
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))
	MCFG_SM510_READ_BA_CB(IOPORT("BA"))
	MCFG_SM510_READ_B_CB(IOPORT("B"))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1421, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1421-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Tronica Thief in Garden (model TG-18)
  * Sharp SM510 label 0019 238E TRONICA (no decap)
  * lcd screen with custom segments, 1-bit sound

***************************************************************************/

class tigarden_state : public hh_sm510_state
{
public:
	tigarden_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 8;
	}

	void tigarden(machine_config &config);
};

// config

static INPUT_PORTS_START( tigarden )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("0")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("1")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("2")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("3")

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("4")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("5")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("6")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("7")

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("8")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("9")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_STOP) PORT_CODE(KEYCODE_DEL_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME(".")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("=")

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_PLUS_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("+")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_MINUS_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("-")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME(UTF8_MULTIPLY)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_SLASH_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME(UTF8_DIVIDE)

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_X) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("ALM")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_SLASH) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("%")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_DEL) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("C")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.5") // S6
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.6") // S7
	PORT_BIT( 0x01, 0x01, IPT_SPECIAL ) PORT_CONDITION("FAKE", 0x03, NOTEQUALS, 0x00) // Up/Sound
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_16WAY
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON1 ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) // F

	PORT_START("IN.7") // S8
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_Z) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Mode")
	PORT_BIT( 0x0e, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_C) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr) PORT_NAME("ACL")

	PORT_START("FAKE") // Up/Sound are electronically the same button
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_16WAY
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Sound")
INPUT_PORTS_END

MACHINE_CONFIG_START(tigarden_state::tigarden)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL)
	MCFG_SM510_R_MASK_OPTION(2) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1515, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1515-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  VTech Electronic Number Muncher
  * Sharp SM511 under epoxy (die label 772)
  * lcd screen with custom segments(no background), 1-bit sound

***************************************************************************/

class nummunch_state : public hh_sm510_state
{
public:
	nummunch_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag)
	{
		m_inp_lines = 8;
	}

	void nummunch(machine_config &config);
};

// config

static INPUT_PORTS_START( nummunch )
	PORT_START("IN.0") // S1
	PORT_BIT( 0x0f, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.1") // S2
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_POWER_OFF ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_R) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)  PORT_NAME("Calc. / Clear")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_ENTER) PORT_CODE(KEYCODE_ENTER_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("=")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.2") // S3
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_W) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Game")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_Q) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Count")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_E) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Quiz")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START("IN.3") // S4
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_A) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Choose +")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_S) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Choose -")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_D) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Choose " UTF8_MULTIPLY)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_F) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Choose " UTF8_DIVIDE)

	PORT_START("IN.4") // S5
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_1) PORT_CODE(KEYCODE_1_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("1")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_0) PORT_CODE(KEYCODE_0_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("0")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_LEFT) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Left")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_RIGHT) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("Right")

	PORT_START("IN.5") // S6
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_5) PORT_CODE(KEYCODE_5_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("5")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_4) PORT_CODE(KEYCODE_4_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("4")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_3) PORT_CODE(KEYCODE_3_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("3")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_2) PORT_CODE(KEYCODE_2_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("2")

	PORT_START("IN.6") // S7
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_9) PORT_CODE(KEYCODE_9_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("9")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_8) PORT_CODE(KEYCODE_8_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("8")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_7) PORT_CODE(KEYCODE_7_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("7")
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_6) PORT_CODE(KEYCODE_6_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("6")

	PORT_START("IN.7") // S8
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_PLUS_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("+")
	PORT_BIT( 0x02, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_MINUS_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME("-")
	PORT_BIT( 0x04, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_ASTERISK) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME(UTF8_MULTIPLY)
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_KEYPAD ) PORT_CODE(KEYCODE_SLASH_PAD) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr) PORT_NAME(UTF8_DIVIDE)

	PORT_START("ACL")
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_POWER_ON ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, acl_button, nullptr)
INPUT_PORTS_END

MACHINE_CONFIG_START(nummunch_state::nummunch)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM511, 32.768_kHz_XTAL)
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(hh_sm510_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(hh_sm510_state, input_w))
	MCFG_SM510_WRITE_R_CB(WRITE8(hh_sm510_state, piezo_r1_w))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1920, 875)
	MCFG_SCREEN_VISIBLE_AREA(0, 1920-1, 0, 875-1)

	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_svg)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END





/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( kdribble )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "584", 0x0000, 0x1000, CRC(1d9022c8) SHA1(64567f9f161e830a0634d5c89917ab866c26c0f8) )

	ROM_REGION( 450339, "svg", 0)
	ROM_LOAD( "kdribble.svg", 0, 450339, CRC(86c3ecc4) SHA1(8dfaeb0f3b35d4b680daaa9f478a6f3decf6ea0a) )
ROM_END


ROM_START( ktopgun ) // except for filler/unused bytes, ROM listing in patent US5137277 "BH003 Top Gun" is same
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "598", 0x0000, 0x1000, CRC(50870b35) SHA1(cda1260c2e1c180995eced04b7d7ff51616dcef5) )

	ROM_REGION( 425832, "svg", 0)
	ROM_LOAD( "ktopgun.svg", 0, 425832, CRC(dc488ac0) SHA1(5a47e5639cb1e61dad3f2169efb99efe3d75896f) )
ROM_END


ROM_START( kcontra ) // except for filler/unused bytes, ROM listing in patent US5120057 "BH002 C (Contra)" program/melody is same
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "773.program", 0x0000, 0x1000, CRC(bf834877) SHA1(055dd56ec16d63afba61ab866481fd9c029fb54d) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "773.melody", 0x000, 0x100, CRC(23d02b99) SHA1(703938e496db0eeacd14fe7605d4b5c39e0a5bc8) )

	ROM_REGION( 721005, "svg", 0)
	ROM_LOAD( "kcontra.svg", 0, 721005, CRC(b5370d0f) SHA1(2f401222d24fa32a4659ef2b64ddac8ac3973c69) )
ROM_END


ROM_START( ktmnt ) // except for filler/unused bytes, ROM listing in patent US5150899 "BH005 TMNT" program/melody is same
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "774.program", 0x0000, 0x1000, CRC(a1064f87) SHA1(92156c35fbbb414007ee6804fe635128a741d5f1) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "774.melody", 0x000, 0x100, CRC(8270d626) SHA1(bd91ca1d5cd7e2a62eef05c0033b19dcdbe441ca) )

	ROM_REGION( 610270, "svg", 0)
	ROM_LOAD( "ktmnt.svg", 0, 610270, CRC(ad9412ed) SHA1(154ee44efcd340dafa1cb84c37a9c3cd42cb42ab) )
ROM_END


ROM_START( kgradius )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "771.program", 0x0000, 0x1000, CRC(830c2afc) SHA1(bb9ebd4e52831cc02cd92dd4b37675f34cf37b8c) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "771.melody", 0x000, 0x100, CRC(4c586b73) SHA1(14c5ab2898013a577f678970a648c374749cc66d) )

	ROM_REGION( 638097, "svg", 0)
	ROM_LOAD( "kgradius.svg", 0, 638097, CRC(3adbc0f1) SHA1(fe426bf2335ce30395ea14ecab6399a93c67816a) )
ROM_END


ROM_START( kloneran )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "781.program", 0x0000, 0x1000, CRC(52b9735f) SHA1(06c5ef6e7e781b1176d4c1f2445f765ccf18b3f7) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "781.melody", 0x000, 0x100, CRC(a393de36) SHA1(55089f04833ccb318524ab2b584c4817505f4019) )

	ROM_REGION( 633120, "svg", 0)
	ROM_LOAD( "kloneran.svg", 0, 633120, CRC(f55e5292) SHA1(d0a91b5cd8a1894e7abc9c505fff4a8e1d3bec7a) )
ROM_END


ROM_START( kblades )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "782.program", 0x0000, 0x1000, CRC(3351a35d) SHA1(84c64b65d3cabfa20c18f4649c9ede2578b82523) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "782.melody", 0x000, 0x100, CRC(e8bf48ba) SHA1(3852c014dc9136566322b4f9e2aab0e3ec3a7387) )

	ROM_REGION( 455113, "svg", 0)
	ROM_LOAD( "kblades.svg", 0, 455113, CRC(e22f44c8) SHA1(ac95a837e20f87f3afc6c234f7407cbfcc438011) )
ROM_END


ROM_START( knfl )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "786.program", 0x0000, 0x1000, CRC(0535c565) SHA1(44cdcd284713ff0b194b24beff9f1b94c8bc63b2) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "786.melody", 0x000, 0x100, CRC(6c80263b) SHA1(d3c21e2f8491fef101907b8e0871b1e1c1ed58f5) )

	ROM_REGION( 571134, "svg", 0)
	ROM_LOAD( "knfl.svg", 0, 571134, CRC(f2c63235) SHA1(70b9232700f5498d3c63c63dd5904c0e19482cc2) )
ROM_END


ROM_START( kbilly )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "788.program", 0x0000, 0x1000, CRC(b8b1f734) SHA1(619dd527187b43276d081cdb1b13e0a9a81f2c6a) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "788.melody", 0x000, 0x100, CRC(cd488bea) SHA1(8fc60081f46e392978d6950c74711fb7ebd154de) )

	ROM_REGION( 598276, "svg", 0)
	ROM_LOAD( "kbilly.svg", 0, 598276, CRC(2969319e) SHA1(5cd1b0a6eee3168142c1d24f167b9ef38ad88402) )
ROM_END


ROM_START( kbucky )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "n58.program", 0x0000, 0x1000, CRC(7c36a0c4) SHA1(1b55ac64a71af746fd0a0f44266fcc92cca77482) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "n58.melody", 0x000, 0x100, CRC(7e99e469) SHA1(3e9a3843c6ab392f5989f3366df87a2d26cb8620) )

	ROM_REGION( 727841, "svg", 0)
	ROM_LOAD( "kbucky.svg", 0, 727841, CRC(c1d78488) SHA1(9ba4fdbce977455b8f1ad4bd2b01faa44bd05bc7) )
ROM_END


ROM_START( kgarfld )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "n62.program", 0x0000, 0x1000, CRC(5a762049) SHA1(26d4d891160d254dfd752734e1047126243f88dd) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "n62.melody", 0x000, 0x100, CRC(232b7d55) SHA1(76f6a19e8182ee3f00c9f4ef007b5dde75a9c00d) )

	ROM_REGION( 581107, "svg", 0)
	ROM_LOAD( "kgarfld.svg", 0, 581107, CRC(bf09a170) SHA1(075cb95535873018409eb15675183490c61b29b9) )
ROM_END


ROM_START( gnw_mmouse )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mc-25", 0x0000, 0x0740, BAD_DUMP CRC(cb820c32) SHA1(7e94fc255f32db725d5aa9e196088e490c1a1443) ) // dumped from Soviet clone

	ROM_REGION( 102453, "svg", 0)
	ROM_LOAD( "gnw_mmouse.svg", 0, 102453, BAD_DUMP CRC(88cc7c49) SHA1(c000d51d1b99750116b97f9bafc0314ea506366d) )
ROM_END

ROM_START( gnw_egg )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "eg-26", 0x0000, 0x0740, BAD_DUMP CRC(cb820c32) SHA1(7e94fc255f32db725d5aa9e196088e490c1a1443) ) // dumped from Soviet clone

	ROM_REGION( 102848, "svg", 0)
	ROM_LOAD( "gnw_egg.svg", 0, 102848, BAD_DUMP CRC(742c2605) SHA1(984d430ad2ff47ad7a3f9b25b7d3f3d51b10cca5) )
ROM_END

ROM_START( nupogodi )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "nupogodi.bin", 0x0000, 0x0740, CRC(cb820c32) SHA1(7e94fc255f32db725d5aa9e196088e490c1a1443) )

	ROM_REGION( 156974, "svg", 0)
	ROM_LOAD( "nupogodi.svg", 0, 156974, CRC(8d522ec6) SHA1(67afeca5eebd16449353ea43070a6b919f7ba408) )
ROM_END

ROM_START( exospace )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "exospace.bin", 0x0000, 0x0740, CRC(553e2b09) SHA1(2b74f8437b881fbb62b61f25435a5bfc66872a9a) )

	ROM_REGION( 66790, "svg", 0)
	ROM_LOAD( "exospace.svg", 0, 66790, BAD_DUMP CRC(df31043a) SHA1(2d8caf42894df699e469652e5f448beaebbcc1ae) )
ROM_END


ROM_START( gnw_mickdon )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "dm-53_565", 0x0000, 0x1000, CRC(e21fc0f5) SHA1(3b65ccf9f98813319410414e11a3231b787cdee6) )

	ROM_REGION( 126434, "svg_top", 0)
	ROM_LOAD( "gnw_mickdon_top.svg", 0, 126434, CRC(ff05f489) SHA1(2a533c7b5d7249d79f8d7795a0d57fd3e32d3d32) )

	ROM_REGION( 122870, "svg_bottom", 0)
	ROM_LOAD( "gnw_mickdon_bottom.svg", 0, 122870, CRC(8f06ddf1) SHA1(69d4b785781600abcdfc01b3902df1d0ae3608cf) )
ROM_END


ROM_START( gnw_dkong2 )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "jr-55_560", 0x0000, 0x1000, CRC(46aed0ae) SHA1(72f75ccbd84aea094148c872fc7cc1683619a18a) )

	ROM_REGION( 267443, "svg_top", 0)
	ROM_LOAD( "gnw_dkong2_top.svg", 0, 267443, CRC(33b26edb) SHA1(600afdf22ff4ac4a4af2de9159287cc6e53dfe3a) )

	ROM_REGION( 390558, "svg_bottom", 0)
	ROM_LOAD( "gnw_dkong2_bottom.svg", 0, 390558, CRC(92d68958) SHA1(aba829bf89b93bf3a4e425c9a8f6eec9e5869bc4) )
ROM_END


ROM_START( gnw_mario )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mw-56", 0x0000, 0x1000, CRC(385e59da) SHA1(2f79281bdf2f2afca2fb5bd7b9a3beeffc9c4eb7) )

	ROM_REGION( 154874, "svg_left", 0)
	ROM_LOAD( "gnw_mario_left.svg", 0, 154874, CRC(73ba4f4a) SHA1(d5df39808a1af8e8ad5e397b4a50313221ab6e3b) )

	ROM_REGION( 202863, "svg_right", 0)
	ROM_LOAD( "gnw_mario_right.svg", 0, 202863, CRC(dd2473c9) SHA1(51aca37abf8e4959b84c441aa2d114e16c7d6010) )
ROM_END


ROM_START( gnw_dkjr )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "dj-101", 0x0000, 0x1000, CRC(8dcfb5d1) SHA1(e0ef578e9362eb9a3cab631376df3cf55978f2de) )

	ROM_REGION( 281161, "svg", 0)
	ROM_LOAD( "gnw_dkjr.svg", 0, 281161, CRC(346b025c) SHA1(dad3f3f73d6c2ff4efb43ffd76e97ba0d5f0da73) )
ROM_END


ROM_START( gnw_mariocm )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "ml-102_577", 0x0000, 0x1000, CRC(c1128dea) SHA1(8647e36f43a0e37756a3c7b6a3f08d4c8243f1cc) )

	ROM_REGION( 302931, "svg", 0)
	ROM_LOAD( "gnw_mariocm.svg", 0, 302931, CRC(5517ae80) SHA1(1902e36d0470ee5548addeb087ea3e7d2c2520a2) )
ROM_END


ROM_START( gnw_smb )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "ym-105.program", 0x0000, 0x1000, CRC(0dff3b12) SHA1(3fa83f88e49ea9d7080fe935ec90ce69acbe8850) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "ym-105.melody", 0x000, 0x100, NO_DUMP )

	ROM_REGION( 648209, "svg", 0)
	ROM_LOAD( "gnw_smb.svg", 0, 648209, CRC(4a6fdb28) SHA1(0a0bc48d82d5b8bf8ef96ef9ce2f87ba6ea850c1) )
ROM_END


ROM_START( gnw_boxing )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "bx-301_744.program", 0x0000, 0x1000, CRC(0fdf0303) SHA1(0b791c9d4874e9534d0a9b7a8968ce02fe4bee96) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "bx-301_744.melody", 0x000, 0x100, CRC(439d943d) SHA1(52880df15ec7513f96482f455ef3d9778aa24750) )

	ROM_REGION( 265174, "svg", 0)
	ROM_LOAD( "gnw_boxing.svg", 0, 265174, CRC(e8a3ab25) SHA1(53e32542b582dcdf4ddd051f182738eee6c732c9) )
ROM_END


ROM_START( tgaunt )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "583", 0x0000, 0x1000, CRC(598d8156) SHA1(9f776e8b9b4321e8118481e6b1304f8a38f9932e) )

	ROM_REGION( 713020, "svg", 0)
	ROM_LOAD( "tgaunt.svg", 0, 713020, CRC(1f65ae21) SHA1(57ca33d073d1096a7fc17f2bdac940868d1ae651) )
ROM_END

ROM_START( trobhood )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "583", 0x0000, 0x1000, CRC(598d8156) SHA1(9f776e8b9b4321e8118481e6b1304f8a38f9932e) )

	ROM_REGION( 704892, "svg", 0)
	ROM_LOAD( "trobhood.svg", 0, 704892, CRC(291fd8db) SHA1(1de6bd0e203f16c44f7d661e44863a1a919f3da9) )
ROM_END


ROM_START( tddragon )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "593", 0x0000, 0x1000, CRC(2642f778) SHA1(fee77acf93e057a8b4627389dfd481c6d9cbd02b) )

	ROM_REGION( 511434, "svg", 0)
	ROM_LOAD( "tddragon.svg", 0, 511434, CRC(641e7ceb) SHA1(bbfc37cc085e00921422f65d9aac9949f871e7b7) )
ROM_END


ROM_START( tkarnov )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "582", 0x0000, 0x1000, CRC(cee85bdd) SHA1(143e39524f1dea523e0575f327ed189343cc87f5) )

	ROM_REGION( 527377, "svg", 0)
	ROM_LOAD( "tkarnov.svg", 0, 527377, CRC(971840fc) SHA1(48db7139fa875e60b44340fb475b6d1081ef5c10) )
ROM_END


ROM_START( tvindictr )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "595", 0x0000, 0x1000, CRC(b574d16f) SHA1(d2cb0f2e21ca2defe49a4b45f4c8e169ae9979ab) )

	ROM_REGION( 314165, "svg", 0)
	ROM_LOAD( "tvindictr.svg", 0, 314165, CRC(2241992c) SHA1(efd44879d1c0d5befd7ea07089418406fc101315) )
ROM_END


ROM_START( tgaiden )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "m82", 0x0000, 0x1000, CRC(278eafb0) SHA1(14396a0010bade0fde705969151200ed432321e7) )

	ROM_REGION( 100000, "svg", 0)
	ROM_LOAD( "tgaiden.svg", 0, 100000, NO_DUMP )
ROM_END


ROM_START( tbatman )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "597", 0x0000, 0x1000, CRC(8b7acc97) SHA1(fe811675dc5c5ef9f6f969685c933926c8b9e868) )

	ROM_REGION( 551890, "svg", 0)
	ROM_LOAD( "tbatman.svg", 0, 551890, CRC(65809ee3) SHA1(5fc38bdb2108d45dc99bce3379253423ea88e0fc) )
ROM_END


ROM_START( tsharr2 )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "m91", 0x0000, 0x1000, CRC(b207ac79) SHA1(9889dfec26089313ba2bdac845a75a26742d09e1) )

	ROM_REGION( 555126, "svg", 0)
	ROM_LOAD( "tsharr2.svg", 0, 555126, CRC(ff43e29b) SHA1(0af02e65a1dcf95958296a292343430670b67ae5) )
ROM_END


ROM_START( tgoldnaxe )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "m94", 0x0000, 0x1000, CRC(af183fbf) SHA1(23716e2a7c4bb4842b2af1a43fe88db44e18dc17) )

	ROM_REGION( 605483, "svg", 0)
	ROM_LOAD( "tgoldnaxe.svg", 0, 605483, CRC(533bea14) SHA1(08d419bd7af5de7216654dc7f978beed95192c2d) )
ROM_END


ROM_START( trobocop2 )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "m96", 0x0000, 0x1000, CRC(3704b60c) SHA1(04275833e1a79fd33226faf060890b66ae54e1d3) )

	ROM_REGION( 463532, "svg", 0)
	ROM_LOAD( "trobocop2.svg", 0, 463532, CRC(c2b92868) SHA1(87912f02bea967c10ba1d8f7c810e3c44b0e3cff) )
ROM_END


ROM_START( taltbeast )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "m88", 0x0000, 0x1000, CRC(1b3d15e7) SHA1(78371230dff872d6c07eefdbc4856c2a3336eb61) )

	ROM_REGION( 667887, "svg", 0)
	ROM_LOAD( "taltbeast.svg", 0, 667887, CRC(1ca9bbf1) SHA1(be844dddee4a95f70ea2adf875d3ee6cda2a6633) )
ROM_END


ROM_START( tswampt )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mb0", 0x0000, 0x1000, CRC(8433530c) SHA1(60716d3bba92dc8ac3f1ee29c5734c9e894a1aff) )

	ROM_REGION( 578505, "svg", 0)
	ROM_LOAD( "tswampt.svg", 0, 578505, CRC(98ff2fbb) SHA1(a5a4e9934b86f69176549f99246b40f323441945) )
ROM_END


ROM_START( tmchammer )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "n63.program", 0x0000, 0x1000, CRC(303aa6f7) SHA1(296689be1ee05238e52e9882812868b2ea96202c) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "n63.melody", 0x000, 0x100, CRC(77c1a5a3) SHA1(c00ae3b7c64dd9db96eab520fe674a40571fc15f) )

	ROM_REGION( 456446, "svg", 0)
	ROM_LOAD( "tmchammer.svg", 0, 456446, CRC(79d6d45d) SHA1(bf6b8c6fdccad657377ad9f721dd22408f6ae775) )
ROM_END


ROM_START( tbtoads )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mb3", 0x0000, 0x1000, CRC(8fa4c55a) SHA1(2be97e63dfed51313e180d7388dd431058db5a51) )

	ROM_REGION( 694365, "svg", 0)
	ROM_LOAD( "tbtoads.svg", 0, 694365, CRC(3af488e9) SHA1(d0e9ec61fac23bb22e508da4fa8bf2a7b8f186cf) )
ROM_END


ROM_START( tsonic )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "n71.program", 0x0000, 0x1000, CRC(44cafd68) SHA1(bf8d0ab88d153fabc688ffec19959209ca79c3db) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "n71.melody", 0x000, 0x100, CRC(bae258c8) SHA1(81cb75d73fab4479cd92fcb13d9cb03cec2afdd5) )

	ROM_REGION( 541450, "svg", 0)
	ROM_LOAD( "tsonic.svg", 0, 541450, CRC(f01835e3) SHA1(25f924af55ffadd2aebf50a89f75571d788d5ac1) )
ROM_END


ROM_START( tdummies )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "me0", 0x0000, 0x1000, CRC(29efae4a) SHA1(0b26913a3fd2fde2b39549f0f7cbc3daaa41eb50) )

	ROM_REGION( 525493, "svg", 0)
	ROM_LOAD( "tdummies.svg", 0, 525493, CRC(a18a5216) SHA1(1238e8c489445e715d4fc53e597820845b386233) )
ROM_END


ROM_START( tsfight2 )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "me1", 0x0000, 0x1000, CRC(73384e94) SHA1(350417d101ce034b3974b4a1d2e04bcb3bf70605) )

	ROM_REGION( 630403, "svg", 0)
	ROM_LOAD( "tsfight2.svg", 0, 630403, CRC(eadc2c81) SHA1(20b2a797f6b9a008c1994eaee7b87e3fe828e837) )
ROM_END


ROM_START( tsddragon )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mf5", 0x0000, 0x1000, CRC(264c8e82) SHA1(470eb2f09a58ef05eb0b7c8e11380ad1d8ce4e1a) )

	ROM_REGION( 753533, "svg", 0)
	ROM_LOAD( "tsddragon.svg", 0, 753533, CRC(fb526049) SHA1(552fe005a6e23e083867b7d1c10d20daa8913a14) )
ROM_END


ROM_START( tnmarebc )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mg0", 0x0000, 0x1000, CRC(5ef21421) SHA1(8fd458575111b89d7c33c969e76703bde5ad2c36) )

	ROM_REGION( 631310, "svg", 0)
	ROM_LOAD( "tnmarebc.svg", 0, 631310, CRC(f9c96205) SHA1(1947d358efd94ae3257ed959172a819798d2c9a1) )
ROM_END


ROM_START( tmkombat )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mg6", 0x0000, 0x1000, CRC(f6375dc7) SHA1(a711199c2623979f19c11067ebfff9355256c2c3) )

	ROM_REGION( 841829, "svg", 0)
	ROM_LOAD( "tmkombat.svg", 0, 841829, CRC(9dc4f58c) SHA1(9c9b080d7f3b777407445c22195990c55c6352ca) )
ROM_END


ROM_START( tshadow )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mj5", 0x0000, 0x1000, CRC(09822d73) SHA1(30cae8b783a4f388193aee248fa18c6c1042e0ec) )

	ROM_REGION( 946450, "svg", 0)
	ROM_LOAD( "tshadow.svg", 0, 946450, CRC(5cab680a) SHA1(8f8f660c08fc56287362b11c183655047fbd91ca) )
ROM_END


ROM_START( tskelwarr )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mk0", 0x0000, 0x1000, CRC(dc7827a1) SHA1(74ff143605684df0c70db604a5f22dbf512044d7) )

	ROM_REGION( 1125002, "svg", 0)
	ROM_LOAD( "tskelwarr.svg", 0, 1125002, CRC(49c6ca24) SHA1(71f4ed98ab558deeb86820b7fbf7534a7b7d6b01) )
ROM_END


ROM_START( tbatfor )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mk3", 0x0000, 0x1000, CRC(9993c382) SHA1(0c89e21024315ce7c086af5390c60f5766028c4f) )

	ROM_REGION( 902364, "svg", 0)
	ROM_LOAD( "tbatfor.svg", 0, 902364, CRC(56889c05) SHA1(dda393ca99196de38ad2e989ec6c292adc36ec5e) )
ROM_END


ROM_START( tjdredd )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "mk5", 0x0000, 0x1000, CRC(7beee5a7) SHA1(9a190197c5751b43a9ab2dc8c536934dc5fc5e83) )

	ROM_REGION( 1051586, "svg", 0)
	ROM_LOAD( "tjdredd.svg", 0, 1051586, CRC(4fcdca0a) SHA1(d4b019fec94890ba6600baf2b2096dbcf3295180) )
ROM_END


ROM_START( tapollo13 )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "10_07", 0x0000, 0x1000, CRC(63d0deaa) SHA1(d5de99d5e0ee08ec2ebeef7189ebac1c008d2e7d) )

	ROM_REGION( 643176, "svg", 0)
	ROM_LOAD( "tapollo13.svg", 0, 643176, CRC(e2dac162) SHA1(4089fa485579d2b87ac49b1cf33d6c2c085ea4c5) )
ROM_END


ROM_START( tsjam )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "10_23", 0x0000, 0x1000, CRC(6eaabfbd) SHA1(f0ecbd6f65fe72ce2d8a452685be2e77a63fc9f0) )

	ROM_REGION( 1046147, "svg", 0)
	ROM_LOAD( "tsjam.svg", 0, 1046147, CRC(6d24e1c9) SHA1(ddbfbd85f70ec964c68f982a8ee8070e3786a85e) )
ROM_END


ROM_START( tigarden )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "0019_238e", 0x0000, 0x1000, CRC(8bd0eadd) SHA1(7bb5eb30d569901dce52d777bc01c0979e4afa06) )

	ROM_REGION( 409084, "svg", 0)
	ROM_LOAD( "tigarden.svg", 0, 409084, CRC(cfda5138) SHA1(1bc4ed65ae0cdca3e1e9458d68ca4d6e0fc0e901) )
ROM_END


ROM_START( nummunch )
	ROM_REGION( 0x1000, "maincpu", 0 )
	ROM_LOAD( "772.program", 0x0000, 0x1000, CRC(2f7ff516) SHA1(132e7c5c4d69170953b2e51731992d6d6ba829f9) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "772.melody", 0x000, 0x100, CRC(96fe463a) SHA1(dcef5eee15a3f6d21e0db1b8ae3fbddc81633fc8) )

	ROM_REGION( 140664, "svg", 0)
	ROM_LOAD( "nummunch.svg", 0, 140664, CRC(879df7e2) SHA1(78d8500a445cbbea0090d4e97b781c1e4ed11dd3) )
ROM_END



//    YEAR  NAME         PARENT   COMP  MACHINE      INPUT        STATE           INIT  COMPANY, FULLNAME, FLAGS
CONS( 1989, kdribble,    0,          0, kdribble,    kdribble,    kdribble_state,    0, "Konami", "Double Dribble (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, ktopgun,     0,          0, ktopgun,     ktopgun,     ktopgun_state,     0, "Konami", "Top Gun (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, kcontra,     0,          0, kcontra,     kcontra,     kcontra_state,     0, "Konami", "Contra (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, ktmnt,       0,          0, ktmnt,       ktmnt,       ktmnt_state,       0, "Konami", "Teenage Mutant Ninja Turtles (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, kgradius,    0,          0, kgradius,    kgradius,    kgradius_state,    0, "Konami", "Gradius (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, kloneran,    0,          0, kloneran,    kloneran,    kloneran_state,    0, "Konami", "Lone Ranger (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, kblades,     0,          0, kblades,     kblades,     kblades_state,     0, "Konami", "Blades of Steel (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, knfl,        0,          0, knfl,        knfl,        knfl_state,        0, "Konami", "NFL Football (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, kbilly,      0,          0, kbilly,      kbilly,      kbilly_state,      0, "Konami", "The Adventures of Bayou Billy (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1991, kbucky,      0,          0, kbucky,      kbucky,      kbucky_state,      0, "Konami", "Bucky O'Hare (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1991, kgarfld,     0,          0, kgarfld,     kgarfld,     kgarfld_state,     0, "Konami", "Garfield (handheld)", MACHINE_SUPPORTS_SAVE )

CONS( 1981, gnw_mmouse,  0,          0, gnw_mmouse,  gnw_mmouse,  gnw_mmouse_state,  0, "Nintendo", "Game & Watch: Mickey Mouse", MACHINE_SUPPORTS_SAVE )
CONS( 1981, gnw_egg,     gnw_mmouse, 0, gnw_egg,     gnw_mmouse,  gnw_mmouse_state,  0, "Nintendo", "Game & Watch: Egg", MACHINE_SUPPORTS_SAVE )
CONS( 1984, nupogodi,    gnw_mmouse, 0, nupogodi,    gnw_mmouse,  gnw_mmouse_state,  0, "Elektronika", "Nu, pogodi!", MACHINE_SUPPORTS_SAVE )
CONS( 1989, exospace,    gnw_mmouse, 0, exospace,    exospace,    gnw_mmouse_state,  0, "Elektronika", "Explorers of Space", MACHINE_SUPPORTS_SAVE )

CONS( 1982, gnw_mickdon, 0,          0, gnw_mickdon, gnw_mickdon, gnw_mickdon_state, 0, "Nintendo", "Game & Watch: Mickey & Donald", MACHINE_SUPPORTS_SAVE )
CONS( 1983, gnw_dkong2,  0,          0, gnw_dkong2,  gnw_dkong2,  gnw_dkong2_state,  0, "Nintendo", "Game & Watch: Donkey Kong II", MACHINE_SUPPORTS_SAVE )
CONS( 1983, gnw_mario,   0,          0, gnw_mario,   gnw_mario,   gnw_mario_state,   0, "Nintendo", "Game & Watch: Mario Bros.", MACHINE_SUPPORTS_SAVE )

CONS( 1982, gnw_dkjr,    0,          0, gnw_dkjr,    gnw_dkjr,    gnw_dkjr_state,    0, "Nintendo", "Game & Watch: Donkey Kong Jr. (new wide screen)", MACHINE_SUPPORTS_SAVE )
CONS( 1983, gnw_mariocm, 0,          0, gnw_mariocm, gnw_mariocm, gnw_mariocm_state, 0, "Nintendo", "Game & Watch: Mario's Cement Factory (new wide screen)", MACHINE_SUPPORTS_SAVE )
CONS( 1988, gnw_smb,     0,          0, gnw_smb,     gnw_smb,     gnw_smb_state,     0, "Nintendo", "Game & Watch: Super Mario Bros. (new wide screen)", MACHINE_SUPPORTS_SAVE | MACHINE_NOT_WORKING )

CONS( 1984, gnw_boxing,  0,          0, gnw_boxing,  gnw_boxing,  gnw_boxing_state,  0, "Nintendo", "Game & Watch: Boxing", MACHINE_SUPPORTS_SAVE )

CONS( 1989, tgaunt,      0,          0, tgaunt,      tgaunt,      tgaunt_state,      0, "Tiger Electronics (licensed from Tengen)", "Gauntlet (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1991, trobhood,    tgaunt,     0, trobhood,    trobhood,    tgaunt_state,      0, "Tiger Electronics", "Robin Hood (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, tddragon,    0,          0, tddragon,    tddragon,    tddragon_state,    0, "Tiger Electronics (licensed from Technos/Tradewest)", "Double Dragon (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, tkarnov,     0,          0, tkarnov,     tkarnov,     tkarnov_state,     0, "Tiger Electronics (licensed from Data East)", "Karnov (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, tvindictr,   0,          0, tvindictr,   tvindictr,   tvindictr_state,   0, "Tiger Electronics (licensed from Tengen)", "Vindicators (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1989, tgaiden,     0,          0, tgaiden,     tgaiden,     tgaiden_state,     0, "Tiger Electronics (licensed from Tecmo)", "Ninja Gaiden (handheld)", MACHINE_SUPPORTS_SAVE | MACHINE_NOT_WORKING )
CONS( 1989, tbatman,     0,          0, tbatman,     tbatman,     tbatman_state,     0, "Tiger Electronics", "Batman (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1990, tsharr2,     0,          0, tsharr2,     tsharr2,     tsharr2_state,     0, "Tiger Electronics (licensed from Sega)", "Space Harrier II (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1990, tgoldnaxe,   0,          0, tgoldnaxe,   tgoldnaxe,   tgoldnaxe_state,   0, "Tiger Electronics (licensed from Sega)", "Golden Axe (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1990, trobocop2,   0,          0, trobocop2,   trobocop2,   trobocop2_state,   0, "Tiger Electronics", "Robocop 2 (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1990, taltbeast,   0,          0, taltbeast,   taltbeast,   taltbeast_state,   0, "Tiger Electronics (licensed from Sega)", "Altered Beast (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1991, tswampt,     0,          0, tswampt,     tswampt,     tswampt_state,     0, "Tiger Electronics", "Swamp Thing (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1991, tmchammer,   0,          0, tmchammer,   tmchammer,   tmchammer_state,   0, "Tiger Electronics", "MC Hammer: U Can't Touch This (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1991, tbtoads,     0,          0, tbtoads,     tbtoads,     tbtoads_state,     0, "Tiger Electronics (licensed from Rare/Tradewest)", "Battletoads (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1992, tsonic,      0,          0, tsonic,      tsonic,      tsonic_state,      0, "Tiger Electronics (licensed from Sega)", "Sonic The Hedgehog (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1993, tdummies,    0,          0, tdummies,    tdummies,    tdummies_state,    0, "Tiger Electronics", "The Incredible Crash Dummies (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1993, tsfight2,    0,          0, tsfight2,    tsfight2,    tsfight2_state,    0, "Tiger Electronics (licensed from Capcom)", "Street Fighter II (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1993, tsddragon,   0,          0, tsddragon,   tsddragon,   tsddragon_state,   0, "Tiger Electronics (licensed from Technos)", "Super Double Dragon (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1993, tnmarebc,    0,          0, tnmarebc,    tnmarebc,    tnmarebc_state,    0, "Tiger Electronics", "Nightmare Before Christmas (handheld)", MACHINE_SUPPORTS_SAVE ) // note: title has no "The"
CONS( 1993, tmkombat,    0,          0, tmkombat,    tmkombat,    tmkombat_state,    0, "Tiger Electronics (licensed from Midway)", "Mortal Kombat (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1994, tshadow,     0,          0, tshadow,     tshadow,     tshadow_state,     0, "Tiger Electronics", "The Shadow (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1994, tskelwarr,   0,          0, tskelwarr,   tskelwarr,   tskelwarr_state,   0, "Tiger Electronics", "Skeleton Warriors - The Dark Crusade (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1995, tbatfor,     0,          0, tbatfor,     tbatfor,     tbatfor_state,     0, "Tiger Electronics", "Batman Forever - Double Dose of Doom (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1995, tjdredd,     0,          0, tjdredd,     tjdredd,     tjdredd_state,     0, "Tiger Electronics", "Judge Dredd (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1995, tapollo13,   0,          0, tapollo13,   tapollo13,   tapollo13_state,   0, "Tiger Electronics", "Apollo 13 (handheld)", MACHINE_SUPPORTS_SAVE )
CONS( 1996, tsjam,       0,          0, tsjam,       tsjam,       tsjam_state,       0, "Tiger Electronics", "Space Jam (handheld)", MACHINE_SUPPORTS_SAVE )

CONS( 1983, tigarden,    0,          0, tigarden,    tigarden,    tigarden_state,    0, "Tronica", "Thief in Garden", MACHINE_SUPPORTS_SAVE )

CONS( 1989, nummunch,    0,          0, nummunch,    nummunch,    nummunch_state,    0, "VTech", "Electronic Number Muncher", MACHINE_SUPPORTS_SAVE )
