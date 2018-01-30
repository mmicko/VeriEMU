// license:BSD-3-Clause
// copyright-holders: F. Ulivi
/*********************************************************************

    82937.cpp

    82937 module (HPIB interface)

    TODO: Implement Parallel Poll response

    Thanks to Tim Nye & Everett Kaser for dumping the 8049 ROM

    Main reference for this module is:
    HP 82937-90007, oct 80, HP82937A HP-IB Installation and theory
    of operation manual

*********************************************************************/

#include "emu.h"
#include "82937.h"
#include "coreutil.h"

// Debugging
#define VERBOSE 0
#include "logmacro.h"

// Bit manipulation
namespace {
	template<typename T> constexpr T BIT_MASK(unsigned n)
	{
		return (T)1U << n;
	}

	template<typename T> void BIT_SET(T& w , unsigned n)
	{
		w |= BIT_MASK<T>(n);
	}
}

// Bits in U3 (m_latch)
constexpr unsigned LATCH_CA_BIT = 5; // Controller Active
constexpr unsigned LATCH_TA_BIT = 4; // Talker Active
constexpr unsigned LATCH_EN_IFC_INT_BIT = 3; // Enable IFC interrupt
constexpr unsigned LATCH_EN_REN_INT_BIT = 2; // Enable REN interrupt
constexpr unsigned LATCH_EN_ATN_INT_BIT = 1; // Enable ATN interrupt
constexpr unsigned LATCH_EN_NDAC_BIT = 0;    // Enable NDAC

// Bits on P1 port of 8049
constexpr unsigned P1_IFC_BIT = 7;
constexpr unsigned P1_REN_BIT = 6;
constexpr unsigned P1_SRQ_BIT = 5;
constexpr unsigned P1_ATN_BIT = 4;
constexpr unsigned P1_EOI_BIT = 3;
constexpr unsigned P1_DAV_BIT = 2;
constexpr unsigned P1_NDAC_BIT = 1;
constexpr unsigned P1_NRFD_BIT = 0;

hp82937_io_card_device::hp82937_io_card_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock)
	: hp80_io_card_device(mconfig , HP82937_IO_CARD , tag , owner , clock),
	  m_cpu(*this , "cpu"),
	  m_translator(*this , "xlator"),
	  m_sw1(*this , "sw1"),
	  m_ieee488(*this , IEEE488_TAG)
{
}

hp82937_io_card_device::~hp82937_io_card_device()
{
}

void hp82937_io_card_device::install_read_write_handlers(address_space& space , uint16_t base_addr)
{
	space.install_readwrite_handler(base_addr , base_addr + 1 , READ8_DEVICE_DELEGATE(m_translator , hp_1mb5_device , cpu_r) , WRITE8_DEVICE_DELEGATE(m_translator , hp_1mb5_device , cpu_w));
}

void hp82937_io_card_device::inten()
{
	m_translator->inten();
}

void hp82937_io_card_device::clear_service()
{
	m_translator->clear_service();
}

WRITE_LINE_MEMBER(hp82937_io_card_device::reset_w)
{
	m_cpu->set_input_line(INPUT_LINE_RESET , state);
	if (state) {
		// When reset is asserted, clear state
		device_reset();
	}
}

READ_LINE_MEMBER(hp82937_io_card_device::t0_r)
{
	return m_iatn;
}

READ8_MEMBER(hp82937_io_card_device::p1_r)
{
	uint8_t res = 0;

	if (BIT(m_sw1->read() , 5)) {
		// System controller
		BIT_SET(res , P1_IFC_BIT);
		BIT_SET(res , P1_REN_BIT);
	} else {
		// Not system controller
		if (m_ieee488->ifc_r()) {
			BIT_SET(res , P1_IFC_BIT);
		}
		if (m_ieee488->ren_r()) {
			BIT_SET(res , P1_REN_BIT);
		}
	}
	if (!BIT(m_latch , LATCH_CA_BIT) || m_ieee488->srq_r()) {
		BIT_SET(res , P1_SRQ_BIT);
	}
	if (m_iatn) {
		BIT_SET(res , P1_ATN_BIT);
	}
	bool ndac = !BIT(m_latch , LATCH_EN_NDAC_BIT) || m_iatn;
	if (m_talker_out) {
		BIT_SET(res , P1_EOI_BIT);
		BIT_SET(res , P1_DAV_BIT);
		if (ndac && m_ieee488->ndac_r()) {
			BIT_SET(res , P1_NDAC_BIT);
		}
		if (m_ieee488->nrfd_r()) {
			BIT_SET(res , P1_NRFD_BIT);
		}
	} else {
		if (m_ieee488->eoi_r()) {
			BIT_SET(res , P1_EOI_BIT);
		}
		if (m_ieee488->dav_r()) {
			BIT_SET(res , P1_DAV_BIT);
		}
		if (ndac) {
			BIT_SET(res , P1_NDAC_BIT);
		}
		BIT_SET(res , P1_NRFD_BIT);
	}

	return res;
}

