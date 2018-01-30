// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * An implementation of the IOGA (I/O Gate Array) devices found in Intergraph
 * InterPro family systems. There is no public documentation for these devices,
 * so the implementation is being built to follow the logic of the system boot
 * ROM and diagnostic tests.
 *
 * The device handles most of the I/O for the system, including timers, DMA,
 * interrupts, and target device interfacing. There is a significant amount of
 * work to be completed before the boot diagnostics will pass without errors,
 * let alone successfully booting CLIX.
 *
 * TODO
 *   - edge/level triggered interrupts
 *   - make mouse a slot device
 *   - improve dma and timers
 */

#include "emu.h"
#include "interpro_ioga.h"

// enables hacks which allow iogadiag tests to complete but break scsi dma
#define IOGA_DMA_DIAG_HACK 0

#define LOG_GENERAL   (1U << 0)
#define LOG_NMI       (1U << 1)
#define LOG_INT       (1U << 2)
#define LOG_DMA       (1U << 3)

#define LOG_FLOPPY    (1U << 4)
#define LOG_SCSI      (1U << 5)
#define LOG_NETWORK   (1U << 6)
#define LOG_MOUSE     (1U << 7)
#define LOG_SERIALDMA (1U << 8)

#define LOG_TIMER0    (1U << 0x10)
#define LOG_TIMER1    (1U << 0x11)
#define LOG_TIMER2    (1U << 0x12)
#define LOG_TIMER3    (1U << 0x13)
#define LOG_TIMERRD   (1U << 0x14)
#define LOG_TIMERS    (LOG_TIMER0 | LOG_TIMER1 | LOG_TIMER2 | LOG_TIMER3)

//#define VERBOSE (LOG_GENERAL | LOG_INT | LOG_NMI | LOG_DMA | LOG_SERIALDMA | LOG_NETWORK)
//#define VERBOSE_IRQ ((1 << IRQ_SCSI) | (1 << IRQ_FLOPPY) | (1 << IRQ_MOUSE) | (1 << IRQ_SERDMA) | (1 << IRQ_SERIAL) | (1 << IRQ_ETHERNET))
//#define VERBOSE_DMA ((1 << DMA_PLOTTER) | (1 << DMA_SCSI) | (1 << DMA_FLOPPY))

#define VERBOSE     0
#define VERBOSE_IRQ 0
#define VERBOSE_DMA 0
#include "logmacro.h"

#define LOGIRQ(irq, ...) do { if (VERBOSE_IRQ & (1U << irq)) (LOG_OUTPUT_FUNC)(__VA_ARGS__); } while (false)
#define LOGDMA(dma, ...) do { if (VERBOSE_DMA & (1U << dma)) (LOG_OUTPUT_FUNC)(__VA_ARGS__); } while (false)

