// license:BSD-3-Clause
// copyright-holders:hap, Sean Riddle
/***************************************************************************

  ** subclass of hh_sm510_state (includes/hh_sm510.h, drivers/hh_sm510.cpp) **

  Tiger R-Zone driver

  This is a backwards console, the heart of the machine is the cartridge.
  The console houses the controller, speaker, power, backlight, and a
  polarizer filter for the screen. The cartridge has the MCU, optional
  sound ROM, and the LCD screen in a translucent window.

  Console family:

  1995: R-Zone HeadGear: Wearable headset, controller is separate,
  red-on-black screen is reflected in front of the right eye.
  1996: R-Zone SuperScreen: Handheld console, inverted filter(aka black
  LCD segments), optional background sheet as with standalone handhelds.
  1997: R-Zone X.P.G - Xtreme Pocket Game: Handheld version of HeadGear.
  1997: R-Zone DataZone: PDA with a built-in SuperScreen.

  TODO:
  - support for SuperScreen. SVG colors will need to be inverted, or maybe
    with artwork or HLSL?
  - add DataZone, will get its own driver

***************************************************************************/

#include "emu.h"
#include "includes/hh_sm510.h"

#include "cpu/sm510/sm510.h"
#include "screen.h"
#include "speaker.h"

// internal artwork
#include "rzone.lh"

class rzone_state : public hh_sm510_state
{
public:
	rzone_state(const machine_config &mconfig, device_type type, const char *tag)
		: hh_sm510_state(mconfig, type, tag),
		m_led_out(*this, "led"),
		m_led_off(*this, "led_off")
	{ }

	output_finder<> m_led_out;
	required_device<timer_device> m_led_off;

	int m_led_pin;
	int m_sctrl;
	int m_sclock;

	TIMER_DEVICE_CALLBACK_MEMBER(led_off_callback) { m_led_out = m_led_pin ? 1 : 0; }
	DECLARE_WRITE_LINE_MEMBER(led_w);
	DECLARE_WRITE_LINE_MEMBER(audio_w);
	DECLARE_WRITE_LINE_MEMBER(sctrl_w);
	DECLARE_WRITE_LINE_MEMBER(sclock_w);
	DECLARE_READ_LINE_MEMBER(sdata_r);

	DECLARE_WRITE8_MEMBER(t1_write_r);
	DECLARE_WRITE8_MEMBER(t1_write_s);
	virtual DECLARE_READ8_MEMBER(input_r) override;

	void t2_update_audio();
	DECLARE_WRITE8_MEMBER(t2_write_r);
	DECLARE_WRITE8_MEMBER(t2_write_s);

	void rzbatfor(machine_config &config);
	void rzindy500(machine_config &config);

protected:
	virtual void machine_start() override;
};


// machine start

void rzone_state::machine_start()
{
	hh_sm510_state::machine_start();

	// resolve handlers
	m_led_out.resolve();

	// zerofill
	m_led_pin = 0;
	m_sctrl = 0;
	m_sclock = 0;

	// register for savestates
	save_item(NAME(m_led_pin));
	save_item(NAME(m_sctrl));
	save_item(NAME(m_sclock));
}


/***************************************************************************

  I/O

***************************************************************************/

// console

WRITE_LINE_MEMBER(rzone_state::led_w)
{
	// LED: enable backlight
	if (state)
		m_led_out = 1;

	// delay led off to prevent flickering
	if (!state && m_led_pin)
		m_led_off->adjust(attotime::from_msec(30));

	m_led_pin = state;
}

WRITE_LINE_MEMBER(rzone_state::audio_w)
{
	// Audio: speaker out
	m_speaker->level_w(state ? 1 : 0);
}

WRITE_LINE_MEMBER(rzone_state::sctrl_w)
{
	// SCTRL: 74165 SH/LD: reload inputs while low
	if (!state || !m_sctrl)
		m_inp_mux = m_inp_matrix[0]->read();

	m_sctrl = state;
}

WRITE_LINE_MEMBER(rzone_state::sclock_w)
{
	// SCLOCK: 74165 CLK: shift inputs on rising edge when 74165 SH/LD is high
	if (m_sctrl && !m_sclock && state)
		m_inp_mux >>= 1;

	m_sclock = state;
}

READ_LINE_MEMBER(rzone_state::sdata_r)
{
	// SDATA: 74165 Q
	sctrl_w(m_sctrl); // reload inputs if needed
	return m_inp_mux & 1;
}


// cartridge type 1: simple SM510

WRITE8_MEMBER(rzone_state::t1_write_r)
{
	// R1: Audio
	audio_w(data & 1);

	// R2: SCTRL
	sctrl_w(data >> 1 & 1);
}

WRITE8_MEMBER(rzone_state::t1_write_s)
{
	// S1: LED
	led_w(data & 1);

	// S2: SCLOCK
	sclock_w(data >> 1 & 1);
}

READ8_MEMBER(rzone_state::input_r)
{
	// K1: SDATA
	return sdata_r();
}


// cartridge type 2: simple SM512, 2 audio lines

void rzone_state::t2_update_audio()
{
	audio_w((m_s >> 2 & 1) | (m_r & 1));
}

WRITE8_MEMBER(rzone_state::t2_write_r)
{
	// R: Audio
	m_r = data;
	t2_update_audio();
}

