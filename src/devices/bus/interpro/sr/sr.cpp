// license:BSD-3-Clause
// copyright-holders:Patrick Mackinlay

/*
 * An initial very primitive emulation of the SR bus for the Intergraph
 * InterPro.
 *
 * The bus is referred to by several different names at different places
 * in the system code, such as SR, SR bus, SRX, SRX/C bus, CBUS and some
 * variations on those. It's likely that SRX is an enhanced version of the
 * original SR bus implementation, and that CBUS is an alternate name.
 *
 * SR bus devices are mapped into the host memory map at a fixed address
 * range per slot, starting at 0x87000000 and incrementing by 0x08000000.
 * Within this address range, the first 32 double words provide access to
 * a 32 byte board signature area, with a format as follows.
 *
 *    u8 board[8]      first four bytes typically zero, next three bytes
 *                     contain a board number (in ASCII) from the list
 *                     below, last byte contains a revision character
 *    u8 eco[8]        engineering change order bits; these are initially
 *                     all set, and are cleared starting from the least
 *                     significant bit of the first byte onward to indicate
 *                     board level changes
 *    u8 feature[8]    used for different purposes by different boards
 *    u8 reserved[2]   always set to 0xff
 *    u16 family       indicate the board type from the list below
 *    u8 footprint[4]  the first three bytes contain the magic number 0x55
 *                     0xaa 0x55, the last byte contains the ones-complement
 *                     of the sum of the other bytes as a checksum
 *
 * The following are the board families taken from the system boot ROM.
 *
 *    Family   Description
 *    ------   -----------
 *    0x0000   I/O processor
 *    0x0001   CLIX engine
 *    0x0002   expansion memory
 *    0x0003   floating-point engine
 *    0x0004   pipe-starter
 *    0x0005   integrated frame buffer
 *    0x0006   32/CGX
 *    0x0007   GRE integrated frame buffer
 *    0x0008   hardcopy/digitizer
 *    0x0009   plotter DMA I/F
 *    0x000a   A/V mux
 *    0x000b   image subsystem I/F
 *    0x000c   VME adapter
 *    0x000d   I/O processor
 *    0x000e   CLIX engine
 *    0x000f   frame grabber
 *    0x0010   screen generator subsystem
 *    0x0011   scanner I/F
 *    0x0012   convolution filter
 *    0x0013   non-linear filter
 *    0x0014   run-length encoder
 *    0x0015   color setter data board
 *    0x0016   EDGE I
 *    0x0017   6000-series system board
 *    0x0018   32-channel async I/F
 *    0x0019   SR QUAD SCSI I/F
 *    0x001a   FDDI I/F
 *    0x001b   EDGE II processor
 *    0x001c   EDGE II frame buffer
 *    0x001d   IOI
 *    0x001e   IOI Clix Engine
 *    0x001f   MAYA bus I/F unit
 *    0x0020   comp/decomp group 4
 *    0x0021   QWIC/SRX I/F
 *    0x0022   SRX test board
 *    0x0023   QWIC bus CPU
 *    0x0024   2000-series system board
 *    0x0025   2000-series graphics board
 *    0x0026   E100/SRX device I/F
 *    0x0027   IKOS H/W simulator I/F
 *    0x0028   enhanced VME adapter
 *    0x0029   QWIC expansion memory
 *    0x002a   image comp/decomp
 *    0x002b   teleconferencing controller
 *    0x002c   Sky 8115-I interface
 *    0x002d   C-bus token ring
 *    0x002e   Screen IV PressFax Subsystem
 *    0x002f   C400E CPU
 *    0x0030   C-bus exp Ethernet
 *    0x0031   2400-series CPU
 *    0x0032   6400-series CPU
 *    0x0033   6000-series GT Graphics
 *    0x0034   C4K QWIC bus processor
 *    0x0038   Visualization Processor
 *    0x0039   2700-series CPU
 *    0x0040   6700-series CPU
 *    0x0041   2800-series CPU
 *    0x0042   6700-series CPU
 *
 * The following are the board names taken from the showconfig.dat file
 * shipped with the operating system.
 *
 *    #     Description
 *    ---   -----------
 *    004   EDGE-2 Processor f/2 2Mp-FB's
 *    008   Image Memory VRAM Board
 *    009   EDGE II Frame Buffer for ImageStation
 *    010   VI50 Image Processor
 *    014   SRX Quad-SCSI Controller
 *    019   C4E CPU Assembly
 *    030   EDGE-2 Processor f/1 or 2 1Mp-FB's
 *    031   EDGE-2 Processor f/1 2Mp-FB
 *    032   CBUS Token Ring
 *    034   256MB QWIC Bus Expansion Memory
 *    046   6400 36-MHz Series System Board
 *    047   2400 Series System Board
 *    068   2400 Graphics f/1 1Mp Monitor (V-60)
 *    069   2400 Graphics f/2 1Mp Monitors (V-60)
 *    070   2400 Graphics f/1 1Mp Monitor (V-76)
 *    071   2400 Graphics f/2 1Mp Monitors (V-76)
 *    081   2400 Graphics f/1 2Mp Monitor (V-60/76)
 *    083   SRX Hard PC Option / 8 Mb
 *    093   EDGE-2 Plus Processor f/1 or 2 1Mp-FB's
 *    094   EDGE-2 Plus Processor f/1 2Mp-FB
 *    095   EDGE-2 Plus Processor f/2 2Mp-FB's
 *    096   EDGE-2 Plus Frame Buffer f/1Mp Monitor
 *    098   6400 32-MHz Sapphire System Board
 *    101   2400 Graphics f/1 1Mp Monitor (V-76)
 *    100   2500 Series System Board
 *    102   2400 Graphics f/2 1Mp Monitors (V-76)
 *    106   C-Bus Series Hard PC Option / 8 Mb
 *    112   SRX Fast Quad-SCSI Controller
 *    115   Edge III Processor
 *    116   Edge III-C Single Ramdac
 *    126   SRX Enhanced VME Adapter
 *    127   6700 Series System Board
 *    128   2700 Series System Board
 *    129   6800 Series System Board
 *    135   GT II Graphics f/1 2Mp Monitor (V-60/76)
 *    136   GT II Graphics f/2 2Mp Monitor (V-60/76)
 *    144   6800 Series System Board
 *    145   2800 Series System Board
 *    217   C-Bus Hard PC Option / 16 Mb
 *    218   SRX Hard PC Option / 16 Mb
 *    512   32C Clix Engine w/6MB
 *    543   Floating Point Engine
 *    548   80386 I/O Processor
 *    577   Analog Video Mux/Summer
 *    588   80186 I/O Processor
 *    595   Pipe Starter
 *    604   Digitizer/HardCopy Controller
 *    605   300 Series Clix Engine w/16MB
 *    617   300 Series SR VME-Adapter
 *    633   32MB Expansion Memory
 *    636   80186 I/O Processor w/CoProcessor
 *    641   Image Subsystem Interface
 *    643   8Mb Integrated Frame Buffer f/1Mp
 *    650   2Mb Integrated Frame Buffer f/1Mp
 *    652   2Mb Integrated Frame Buffer f/1Mp
 *    657   32C Clix Engine w/8MB
 *    663   200 Series SR VME-Adapter
 *    664   32C Clix Engine w/16MB
 *    675   SR Plotter-DMA-Interface
 *    677   32C Clix Engine w/6MB
 *    686   8Mb Integrated Frame Buffer f/2Mp
 *    693   Clix Engine w/8MB
 *    694   Clix Engine w/16MB
 *    722   SR Frame Grabber NTSC
 *    730   300/400 Series Clix Engine w/8MB
 *    732   Clix Engine w/16MB
 *    739   16MB Expansion Memory
 *    765   6000 System Board w/16MB
 *    776   Runlength Encoder
 *    777   Scanner Interface
 *    778   Nonlinear Filter
 *    779   SIP Convolution Filter
 *    789   32C Clix Engine w/6MB
 *    792   Digitizer/HardCopy Controller Plus
 *    796   2Mb Integrated Frame Buffer f/1Mp -T
 *    799   80386 I/O Processor -T
 *    801   Clix Engine w/8MB
 *    819   QWIC Bus Clix Engine w/256K CB CACHE
 *    820   QWIC System Interface w/64MB ECC
 *    821   SRX 32 Channel RS232 Controller
 *    822   300 Series Clix Engine w/16MB ECC
 *    823   300/400 Series Clix Engine w/8MB ECC
 *    825   32C Clix Engine w/12MB
 *    826   FDDI Communications Processor
 *    828   EDGE-1 Graphics f/1 1Mp Monitor (55K/60)
 *    837   Analog Video Mux/Summer -T
 *    838   Floating Point Engine -T
 *    844   32MB Expansion Memory -T
 *    849   EDGE-1 Graphics f/1 2Mp Monitor (55K/60)
 *    851   Clix Engine w/16MB ECC -T
 *    852   Digitizer/HardCopy Controller Plus -T
 *    853   Clix Engine w/16MB ECC -T
 *    883   6000 System Board w/8MB
 *    887   SR Frame Grabber PAL
 *    894   Clix Engine w/16MB ECC
 *    896   EDGE-2/Plus Frame Buffer f/2Mp Monitor (V-60)
 *    897   EDGE-2 Processor f/1 2Mp-FB
 *    904   EDGE-1 Graphics f/2 1Mp Monitors (55K/60)
 *    905   SRX Frame Grabber NTSC
 *    906   SRX Frame Grabber PAL
 *    915   64MB QWIC Bus Expansion Memory
 *    917   Input Output Interface
 *    932   IOI Clix Engine w/16MB ECC
 *    956   SRX Enhanced VME-Adapter
 *    958   VME-Controller f/SRX Interface
 *    962   2000 System Board
 *    963   2000 Graphics f/1 1Mp Monitor
 *    965   EDGE-1 Graphics f/1 1Mp Monitor (66K/72)
 *    966   EDGE-1 Graphics f/2 1Mp Monitors (66K/72)
 *    977   6000 System Board w/32MB parity
 *    978   6000 System Board w/48MB parity
 *    979   6000 System Board w/64MB parity
 *    980   6200 System Board w/8MB parity
 *    981   6200 System Board w/16MB parity
 *    982   6200 System Board w/32MB parity
 *    983   6200 System Board w/48MB parity
 *    984   6200 System Board w/64MB parity
 *    31275   Raster Data Board
 *    31277   Screener III A Board
 *    31277/8   Screener III A+B Board
 *    A59   200 Series Clix Engine w/16MB parity
 *    A61   200 Series Clix Engine w/8MB parity
 *    A63   EDGE-2 Frame Buffer f/1Mp Monitor (55K/60)
 *    A77   200 Series Clix Engine w/16MB
 *    A79   2000 Graphics f/2 1Mp Monitors
 *    A80   QWIC System Interface w/16MB ECC
 *    A81   300 Series Clix Engine w/16MB ECC
 *    A86   SRX Teleconferencing Controller
 *    A95   IOI Clix Engine w/32MB ECC
 *    A96   IOI Clix Engine w/64MB ECC
 *    B13   System Board w/16MB parity -T
 *    B14   System Board w/48MB parity -T
 *    B15   EDGE-1 Graphics f/1 1Mp Monitor (55K/60) -T
 *    B16   EDGE-1 Graphics f/2 1Mp Monitors (55K/60) -T
 *    B17   IOI Clix Engine w/16MB ECC -T
 *    B18   Input Output Interface -T
 *    B20   SRX 32 Channel RS232 Controller -T
 *    B21   FDDI Communications Processor -T
 *    B22   SRX Quad-SCSI Controller -T
 *    b23   JPEG Compression/Decompression I/F
 *    B50   Application-Specific Acceleration Proc. II
 *    B63   IOI Clix Engine w/64MB ECC -T
 *    B67   GT Plus Graphics f/1 1Mp Monitor (V-76)
 *    B68   GT II Graphics f/1 1Mp Monitor (V-76)
 *    B70   GT II Graphics f/2 1Mp Monitors (V-76)
 *    B92   GT II Graphics f/1 2Mp Monitor (V-60/76)
 *    B93   GT II Graphics f/2 2Mp Monitors (V-60/76)
 *    B99   NTSC Frame Grabber for 3000 Series
 *    C01   PAL Frame Grabber for 3000 series
 *    C02   NTSC Frame Grabber for 6000 series
 *    C03   PAL Frame Grabber for 6000 series
 *    c05   25Mhz GTII Graphics f/1 1Mp Monitor
 *    c06   25Mhz GTII Graphics f/2 1Mp Monitors
 *    C41   GTII 60/76Hz Graphics f/1 2Mp Monitor
 *    C42   GTII 60/76Hz Graphics f/2 2Mp Monitor
 */