const interpro_ioga_device::interrupt_data_t interpro_ioga_device::m_interrupt_data[] =
{
	{ INT_HARD_IN, IRQ_TIMER2,   IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "internal int 3", "timer 2" }, // either external or internal?
	{ INT_HARD_IN, IRQ_TIMER3,   IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "internal int 4", "timer 3" }, // either external or internal?
	{ INT_HARD_EX, IRQ_SCSI,     IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 0", "scsi" },    // IRQ_ENABLE_INTERNAL
	{ INT_HARD_EX, IRQ_FLOPPY,   IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 1", "floppy" },  // IRQ_ENABLE_INTERNAL
	{ INT_HARD_EX, IRQ_PLOTTER,  IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 2", "plotter" }, // IRQ_ENABLE_INTERNAL

	{ INT_HARD_EX, IRQ_SRXCBUS0, IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 3", "srx/cbus 0" },
	{ INT_HARD_EX, IRQ_SRXCBUS1, IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 4", "srx/cbus 1" },
	{ INT_HARD_EX, IRQ_SRXCBUS2, IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 5", "srx/cbus 2" },

	{ INT_HARD_EX, IRQ_VB,       IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 6", "vb" },
	{ INT_HARD_EX, IRQ_9,        IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 7", "?" },
	{ INT_HARD_EX, IRQ_CBUS3,    IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 8", "cbus 3" },
	{ INT_HARD_EX, IRQ_RTC,      IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 9", "clock/calendar" },
	{ INT_HARD_EX, IRQ_12,       IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 10", "sga?" },

	{ INT_HARD_IN, IRQ_MOUSE,    IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "internal int 0", "mouse" },
	{ INT_HARD_IN, IRQ_TIMER0,   IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "internal int 1", "timer 0" },
	{ INT_HARD_IN, IRQ_TIMER1,   IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "internal int 2", "timer 1" },
	{ INT_HARD_IN, IRQ_SERDMA,   IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "internal int 5", "serial dma" }, // IRQ_ENABLE_EXTERNAL << 0 | IRQ_ENABLE_EXTERNAL << 1 | IRQ_ENABLE_EXTERNAL << 2

	{ INT_HARD_EX, IRQ_SERIAL,   IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 11", "serial" },
	{ INT_HARD_EX, IRQ_ETHERNET, IRQ_PENDING | IRQ_ENABLE_EXTERNAL, "external int 12", "ethernet" }, // IRQ_ENABLE_INTERNAL

	{ INT_SOFT_LO, IRQ_SOFT0,    0x0100, "soft int 0", "soft" },
	{ INT_SOFT_LO, IRQ_SOFT1,    0x0200, "soft int 1", "soft" },
	{ INT_SOFT_LO, IRQ_SOFT2,    0x0400, "soft int 2", "soft" },
	{ INT_SOFT_LO, IRQ_SOFT3,    0x0800, "soft int 3", "soft" },
	{ INT_SOFT_LO, IRQ_SOFT4,    0x1000, "soft int 4", "soft" },
	{ INT_SOFT_LO, IRQ_SOFT5,    0x2000, "soft int 5", "soft" },
	{ INT_SOFT_LO, IRQ_SOFT6,    0x4000, "soft int 6", "soft" },
	{ INT_SOFT_LO, IRQ_SOFT7,    0x8000, "soft int 7", "soft" },

	{ INT_SOFT_HI, IRQ_SOFT8,    IRQ_PENDING, "soft int 8", "soft" },
	{ INT_SOFT_HI, IRQ_SOFT9,    IRQ_PENDING, "soft int 9", "soft" },
	{ INT_SOFT_HI, IRQ_SOFT10,   IRQ_PENDING, "soft int 10", "soft" },
	{ INT_SOFT_HI, IRQ_SOFT11,   IRQ_PENDING, "soft int 11", "soft" },
	{ INT_SOFT_HI, IRQ_SOFT12,   IRQ_PENDING, "soft int 12", "soft" },
	{ INT_SOFT_HI, IRQ_SOFT13,   IRQ_PENDING, "soft int 13", "soft" },
	{ INT_SOFT_HI, IRQ_SOFT14,   IRQ_PENDING, "soft int 14", "soft" },
	{ INT_SOFT_HI, IRQ_SOFT15,   IRQ_PENDING, "soft int 15", "soft" },
};

DEVICE_ADDRESS_MAP_START(map, 32, interpro_ioga_device)

	AM_RANGE(0x0c, 0x1b) AM_READWRITE(dma_plotter_r, dma_plotter_w)
	AM_RANGE(0x1c, 0x1f) AM_READWRITE(dma_plotter_eosl_r, dma_plotter_eosl_w)
	AM_RANGE(0x20, 0x2f) AM_READWRITE(dma_scsi_r, dma_scsi_w)
	AM_RANGE(0x30, 0x3f) AM_READWRITE(dma_floppy_r, dma_floppy_w)
	AM_RANGE(0x40, 0x43) AM_READWRITE(serial_dma0_addr_r, serial_dma0_addr_w)
	AM_RANGE(0x44, 0x47) AM_READWRITE(serial_dma0_ctrl_r, serial_dma0_ctrl_w)
	AM_RANGE(0x48, 0x4b) AM_READWRITE(serial_dma1_addr_r, serial_dma1_addr_w)
	AM_RANGE(0x4c, 0x4f) AM_READWRITE(serial_dma1_ctrl_r, serial_dma1_ctrl_w)
	AM_RANGE(0x50, 0x53) AM_READWRITE(serial_dma2_addr_r, serial_dma2_addr_w)
	AM_RANGE(0x54, 0x57) AM_READWRITE(serial_dma2_ctrl_r, serial_dma2_ctrl_w)

	// 5a - sib control?
	AM_RANGE(0x5c, 0x7f) AM_READWRITE16(icr_r, icr_w, 0xffffffff)
	AM_RANGE(0x80, 0x83) AM_READWRITE16(icr18_r, icr18_w, 0x0000ffff)
	AM_RANGE(0x80, 0x83) AM_READWRITE8(softint_r, softint_w, 0x00ff0000)
	AM_RANGE(0x80, 0x83) AM_READWRITE8(nmictrl_r, nmictrl_w, 0xff000000)
	AM_RANGE(0x84, 0x87) AM_READWRITE(mouse_status_r, mouse_status_w)
	AM_RANGE(0x88, 0x8b) AM_READWRITE(prescaler_r, prescaler_w)
	AM_RANGE(0x8c, 0x8f) AM_READWRITE(timer0_r, timer0_w)
	AM_RANGE(0x90, 0x93) AM_READWRITE(timer1_r, timer1_w)
	AM_RANGE(0x94, 0x97) AM_READ(error_address_r)
	AM_RANGE(0x98, 0x9b) AM_READ(error_businfo_r)
	AM_RANGE(0x9c, 0x9f) AM_READWRITE16(arbctl_r, arbctl_w, 0x0000ffff)
	AM_RANGE(0xa0, 0xa3) AM_READWRITE(timer2_count_r, timer2_count_w)
	AM_RANGE(0xa4, 0xa7) AM_READWRITE(timer2_value_r, timer2_value_w)
	AM_RANGE(0xa8, 0xab) AM_READWRITE(timer3_r, timer3_w)
	AM_RANGE(0xac, 0xaf) AM_READWRITE(bus_timeout_r, bus_timeout_w) // boot writes 0x64
	AM_RANGE(0xb0, 0xbf) AM_READWRITE16(softint_vector_r, softint_vector_w, 0xffffffff)

	//c0, c4, c8 -ethernet address a,b,c?
ADDRESS_MAP_END

DEVICE_ADDRESS_MAP_START(map, 32, turquoise_ioga_device)
	AM_INHERIT_FROM(interpro_ioga_device::map)

	AM_RANGE(0x00, 0x03) AM_READWRITE(eth_base_r, eth_base_w)
	AM_RANGE(0x04, 0x07) AM_READWRITE16(eth_control_r, eth_control_w, 0xffff)
ADDRESS_MAP_END

DEVICE_ADDRESS_MAP_START(map, 32, sapphire_ioga_device)
	AM_INHERIT_FROM(interpro_ioga_device::map)

	AM_RANGE(0x00, 0x03) AM_READWRITE(eth_remap_r, eth_remap_w)
	AM_RANGE(0x04, 0x07) AM_READWRITE(eth_mappg_r, eth_mappg_w)
	AM_RANGE(0x08, 0x0b) AM_READWRITE(eth_control_r, eth_control_w)
ADDRESS_MAP_END

static INPUT_PORTS_START(interpro_mouse)
	PORT_START("mouse_buttons")
	PORT_BIT(0x01, IP_ACTIVE_HIGH, IPT_BUTTON1) PORT_NAME("Mouse Left Button") PORT_CODE(MOUSECODE_BUTTON1) PORT_CHANGED_MEMBER(DEVICE_SELF, interpro_ioga_device, mouse_button, nullptr)
	PORT_BIT(0x02, IP_ACTIVE_HIGH, IPT_BUTTON2) PORT_NAME("Mouse Middle Button") PORT_CODE(MOUSECODE_BUTTON3) PORT_CHANGED_MEMBER(DEVICE_SELF, interpro_ioga_device, mouse_button, nullptr)
	PORT_BIT(0x04, IP_ACTIVE_HIGH, IPT_BUTTON3) PORT_NAME("Mouse Right Button") PORT_CODE(MOUSECODE_BUTTON2) PORT_CHANGED_MEMBER(DEVICE_SELF, interpro_ioga_device, mouse_button, nullptr)

	PORT_START("mouse_x")
	PORT_BIT(0xff, 0x00, IPT_MOUSE_X) PORT_SENSITIVITY(50) PORT_KEYDELTA(0) PORT_PLAYER(1) PORT_CHANGED_MEMBER(DEVICE_SELF, interpro_ioga_device, mouse_x, nullptr)
	PORT_START("mouse_y")
	PORT_BIT(0xff, 0x00, IPT_MOUSE_Y) PORT_SENSITIVITY(50) PORT_KEYDELTA(0) PORT_PLAYER(1) PORT_CHANGED_MEMBER(DEVICE_SELF, interpro_ioga_device, mouse_y, nullptr)
INPUT_PORTS_END

DEFINE_DEVICE_TYPE(TURQUOISE_IOGA, turquoise_ioga_device, "ioga_t", "I/O Gate Array (Turquoise)")
DEFINE_DEVICE_TYPE(SAPPHIRE_IOGA, sapphire_ioga_device, "ioga_s", "I/O Gate Array (Sapphire)")

interpro_ioga_device::interpro_ioga_device(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, uint32_t clock)
	: device_t(mconfig, type, tag, owner, clock)
	, m_memory_tag(nullptr)
	, m_memory_space(nullptr)
	, m_out_nmi_func(*this)
	, m_out_irq_func(*this)
	, m_out_ivec_func(*this)
	, m_fdc_tc_func(*this)
	, m_eth_ca_func(*this)
	, m_dma_channel{
		{ 0,0,0,0,CLEAR_LINE,IDLE, {*this}, {*this}, ARBCTL_BGR_PLOT, DMA_PLOTTER, "plotter" },
		{ 0,0,0,0,CLEAR_LINE,IDLE, {*this}, {*this}, ARBCTL_BGR_SCSI, DMA_SCSI, "scsi" },
		{ 0,0,0,0,CLEAR_LINE,IDLE, {*this}, {*this}, ARBCTL_BGR_FDC, DMA_FLOPPY, "floppy" } }
	, m_serial_dma_channel{
		{ 0,0,CLEAR_LINE, {*this}, {*this}, ARBCTL_BGR_SER0, 0, "serial0" },
		{ 0,0,CLEAR_LINE, {*this}, {*this}, ARBCTL_BGR_SER1, 1, "serial1" },
		{ 0,0,CLEAR_LINE, {*this}, {*this}, ARBCTL_BGR_SER2, 2, "serial2" } }
{
}

turquoise_ioga_device::turquoise_ioga_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: interpro_ioga_device(mconfig, TURQUOISE_IOGA, tag, owner, clock)
{
}

sapphire_ioga_device::sapphire_ioga_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: interpro_ioga_device(mconfig, SAPPHIRE_IOGA, tag, owner, clock)
{
}

void interpro_ioga_device::static_set_memory(device_t &device, const char *const tag, const int spacenum)
{
	interpro_ioga_device &ioga = dynamic_cast<interpro_ioga_device &>(device);

	ioga.m_memory_tag = tag;
	ioga.m_memory_spacenum = spacenum;
}

void interpro_ioga_device::device_start()
{
	assert_always(m_memory_tag != nullptr, "memory tag and address space number must be configured");

	// get the memory space
	device_memory_interface *memory;
	siblingdevice(m_memory_tag)->interface(memory);
	m_memory_space = &memory->space(m_memory_spacenum);

	// resolve callbacks
	m_out_nmi_func.resolve();
	m_out_irq_func.resolve();
	m_out_ivec_func.resolve();
	m_fdc_tc_func.resolve();
	m_eth_ca_func.resolve();

	for (dma_channel_t &dma_channel : m_dma_channel)
	{
		dma_channel.device_r.resolve();
		dma_channel.device_w.resolve();
	}

	for (serial_dma_channel_t &dma_channel : m_serial_dma_channel)
	{
		dma_channel.device_r.resolve();
		dma_channel.device_w.resolve();
	}

	// allocate timers
	m_interrupt_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(interpro_ioga_device::interrupt_check), this));
	m_dma_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(interpro_ioga_device::dma), this));
	m_serial_dma_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(interpro_ioga_device::serial_dma), this));

	m_timer0 = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(interpro_ioga_device::timer0), this));
	m_timer1 = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(interpro_ioga_device::timer1), this));
	m_timer2 = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(interpro_ioga_device::timer2), this));
	m_timer3 = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(interpro_ioga_device::timer3), this));

	m_mouse_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(interpro_ioga_device::mouse_timer), this));
	m_eth_reset_timer = machine().scheduler().timer_alloc(timer_expired_delegate(FUNC(interpro_ioga_device::eth_reset), this));
}

