// license:BSD-3-Clause
// copyright-holders:Brad Oliver, Bernd Wiebelt, Allard van der Bas

#ifndef BWIDOW_H_
#define BWIDOW_H_

#define MASTER_CLOCK (XTAL(12'096'000))
#define CLOCK_3KHZ   (MASTER_CLOCK / 4096)


class bwidow_state : public driver_device
{
public:
	bwidow_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag) ,
		m_maincpu(*this, "maincpu"),
		m_in3(*this, "IN3"),
		m_in4(*this, "IN4"),
		m_dsw2(*this, "DSW2") { }

	int m_lastdata;
	DECLARE_READ8_MEMBER(spacduel_IN3_r);
	DECLARE_READ8_MEMBER(bwidowp_in_r);
	DECLARE_WRITE8_MEMBER(bwidow_misc_w);
	DECLARE_WRITE8_MEMBER(spacduel_coin_counter_w);
	DECLARE_WRITE8_MEMBER(irq_ack_w);
	DECLARE_CUSTOM_INPUT_MEMBER(clock_r);
	required_device<cpu_device> m_maincpu;
	optional_ioport m_in3;
	optional_ioport m_in4;
	optional_ioport m_dsw2;
	void spacduel(machine_config &config);
	void gravitar(machine_config &config);
	void bwidowp(machine_config &config);
	void bwidow(machine_config &config);
	void lunarbat(machine_config &config);
	void bwidow_audio(machine_config &config);
	void gravitar_audio(machine_config &config);
};

#endif /* BWIDOW_H_ */
