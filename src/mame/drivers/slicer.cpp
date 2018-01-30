// license:BSD-3-Clause
// copyright-holders:Carl

// Slicer Computers Slicer 80186 SBC
// The bios makefile refers to a "exe3bin" utility, this can be substituted with FreeDOS exe2bin and the /l=0xf800 option
// which will fixup the relocations

#include "emu.h"
#include "cpu/i86/i186.h"
#include "machine/74259.h"
#include "machine/wd_fdc.h"
#include "machine/mc68681.h"
#include "bus/rs232/rs232.h"
#include "bus/isa/isa.h"
#include "bus/scsi/scsi.h"

class slicer_state : public driver_device
{
public:
	slicer_state(const machine_config &mconfig, device_type type, const char *tag) :
		driver_device(mconfig, type, tag),
		m_fdc(*this, "fdc"),
		m_sasi(*this, "sasi")
	{
	}

	DECLARE_WRITE8_MEMBER(sio_out_w);
	template<unsigned int drive> DECLARE_WRITE_LINE_MEMBER(drive_sel_w);

	void slicer(machine_config &config);
protected:
	required_device<fd1797_device> m_fdc;
	required_device<scsi_port_device> m_sasi;
};

WRITE8_MEMBER(slicer_state::sio_out_w)
{
	floppy_image_device *floppy;
	int state = (data & 0x80) ? 0 : 1;
	char devname[8];

	for(int i = 0; i < 4; i++)
	{
		sprintf(devname, "%d", i);
		floppy = m_fdc->subdevice<floppy_connector>(devname)->get_device();
		if(floppy)
			floppy->mon_w(state);
	}
}

template<unsigned int drive>
WRITE_LINE_MEMBER(slicer_state::drive_sel_w)
{
	floppy_image_device *floppy;
	char devname[8];

	if (!state)
		return;

	sprintf(devname, "%d", drive);
	floppy = m_fdc->subdevice<floppy_connector>(devname)->get_device();
	m_fdc->set_floppy(floppy);
}

static ADDRESS_MAP_START( slicer_map, AS_PROGRAM, 16, slicer_state )
	AM_RANGE(0x00000, 0x3ffff) AM_RAM // fixed 256k for now
	AM_RANGE(0xf8000, 0xfffff) AM_ROM AM_REGION("bios", 0)
ADDRESS_MAP_END

static ADDRESS_MAP_START( slicer_io, AS_IO, 16, slicer_state )
	ADDRESS_MAP_UNMAP_HIGH
	AM_RANGE(0x0000, 0x007f) AM_DEVREADWRITE8("fdc", fd1797_device, read, write, 0x00ff) //PCS0
	AM_RANGE(0x0080, 0x00ff) AM_DEVREADWRITE8("duart", scn2681_device, read, write, 0x00ff) //PCS1
	AM_RANGE(0x0100, 0x0107) AM_MIRROR(0x0078) AM_DEVWRITE8("drivelatch", ls259_device, write_d0, 0x00ff) //PCS2
	// TODO: 0x180 sets ack
	AM_RANGE(0x0180, 0x0181) AM_DEVREAD8("sasi_data_in", input_buffer_device, read, 0x00ff) AM_DEVWRITE8("sasi_data_out", output_latch_device, write, 0x00ff) //PCS3
	AM_RANGE(0x0180, 0x0181) AM_DEVREAD8("sasi_ctrl_in", input_buffer_device, read, 0xff00)
	AM_RANGE(0x0184, 0x0185) AM_DEVREAD8("sasi_data_in", input_buffer_device, read, 0x00ff) AM_DEVWRITE8("sasi_data_out", output_latch_device, write, 0x00ff)
	AM_RANGE(0x0184, 0x0185) AM_DEVREAD8("sasi_ctrl_in", input_buffer_device, read, 0xff00)
ADDRESS_MAP_END

static SLOT_INTERFACE_START( slicer_floppies )
	SLOT_INTERFACE("525dd", FLOPPY_525_DD)
	SLOT_INTERFACE("8dsdd", FLOPPY_8_DSDD)
SLOT_INTERFACE_END