void interpro_ioga_device::device_reset()
{
	// initialise interrupt state
	m_active_interrupt = nullptr;
	m_nmi_state = CLEAR_LINE;
	m_irq_state = CLEAR_LINE;
	m_ivec = 0;
	m_hwint_forced = 0;

	// initialise dma state
	for (dma_channel_t &dma_channel : m_dma_channel)
	{
		dma_channel.drq_state = CLEAR_LINE;
		dma_channel.state = IDLE;
	}

	for (serial_dma_channel_t &dma_channel : m_serial_dma_channel)
		dma_channel.drq_state = CLEAR_LINE;

	// disable timers
	m_interrupt_timer->enable(false);
	m_dma_timer->enable(false);
	m_serial_dma_timer->enable(false);

	m_timer0->enable(false);
	m_timer1->enable(false);
	m_timer2->enable(false);
	m_timer3->enable(false);

	m_mouse_timer->enable(false);
	m_eth_reset_timer->enable(false);

	// configure timer 0 for 60Hz
	m_timer0_count = 0;
	m_timer0_limit = 0;
	m_timer0->adjust(attotime::zero, 0, attotime::from_hz(60));

	m_timer1_count = 0;
	m_timer2_count = 0;
	m_timer2_value = 0;
	m_timer3_count = 0;

	m_prescaler = 0;
}

ioport_constructor interpro_ioga_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(interpro_mouse);
}

void interpro_ioga_device::set_nmi_line(int state)
{
	LOGMASKED(LOG_NMI, "nmi: %s (%s)\n", state ? "asserted" : "cleared", machine().describe_context());

	if (state == ASSERT_LINE)
	{
		// check if nmi is enabled
		if (((m_nmictrl & NMI_IE) == NMI_IE) || ((m_nmictrl & (NMI_ALL | NMI_ENABLE1)) == (NMI_ALL | NMI_ENABLE1)))
		{
			LOGMASKED(LOG_NMI, "nmi: asserting output nmi line\n");

			// if level triggered, disable input from pin
			if ((m_nmictrl & NMI_EDGE) == 0)
				m_nmictrl &= ~NMI_ENABLE2;

			nmi(ASSERT_LINE);
		}
	}
	else
		nmi(CLEAR_LINE);
}

void interpro_ioga_device::set_int_line(interrupt_type type, int number, int state)
{
	LOGIRQ(number, "irq: set_int_line(%d, %d, %d)\n", type, number, state);
#if 0
	// preliminary edge/level triggered interrupt handling
	if (type == INT_HARD_IN || type == INT_HARD_EX)
	{
		/*
		 *  existing   new    type   result
		 *   clear    clear   edge    nothing
		 *   clear    assert  edge    pending if not masked
		 *   assert   clear   edge    nothing
		 *   assert   assert  edge    nothing

		 *   clear    clear   level   nothing
		 *   clear    assert  level   pending
		 *   assert   clear   level   !pending
		 *   assert   assert  level   nothing
		 */

		u16 icr = get_icr(type, number);
		u32 mask = 1 << number;

		if (icr & IRQ_EDGE)
		{
			if ((m_line_state & mask) == 0 && state)
				set_pending(type, number, true);
		}
		else
		{
			if (((m_line_state & mask) == 0) ^ (state != ASSERT_LINE))
				set_pending(type, number, state == ASSERT_LINE);
		}

		m_line_state = (state == ASSERT_LINE) ? (m_line_state | mask) : (m_line_state & ~mask);
	}
	else
#else
	(void)m_line_state;
#endif
	set_pending(type, number, state == ASSERT_LINE);

	m_interrupt_timer->adjust(attotime::zero);
}