#include "emu.h"
#include "sr.h"

#define VERBOSE 0
#include "logmacro.h"

DEFINE_DEVICE_TYPE(SR_SLOT, sr_slot_device, "sr_slot", "InterPro SR bus slot")

sr_slot_device::sr_slot_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, SR_SLOT, tag, owner, clock)
	, device_slot_interface(mconfig, *this)
	, m_sr_tag(nullptr)
	, m_sr_slot_tag(nullptr)
{
}

void sr_slot_device::static_set_sr_slot(device_t &device, const char *tag, const char *slot_tag)
{
	sr_slot_device &sr_card = dynamic_cast<sr_slot_device &>(device);

	sr_card.m_sr_tag = tag;
	sr_card.m_sr_slot_tag = slot_tag;
}

void sr_slot_device::device_start()
{
	device_sr_card_interface *dev = dynamic_cast<device_sr_card_interface *>(get_card_device());

	if (dev)
		device_sr_card_interface::static_set_sr_tag(*dev, m_sr_tag, m_sr_slot_tag);
}

DEFINE_DEVICE_TYPE(SR, sr_device, "sr", "InterPro SR bus")

sr_device::sr_device(const machine_config &mconfig, const char *tag, device_t *owner, u32 clock)
	: device_t(mconfig, SR, tag, owner, clock)
	, m_out_irq0_cb(*this)
	, m_out_irq1_cb(*this)
	, m_out_irq2_cb(*this)
	, m_memory_tag(nullptr)
{
}