WRITE8_MEMBER(hp82937_io_card_device::p1_w)
{
	update_signals();
	update_data_out();
}

READ8_MEMBER(hp82937_io_card_device::dio_r)
{
	if (m_dio_out) {
		return 0xff;
	} else {
		return m_ieee488->dio_r();
	}
}

WRITE8_MEMBER(hp82937_io_card_device::dio_w)
{
	update_data_out();
}

READ8_MEMBER(hp82937_io_card_device::switch_r)
{
	return m_sw1->read() | 0xc0;
}

WRITE8_MEMBER(hp82937_io_card_device::latch_w)
{
	LOG("latch=%02x\n" , data);
	m_latch = data;
	update_signals();
	update_data_out();
}

WRITE_LINE_MEMBER(hp82937_io_card_device::ieee488_ctrl_w)
{
	update_signals();
	update_data_out();
}

static INPUT_PORTS_START(hp82937_port)
	MCFG_HP80_IO_SC(7)
	PORT_START("sw1")
	PORT_DIPNAME(0x1f , 0x15 , "HPIB address")
	PORT_DIPLOCATION("S1:7,6,5,4,3")
	PORT_DIPSETTING(0x00 , "0")
	PORT_DIPSETTING(0x01 , "1")
	PORT_DIPSETTING(0x02 , "2")
	PORT_DIPSETTING(0x03 , "3")
	PORT_DIPSETTING(0x04 , "4")
	PORT_DIPSETTING(0x05 , "5")
	PORT_DIPSETTING(0x06 , "6")
	PORT_DIPSETTING(0x07 , "7")
	PORT_DIPSETTING(0x08 , "8")
	PORT_DIPSETTING(0x09 , "9")
	PORT_DIPSETTING(0x0a , "10")
	PORT_DIPSETTING(0x0b , "11")
	PORT_DIPSETTING(0x0c , "12")
	PORT_DIPSETTING(0x0d , "13")
	PORT_DIPSETTING(0x0e , "14")
	PORT_DIPSETTING(0x0f , "15")
	PORT_DIPSETTING(0x10 , "16")
	PORT_DIPSETTING(0x11 , "17")
	PORT_DIPSETTING(0x12 , "18")
	PORT_DIPSETTING(0x13 , "19")
	PORT_DIPSETTING(0x14 , "20")
	PORT_DIPSETTING(0x15 , "21")
	PORT_DIPSETTING(0x16 , "22")
	PORT_DIPSETTING(0x17 , "23")
	PORT_DIPSETTING(0x18 , "24")
	PORT_DIPSETTING(0x19 , "25")
	PORT_DIPSETTING(0x1a , "26")
	PORT_DIPSETTING(0x1b , "27")
	PORT_DIPSETTING(0x1c , "28")
	PORT_DIPSETTING(0x1d , "29")
	PORT_DIPSETTING(0x1e , "30")
	PORT_DIPSETTING(0x1f , "31")
	PORT_DIPNAME(0x20 , 0x20 , "Sys. controller")
	PORT_DIPLOCATION("S1:2")
	PORT_DIPSETTING(0x00 , DEF_STR(Off))
	PORT_DIPSETTING(0x20 , DEF_STR(On))
INPUT_PORTS_END

ioport_constructor hp82937_io_card_device::device_input_ports() const
{
	return INPUT_PORTS_NAME(hp82937_port);
}

void hp82937_io_card_device::device_start()
{
	save_item(NAME(m_dio_out));
	save_item(NAME(m_talker_out));
	save_item(NAME(m_iatn));
	save_item(NAME(m_latch));
}

void hp82937_io_card_device::device_reset()
{
	hp80_io_card_device::device_reset();

	m_latch = 0;
	m_updating = false;
	update_signals();
	update_data_out();
}

void hp82937_io_card_device::update_data_out()
{
	m_ieee488->dio_w(m_dio_out ? m_cpu->p2_r(machine().dummy_space() , 0) : 0xff);
}