/*
 * Interrupts
 */
IRQ_CALLBACK_MEMBER(interpro_ioga_device::acknowledge_interrupt)
{
	switch (irqline)
	{
	case INPUT_LINE_IRQ0:
		// clear pending flag
		set_pending(m_active_interrupt->type, m_active_interrupt->number, false);

		// clear irq/ivec
		irq(CLEAR_LINE, 0);

		m_active_interrupt = nullptr;
		break;

	case INPUT_LINE_NMI:
		nmi(CLEAR_LINE);
		break;
	}

	m_interrupt_timer->adjust(attotime::zero);

	return 0;
}

TIMER_CALLBACK_MEMBER(interpro_ioga_device::interrupt_check)
{
	// find highest priority unmasked pending interrupt
	u16 ivec = m_ivec;
	for (const interrupt_data_t &interrupt : m_interrupt_data)
	{
		u16 icr = get_icr(interrupt.type, interrupt.number);

		// test for pending and not masked
		if ((icr & interrupt.mask) == interrupt.mask)
		{
			// check priority
			if (m_active_interrupt == nullptr || (icr & IRQ_VECTOR) < ivec)
			{
				m_active_interrupt = &interrupt;
				ivec = icr & IRQ_VECTOR;
			}
		}
	}

	// assert irq and ivec
	if (m_active_interrupt != nullptr)
		irq(ASSERT_LINE, ivec);
}

void interpro_ioga_device::set_pending(interrupt_type type, int number, bool pending)
{
	switch (type)
	{
	case INT_HARD_IN:
	case INT_HARD_EX:
		if (pending)
			m_hwicr[number] |= IRQ_PENDING;
		else
			m_hwicr[number] &= ~IRQ_PENDING;
		break;

	case INT_SOFT_LO:
		if (pending)
			m_softint |= (1 << number);
		else
			m_softint &= ~(1 << number);
		break;

	case INT_SOFT_HI:
		if (pending)
			m_swicr[number] |= IRQ_PENDING;
		else
			m_swicr[number] &= ~IRQ_PENDING;
		break;
	}
}

bool interpro_ioga_device::nmi(int state)
{
	if (m_nmi_state != state)
	{
		m_nmi_state = state;
		m_out_nmi_func(state);

		return true;
	}
	else
		return false;
}

bool interpro_ioga_device::irq(int state, u8 ivec)
{
	if (ivec != m_ivec)
	{
		LOGIRQ(m_active_interrupt->number, "setting ivec 0x%02x\n", ivec);

		m_ivec = ivec;
		machine().scheduler().synchronize(timer_expired_delegate(FUNC(interpro_ioga_device::set_ivec), this));
	}

	if (m_irq_state != state)
	{
		LOGIRQ(m_active_interrupt->number, "%s %s number %d source %s (%s)\n",
			state ? "asserting" : "clearing", m_active_interrupt->name, m_active_interrupt->number, m_active_interrupt->source, machine().describe_context());

		m_irq_state = state;
		m_out_irq_func(state);

		return true;
	}
	else
		return false;
}

TIMER_CALLBACK_MEMBER(interpro_ioga_device::set_ivec)
{
	m_out_ivec_func(m_ivec);
}

u16 interpro_ioga_device::get_icr(interrupt_type type, int number) const
{
	switch (type)
	{
	case INT_HARD_IN:
	case INT_HARD_EX:
		return m_hwicr[number];

	case INT_SOFT_LO:
		return (m_softint << 8) | (0x8f + number * 0x10);

	case INT_SOFT_HI:
		return m_swicr[number];
	}

	return 0;
}

WRITE16_MEMBER(interpro_ioga_device::icr_w)
{
	/*
	 * It appears that writing the pending flag high and then low again is
	 * intended to force an interrupt to be generated. We record the initial
	 * write in m_hwint_forced, and when a subsequent write occurrs, turn the
	 * pending bit on to trigger the interrupt.
	 *
	 * FIXME: should we only flag a forced interrupt if pending is written high
	 * from low? Maybe all of this can be eliminated by the edge/level trigger
	 * logic in the future.
	 */

	LOGIRQ(offset, "irq: interrupt vector %d = 0x%04x (%s)\n", offset, data, machine().describe_context());

	if (data & IRQ_PENDING)
	{
		// record interrupt pending forced
		m_hwint_forced |= 1 << offset;

		// store all bits except pending
		m_hwicr[offset] = (m_hwicr[offset] & IRQ_PENDING) | (data & ~IRQ_PENDING);

		// FIXME: is it possible trigger a pending interrupt by unmasking it here?
	}
	else if (m_hwint_forced & (1 << offset))
	{
		// interrupt is being forced
		m_hwicr[offset] = data;

		// clear forced flag
		m_hwint_forced &= ~(1 << offset);

		// FIXME: in/ex
		LOGIRQ(offset, "irq: forcing interrupt vector %d\n", offset);
		set_int_line(INT_HARD_IN, offset, ASSERT_LINE);
	}
	else
		// otherwise just store the value
		m_hwicr[offset] = data;
}

WRITE8_MEMBER(interpro_ioga_device::softint_w)
{
	// check for forced interrupts
	const u8 forced = m_softint & ~data;

	// store the written value
	m_softint = data;

	// force interrupts if needed
	if (forced != 0)
	{
		for (int i = 0; i < 8; i++)
			if (forced & (1 << i))
				set_int_line(INT_SOFT_LO, i, ASSERT_LINE);
	}
}

WRITE8_MEMBER(interpro_ioga_device::nmictrl_w)
{
	// check for a forced nmi (NMI_NEGPOL written from 1 to 0 with NMI_IE set)
	const bool forced = (m_nmictrl & NMI_NEGPOL) && ((data & (NMI_NEGPOL | NMI_IE)) == NMI_IE);

	LOGMASKED(LOG_NMI, "nmi: nmictrl = 0x%02x (%s)\n", data, machine().describe_context());
	m_nmictrl = data;

	if (forced)
	{
		LOGMASKED(LOG_NMI, "nmi: forcing nmi\n");

		set_nmi_line(ASSERT_LINE);
	}
}

WRITE16_MEMBER(interpro_ioga_device::softint_vector_w)
{
	// check for forced interrupt
	const bool forced = (m_swicr[offset] & IRQ_PENDING) && !(data & IRQ_PENDING);

	// store the written value
	m_swicr[offset] = data;

	// force interrupt if needed
	if (forced)
		set_int_line(INT_SOFT_HI, offset, ASSERT_LINE);
}

/*
 * DMA
 */