MACHINE_CONFIG_START(slicer_state::slicer)
	MCFG_CPU_ADD("maincpu", I80186, XTAL(16'000'000) / 2)
	MCFG_CPU_PROGRAM_MAP(slicer_map)
	MCFG_CPU_IO_MAP(slicer_io)

	MCFG_DEVICE_ADD("duart", SCN2681, XTAL(3'686'400))
	MCFG_MC68681_IRQ_CALLBACK(DEVWRITELINE("maincpu", i80186_cpu_device, int0_w))
	MCFG_MC68681_A_TX_CALLBACK(DEVWRITELINE("rs232_1", rs232_port_device, write_txd))
	MCFG_MC68681_B_TX_CALLBACK(DEVWRITELINE("rs232_2", rs232_port_device, write_txd))
	MCFG_MC68681_OUTPORT_CALLBACK(WRITE8(slicer_state, sio_out_w))

	MCFG_RS232_PORT_ADD("rs232_1", default_rs232_devices, "terminal")
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("duart", scn2681_device, rx_a_w))
	MCFG_RS232_PORT_ADD("rs232_2", default_rs232_devices, nullptr)
	MCFG_RS232_RXD_HANDLER(DEVWRITELINE("duart", scn2681_device, rx_b_w))

	MCFG_FD1797_ADD("fdc", XTAL(16'000'000)/2/8)
	MCFG_WD_FDC_INTRQ_CALLBACK(DEVWRITELINE("maincpu", i80186_cpu_device, int1_w))
	MCFG_WD_FDC_DRQ_CALLBACK(DEVWRITELINE("maincpu", i80186_cpu_device, drq0_w))
	MCFG_FLOPPY_DRIVE_ADD("fdc:0", slicer_floppies, "525dd", floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD("fdc:1", slicer_floppies, nullptr, floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD("fdc:2", slicer_floppies, nullptr, floppy_image_device::default_floppy_formats)
	MCFG_FLOPPY_DRIVE_ADD("fdc:3", slicer_floppies, nullptr, floppy_image_device::default_floppy_formats)

	MCFG_DEVICE_ADD("drivelatch", LS259, 0) // U29
	MCFG_ADDRESSABLE_LATCH_Q0_OUT_CB(DEVWRITELINE("sasi", scsi_port_device, write_sel))
	MCFG_ADDRESSABLE_LATCH_Q1_OUT_CB(DEVWRITELINE("sasi", scsi_port_device, write_rst))
	MCFG_ADDRESSABLE_LATCH_Q2_OUT_CB(WRITELINE(slicer_state, drive_sel_w<3>))
	MCFG_ADDRESSABLE_LATCH_Q3_OUT_CB(WRITELINE(slicer_state, drive_sel_w<2>))
	MCFG_ADDRESSABLE_LATCH_Q4_OUT_CB(WRITELINE(slicer_state, drive_sel_w<1>))
	MCFG_ADDRESSABLE_LATCH_Q5_OUT_CB(WRITELINE(slicer_state, drive_sel_w<0>))
	MCFG_ADDRESSABLE_LATCH_Q7_OUT_CB(DEVWRITELINE("fdc", fd1797_device, dden_w))

	MCFG_DEVICE_ADD("sasi", SCSI_PORT, 0)
	MCFG_SCSI_DATA_INPUT_BUFFER("sasi_data_in")
	MCFG_SCSI_BSY_HANDLER(DEVWRITELINE("sasi_ctrl_in", input_buffer_device, write_bit3))
	MCFG_SCSI_MSG_HANDLER(DEVWRITELINE("sasi_ctrl_in", input_buffer_device, write_bit4))
	MCFG_SCSI_CD_HANDLER(DEVWRITELINE("sasi_ctrl_in", input_buffer_device, write_bit5))
	MCFG_SCSI_REQ_HANDLER(DEVWRITELINE("sasi_ctrl_in", input_buffer_device, write_bit6))
	MCFG_SCSI_IO_HANDLER(DEVWRITELINE("sasi_ctrl_in", input_buffer_device, write_bit7))

	MCFG_SCSI_OUTPUT_LATCH_ADD("sasi_data_out", "sasi")
	MCFG_DEVICE_ADD("sasi_data_in", INPUT_BUFFER, 0)
	MCFG_DEVICE_ADD("sasi_ctrl_in", INPUT_BUFFER, 0)
MACHINE_CONFIG_END

ROM_START( slicer )
	ROM_REGION(0x8001, "bios", 0)
	// built from sources, reset.asm adds an extra byte
	ROM_LOAD("epbios.bin", 0x0000, 0x8001, CRC(96fe9dd4) SHA1(5fc43454fe7d51f2ae97aef822155dcd28eb7f23))

	ROM_REGION(0x10000, "user1", 0)
	//slicer_h.bin : main slicer board, high byte
	//slicer_l.bin : main slicer board, low byte
	//slvid_cg.bin : slicer video/keyboard expansion board, character generator
	//slvid_e.bin : slicer video/keyboard expansion board, even byte
	//slvid_o.bin : slicer video/keyboard expansion board, odd byte
	ROM_LOAD( "slicer_h.bin", 0x000000, 0x004000, CRC(1f9a79b7) SHA1(2070c6818d39fe7ec4370fc2304469793a126731) )
	ROM_LOAD( "slicer_l.bin", 0x000000, 0x004000, CRC(6feef94b) SHA1(174488591b727a4130166bcb2e83c0e74323d43b) )
	ROM_LOAD( "slvid_cg.bin", 0x000000, 0x001000, CRC(d4d9ac2f) SHA1(866c760320b224ba8670501ea905de32193acedc) )
	ROM_LOAD( "slvid_o.bin",  0x000000, 0x001000, CRC(c62dda77) SHA1(1d0b9abc53412b0725072d4c33c478fb5358ab5c) )
	ROM_LOAD( "slvid_e.bin",  0x000000, 0x001000, CRC(8694274f) SHA1(8373baaea8d689bf52699b587942a57f26baf740) )
ROM_END

COMP( 1983, slicer, 0, 0, slicer, 0, slicer_state, 0, "Slicer Computers", "Slicer", MACHINE_NO_SOUND )
