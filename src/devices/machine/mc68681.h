// license:BSD-3-Clause
// copyright-holders:Mariusz Wojcieszek, R. Belmont
#ifndef MAME_MACHINE_MC68681_H
#define MAME_MACHINE_MC68681_H

#pragma once


#define MCFG_MC68681_IRQ_CALLBACK(_cb) \
	devcb = &duart_base_device::set_irq_cb(*device, DEVCB_##_cb);

#define MCFG_MC68681_A_TX_CALLBACK(_cb) \
	devcb = &duart_base_device::set_a_tx_cb(*device, DEVCB_##_cb);

#define MCFG_MC68681_B_TX_CALLBACK(_cb) \
	devcb = &duart_base_device::set_b_tx_cb(*device, DEVCB_##_cb);

// deprecated: use ipX_w() instead
#define MCFG_MC68681_INPORT_CALLBACK(_cb) \
	devcb = &duart_base_device::set_inport_cb(*device, DEVCB_##_cb);

#define MCFG_MC68681_OUTPORT_CALLBACK(_cb) \
	devcb = &duart_base_device::set_outport_cb(*device, DEVCB_##_cb);

#define MCFG_MC68681_SET_EXTERNAL_CLOCKS(_a, _b, _c, _d) \
	duart_base_device::static_set_clocks(*device, _a, _b, _c, _d);

// SC28C94 specific callbacks
#define MCFG_SC28C94_ADD(_tag, _clock) \
	MCFG_DEVICE_ADD(_tag, SC28C94, _clock)

#define MCFG_SC28C94_C_TX_CALLBACK(_cb) \
	devcb = &sc28c94_device::set_c_tx_cb(*device, DEVCB_##_cb);

#define MCFG_SC28C94_D_TX_CALLBACK(_cb) \
	devcb = &sc28c94_device::set_d_tx_cb(*device, DEVCB_##_cb);

// MC68340SERIAL specific callbacks
#define MCFG_MC68340DUART_ADD(_tag, _clock) \
	MCFG_DEVICE_ADD(_tag, MC68340_DUART, _clock)

#define MC68681_RX_FIFO_SIZE                3

// forward declaration
class duart_base_device;

// duart_channel class
class duart_channel : public device_t, public device_serial_interface
{
public:
	duart_channel(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	// device_serial overrides
	virtual void rcv_complete() override;    // Rx completed receiving byte
	virtual void tra_complete() override;    // Tx completed sending byte
	virtual void tra_callback() override;    // Tx send bit

	uint8_t read_chan_reg(int reg);
	void write_chan_reg(int reg, uint8_t data);
	void update_interrupts();

	uint8_t read_rx_fifo();

	void ACR_updated();

	uint8_t get_chan_CSR();

	// Access methods needed for 68340 serial module register model
	uint8_t read_MR1(){ return MR1; }
	uint8_t read_MR2(){ return MR2; }
	void write_MR1(uint8_t data){ MR1 = data; }
	void write_MR2(uint8_t data){ MR2 = data; }

private:
	/* Registers */
	uint8_t CR;  /* Command register */
	uint8_t CSR; /* Clock select register */
	uint8_t MR1; /* Mode register 1 */
	uint8_t MR2; /* Mode register 2 */
	uint8_t MR_ptr; /* Mode register pointer */
	uint8_t SR;  /* Status register */

	/* State */
	int tx_baud_rate, rx_baud_rate;

	/* Receiver */
	uint8_t rx_enabled;
	uint8_t rx_fifo[MC68681_RX_FIFO_SIZE];
	int   rx_fifo_read_ptr;
	int   rx_fifo_write_ptr;
	int   rx_fifo_num;

	int m_ch;

	/* Transmitter */
	uint8_t tx_enabled;
	uint8_t tx_data;
	uint8_t tx_ready;

	duart_base_device *m_uart;

	void write_MR(uint8_t data);
	void write_CR(uint8_t data);
	void write_TX(uint8_t data);
	void recalc_framing();
};

class duart_base_device : public device_t
{
	friend class duart_channel;

public:
	required_device<duart_channel> m_chanA;
	required_device<duart_channel> m_chanB;
	optional_device<duart_channel> m_chanC;
	optional_device<duart_channel> m_chanD;

	// inline configuration helpers
	static void static_set_clocks(device_t &device, int clk3, int clk4, int clk5, int clk6);
	static void static_set_clocks(device_t &device, const XTAL &clk3, const XTAL &clk4, const XTAL &clk5, const XTAL &clk6) {
		static_set_clocks(device, clk3.value(), clk4.value(), clk5.value(), clk6.value());
	}

	// API
	virtual DECLARE_READ8_MEMBER(read);
	virtual DECLARE_WRITE8_MEMBER(write);

	DECLARE_WRITE_LINE_MEMBER( rx_a_w ) { m_chanA->device_serial_interface::rx_w((uint8_t)state); }
	DECLARE_WRITE_LINE_MEMBER( rx_b_w ) { m_chanB->device_serial_interface::rx_w((uint8_t)state); }

	template <class Object> static devcb_base &set_irq_cb(device_t &device, Object &&cb) { return downcast<duart_base_device &>(device).write_irq.set_callback(std::forward<Object>(cb)); }
	template <class Object> static devcb_base &set_a_tx_cb(device_t &device, Object &&cb) { return downcast<duart_base_device &>(device).write_a_tx.set_callback(std::forward<Object>(cb)); }
	template <class Object> static devcb_base &set_b_tx_cb(device_t &device, Object &&cb) { return downcast<duart_base_device &>(device).write_b_tx.set_callback(std::forward<Object>(cb)); }
	template <class Object> static devcb_base &set_inport_cb(device_t &device, Object &&cb) { return downcast<duart_base_device &>(device).read_inport.set_callback(std::forward<Object>(cb)); }
	template <class Object> static devcb_base &set_outport_cb(device_t &device, Object &&cb) { return downcast<duart_base_device &>(device).write_outport.set_callback(std::forward<Object>(cb)); }

	// new-style push handlers for input port bits
	DECLARE_WRITE_LINE_MEMBER( ip0_w );
	DECLARE_WRITE_LINE_MEMBER( ip1_w );
	DECLARE_WRITE_LINE_MEMBER( ip2_w );
	DECLARE_WRITE_LINE_MEMBER( ip3_w );
	DECLARE_WRITE_LINE_MEMBER( ip4_w );
	DECLARE_WRITE_LINE_MEMBER( ip5_w );
	DECLARE_WRITE_LINE_MEMBER( ip6_w );

	bool irq_pending() const { return (ISR & IMR) != 0; }

protected:
	duart_base_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);

	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void device_add_mconfig(machine_config &config) override;

	devcb_write_line write_irq, write_a_tx, write_b_tx, write_c_tx, write_d_tx;
	devcb_read8 read_inport;
	devcb_write8 write_outport;
	int32_t ip3clk, ip4clk, ip5clk, ip6clk;

protected:
	virtual void update_interrupts();

private:
	TIMER_CALLBACK_MEMBER( duart_timer_callback );

	/* registers */
	uint8_t ACR;  /* Auxiliary Control Register */
	uint8_t IMR;  /* Interrupt Mask Register */
	uint8_t ISR;  /* Interrupt Status Register */
	uint8_t OPCR; /* Output Port Conf. Register */
	uint8_t OPR;  /* Output Port Register */
	PAIR  CTR;  /* Counter/Timer Preset Value */
	uint8_t IPCR; /* Input Port Control Register */

	/* state */
	uint8_t IP_last_state; /* last state of IP bits */

	/* timer */
	uint8_t half_period;
	emu_timer *duart_timer;

	double get_ct_rate();
	uint16_t get_ct_count();
	void start_ct(int count);
	int calc_baud(int ch, uint8_t data);
	void clear_ISR_bits(int mask);
	void set_ISR_bits(int mask);

	int get_ch(duart_channel *ch)
	{
		if (ch == m_chanA)
		{
			return 0;
		}
		else if (ch == m_chanB)
		{
			return 1;
		}
		else if (ch == m_chanC)
		{
			return 2;
		}

		return 3;
	}

	duart_channel *get_channel(int chan);
};

class scn2681_device : public duart_base_device
{
public:
	scn2681_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);
};

class mc68681_device : public duart_base_device
{
public:
	mc68681_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	virtual DECLARE_READ8_MEMBER(read) override;
	virtual DECLARE_WRITE8_MEMBER(write) override;
	uint8_t get_irq_vector() { m_read_vector = true; return IVR; }

protected:
	virtual void device_start() override;
	virtual void device_reset() override;
	virtual void update_interrupts() override;

private:
	bool m_read_vector; // if this is read and IRQ is active, it counts as pulling IACK

	uint8_t IVR;  /* Interrupt Vector Register */
};

class sc28c94_device : public duart_base_device
{
public:
	sc28c94_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	template <class Object> static devcb_base &set_c_tx_cb(device_t &device, Object &&cb) { return downcast<sc28c94_device &>(device).write_c_tx.set_callback(std::forward<Object>(cb)); }
	template <class Object> static devcb_base &set_d_tx_cb(device_t &device, Object &&cb) { return downcast<sc28c94_device &>(device).write_d_tx.set_callback(std::forward<Object>(cb)); }

	DECLARE_WRITE_LINE_MEMBER( rx_c_w ) { m_chanC->device_serial_interface::rx_w((uint8_t)state); }
	DECLARE_WRITE_LINE_MEMBER( rx_d_w ) { m_chanD->device_serial_interface::rx_w((uint8_t)state); }

	virtual DECLARE_READ8_MEMBER(read) override;
	virtual DECLARE_WRITE8_MEMBER(write) override;

protected:
	virtual void device_add_mconfig(machine_config &config) override;

private:
};

class mc68340_duart_device : public duart_base_device
{
public:
	mc68340_duart_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	virtual DECLARE_READ8_MEMBER(read) override;
	virtual DECLARE_WRITE8_MEMBER(write) override;

protected:
	virtual void device_add_mconfig(machine_config &config) override;
	mc68340_duart_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock);
};

DECLARE_DEVICE_TYPE(SCN2681, scn2681_device)
DECLARE_DEVICE_TYPE(MC68681, mc68681_device)
DECLARE_DEVICE_TYPE(SC28C94, sc28c94_device)
DECLARE_DEVICE_TYPE(MC68340_DUART, mc68340_duart_device)
DECLARE_DEVICE_TYPE(DUART_CHANNEL, duart_channel)

#endif // MAME_MACHINE_MC68681_H