TIMER_CALLBACK_MEMBER(interpro_ioga_device::dma)
{
	for (dma_channel_t &dma_channel : m_dma_channel)
	{
		// check if there's something to transfer
		if (dma_channel.transfer_count == 0 || (dma_channel.control & DMA_CTRL_BGR) == 0)
			continue;

		// check if the device is requesting a transfer
		if (dma_channel.drq_state == CLEAR_LINE)
			continue;

		// check if the bus is available
		if ((m_arbctl & dma_channel.arb_mask) == 0)
		{
			// set bus wait flag and abort
			dma_channel.control |= DMA_CTRL_WAIT;

			continue;
		}
		else
			// clear bus wait flag
			dma_channel.control &= ~DMA_CTRL_WAIT;

		//LOGMASKED(LOG_DMA, "dma: transfer %s device begun, channel = %d, control 0x%08x, real address 0x%08x, virtual address 0x%08x, count 0x%08x\n",
		//  (dma_channel.control & DMA_CTRL_WRITE) ? "to" : "from", dma_channel.channel, dma_channel.control, dma_channel.real_address, dma_channel.virtual_address, dma_channel.transfer_count);

		// transfer from the memory to device or device to memory
		while (dma_channel.transfer_count && dma_channel.drq_state)
		{
			// transfer from the memory to device or device to memory
			// TODO: implement virtual addressing when DMA_CTRL_VIRTUAL is set

			if (dma_channel.control & DMA_CTRL_WRITE)
				dma_channel.device_w(m_memory_space->read_byte(dma_channel.real_address));
			else
				m_memory_space->write_byte(dma_channel.real_address, dma_channel.device_r());

			// increment addresses and decrement count
			dma_channel.real_address++;
			dma_channel.virtual_address++;
			dma_channel.transfer_count--;
		}

		// check if the transfer is complete
		if (dma_channel.transfer_count == 0)
		{
			LOGMASKED(LOG_DMA, "dma: transfer %s device ended, channel = %d, control 0x%08x, real address 0x%08x, virtual address 0x%08x, count 0x%08x\n",
				(dma_channel.control & DMA_CTRL_WRITE) ? "to" : "from", dma_channel.channel, dma_channel.control, dma_channel.real_address, dma_channel.virtual_address, dma_channel.transfer_count);

			if (dma_channel.channel == DMA_FLOPPY)
			{
				LOGMASKED(LOG_DMA | LOG_FLOPPY, "dma: asserting fdc terminal count line\n");

				m_fdc_tc_func(ASSERT_LINE);
				m_fdc_tc_func(CLEAR_LINE);
			}

			// clear bus grant required
			dma_channel.control &= ~DMA_CTRL_BGR;

			// set transfer count zero flag
			dma_channel.control |= DMA_CTRL_TCZERO;
		}

#if IOGA_DMA_DIAG_HACK
#define TAG ((dma_channel.control & DMA_CTRL_TAG) >> 3)

				// hacks for forced dma bus error diagnostic tests
				if ((dma_channel.control & 0xfe000000 && dma_channel.control & 0xe00) || ((dma_channel.control & DMA_CTRL_WMASK) == 0x41000000))
				if (dma_channel.real_address & 0xff000000 || dma_channel.real_address == 0)
				{
					LOGDMA(dma_channel.channel, "dma: forced bus error hack, control 0x%08x\n", dma_channel.control);

					// (7.0267) trigger an interrupt
					m_hwicr[dma_channel.channel + 1] |= IRQ_PENDING;

					// (7.0268) set bus error bit
					dma_channel.control |= DMA_CTRL_BERR;

					// 7.0269, 7.0276, 7.0281, 7.0289: set error address from virtual or real dma address
					// HACK: don't set error address for 7.0276 special case
					if (!(dma_channel.control == 0x65400600 && dma_channel.real_address != 0))
						m_error_address = dma_channel.control & DMA_CTRL_VIRTUAL ? dma_channel.virtual_address : dma_channel.real_address;

					// compute bus error cycle type from control register
					u8 cycle_type = 0x30;
					switch ((dma_channel.control >> 24) & 0x8c)
					{
					case 0x00: cycle_type |= 2; break;
					case 0x04: cycle_type |= 1; break;
					case 0x08: cycle_type |= 3; break;
					case 0x80: cycle_type |= 4; break;
					case 0x84: cycle_type |= 8; break;
					}

					switch (dma_channel.control & ~DMA_CTRL_BERR)
					{
					case 0x61000800: // VIRTUAL | WRITE | TAG(3)
						// (7.0266) trigger an nmi
						m_nmi_pending = true;

						// (7.0270) set error cycle type 0x52f0: SNAPOK | BERR | BG(IOD) | TAG(0c0) | CT(30)
						m_error_businfo = BINFO_SNAPOK | BINFO_BERR | BINFO_BG_IOD | 0xf0;
						break;

					case 0x65000600: // VIRTUAL | WRITE | X | TAG(4)
						if (dma_channel.real_address != 0)
						{
							// (7.0275) control register expect 0x64400800
							dma_channel.control &= ~0x600;
							dma_channel.control |= 0x800;

							// (7.0277) set error cycle type 0x5331: SNAPOK | BERR | BG(IOD) | TAG(100) | CT(31)
							m_error_businfo = BINFO_SNAPOK | BINFO_BERR | BINFO_BG_IOD | TAG | cycle_type;
						}
						else
						{
							// (7.0287) set error cycle type 0x62f0: SNAPOK | MMBE | BG(IOD) | TAG(0c0) | CT(30)
							m_error_businfo = BINFO_SNAPOK | BINFO_MMBE | BINFO_BG_IOD | TAG | 0x30;
						}
						break;

					default:
						m_error_businfo = BINFO_SNAPOK | BINFO_BERR | BINFO_BG_IOD | TAG | cycle_type;
						break;
					}

					dma_channel.state = COMPLETE;
				}
#endif
	}
}

void interpro_ioga_device::drq(int state, int channel)
{
	dma_channel_t &dma_channel = m_dma_channel[channel];

	dma_channel.drq_state = state;

	// log every 256 bytes
	if ((dma_channel.transfer_count & 0xff) == 0)
	{
		LOGMASKED(LOG_DMA, "dma: drq for channel %d %s transfer_count 0x%08x\n", channel, state ? "asserted" : "deasserted", dma_channel.transfer_count);
	}

	if (state)
		m_dma_timer->adjust(attotime::zero);
}

u32 interpro_ioga_device::dma_r(address_space &space, offs_t offset, u32 mem_mask, dma_channel channel) const
{
	const dma_channel_t &dma_channel = m_dma_channel[channel];

	switch (offset)
	{
	case 0:
		return dma_channel.real_address;

	case 1:
		return dma_channel.virtual_address;

	case 2:
		return dma_channel.transfer_count;

	case 3:
		return dma_channel.control;

	default:
		logerror("dma_r: unknown dma register %d\n", offset);
		return 0;
	}
}