void hp82937_io_card_device::update_signals()
{
	// Avoid recursive re-enter when writing to IEEE488 signals
	if (m_updating) {
		return;
	}
	m_updating = true;
	bool ctrl_active = BIT(m_latch , LATCH_CA_BIT);
	uint8_t p1 = m_cpu->p1_r(machine().dummy_space() , 0);
	m_iatn = BIT(p1 , P1_ATN_BIT);
	if (ctrl_active) {
		m_ieee488->atn_w(m_iatn);
		m_ieee488->srq_w(1);
	} else {
		m_ieee488->atn_w(1);
		m_iatn = m_iatn && m_ieee488->atn_r();
		m_ieee488->srq_w(BIT(p1 , P1_SRQ_BIT));
	}
	m_talker_out = (ctrl_active && !m_iatn) || (BIT(m_latch , LATCH_TA_BIT) && m_iatn);
	if (m_talker_out) {
		m_ieee488->nrfd_w(1);
		m_ieee488->dav_w(BIT(p1 , P1_DAV_BIT));
		m_ieee488->eoi_w(BIT(p1 , P1_EOI_BIT));
		m_ieee488->ndac_w(1);

	} else {
		m_ieee488->nrfd_w(BIT(p1 , P1_NRFD_BIT));
		m_ieee488->dav_w(1);
		m_ieee488->eoi_w(1);
		bool ndac = BIT(p1 , P1_NDAC_BIT);
		if (BIT(m_latch , LATCH_EN_NDAC_BIT) && !m_iatn) {
			ndac = false;
		}
		m_ieee488->ndac_w(ndac);
	}
	bool iren = BIT(p1 , P1_REN_BIT);
	if (BIT(m_sw1->read() , 5)) {
		// System controller
		m_ieee488->ren_w(iren);
		m_ieee488->ifc_w(BIT(p1 , P1_IFC_BIT));
	} else {
		// Not system controller
		m_ieee488->ren_w(1);
		iren = iren && m_ieee488->ren_r();
		m_ieee488->ifc_w(1);
	}
	bool not_u8_1 = m_iatn || m_ieee488->eoi_r();
	m_dio_out = not_u8_1 && m_talker_out;
	bool irq = (BIT(m_latch , LATCH_EN_IFC_INT_BIT) && !m_ieee488->ifc_r()) ||
		(BIT(m_latch , LATCH_EN_REN_INT_BIT) && iren) ||
		(BIT(m_latch , LATCH_EN_ATN_INT_BIT) && !m_iatn);
	m_cpu->set_input_line(MCS48_INPUT_IRQ , irq);
	m_updating = false;
}

ROM_START(hp82937)
	ROM_REGION(0x800 , "cpu" , 0)
	ROM_LOAD("1820-2437.bin" , 0 , 0x800 , CRC(687d1559) SHA1(44dfc8c3f431cd37a270b094f1db751214009214))
ROM_END

static ADDRESS_MAP_START(cpu_io_map , AS_IO , 8 , hp82937_io_card_device)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x00 , 0x01) AM_DEVREADWRITE("xlator" , hp_1mb5_device , uc_r , uc_w)
	AM_RANGE(0x03 , 0x03) AM_READWRITE(switch_r , latch_w)
ADDRESS_MAP_END

const tiny_rom_entry *hp82937_io_card_device::device_rom_region() const
{
	return ROM_NAME(hp82937);
}

MACHINE_CONFIG_START(hp82937_io_card_device::device_add_mconfig)
	MCFG_CPU_ADD("cpu" , I8049 , XTAL(11'000'000))
	MCFG_CPU_IO_MAP(cpu_io_map)
	MCFG_MCS48_PORT_T0_IN_CB(READLINE(hp82937_io_card_device , t0_r))
	MCFG_MCS48_PORT_T1_IN_CB(DEVREADLINE("xlator" , hp_1mb5_device , int_r))
	MCFG_MCS48_PORT_P1_IN_CB(READ8(hp82937_io_card_device , p1_r))
	MCFG_MCS48_PORT_P1_OUT_CB(WRITE8(hp82937_io_card_device , p1_w))
	MCFG_MCS48_PORT_P2_IN_CB(READ8(hp82937_io_card_device , dio_r))
	MCFG_MCS48_PORT_P2_OUT_CB(WRITE8(hp82937_io_card_device , dio_w))

	MCFG_DEVICE_ADD("xlator" , HP_1MB5 , 0)
	MCFG_1MB5_IRL_HANDLER(WRITELINE(hp82937_io_card_device , irl_w))
	MCFG_1MB5_HALT_HANDLER(WRITELINE(hp82937_io_card_device , halt_w))
	MCFG_1MB5_RESET_HANDLER(WRITELINE(hp82937_io_card_device , reset_w))

	MCFG_IEEE488_SLOT_ADD("ieee_dev" , 0 , hp_ieee488_devices , nullptr)
	MCFG_IEEE488_BUS_ADD()
	MCFG_IEEE488_IFC_CALLBACK(WRITELINE(hp82937_io_card_device , ieee488_ctrl_w))
	MCFG_IEEE488_ATN_CALLBACK(WRITELINE(hp82937_io_card_device , ieee488_ctrl_w))
	MCFG_IEEE488_REN_CALLBACK(WRITELINE(hp82937_io_card_device , ieee488_ctrl_w))
	MCFG_IEEE488_EOI_CALLBACK(WRITELINE(hp82937_io_card_device , ieee488_ctrl_w))
MACHINE_CONFIG_END

// device type definition
DEFINE_DEVICE_TYPE(HP82937_IO_CARD, hp82937_io_card_device, "hp82937", "HP82937 card")