void sr_device::static_set_memory(device_t &device, const char *const tag, const int data_spacenum, const int io_spacenum)
{
	sr_device &sr = dynamic_cast<sr_device &>(device);

	sr.m_memory_tag = tag;
	sr.m_data_spacenum = data_spacenum;
	sr.m_io_spacenum = io_spacenum;
}

void sr_device::device_start()
{
	assert_always(m_memory_tag != nullptr, "memory tag and address spaces must be configured");

	// get the memory spaces
	device_memory_interface *memory;
	siblingdevice(m_memory_tag)->interface(memory);
	m_data_space = &memory->space(m_data_spacenum);
	m_io_space = &memory->space(m_io_spacenum);

	// resolve callbacks
	m_out_irq0_cb.resolve_safe();
	m_out_irq1_cb.resolve_safe();
	m_out_irq2_cb.resolve_safe();

	// empty the slots
	m_slot_count = 0;
	for (device_sr_card_interface *&slot : m_slot)
		slot = nullptr;
}

void sr_device::device_reset()
{
}

device_sr_card_interface::device_sr_card_interface(const machine_config &mconfig, device_t &device)
	: device_slot_card_interface(mconfig, device)
	, m_sr(nullptr)
	, m_sr_tag(nullptr)
	, m_sr_slot_tag(nullptr)
{
}

device_sr_card_interface::~device_sr_card_interface()
{
}

void device_sr_card_interface::static_set_sr_tag(device_t &device, const char *tag, const char *slot_tag)
{
	device_sr_card_interface &sr_card = dynamic_cast<device_sr_card_interface &>(device);

	sr_card.m_sr_tag = tag;
	sr_card.m_sr_slot_tag = slot_tag;
}

void device_sr_card_interface::set_sr_device()
{
	// get a reference to the bus
	m_sr = dynamic_cast<sr_device *>(device().machine().device(m_sr_tag));

	// install the card in the next available slot
	m_sr->install_card(*this, &device_sr_card_interface::map);
}

sr_card_device_base::sr_card_device_base(const machine_config &mconfig, device_type type, const char *tag, device_t *owner, u32 clock, const char *idprom_region)
	: device_t(mconfig, type, tag, owner, clock)
	, device_sr_card_interface(mconfig, *this)
	, m_idprom_region(idprom_region)
{
}

void sr_card_device_base::device_start()
{
	set_sr_device();
}