void interpro_ioga_device::dma_w(address_space &space, offs_t offset, u32 data, u32 mem_mask, dma_channel channel)
{
	dma_channel_t &dma_channel = m_dma_channel[channel];

	switch (offset)
	{
	case 0:
		LOGMASKED(LOG_DMA, "dma: channel %d real address = 0x%08x (%s)\n", channel, data, machine().describe_context());
		dma_channel.real_address = data;
		break;

	case 1:
		LOGMASKED(LOG_DMA, "dma: channel %d virtual address = 0x%08x (%s)\n", channel, data, machine().describe_context());
		dma_channel.virtual_address = data & ~0x3;
		break;

	case 2:
		LOGMASKED(LOG_DMA, "dma: channel %d transfer count = 0x%08x (%s)\n", channel, data, machine().describe_context());
		dma_channel.transfer_count = data;
		break;

	case 3:
		LOGMASKED(LOG_DMA, "dma: channel %d control = 0x%08x (%s)\n", channel, data, machine().describe_context());

		// (7.0272) if bus error flag is set, clear existing bus error (otherwise retain existing state)
		if (data & DMA_CTRL_BERR)
			dma_channel.control = data & DMA_CTRL_WMASK;
		else
			dma_channel.control = (data & DMA_CTRL_WMASK) | (dma_channel.control & DMA_CTRL_BERR);

		dma_channel.state = COMMAND;
		break;
	}
}

TIMER_CALLBACK_MEMBER(interpro_ioga_device::serial_dma)
{
	LOGMASKED(LOG_SERIALDMA, "dma: serial_dma()\n");

	for (serial_dma_channel_t &dma_channel : m_serial_dma_channel)
	{
		// check if there's something to transfer
		if ((dma_channel.control & SDMA_COUNT) == 0)
			continue;

		// check if the device is requesting a transfer
		if (dma_channel.drq_state == CLEAR_LINE)
			continue;

		// check if the bus is available
		if ((m_arbctl & dma_channel.arb_mask) == 0)
			continue;

		// transfer from the memory to device or device to memory
		while ((dma_channel.control & SDMA_COUNT) && dma_channel.drq_state)
		{
			// TODO: work out which control register bits indicate read from device
			if (dma_channel.control & SDMA_SEND)
			{
				u8 data = m_memory_space->read_byte(dma_channel.address);

				LOGMASKED(LOG_SERIALDMA, "dma: transmitting byte 0x%02x to serial channel %d\n", data, dma_channel.channel);

				dma_channel.device_w(data);
			}
			else
			{
				u8 data = dma_channel.device_r();

				LOGMASKED(LOG_SERIALDMA, "dma: receiving byte 0x%02x from serial channel %d\n", data, dma_channel.channel);

				m_memory_space->write_byte(dma_channel.address, data);
			}

			// increment address and decrement count
			dma_channel.address++;
			dma_channel.control = (dma_channel.control & SDMA_CONTROL) | ((dma_channel.control & SDMA_COUNT) - 1);
		}

		if ((dma_channel.control & SDMA_COUNT) == 0)
		{
			// transfer count zero
			dma_channel.control |= SDMA_TCZERO;

			// raise an interrupt
			set_int_line(INT_HARD_IN, IRQ_SERDMA, ASSERT_LINE);
		}
	}
}

void interpro_ioga_device::serial_drq(int state, int channel)
{
	serial_dma_channel_t &dma_channel = m_serial_dma_channel[channel];

	dma_channel.drq_state = state;

	LOGMASKED(LOG_SERIALDMA, "dma: drq for serial channel %d %s count 0x%04x\n", channel, state ? "asserted" : "deasserted", dma_channel.control & SDMA_COUNT);

	if (state && (dma_channel.control & SDMA_COUNT))
		m_serial_dma_timer->adjust(attotime::zero);
}

void interpro_ioga_device::serial_dma_addr_w(address_space &space, offs_t offset, u32 data, u32 mem_mask, int channel)
{
	LOGMASKED(LOG_SERIALDMA, "dma: serial channel %d address 0x%08x mask 0x%08x (%s)\n", channel, data, mem_mask, machine().describe_context());

	COMBINE_DATA(&m_serial_dma_channel[channel].address);
}

void interpro_ioga_device::serial_dma_ctrl_w(address_space &space, offs_t offset, u32 data, u32 mem_mask, int channel)
{
	serial_dma_channel_t &dma_channel = m_serial_dma_channel[channel];

	LOGMASKED(LOG_SERIALDMA, "dma: serial channel %d control 0x%08x mask 0x%08x (%s)\n", channel, data, mem_mask, machine().describe_context());

	COMBINE_DATA(&dma_channel.control);

	if (dma_channel.control & SDMA_COUNT)
		m_serial_dma_timer->adjust(attotime::zero);
}

/*
 * Bus arbitration and control
 */
WRITE16_MEMBER(interpro_ioga_device::arbctl_w)
{
	LOGMASKED(LOG_DMA | LOG_SERIALDMA, "dma: arbctl = 0x%04x (%s)\n", data, machine().describe_context());

	m_arbctl = data;

	// trigger serial dma waiting for bus access
	if (m_arbctl & (ARBCTL_BGR_SER0 | ARBCTL_BGR_SER1 | ARBCTL_BGR_SER2))
		m_serial_dma_timer->adjust(attotime::zero);
}

READ32_MEMBER(interpro_ioga_device::error_businfo_r)
{
	const u32 result = m_error_businfo;

	// clear register after reading
	if (!machine().side_effect_disabled())
		m_error_businfo = 0;

	return result;
}

/*
 * Timers
 */
READ32_MEMBER(interpro_ioga_device::timer0_r) const
{
	LOGMASKED(LOG_TIMERRD, "timer0_r data 0x%08x mask 0x%08x (%s)\n", m_timer0_count, mem_mask, machine().describe_context());

	return m_timer0_count;
}

WRITE32_MEMBER(interpro_ioga_device::timer0_w)
{
	//m_timer0_limit = data;
	m_timer0_count = data;

	m_timer0->adjust(attotime::zero, 0, attotime::from_hz(60));

	LOGMASKED(LOG_TIMER0, "timer0_w data 0x%08x mask 0x%08x (%s)\n", data, mem_mask, machine().describe_context());
}

TIMER_CALLBACK_MEMBER(interpro_ioga_device::timer0)
{
	if (m_timer0_count == 0)
		set_int_line(INT_HARD_IN, IRQ_TIMER0, ASSERT_LINE);
	else
		m_timer0_count++;

	//m_hwicr[IRQ_VB] |= IRQ_PENDING; // this makes mouse diag countdown work (at half correct speed)
	set_int_line(INT_HARD_EX, IRQ_12, ASSERT_LINE); // FIXME: 60Hz timer on InterPro 2000
}

READ32_MEMBER(interpro_ioga_device::timer1_r) const
{
	u32 result = m_timer1_count & TIMER1_COUNT;

	// set the start bit if the timer is currently enabled
	if (m_timer1->enabled())
		result |= TIMER1_START;
	else if (m_timer1->param())
		result |= TIMER1_EXPIRED;

	LOGMASKED(LOG_TIMERRD, "timer1_r data 0x%08x mask 0x%08x (%s)\n", result, mem_mask, machine().describe_context());

	return result;
}