WRITE8_MEMBER(rzone_state::t2_write_s)
{
	// S1: SCTRL
	sctrl_w(data & 1);

	// S2: SCLOCK
	sclock_w(data >> 1 & 1);

	// S3: Audio
	m_s = data;
	t2_update_audio();

	// S4: LED
	led_w(data >> 3 & 1);
}



/***************************************************************************

  Inputs

***************************************************************************/

static INPUT_PORTS_START( rzone )
	PORT_START("IN.0")
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_POWER_ON ) PORT_CHANGED_MEMBER(DEVICE_SELF, hh_sm510_state, input_changed, nullptr)
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP )
	PORT_BIT( 0x0004, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT )
	PORT_BIT( 0x0008, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT )
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN )
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_BUTTON1 ) // A
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_BUTTON2 ) // B
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_BUTTON3 ) // C
	PORT_BIT( 0x0100, IP_ACTIVE_HIGH, IPT_BUTTON4 ) // D
	PORT_BIT( 0x0200, IP_ACTIVE_HIGH, IPT_VOLUME_DOWN ) PORT_NAME("Sound")
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_SELECT )
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_POWER_OFF )
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_BUTTON5 ) PORT_NAME("Pause")
	PORT_BIT( 0x2000, IP_ACTIVE_HIGH, IPT_START )
INPUT_PORTS_END



/***************************************************************************

  Machine Config

***************************************************************************/

MACHINE_CONFIG_START(rzone_state::rzindy500)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM510, 32.768_kHz_XTAL) // no external XTAL
	MCFG_SM510_R_MASK_OPTION(SM510_R_CONTROL_OUTPUT) // confirmed
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(rzone_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(rzone_state, t1_write_s))
	MCFG_SM510_WRITE_R_CB(WRITE8(rzone_state, t1_write_r))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1425, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1425-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD("led_off", rzone_state, led_off_callback)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_rzone)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END

MACHINE_CONFIG_START(rzone_state::rzbatfor)

	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", SM512, 32.768_kHz_XTAL) // no external XTAL
	MCFG_SM510_WRITE_SEGS_CB(WRITE16(hh_sm510_state, sm510_lcd_segment_w))
	MCFG_SM510_READ_K_CB(READ8(rzone_state, input_r))
	MCFG_SM510_WRITE_S_CB(WRITE8(rzone_state, t2_write_s))
	MCFG_SM510_WRITE_R_CB(WRITE8(rzone_state, t2_write_r))

	/* video hardware */
	MCFG_SCREEN_SVG_ADD("screen", "svg")
	MCFG_SCREEN_REFRESH_RATE(50)
	MCFG_SCREEN_SIZE(1368, 1080)
	MCFG_SCREEN_VISIBLE_AREA(0, 1368-1, 0, 1080-1)

	MCFG_TIMER_DRIVER_ADD("led_off", rzone_state, led_off_callback)
	MCFG_TIMER_DRIVER_ADD_PERIODIC("display_decay", hh_sm510_state, display_decay_tick, attotime::from_msec(1))
	MCFG_DEFAULT_LAYOUT(layout_rzone)

	/* sound hardware */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 0.25)
MACHINE_CONFIG_END



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( rzindy500 )
	ROM_REGION( 0x1000, "maincpu", 0 ) // SM510 under epoxy, KMS10 22
	ROM_LOAD( "kms10_22", 0x0000, 0x1000, CRC(99a746d0) SHA1(64264499d45a566fa9a0801c20e7fa27eac18da6) )

	ROM_REGION( 533411, "svg", 0)
	ROM_LOAD( "rzindy500.svg", 0, 533411, CRC(cfc85677) SHA1(014b9123d81fba1488b4a22a6b6fd0c09e22c1ea) )
ROM_END

ROM_START( rzbatfor )
	ROM_REGION( 0x1000, "maincpu", 0 ) // SM512 under epoxy, KMN1202 (not the same game as the standalone Tiger handheld)
	ROM_LOAD( "kmn1202.program", 0x0000, 0x1000, CRC(27abdb52) SHA1(b356ff80b628244da588b4748404b78d7a57eccd) )

	ROM_REGION( 0x100, "maincpu:melody", 0 )
	ROM_LOAD( "kmn1202.melody", 0x000, 0x100, CRC(d794746c) SHA1(f0706c5100c090c65fcb2d768b5a5b4a55b29e04) )

	ROM_REGION( 652556, "svg", 0)
	ROM_LOAD( "rzbatfor.svg", 0, 652556, CRC(4d850489) SHA1(31a2a1e9209c0f77dbc268cddbfa4a67478734a7) )
ROM_END


//    YEAR  NAME       PARENT  COMP MACHINE    INPUT      STATE        INIT  COMPANY, FULLNAME, FLAGS
CONS( 1995, rzindy500, 0,        0, rzindy500, rzone,     rzone_state,    0, "Tiger Electronics (licensed from Sega)", "R-Zone: Indy 500", MACHINE_SUPPORTS_SAVE )
CONS( 1995, rzbatfor,  0,        0, rzbatfor,  rzone,     rzone_state,    0, "Tiger Electronics", "R-Zone: Batman Forever", MACHINE_SUPPORTS_SAVE )
