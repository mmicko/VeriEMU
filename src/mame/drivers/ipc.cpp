// license:BSD-3-Clause
// copyright-holders:Robbbert
/***************************************************************************

        Intel iPB and iPC

        17/12/2009 Skeleton driver.

        22/04/2011 Connected to a terminal, it responds. Modernised.

        --> When started, you must press Space, then it will start to work.

        Monitor commands:
        A
        Dn n - dump memory
        E
        Fn n n - fill memory
        G
        Hn n - hex arithmetic
        Mn n n - move (copy) memory block
        N
        Q
        R
        Sn - modify a byte of memory
        W - display memory in Intel? format
        X - show and modify registers


        Preliminary Memory Map
        E800-F7FF BIOS ROM area
        F800-FFFF Monitor ROM (or other user interface)

        I/O F4/F5 main console input and output
        I/O F6/F7 alternate console input

        ToDo:
        - Everything!
        - iPC - Find missing rom F800-FFFF

****************************************************************************/

#include "emu.h"
#include "cpu/i8085/i8085.h"
#include "machine/i8251.h"
#include "machine/pit8253.h"
#include "bus/rs232/rs232.h"


class ipc_state : public driver_device
{
public:
	ipc_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
	{ }

	void ipc(machine_config &config);
private:
	virtual void machine_reset() override;
	required_device<cpu_device> m_maincpu;
};


static ADDRESS_MAP_START(ipc_mem, AS_PROGRAM, 8, ipc_state)
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0xdfff) AM_RAM
	AM_RANGE(0xe800, 0xffff) AM_ROM AM_REGION("roms", 0)
ADDRESS_MAP_END

static ADDRESS_MAP_START( ipc_io, AS_IO, 8, ipc_state)
	ADDRESS_MAP_UNMAP_HIGH
	ADDRESS_MAP_GLOBAL_MASK(0xff)
	AM_RANGE(0xf0, 0xf3) AM_DEVREADWRITE("pit", pit8253_device, read, write)
	AM_RANGE(0xf4, 0xf4) AM_DEVREADWRITE("uart1", i8251_device, data_r, data_w)
	AM_RANGE(0xf5, 0xf5) AM_DEVREADWRITE("uart1", i8251_device, status_r, control_w)
	AM_RANGE(0xf6, 0xf6) AM_DEVREADWRITE("uart2", i8251_device, data_r, data_w)
	AM_RANGE(0xf7, 0xf7) AM_DEVREADWRITE("uart2", i8251_device, status_r, control_w)
ADDRESS_MAP_END

/* Input ports */
static INPUT_PORTS_START( ipc )
INPUT_PORTS_END


void ipc_state::machine_reset()
{
	m_maincpu->set_state_int(i8085a_cpu_device::I8085_PC, 0xE800);
}


MACHINE_CONFIG_START(ipc_state::ipc)
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu",I8085A, XTAL(19'660'800) / 4)
	MCFG_CPU_PROGRAM_MAP(ipc_mem)
	MCFG_CPU_IO_MAP(ipc_io)

	MCFG_DEVICE_ADD("pit", PIT8253, 0)
	MCFG_PIT8253_CLK0(XTAL(19'660'800) / 16)
	MCFG_PIT8253_CLK1(XTAL(19'660'800) / 16)
	MCFG_PIT8253_CLK2(XTAL(19'660'800) / 16)
	MCFG_PIT8253_OUT0_HANDLER(DEVWRITELINE("uart1", i8251_device, write_txc))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("uart1", i8251_device, write_rxc))
	MCFG_PIT8253_OUT1_HANDLER(DEVWRITELINE("uart2", i8251_device, write_txc))
	MCFG_DEVCB_CHAIN_OUTPUT(DEVWRITELINE("uart2", i8251_device, write_rxc))

	MCFG_DEVICE_ADD("uart1", I8251, 0) // 8 data bits, no parity, 1 stop bit, 9600 baud
	MCFG_I8251_TXD_HANDLER(DEVWRITELINE("rs232a", rs232_port_device, write_txd))
	MCFG_I8251_DTR_HANDLER(DEVWRITELINE("rs232a", rs232_port_device, write_dtr))
	MCFG_I8251_RTS_HANDLER(DEVWRITELINE("rs232a", rs232_port_device, write_rts))

	MCFG_RS232_PORT_ADD("rs232a", default_rs232_devices, "terminal")
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("uart1", i8251_device, write_rxd))
	MCFG_RS232_DSR_HANDLER(DEVWRITELINE("uart1", i8251_device, write_dsr))
	MCFG_RS232_CTS_HANDLER(DEVWRITELINE("uart1", i8251_device, write_cts))

	MCFG_DEVICE_ADD("uart2", I8251, 0) // 8 data bits, no parity, 2 stop bits, 2400 baud
	MCFG_I8251_TXD_HANDLER(DEVWRITELINE("rs232b", rs232_port_device, write_txd))
	MCFG_I8251_DTR_HANDLER(DEVWRITELINE("rs232b", rs232_port_device, write_dtr))
	MCFG_I8251_RTS_HANDLER(DEVWRITELINE("rs232b", rs232_port_device, write_rts))

	MCFG_RS232_PORT_ADD("rs232b", default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("uart2", i8251_device, write_rxd))
	MCFG_RS232_DSR_HANDLER(DEVWRITELINE("uart2", i8251_device, write_dsr))
	MCFG_RS232_CTS_HANDLER(DEVWRITELINE("uart2", i8251_device, write_cts))
MACHINE_CONFIG_END

/* ROM definition */
ROM_START( ipb )
	ROM_REGION( 0x1800, "roms", ROMREGION_ERASEFF )
	ROM_LOAD( "ipb_e8_v1.3.bin", 0x0000, 0x0800, CRC(fc9d4703) SHA1(2ce078e1bcd8b24217830c54bcf04c5d146d1b76) )
	ROM_LOAD( "ipb_f8_v1.3.bin", 0x1000, 0x0800, CRC(966ba421) SHA1(d6a904c7d992a05ed0f451d7d34c1fc8de9547ee) )
ROM_END

ROM_START( ipc )
	ROM_REGION( 0x1800, "roms", ROMREGION_ERASEFF )
	ROM_LOAD( "ipc_u82_v1.3_104584-001.bin", 0x0000, 0x1000, CRC(0889394f) SHA1(b7525baf1884a7d67402dea4b5566016a9861ef2) )

	// required rom is missing. Using this one from 'ipb' for now.
	ROM_LOAD( "ipb_f8_v1.3.bin", 0x1000, 0x0800, BAD_DUMP CRC(966ba421) SHA1(d6a904c7d992a05ed0f451d7d34c1fc8de9547ee) )
ROM_END

/* Driver */

/*    YEAR  NAME    PARENT  COMPAT   MACHINE   INPUT  STATE      INIT   COMPANY    FULLNAME  FLAGS */
COMP( 19??, ipb,    0,      0,       ipc,      ipc,   ipc_state, 0,     "Intel",   "iPB",    MACHINE_NO_SOUND_HW )
COMP( 19??, ipc,    ipb,    0,       ipc,      ipc,   ipc_state, 0,     "Intel",   "iPC",    MACHINE_NOT_WORKING | MACHINE_NO_SOUND_HW )