WRITE32_MEMBER(interpro_ioga_device::timer1_w)
{
	// disable the timer
	m_timer1->enable(false);

	// store the timer count value
	m_timer1_count = data & TIMER1_COUNT;

	// start the timer if necessary
	if (data & TIMER1_START)
	{
		LOGMASKED(LOG_TIMER1, "timer1_w data 0x%08x mask 0x%08x prescaler 0x%04x (%s)\n", data, mem_mask, m_prescaler & 0x7fff, machine().describe_context());

		// theory: timer 1 is 10MHz (typical prescaler value of 1000 and timer value of 100 giving a delay of 100ms)
		// FIXME: division by 50 is sufficient to pass iogadiag timer 1 tests
		m_timer1->adjust(attotime::zero, false, attotime::from_usec((m_prescaler & 0x7fff) / 50));
	}
}

TIMER_CALLBACK_MEMBER(interpro_ioga_device::timer1)
{
	// decrement timer count value
	m_timer1_count--;

	// check if timer has expired
	if (m_timer1_count == 0)
	{
		LOGMASKED(LOG_TIMER1, "timer1 expired\n");

		// disable timer and set the zero flag
		m_timer1->enable(false);
		m_timer1->set_param(true);

		// throw an interrupt
		set_int_line(INT_HARD_IN, IRQ_TIMER1, ASSERT_LINE);
	}
}

READ32_MEMBER(interpro_ioga_device::timer2_count_r) const
{
	LOGMASKED(LOG_TIMERRD, "timer2_count_r data 0x%08x mask 0x%08x (%s)\n", m_timer2_count, mem_mask, machine().describe_context());

	return m_timer2_count;
}

WRITE32_MEMBER(interpro_ioga_device::timer2_count_w)
{
	m_timer2_count = data;

	LOGMASKED(LOG_TIMER2, "timer2_count_w data 0x%08x mask 0x%08x (%s)\n", data, mem_mask, machine().describe_context());
}

READ32_MEMBER(interpro_ioga_device::timer2_value_r) const
{
	LOGMASKED(LOG_TIMERRD, "timer2_value_r data 0x%08x mask 0x%08x (%s)\n", m_timer2_value, mem_mask, machine().describe_context());

	return m_timer2_value;
}

WRITE32_MEMBER(interpro_ioga_device::timer2_value_w)
{
	m_timer2_value = data;
	m_timer2_count = data; // ?

	LOGMASKED(LOG_TIMER2, "timer2_value_w data 0x%08x mask 0x%08x (%s)\n", data, mem_mask, machine().describe_context());
}

READ32_MEMBER(interpro_ioga_device::timer3_r) const
{
	u32 result = m_timer3_count & TIMER3_COUNT;

	if (m_timer3->enabled())
		result |= TIMER3_START;
	else if (m_timer3->param())
		result |= TIMER3_EXPIRED;

	LOGMASKED(LOG_TIMERRD, "timer3_r data 0x%08x mask 0x%08x (%s)\n", result, mem_mask, machine().describe_context());

	return result;
}

WRITE32_MEMBER(interpro_ioga_device::timer3_w)
{
	// stop the timer so it won't trigger while we're fiddling with it
	m_timer3->enable(false);

	// write the new value to the timer register
	m_timer3_count = data & TIMER3_COUNT;

	// start the timer if necessary
	if (data & TIMER3_START)
	{
		LOGMASKED(LOG_TIMER3, "timer3_w data 0x%08x mask 0x%08x (%s)\n", data, mem_mask, machine().describe_context());

		// theory: timer 3 is 12.5MHz (typical value of 12500 giving a delay of 1ms)
		m_timer3->adjust(attotime::zero, false, attotime::from_hz(XTAL(12'500'000)));
	}
}

TIMER_CALLBACK_MEMBER(interpro_ioga_device::timer3)
{
	// decrement timer count value
	m_timer3_count--;

	// check for expiry
	if (m_timer3_count == 0)
	{
		LOGMASKED(LOG_TIMER3, "timer3 expired\n");

		// disable timer and set the zero flag
		m_timer3->enable(false);
		m_timer3->set_param(true);

		// throw an interrupt
		set_int_line(INT_HARD_IN, IRQ_TIMER3, ASSERT_LINE);
	}
}

READ32_MEMBER(interpro_ioga_device::prescaler_r) const
{
	// FIXME: prescaler only used with timer 1?
	LOGMASKED(LOG_TIMERRD, "prescaler_r data 0x%08x mask 0x%08x (%s)\n", m_prescaler, mem_mask, machine().describe_context());

	return m_prescaler;
}

WRITE32_MEMBER(interpro_ioga_device::prescaler_w)
{
	LOGMASKED(LOG_TIMER1, "prescaler_w data 0x%08x mask 0x%08x (%s)\n", data, mem_mask, machine().describe_context());

	// this logic satisfies prescaler tests, but fails timer prescaler tests
	if ((data & 0x7fff) < 0x100 && (data & 0x7fff) != 0)
		m_prescaler = (data ^ 0xffff0000);
	else
		m_prescaler = (data ^ 0xffff0000) - 0x10000;
}

/*
 * Mouse
 */
READ32_MEMBER(interpro_ioga_device::mouse_status_r)
{
	const u32 result = m_mouse_status;

	LOGMASKED(LOG_MOUSE, "mouse_status_r status 0x%08x mask 0x%08x (%s)\n", result, mem_mask, machine().describe_context());

	// clear xpos and ypos fields
	if (!machine().side_effect_disabled())
		m_mouse_status &= ~(MOUSE_XPOS | MOUSE_YPOS);

	return result;
}

WRITE32_MEMBER(interpro_ioga_device::mouse_status_w)
{
	LOGMASKED(LOG_MOUSE, "mouse_status_w status 0x%08x mask 0x%08x (%s)\n", data, mem_mask, machine().describe_context());

	m_mouse_status |= (data & MOUSE_COUNTER);

	// FIXME: diagnostic test fails with value >= 0x1000
	if (data & MOUSE_COUNTER)
		m_mouse_timer->adjust(attotime::from_nsec(data & MOUSE_COUNTER));
}

INPUT_CHANGED_MEMBER(interpro_ioga_device::mouse_button)
{
	if (newval)
		m_mouse_status |= (field.mask() << 16);
	else
		m_mouse_status &= ~(field.mask() << 16);

	set_int_line(INT_HARD_IN, IRQ_MOUSE, ASSERT_LINE);
}

INPUT_CHANGED_MEMBER(interpro_ioga_device::mouse_x)
{
	// compute x delta
	int delta = newval - oldval;
	if (delta > 0x80)
		delta -= 0x100;
	else if (delta < -0x80)
		delta += 0x100;

	// set new x delta
	m_mouse_status &= ~MOUSE_XPOS;
	m_mouse_status |= ((delta << 8) & MOUSE_XPOS);

	set_int_line(INT_HARD_IN, IRQ_MOUSE, ASSERT_LINE);
}

INPUT_CHANGED_MEMBER(interpro_ioga_device::mouse_y)
{
	// compute y delta
	int delta = newval - oldval;
	if (delta > 0x80)
		delta -= 0x100;
	else if (delta < -0x80)
		delta += 0x100;

	// set new x delta
	m_mouse_status &= ~MOUSE_YPOS;
	m_mouse_status |= ((delta << 0) & MOUSE_YPOS);

	set_int_line(INT_HARD_IN, IRQ_MOUSE, ASSERT_LINE);
}

TIMER_CALLBACK_MEMBER(interpro_ioga_device::mouse_timer)
{
	set_int_line(INT_HARD_IN, IRQ_MOUSE, ASSERT_LINE);
}

/*
 * Ethernet
 */
WRITE32_MEMBER(turquoise_ioga_device::eth_base_w)
{
	LOGMASKED(LOG_NETWORK, "eth: base_w 0x%08x mem_mask 0x%08x (%s)\n", data, mem_mask, machine().describe_context());

	m_eth_base = ((m_eth_base & ~mem_mask) | (data & mem_mask)) & ETH_BASE_MASK;
}

WRITE16_MEMBER(turquoise_ioga_device::eth_control_w)
{
	LOGMASKED(LOG_NETWORK, "eth: control_w 0x%04x mem_mask 0x%04x (%s)\n", data, mem_mask, machine().describe_context());

	m_eth_control = data;

	// ethernet device reset (active low)
	if ((data & ETH_RESET) == 0)
	{
		// reset the ethernet device
		siblingdevice("eth")->reset();

		// clear the reset flag (the 250ns delay is long enough to pass diagnostic tests)
		m_eth_reset_timer->adjust(attotime::from_nsec(250));
	}

	// ethernet channel attention
	if (data & ETH_CA)
	{
		m_eth_ca_func(ASSERT_LINE);
		m_eth_ca_func(CLEAR_LINE);
	}
}

WRITE16_MEMBER(turquoise_ioga_device::eth_w)
{
	const u32 address = m_eth_base | ((offset << 1) & ~ETH_BASE_MASK);

	LOGMASKED(LOG_NETWORK, "eth_w address 0x%08x mask 0x%04x data 0x%04x\n", address, mem_mask, data);
	m_memory_space->write_word(address, data, mem_mask);
}

READ16_MEMBER(turquoise_ioga_device::eth_r)
{
	const u32 address = m_eth_base | ((offset << 1) & ~ETH_BASE_MASK);

	const u16 data = m_memory_space->read_word(address, mem_mask);
	LOGMASKED(LOG_NETWORK, "eth_r 0x%08x mask 0x%04x data 0x%04x\n", address, mem_mask, data);

	return data;
}

TIMER_CALLBACK_MEMBER(turquoise_ioga_device::eth_reset)
{
	LOGMASKED(LOG_NETWORK, "eth: reset flag cleared\n");

	// clear ethernet reset flag
	m_eth_control &= ~ETH_RESET;
}

WRITE32_MEMBER(sapphire_ioga_device::eth_remap_w)
{
	LOGMASKED(LOG_NETWORK, "eth: remap = 0x%08x (%s)\n", data, machine().describe_context());

	m_eth_remap = data & ~0xf;
}

WRITE32_MEMBER(sapphire_ioga_device::eth_mappg_w)
{
	LOGMASKED(LOG_NETWORK, "eth: map page = 0x%08x (%s)\n", data, machine().describe_context());

	m_eth_mappg = data & ~0xf;
}

READ32_MEMBER(sapphire_ioga_device::eth_control_r) const
{
	LOGMASKED(LOG_NETWORK, "eth: control_r 0x%08x (%s)\n", m_eth_control, machine().describe_context());

	return m_eth_control;
}

WRITE32_MEMBER(sapphire_ioga_device::eth_control_w)
{
	LOGMASKED(LOG_NETWORK, "eth: control_w 0x%08x mem_mask 0x%08x (%s)\n", data, mem_mask, machine().describe_context());

	m_eth_control = data & ETH_MASK;

	// ethernet device reset
	if ((data & ETH_RESET) == 0)
	{
		// reset the ethernet device
		siblingdevice("eth")->reset();

		// clear the reset flag (the 250ns delay is long enough to pass diagnostic tests)
		m_eth_reset_timer->adjust(attotime::from_nsec(250));
	}

	// ethernet channel attention
	if (data & ETH_CA)
	{
		m_eth_ca_func(ASSERT_LINE);
		m_eth_ca_func(CLEAR_LINE);
	}
}

WRITE32_MEMBER(sapphire_ioga_device::eth_w)
{
	// top two bits give channel (0=A, 4=B, 8=C, f=?)
	const int channel = offset >> 28;
	u32 address = (offset << 2) & 0x3fffffff;

	if ((m_eth_control & ETH_MAPEN) && (address & ETH_MAPPG) == (m_eth_mappg & ETH_MAPPG))
	{
		address &= ~(m_eth_mappg & ETH_MAPPG);
		address |= (m_eth_remap & ETH_REMAP_ADDR);

		LOGMASKED(LOG_NETWORK, "eth_w address 0x%08x remapped 0x%08x\n", offset << 2, address);
	}

	LOGMASKED(LOG_NETWORK, "eth_w channel %c address 0x%08x mask 0x%08x data 0x%08x\n", channel + 'A', address, mem_mask, data);
	m_memory_space->write_dword(address, data, mem_mask);
}

READ32_MEMBER(sapphire_ioga_device::eth_r)
{
	// top two bits give channel (0=A, 4=B, 8=C, f=?)
	const int channel = offset >> 28;
	u32 address = (offset << 2) & 0x3fffffff;

	if ((m_eth_control & ETH_MAPEN) && (address & ETH_MAPPG) == (m_eth_mappg & ETH_MAPPG))
	{
		address &= ~(m_eth_mappg & ETH_MAPPG);
		address |= (m_eth_remap & ETH_REMAP_ADDR);
		address &= 0x3fffffff;

		LOGMASKED(LOG_NETWORK, "eth_r address 0x%08x remapped 0x%08x\n", offset << 2, address);
	}

	u32 data = m_memory_space->read_dword(address, mem_mask);
	LOGMASKED(LOG_NETWORK, "eth_r channel %c address 0x%08x mask 0x%08x data 0x%08x\n", channel + 'A', address, mem_mask, data);
	return data;
}

TIMER_CALLBACK_MEMBER(sapphire_ioga_device::eth_reset)
{
	LOGMASKED(LOG_NETWORK, "eth: reset flag cleared\n");

	// clear ethernet reset flag
	m_eth_control &= ~ETH_RESET;
}
