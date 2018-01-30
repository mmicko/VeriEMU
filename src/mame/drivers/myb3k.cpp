// license:BSD-3-Clause
// copyright-holders:Angelo Salese, Joakim Larsson Edstrom
/***************************************************************************

    Matsushita Mybrain 3000 / Panasonic JB-3000 / Ericsson Step/One

    preliminary driver by Angelo Salese

    Slotified and corrected by Joakim Larsson Edstrom based on
    Step/One service manuals: http://nivelleringslikaren.eu/stepone/

    TODO:
    - Fix proper cursor support
    - Fix Hi-Res 640x400 mode when MC6845 supports interlaced mode
    - Expansion Unit with 6 more ISA8 slots
    - Proper waitstate support when 8088 CPU core admits it and remove the workaround in machine_start

    PC INCOMPATIBILITIES:
    - COM card lives at io address 0x540
    - FDC card lives at io address 0x20
    - DMA channel 0 is not part of ISA8 but implemented on B8 (DREQ0/SRDY)
      and B19 (DACK0/MEMREF)
    - Keyboard is not interfaced through 8255
    - Non standard graphics board

    These and other incompatibilities required many PC software's to be
    recompiled to work on this computer.

****************************************************************************/
#include "emu.h"
#include "cpu/i86/i86.h"
#include "machine/i8255.h"
#include "machine/myb3k_kbd.h"
#include "machine/pic8259.h"
#include "machine/pit8253.h"
#include "machine/i8257.h"
#include "sound/spkrdev.h"
#include "softlist.h"
#include "speaker.h"
#include "machine/ram.h"
#include "machine/wd_fdc.h"
#include "bus/isa/isa.h"
#include "bus/isa/myb3k_com.h"
#include "bus/isa/myb3k_fdc.h"
#include "bus/centronics/ctronics.h"
#include "video/mc6845.h"
#include "screen.h"

#define LOG_PPI     (1U << 1)
#define LOG_PIT     (1U << 2)
#define LOG_PIC     (1U << 3)
#define LOG_CRT     (1U << 4)
#define LOG_DMA     (1U << 5)
#define LOG_KBD     (1U << 6)
#define LOG_VMOD    (1U << 7)
#define LOG_PIX     (1U << 8)
#define LOG_M2      (1U << 9)
#define LOG_M3      (1U << 10)
#define LOG_SCRL    (1U << 11)
#define LOG_CENT    (1U << 12)
#define LOG_RAM     (1U << 13)

//#define VERBOSE (LOG_VMOD|LOG_RAM)
//#define LOG_OUTPUT_STREAM std::cout

#include "logmacro.h"

#define LOGPPI(...)  LOGMASKED(LOG_PPI,  __VA_ARGS__)
#define LOGPIT(...)  LOGMASKED(LOG_PIT,  __VA_ARGS__)
#define LOGPIC(...)  LOGMASKED(LOG_PIC,  __VA_ARGS__)
#define LOGCRT(...)  LOGMASKED(LOG_CRT,  __VA_ARGS__)
#define LOGDMA(...)  LOGMASKED(LOG_DMA,  __VA_ARGS__)
#define LOGKBD(...)  LOGMASKED(LOG_KBD,  __VA_ARGS__)
#define LOGVMOD(...) LOGMASKED(LOG_VMOD, __VA_ARGS__)
#define LOGPIX(...)  LOGMASKED(LOG_PIX,  __VA_ARGS__)
#define LOGM2(...)   LOGMASKED(LOG_M2,   __VA_ARGS__)
#define LOGM3(...)   LOGMASKED(LOG_M3,   __VA_ARGS__)
#define LOGSCRL(...) LOGMASKED(LOG_SCRL, __VA_ARGS__)
#define LOGCENT(...) LOGMASKED(LOG_CENT, __VA_ARGS__)
#define LOGRAM(...)  LOGMASKED(LOG_RAM, __VA_ARGS__)

#ifdef _MSC_VER
#define FUNCNAME __func__
#else
#define FUNCNAME __PRETTY_FUNCTION__
#endif

class myb3k_state : public driver_device
{
public:
	myb3k_state(const machine_config &mconfig, device_type type, const char *tag)
		: driver_device(mconfig, type, tag)
		, m_maincpu(*this, "maincpu")
		, m_ram(*this, RAM_TAG)
		, m_pic8259(*this, "pic")
		, m_pit8253(*this, "pit")
		, m_ppi8255(*this, "ppi")
		, m_dma8257(*this, "dma")
		, m_speaker(*this, "speaker")
		, m_kb(*this, "myb3k_keyboard")
		, m_crtc(*this, "crtc")
		, m_vram(*this, "vram")
		, m_isabus(*this, "isa")
		, m_centronics(*this, "centronics")
		, m_io_dsw1(*this, "DSW1")
		, m_io_monitor(*this, "MONITOR")
		, m_io_j4(*this, "J4")
		, m_io_j5(*this, "J5")
		, m_screen(*this, "screen")
	{ }

	/* Interrupt controller */
	DECLARE_WRITE_LINE_MEMBER( pic_int_w );

	/* Parallel port */
	DECLARE_READ8_MEMBER(ppi_portb_r);
	DECLARE_WRITE8_MEMBER(ppi_portc_w);

	/* DMA controller */
	DECLARE_WRITE_LINE_MEMBER( hrq_w );
	DECLARE_WRITE_LINE_MEMBER( tc_w );
	DECLARE_WRITE8_MEMBER(dma_segment_w);
	DECLARE_READ8_MEMBER(dma_memory_read_byte);
	DECLARE_WRITE8_MEMBER(dma_memory_write_byte);
	DECLARE_READ8_MEMBER( io_dack0_r )  { uint8_t tmp = m_isabus->dack_r(0); LOGDMA("%s: %02x\n", FUNCNAME, tmp); return tmp; }
	DECLARE_READ8_MEMBER( io_dack1_r )  { uint8_t tmp = m_isabus->dack_r(1); LOGDMA("%s: %02x\n", FUNCNAME, tmp); return tmp; }
	DECLARE_READ8_MEMBER( io_dack2_r )  { uint8_t tmp = m_isabus->dack_r(2); LOGDMA("%s: %02x\n", FUNCNAME, tmp); return tmp; }
	DECLARE_READ8_MEMBER( io_dack3_r )  { uint8_t tmp = m_isabus->dack_r(3); LOGDMA("%s: %02x\n", FUNCNAME, tmp); return tmp; }
	DECLARE_WRITE8_MEMBER( io_dack0_w ) { LOGDMA("%s: %02x\n", FUNCNAME, data); m_isabus->dack_w(0,data); }
	DECLARE_WRITE8_MEMBER( io_dack1_w ) { LOGDMA("%s: %02x\n", FUNCNAME, data); m_isabus->dack_w(1,data); }
	DECLARE_WRITE8_MEMBER( io_dack2_w ) { LOGDMA("%s: %02x\n", FUNCNAME, data); m_isabus->dack_w(2,data); }
	DECLARE_WRITE8_MEMBER( io_dack3_w ) { LOGDMA("%s: %02x\n", FUNCNAME, data); m_isabus->dack_w(3,data); }
	DECLARE_WRITE_LINE_MEMBER( dack0_w ){ LOGDMA("%s: %d\n", FUNCNAME, state); select_dma_channel(0, state); }
	DECLARE_WRITE_LINE_MEMBER( dack1_w ){ LOGDMA("%s: %d\n", FUNCNAME, state); select_dma_channel(1, state); }
	DECLARE_WRITE_LINE_MEMBER( dack2_w ){ LOGDMA("%s: %d\n", FUNCNAME, state); select_dma_channel(2, state); }
	DECLARE_WRITE_LINE_MEMBER( dack3_w ){ LOGDMA("%s: %d\n", FUNCNAME, state); select_dma_channel(3, state); }

	/* Timer */
	DECLARE_WRITE_LINE_MEMBER( pit_out1_changed );

	/* Video controller */
	MC6845_UPDATE_ROW(crtc_update_row);

	/* ISA+ Expansion bus */
	DECLARE_WRITE_LINE_MEMBER( isa_irq5_w );
	DECLARE_WRITE_LINE_MEMBER( isa_irq7_w );

	/* Centronics  */
	DECLARE_WRITE_LINE_MEMBER (centronics_ack_w);
	DECLARE_WRITE_LINE_MEMBER (centronics_busy_w);
	DECLARE_WRITE_LINE_MEMBER (centronics_perror_w);
	DECLARE_WRITE_LINE_MEMBER (centronics_select_w);

	/* Keyboard */
	DECLARE_READ8_MEMBER(myb3k_kbd_r);
	void kbd_set_data_and_interrupt(u8 data);

	/* Video Controller */
	DECLARE_WRITE8_MEMBER(myb3k_video_mode_w);

	/* Monitor */
	DECLARE_INPUT_CHANGED_MEMBER(monitor_changed);

	/* Status bits */
	DECLARE_READ8_MEMBER(myb3k_io_status_r);

	void stepone(machine_config &config);
	void jb3000(machine_config &config);
	void myb3k(machine_config &config);

protected:
	virtual void machine_start() override;
	virtual void machine_reset() override;

private:
	/* Interrupt Controller */
	void pic_ir5_w(int source, int state);
	void pic_ir7_w(int source, int state);

	/* Jumper fields J4/J5 */
	enum
	{
		PPI_PC3  = 0x01,
		ISA_IRQ5 = 0x02,
		ISA_IRQ7 = 0x04,
		CENT_ACK = 0x08
	};

	/* Paralell port */
	enum
	{
		PC0_STROBE  = 0x01, // Printer interface
		PC1_SETPAGE = 0x02, // Graphics circuit
		PC2_DISPST  = 0x04, // Graphics circuit
		PC3_LPENB   = 0x08, // Lightpen enable
		PC4_CURSR   = 0x10, // Cursor Odd/Even
		PC5_BUZON   = 0x20, // Speaker On/Off
		PC6_CMTWRD  = 0x40,
		PC7_CMTEN   = 0x80  // Cassette or Speaker
	};

	/* DMA controller */
	void select_dma_channel(int channel, bool state);

	/* Timer */
	enum
	{
		TIMER_ID_KEY_INTERRUPT
	};
	void device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr) override;

	/* Status bits */
	enum
	{
		IOSTAT_BUSY  = 0x01,
		IOSTAT_NONE  = 0x02,
		IOSTAT_PRPE  = 0x04,
		IOSTAT_FAULT = 0x08
	};

	required_device<cpu_device> m_maincpu;
	required_device<ram_device> m_ram;
	required_device<pic8259_device> m_pic8259;
	required_device<pit8253_device> m_pit8253;
	required_device<i8255_device> m_ppi8255;
	required_device<i8257_device> m_dma8257;
	required_device<speaker_sound_device>   m_speaker;
	required_device<myb3k_keyboard_device> m_kb;
	required_device<h46505_device> m_crtc;
	required_shared_ptr<uint8_t> m_vram;
	required_device<isa8_device> m_isabus;
	optional_device<centronics_device> m_centronics;

	required_ioport m_io_dsw1;
	required_ioport m_io_monitor;
	required_ioport m_io_j4;
	required_ioport m_io_j5;
	required_device<screen_device> m_screen;

	int m_dma_channel;
	bool m_cur_tc;
	uint8_t m_kbd_data; // Data inside the 74LS164 serial to parallel converter.
	uint8_t m_vmode;
	rgb_t (*m_pal)[8];
	rgb_t m_cpal[8];
	rgb_t m_mpal[8];
	uint8_t m_portc;
	uint8_t m_dma_page[4]; // a 74670, 4 x 4 bit storage latch
	int8_t m_io_status;
};

READ8_MEMBER(myb3k_state::myb3k_io_status_r)
{
	LOGCENT("%s\n", FUNCNAME);
	return m_io_status & 0x0f;
}

READ8_MEMBER( myb3k_state::myb3k_kbd_r )
{
	LOGKBD("%s: %02x\n", FUNCNAME, m_kbd_data);

	/* IN from port 0x04 enables a 74LS244 buffer that
	   presents to the CPU the parallell bits from the 74LS164
	   serial to parallel converter.*/
	m_pic8259->ir1_w(CLEAR_LINE);
	return m_kbd_data;
}

void myb3k_state::kbd_set_data_and_interrupt(u8 data) {
	LOGKBD("%s: %02x\n", FUNCNAME, data);
	m_kbd_data = data;

	/* The INT7 line is pulled low when a clock is detected from the keyboard. */
	m_pic8259->ir1_w(CLEAR_LINE);

	/* When the clock stops, the INT7 line goes back high. This triggers the interrupt.
	   We simulate the time it takes to send the 8 bits over the serial line
	   here. It should be 0.8ms but is rounded off to 1ms. */
	timer_set(attotime::from_msec(1), TIMER_ID_KEY_INTERRUPT);
}

void myb3k_state::device_timer(emu_timer &timer, device_timer_id id, int param, void *ptr)
{
	switch (id)
	{
	case TIMER_ID_KEY_INTERRUPT:
		/* The serial transfer of 8 bits is complete. Now trigger INT7. */
		m_pic8259->ir1_w(ASSERT_LINE);
		break;
	}
}

MC6845_UPDATE_ROW( myb3k_state::crtc_update_row )
{
	/* The 6845 is not programmed to get 80 character modes or 400 pixels width but this is managed by external circuitry that selects
	   the apropriate pixelclock based on the video mode latch at i/o address 0x04. This callback always get x_count set to 40  */
	uint8_t row_length = (m_vmode & 0x02) || (m_vmode == 0)  ? x_count * 2 : x_count;
	for (int x_pos = 0; x_pos < row_length; x_pos++)
	{
		uint16_t page = (m_portc & PC1_SETPAGE) ? 0x8000 : 0;

		if ((m_portc & PC2_DISPST) == 0)
		{
			for (int pxl = 0; pxl < 8; pxl++)
			{
				bitmap.pix32(y, ( x_pos * 8) + pxl) = rgb_t::black();
			}
		}
		else
		{
			switch (m_vmode)
			{
			case 0:
				{
					uint32_t rowstart = (((x_pos + ma * 2) * 16 + ra) & 0x7fff) + page;
					uint32_t pdat  = ((m_vram[rowstart +  0]  & 0xff) << 16); // Green 8 bits
					pdat |= ((m_vram[rowstart +  8] & 0xf0) << 8);  // Red upper 4 bits
					pdat |= ((m_vram[rowstart +  8] & 0x0f) <<  4); // Blue upper 4 bits
					pdat |= ((m_vram[rowstart +  8] & 0xf0) <<  4); // Red lower 4 bits
					pdat |= ((m_vram[rowstart +  8] & 0x0f) <<  0); // Blue lower 4 bits
					for (int pxl = 0; pxl < 8; pxl++)
					{
						uint16_t pind = 0;
						/* Pixeldata for 8 pixels are stored in pdat as GGRRBB  */
						pind = (((pdat & (0x800000 >> pxl)) ? 0x04 : 0x00) |
							((pdat & (0x008000 >> pxl)) ? 0x02 : 0x00) |
							((pdat & (0x000080 >> pxl)) ? 0x01 : 0x00) );

						/* Check if we are in a cursor and create cursor if so */
						//pind ^= ((cursor_x != -1 && x_pos == cursor_x && ra == 7) ? 7 : 0);

						/* Create the grey scale */
						bitmap.pix32(y, ( x_pos * 8) + pxl) = (*m_pal)[pind & 0x07];
					}
				}
				break;
			case 1:
				{
					uint32_t pdat;
					// Crude fix of 36CH mode
					if ((m_io_dsw1->read() & 0x0c) == 0 && ra > 7)
					{
						// text mode only, 36x25 char, 8 color, discarding raster rows with what appears to be garbish
						// needs to be implemented LLE to verify the function of that data, possibly related to Kanji support
						pdat = 0x00; // This gets wrong when background color is not black
					}
					else
					{   // 320x200, 40x25 char, 8 color
						uint32_t rowstart = (((x_pos + ma) * 32 + ra) & 0x7fff) + page;
						pdat  = ((m_vram[rowstart +  0]  & 0xff) << 16); // Green 8 bits
						pdat |= ((m_vram[rowstart +  8]  & 0xf0) << 8);  // Red upper 4 bits
						pdat |= ((m_vram[rowstart +  8]  & 0x0f) <<  4); // Blue upper 4 bits
						pdat |= ((m_vram[rowstart +  24] & 0xf0) <<  4); // Red lower 4 bits
						pdat |= ((m_vram[rowstart +  24] & 0x0f) <<  0); // Blue lower 4 bits
						if (pdat != 0)
						{
							LOGPIX(" - PDAT:%06x from offset %04x RA=%d\n", pdat, rowstart + 0, ra);
						}
					}

					for (int pxl = 0; pxl < 8; pxl++)
					{
						uint16_t pind = 0;
						/* Pixeldata for 8 pixels are stored in pdat as GGRRBB  */
						pind = (((pdat & (0x800000 >> pxl)) ? 0x04 : 0x00) |
							((pdat & (0x008000 >> pxl)) ? 0x02 : 0x00) |
							((pdat & (0x000080 >> pxl)) ? 0x01 : 0x00) );

						/* Check if we are in a cursor and create cursor if so */
						pind ^= ((cursor_x != -1 && x_pos == cursor_x && ra == 7) ? 7 : 0);

						/* Pick up the color */
						bitmap.pix32(y, ( x_pos * 8) + pxl) = (*m_pal)[pind & 0x07];
					}
				}
				break;

			case 6: /* 640x400, 80 char, white on black -  fall through to case 2 if monochrome monitor
			       Mode 6 is an interlaced mode so should induce flicker on the color monitor but be
			       ok on the monochrome green monitor */
			case 2: /* 640x200, 80 char, white on black */
				{
					uint32_t rowstart = (((x_pos + ma * 2) * 16 + ra) & 0x7fff) + page;
					uint16_t pdat = m_vram[rowstart];
					//if (pdat16 != 0)
					LOGM2(" - PDAT:%06x from offset %04x RA=%d X:%d Y:%d DE:%d HBP:%d VBP:%d\n", pdat, (x_pos * 2 + ma * 2) * 16 + ra + page + 0, ra, x_pos, y, de, hbp, vbp);

					/* Check if we are in a cursor and create cursor if so */
					//pdat ^= ((cursor_x != -1 && x_pos/2 == cursor_x && ra == 7) ? 0xff : 0);

					for (int pxl = 0; pxl < 8; pxl++)
					{
						if ((pdat & (0x80 >> pxl)) != 0)
						{
							bitmap.pix32(y, ( x_pos * 8) + pxl) = (*m_pal)[0x07];
						}
						else
						{
							bitmap.pix32(y, ( x_pos * 8) + pxl) = rgb_t::black();
						}
					}
				}
				break;
			default:
				logerror("unimplemented video mode: %02x\n", m_vmode);
			}
		}
	}
}

/*
 * Setup of 6845 in different graphics modes from basica command line
 *  screen chars per row 36/40/80, rows 20/25, mode 0/1/2/3
 *
 *  chars 36  40  80  80  36  40  80  36  40
 *  rows  25  25  25  25  20  20  20  25  25
 *  mode   1   1   2   3   0   0   0   0   0 - as per basica "screen x,y,mode"
 *----------------------------------------------------------------------------
 *  R0    55  55  55  55  55  55  55  55  55 - Horizontal Total
 *  R1    40  40  40  40  40  40  40  40  40 - Horizontal Displayed
 *  R2    44  44  44  44  44  44  44  44  44 - Horizontal Sync Position
 *  R3   132 132 132  52 132 132 132  52  52 - Sync Width
 *  R4    31  31  31  26  24  24  24  26  26 - Vertical Total
 *  R5     0   0   0   7   9   9   9   7   7 - Vertical Total Adjust
 *  R6    25  25  25  25  20  20  20  25  25 - Vertical Displayed
 *  R7    27  27  27  25  22  22  22  25  25 - Vertical Sync Position
 *  R8     0   0   0   3   0   0   0   3   3 - Interlace & Skew
 *  R9     7   7   7  14   9   9   9  14  14 - Maximum Raster Address
 *  R10   96  96  96  96  96  96  96  96  96 - Cursor Start Address
 *  R11    7   7   7  15   9   9   9  15  15 - Cursor End Address
 *  R12    0   0   0   0   0   0   0   0   0 - Start Address (H)
 *  R13    0   0   0   0   0   0   0   0   0 - Start Address (L)
 * ---------------------------------------------------------------------------
 *  vmode  1   1   2   6   1   1   2   5   5 - as per 3 bits latch at I/O 0x04
 *  xres 320 320 640 640 320 320 640 320 320
 *  yres 200 200 200 400 200 200 200 400 400
 *  char  40  40  80  80  40  40  80  40  40
 */

WRITE8_MEMBER( myb3k_state::myb3k_video_mode_w )
{
	LOG("%s: %02x\n", FUNCNAME, data);
	LOGVMOD("Video Mode %02x\n", data);

	/* ---- -x-- interlace mode */
	/* ---- --xx horizontal step count (number of offsets of vram RAM data to skip, 64 >> n) */

	m_vmode = data;
	switch (data & 7)
	{
	case 0: // Disambiguity between reality and the service manual. Reality is 640x200 in 8 color or tones!
			{
			LOGVMOD(" - 640x200 on 80x25  \n");
			rectangle rect(0, 640 - 1, 0, 200 - 1);
			m_screen->configure(640, 200, rect, HZ_TO_ATTOSECONDS(50));
			break;
		}

	case 1: /* 320x200 */
		{
			LOGVMOD(" - 320x200, 40 char, 8 color or 8 tones of green...\n");
			rectangle rect(0, 320 - 1, 0, 200 - 1);
			m_screen->configure(320, 200, rect, HZ_TO_ATTOSECONDS(50));
		}
		break;

	case 2: /* 640x200 - boots up in this mode */
		{
			LOGVMOD(" - 640x200, 80 char, white on black...\n");
			rectangle rect(0, 640 - 1, 0, 200 - 1);
			m_screen->configure(640, 200, rect, HZ_TO_ATTOSECONDS(50));
		}
		break;

	case 3: /* Fail  */
		LOGVMOD(" - bad mode...\n");
		break;

	case 4: /* Fail  */
		LOGVMOD(" - bad mode...\n");
		break;

	case 5: /* 320x400 */
		{
			LOGVMOD("320x400, 40 char, white on black\n");
			rectangle rect(0, 320 - 1, 0, 400 - 1);
			m_screen->configure(320, 400, rect, HZ_TO_ATTOSECONDS(50));
		}
		break;

	case 6: /* 640x400 */
		{
			LOGVMOD("640x400, 80 char, white on black\n");
			rectangle rect(0, 640 - 1, 0, 400 - 1);
			m_screen->configure(640, 400, rect, HZ_TO_ATTOSECONDS(50));
		}
		break;

	case 7: /* Fail  */
		LOGVMOD(" - bad mode...\n");
		break;
	default: logerror("Wrong Video Mode %d, contact the maintainer\n", data);
	}
}

/**********************************************************
 *
 * PPI8255 interface
 *
 *
 * PORT A (output)
 *
 *  Printer data lines
 *
 * PORT B (input)
 *
 *  SW1
 *
 * PORT C
 * 0 - PC0 - STROBE       - Printer handshake
 * 1 - PC1 - SET PAGE     - Video RAM page
 * 2 - PC2 - DISP ST      -
 * 3 - PC3 - LPSTB        - Light Pen
 * 4 - PC4 - CURS ODD/EVN -
 * 5 - PC5 - BUZON        - ON-OFF of speaker circuit
 * 6 - PC6 - CMTWD
 * 7 - PC7 - CMTEN        - Separation from cassette interface
 *
 * Mybrain 3000, JB-3000, Step/One SW1: (Service manual numbers switches 1-8)
 * 0   - Unused
 * 1   - Unused
 * 2+3 - Display Mode
 *       OFF OFF - 36 Char/line, 10 raster
 *       ON  OFF - 40 Char/line,  8 raster
 *       OFF ON  - 80 Char/line, 16 raster
 *       ON  ON  - 80 Char/line,  8 raster
 * 4   - Expansion Unit
 *       OFF     - None
 *       ON      - Attached
 * 5+6 - Boot drive
 *       OFF OFF - A:
 *       ON  OFF - B:
 *       OFF ON  - C:
 *       ON  ON  - D:
 * 7   - Boot drive type
 *       OFF     - 5.25 inch Flexible Disk Drive
 *       ON      - 8 inch Flexible Disk Unit
 *
 * Mybrain 3000, JB-3000, Step/One SW2:  (Service manual numbers switches 1-8)
 * 0   - Check Mode (BIOS version info on boot screen)
 *       OFF     - Yes
 *       ON      - No
 * 1-3 - Reserved
 * 4   - Always off (yes so service manual says)
 * 5   - Number of disk drives
 *       OFF     - 2 drives
 *       ON      - 1 drive
 * 6-7   Initial Setting of "basic RS232 Adapter", read by software
 *       OFF OFF - ?
 *       ON  OFF - ?
 *       OFF ON  - ?
 *       ON  ON  - ?
 **********************************************************/
static ADDRESS_MAP_START(myb3k_map, AS_PROGRAM, 8, myb3k_state)
	ADDRESS_MAP_UNMAP_HIGH
	//AM_RANGE(0x00000,0x3ffff) AM_RAM // It's either 128Kb or 256Kb RAM installed by machine_start()
	AM_RANGE(0x40000,0x7ffff) AM_NOP
	AM_RANGE(0x80000,0xcffff) AM_NOP // Expansion Unit connected through an ISA8 cable
	AM_RANGE(0xd0000,0xeffff) AM_RAM AM_SHARE("vram")  // Area 6, physical at 30000-3FFFF (128Kb RAM) or 10000-1FFFF (256KB RAM)
	AM_RANGE(0xf0000,0xfffff) AM_ROM AM_REGION("ipl", 0) // Area 7, 8 x 8Kb
ADDRESS_MAP_END

static ADDRESS_MAP_START(myb3k_io, AS_IO, 8, myb3k_state)
	ADDRESS_MAP_UNMAP_LOW
	/* Main Unit 0-0x7ff */
	// 0-3 8255A PPI parallel port
	AM_RANGE(0x00, 0x03) AM_DEVREADWRITE("ppi", i8255_device, read, write)

	// Discrete latches
	AM_RANGE(0x04, 0x04) AM_READ(myb3k_kbd_r)
	AM_RANGE(0x04, 0x04) AM_WRITE(myb3k_video_mode_w) // b0=40CH, b1=80CH, b2=16 raster
	AM_RANGE(0x05, 0x05) AM_READ(myb3k_io_status_r) // printer connector bits: b3=fault b2=paper out b1 or b0=busy
	AM_RANGE(0x05, 0x05) AM_WRITE(dma_segment_w) // b0-b3=addr, b6=A b7=B
	AM_RANGE(0x06, 0x06) AM_READ_PORT("DSW2")

	// 8-9 8259A interrupt controller
	AM_RANGE(0x08, 0x09) AM_DEVREADWRITE("pic", pic8259_device, read, write)

	// c-f 8253 PIT timer
	AM_RANGE(0x0c, 0x0f) AM_DEVREADWRITE("pit", pit8253_device, read, write)

	// 10-18 8257 DMA
	AM_RANGE(0x10, 0x18) AM_DEVREADWRITE("dma", i8257_device, read, write)

	// 1c-1d HD46505S CRTC
	AM_RANGE(0x1c, 0x1c) AM_DEVREADWRITE("crtc", h46505_device, status_r, address_w)
	AM_RANGE(0x1d, 0x1d) AM_DEVREADWRITE("crtc", h46505_device, register_r, register_w)

	/* Expansion Unit 0x800 - 0xfff */
ADDRESS_MAP_END

/* Input ports - from Step/One service manual */
static INPUT_PORTS_START( myb3k )
	PORT_START("J4")
	PORT_CONFNAME( 0x03, 0x01, "IRQ5 source")
	PORT_CONFSETTING(    0x01, "ISA IRQ5")
	PORT_CONFSETTING(    0x02, "PPI PC3 (Light Pen)")

	PORT_START("J5")
	PORT_CONFNAME( 0x0c, 0x08, "IRQ7 source")
	PORT_CONFSETTING(    0x04, "ISA IRQ7")
	PORT_CONFSETTING(    0x08, "Centronics Ack")

	PORT_START("MONITOR")
	PORT_CONFNAME( 0x01, 0x00, "Monitor") PORT_CHANGED_MEMBER(DEVICE_SELF, myb3k_state, monitor_changed, 0)
	PORT_CONFSETTING(    0x00, "Color")
	PORT_CONFSETTING(    0x01, "Monochrome")

	PORT_START("DSW1")
	PORT_DIPUNUSED_DIPLOC(0x01, 0x01, "SW1:1")
	PORT_DIPUNUSED_DIPLOC(0x02, 0x02, "SW1:2")
	PORT_DIPNAME( 0x0c, 0x00, "Display Mode") PORT_DIPLOCATION("SW1:3,4")
	PORT_DIPSETTING(    0x0c, "80CH 8 raster" )
	PORT_DIPSETTING(    0x04, "80CH 16 raster" )
	PORT_DIPSETTING(    0x08, "40CH 8 raster" )
	PORT_DIPSETTING(    0x00, "36CH 10 raster" )
	PORT_DIPNAME( 0x10, 0x10, "Expansion Unit" ) PORT_DIPLOCATION("SW1:5")
	PORT_DIPSETTING(    0x00, "Attached" )
	PORT_DIPSETTING(    0x10, "None" )
	PORT_DIPNAME( 0x60, 0x60, "Flexible Disk Drive for boot" )  PORT_DIPLOCATION("SW1:6,7")
	PORT_DIPSETTING(    0x60, "Drive A" )
	PORT_DIPSETTING(    0x20, "Drive B" )
	PORT_DIPSETTING(    0x40, "Drive C" )
	PORT_DIPSETTING(    0x00, "Drive D" )
	PORT_DIPNAME( 0x80, 0x80, "Flexible Disk Drive type for boot" )  PORT_DIPLOCATION("SW1:8")
	PORT_DIPSETTING(    0x00, "8-inch Flexible Disk Unit" )     // 0x520-0x524 range
	PORT_DIPSETTING(    0x80, "5.25-inch Flexible Disk Drive" ) // 0x20-0x24 range

	PORT_START("DSW2")
	PORT_DIPNAME( 0x01, 0x00, "Check mode" )  PORT_DIPLOCATION("SW2:1") // ROM information
	PORT_DIPSETTING(    0x01, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPUNUSED_DIPLOC(0x02, 0x02, "SW2:2")
	PORT_DIPUNUSED_DIPLOC(0x04, 0x04, "SW2:3")
	PORT_DIPUNUSED_DIPLOC(0x08, 0x08, "SW2:4")
	PORT_DIPNAME( 0x10, 0x10, "Always off" ) PORT_DIPLOCATION("SW2:5") // Some factory magic maybe?
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Number of disk drive(s)" ) PORT_DIPLOCATION("SW2:6")
	PORT_DIPSETTING(    0x20, "2 drives" )
	PORT_DIPSETTING(    0x00, "1 drive" )
	PORT_DIPNAME( 0xc0, 0xc0, "Initial setting of basic RS-232C Adapter" ) PORT_DIPLOCATION("SW2:7,8")
	PORT_DIPSETTING(    0xc0, "00" )
	PORT_DIPSETTING(    0x80, "01" )
	PORT_DIPSETTING(    0x40, "10" )
	PORT_DIPSETTING(    0x00, "11" )
INPUT_PORTS_END

INPUT_CHANGED_MEMBER(myb3k_state::monitor_changed)
{
	if ((m_io_monitor->read() & 1) == 1)
	{
		m_pal = &m_mpal;
	}
	else
	{
		m_pal = &m_cpal;
	}
}

void myb3k_state::machine_start()
{
	LOG("%s\n", FUNCNAME);

	/* Color palette for use with a RGB color CRT monitor such as the 12" Ericsson DU4720:
	   76 degrees deflection, WxHxD: 373x375x428mm, Weight 12.8 Kg, 215x134.4mm display area,
	   RGB signal with separate syncs */
	m_cpal[0] = rgb_t(  0,   0,   0); // black   0.29v
	m_cpal[1] = rgb_t(  0,   0, 255); // blue    0.52v
	m_cpal[2] = rgb_t(255,   0,   0); // red     0.58v
	m_cpal[3] = rgb_t(255,   0, 255); // magenta 0.63v
	m_cpal[4] = rgb_t(  0, 255,   0); // green   0.71v
	m_cpal[5] = rgb_t(  0, 255, 255); // cyan    0.80v
	m_cpal[6] = rgb_t(255, 255,   0); // yellow  0.90v
	m_cpal[7] = rgb_t(255, 255, 255); // white   1.04v

	/* Monochrome offset based on the voltage levels from the Service Manual, relation between colors anc voltage might
	   not be linear though so may need visual tweaking for a Green P39 phospor CRT such as the 12" Ericsson DU4721
	   90 degrees deflection, WxHxD: 373x370x351mm, Weight 9.3 Kg, 215x134.4mm display area, Composite video signal */
	m_mpal[0] = rgb_t((uint8_t)(( (0.29 - 0.29) / (1.04 - 0.29) ) * 74), (uint8_t)(( (0.29 - 0.29) / (1.04 - 0.29) ) * 255), 0); // 0.29v
	m_mpal[1] = rgb_t((uint8_t)(( (0.52 - 0.29) / (1.04 - 0.29) ) * 74), (uint8_t)(( (0.52 - 0.29) / (1.04 - 0.29) ) * 255), 0); // 0.52v
	m_mpal[2] = rgb_t((uint8_t)(( (0.58 - 0.29) / (1.04 - 0.29) ) * 74), (uint8_t)(( (0.58 - 0.29) / (1.04 - 0.29) ) * 255), 0); // 0.58v
	m_mpal[3] = rgb_t((uint8_t)(( (0.63 - 0.29) / (1.04 - 0.29) ) * 74), (uint8_t)(( (0.63 - 0.29) / (1.04 - 0.29) ) * 255), 0); // 0.63v
	m_mpal[4] = rgb_t((uint8_t)(( (0.71 - 0.29) / (1.04 - 0.29) ) * 74), (uint8_t)(( (0.71 - 0.29) / (1.04 - 0.29) ) * 255), 0); // 0.71v
	m_mpal[5] = rgb_t((uint8_t)(( (0.80 - 0.29) / (1.04 - 0.29) ) * 74), (uint8_t)(( (0.80 - 0.29) / (1.04 - 0.29) ) * 255), 0); // 0.80v
	m_mpal[6] = rgb_t((uint8_t)(( (0.90 - 0.29) / (1.04 - 0.29) ) * 74), (uint8_t)(( (0.90 - 0.29) / (1.04 - 0.29) ) * 255), 0); // 0.90v
	m_mpal[7] = rgb_t((uint8_t)(( (1.04 - 0.29) / (1.04 - 0.29) ) * 74), (uint8_t)(( (1.04 - 0.29) / (1.04 - 0.29) ) * 255), 0); // 1.04v

	/* Init a default palette */
	m_pal = &m_mpal; // In case screen starts rendering before machine_reset where we read the settings

	/* CPU can only access RAM 50% of the time and the CRTC the other 50%. This waitstate workaround gives
	   close enough performance of the DOS 1.25 "basica demo" compared to the real hardware */
	m_maincpu->set_clock_scale(0.5f);

	/* Setup ram - NOTE; video RAM is really a mirror of last 64KB of main memory and not the opposite as indicated below. */
	if (m_ram->size() <= (1024 * 128))
	{
		LOGRAM("128KB System\n");
		m_maincpu->space(AS_PROGRAM).install_ram(0, 0xffff, m_ram->pointer()); // Install first 64KB of main memory
		m_maincpu->space(AS_PROGRAM).install_ram(0x10000, 0x1ffff, m_vram);    // Install mirror of video RAM as the last 64Kb of main memory
	}
	else
	{
		LOGRAM("256KB System\n");
		m_maincpu->space(AS_PROGRAM).install_ram(0, 0x2ffff, m_ram->pointer()); // Install 192KB (128KB Storage Module + first 64KB of main memory)
		m_maincpu->space(AS_PROGRAM).install_ram(0x30000, 0x3ffff, m_vram);     // Install mirror of video RAM as the last 64KB of main memory
	}

	/* No key presses allowed yet */
	m_kbd_data = 0;

	save_item (NAME (m_dma_channel));
	save_item (NAME (m_cur_tc));
	save_item (NAME (m_kbd_data));
	save_item (NAME (m_vmode));
	save_item (NAME (m_portc));
	save_item (NAME (*m_pal));
	save_pointer(NAME(m_mpal), sizeof(m_mpal));
	save_pointer(NAME(m_cpal), sizeof(m_cpal));
	save_pointer(NAME(m_dma_page), sizeof(m_dma_page));
	save_item (NAME (m_io_status));
}

void myb3k_state::machine_reset()
{
	LOG("%s\n", FUNCNAME);
	m_cur_tc = false;
	m_dma_channel = -1;
	m_vmode = 0;
	m_portc = 0;
	memset(m_dma_page, 0, sizeof(m_dma_page));
	m_pal = (m_io_monitor-> read() & 1) == 1 ? &m_mpal : &m_cpal;
}

void myb3k_state::select_dma_channel(int channel, bool state)
{
	LOGDMA("%s: %d:%d\n", FUNCNAME, channel, state);
	if(!state) {
		m_dma_channel = channel;
		if(!m_cur_tc)
			m_isabus->eop_w(channel, ASSERT_LINE );

	} else if(m_dma_channel == channel) {
		m_dma_channel = -1;
		if(m_cur_tc)
			m_isabus->eop_w(channel, CLEAR_LINE );
	}
}

WRITE_LINE_MEMBER( myb3k_state::tc_w )
{
	LOGDMA("%s: %d\n", FUNCNAME, state);
	if(m_dma_channel != -1 && (state == ASSERT_LINE) != m_cur_tc)
		m_isabus->eop_w(m_dma_channel, m_cur_tc ? ASSERT_LINE : CLEAR_LINE );
	m_cur_tc = state == ASSERT_LINE;
}

WRITE_LINE_MEMBER(myb3k_state::pic_int_w)
{
	LOGPIC("%s: %d\n", FUNCNAME, state);
	m_maincpu->set_input_line(0, state);
}

/* pic_ir5_w - select interrupt source depending on jumper J4 setting, either ISA IRQ5 or PPI PC3 (Light Pen) */
void myb3k_state::pic_ir5_w(int source, int state)
{
	LOGPIC("%s: %d\n", FUNCNAME, state);
	if (!machine().paused() && (source & m_io_j4->read()))
		m_pic8259->ir5_w(state);
}

/* pic_ir7_w - select interrupt source depending on jumper J5 setting, either ISA IRQ7 or Centronics Ack */
void myb3k_state::pic_ir7_w(int source, int state)
{
	LOGPIC("%s: %d\n", FUNCNAME, state);
	if (!machine().paused() && (source & m_io_j5->read()))
		m_pic8259->ir7_w(state);
}

WRITE_LINE_MEMBER( myb3k_state::pit_out1_changed )
{
	LOGPIT("%s: %d\n", FUNCNAME, state);
	m_speaker->level_w(state ? 1 : 0);
}

WRITE8_MEMBER(myb3k_state::dma_segment_w)
{
	LOGDMA("%s: %02x\n", FUNCNAME, data);
	m_dma_page[(data >> 6) & 3] = (data & 0x0f);
}

WRITE_LINE_MEMBER(myb3k_state::hrq_w)
{
	LOGDMA("%s: %d\n", FUNCNAME, state);

	// Should connect to hold input clocked by DMA clock but hold isn't emulated
	m_maincpu->set_input_line(INPUT_LINE_HALT, state);

	// Should be called from CPU clocked by the DMA clock but hlda output isn't emulated
	m_dma8257->hlda_w(state);
}

READ8_MEMBER(myb3k_state::dma_memory_read_byte)
{
	assert(m_dma_channel != -1);

	address_space& prog_space = m_maincpu->space(AS_PROGRAM);
	uint8_t tmp = prog_space.read_byte(offset | m_dma_page[m_dma_channel & 3] << 16);
	LOGDMA("%s: %x:%04x => %02x\n", FUNCNAME, m_dma_channel, offset, tmp);

	return tmp;
}

WRITE8_MEMBER(myb3k_state::dma_memory_write_byte)
{
	assert(m_dma_channel != -1);

	address_space& prog_space = m_maincpu->space(AS_PROGRAM);
	LOGDMA("%s: %x:%04x <= %02x\n", FUNCNAME, m_dma_channel, offset, data);

	return prog_space.write_byte(offset |  m_dma_page[m_dma_channel & 3] << 16, data);
}

READ8_MEMBER( myb3k_state::ppi_portb_r )
{
	LOGPPI("%s\n", FUNCNAME);

	return m_io_dsw1->read();
}

WRITE8_MEMBER( myb3k_state::ppi_portc_w )
{
	LOGPPI("%s: %02x\n", FUNCNAME, data);
	LOGPPI(" - STROBE : %d\n", (data & PC0_STROBE)  ? 1 : 0);
	LOGPPI(" - SETPAGE: %d\n", (data & PC1_SETPAGE) ? 1 : 0);
	LOGPPI(" - DISPST : %d\n", (data & PC2_DISPST)  ? 1 : 0);
	LOGPPI(" - LPENB  : %d\n", (data & PC3_LPENB)   ? 1 : 0);
	LOGPPI(" - CURSR  : %d\n", (data & PC4_CURSR)   ? 1 : 0);
	LOGPPI(" - BUZON  : %d\n", (data & PC5_BUZON)   ? 1 : 0);
	LOGPPI(" - CMTWRD : %d\n", (data & PC6_CMTWRD)  ? 1 : 0);
	LOGPPI(" - CMTEN  : %d\n", (data & PC7_CMTEN)   ? 1 : 0);
	LOGPPI(" => CMTEN: %d BUZON: %d\n", (data & PC7_CMTEN) ? 1 : 0, (data & PC5_BUZON)? 1 : 0);

	/* Centronics strobe signal */
	LOGCENT("Centronics strobe %d\n", (data & PC0_STROBE) ? 1 : 0);
	m_centronics->write_strobe((data & PC0_STROBE) ? 1 : 0);

	/*
	 * The actual logic around enabling the buzzer is a bit more complicated involving the cassette interface
	 * According to the schematics gate1 is enabled if either
	 *  (CMTEN is inactive high and BUZON active high) OR
	 *  (CMTEN is active   low  and CMTRD is inactive high)
	 * and CMTRD is low). Problem is that the schematics fails to show where CMTRD comes from so only the first case is emulated
	 */
	m_pit8253->write_gate1(!(data & PC5_BUZON) && (data & PC7_CMTEN)? 1 : 0);

	/* Update light pen interrupt */
	pic_ir5_w(PPI_PC3, (data & PC3_LPENB) ? 1 : 0);

	m_portc = data;

	return;
}

/* ISA IRQ5 handler */
WRITE_LINE_MEMBER (myb3k_state::isa_irq5_w)
{
		LOGCENT("%s %d\n", FUNCNAME, state);
		pic_ir5_w(ISA_IRQ5, state);
}

/* ISA IRQ7 handler */
WRITE_LINE_MEMBER (myb3k_state::isa_irq7_w)
{
		LOGCENT("%s %d\n", FUNCNAME, state);
		pic_ir7_w(ISA_IRQ7, state);
}

/* Centronics ACK handler */
WRITE_LINE_MEMBER (myb3k_state::centronics_ack_w)
{
		LOGCENT("%s %d\n", FUNCNAME, state);
		pic_ir7_w(CENT_ACK, state);
}

/* Centronics BUSY handler
 * The busy line is enterring the schematics from the connector but is lost to its way to the status latch
 * but there is only two possibilities, either D0 or D1  */
WRITE_LINE_MEMBER (myb3k_state::centronics_busy_w){
	LOGCENT("%s %d\n", FUNCNAME, state);
	if (state == ASSERT_LINE)
		m_io_status &= ~IOSTAT_BUSY;
	else
		m_io_status |= IOSTAT_BUSY;
}

/* Centronics PERROR handler */
WRITE_LINE_MEMBER (myb3k_state::centronics_perror_w){
	LOGCENT("%s %d\n", FUNCNAME, state);
	if (state == ASSERT_LINE)
		m_io_status &= ~IOSTAT_FAULT;
	else
		m_io_status |= IOSTAT_FAULT;
}

/* Centronics SELECT handler - The centronics select signal is not used by this hardware */
WRITE_LINE_MEMBER (myb3k_state::centronics_select_w){
		LOGCENT("%s %d - not used by machine\n", FUNCNAME, state);
}

static SLOT_INTERFACE_START(stepone_isa_cards)
	SLOT_INTERFACE("myb3k_com", ISA8_MYB3K_COM)
	SLOT_INTERFACE("myb3k_fdc4710", ISA8_MYB3K_FDC4710)
	SLOT_INTERFACE("myb3k_fdc4711", ISA8_MYB3K_FDC4711)
SLOT_INTERFACE_END

MACHINE_CONFIG_START(myb3k_state::myb3k)
	/* basic machine hardware */
	MCFG_CPU_ADD("maincpu", I8088, XTAL(14'318'181) / 3) /* 14.3182 main crystal divided by three through a 8284A */
	MCFG_CPU_PROGRAM_MAP(myb3k_map)
	MCFG_CPU_IO_MAP(myb3k_io)
	MCFG_CPU_IRQ_ACKNOWLEDGE_DEVICE("pic", pic8259_device, inta_cb)

	/* RAM options */
	MCFG_RAM_ADD(RAM_TAG)
	MCFG_RAM_DEFAULT_SIZE("256K")
	MCFG_RAM_EXTRA_OPTIONS("128K, 256K")

	/* Interrupt controller */
	MCFG_DEVICE_ADD("pic", PIC8259, 0)
	MCFG_PIC8259_OUT_INT_CB(WRITELINE(myb3k_state, pic_int_w))

	/* Parallel port */
	MCFG_DEVICE_ADD("ppi", I8255A, 0)
	MCFG_I8255_OUT_PORTA_CB(DEVWRITE8("cent_data_out", output_latch_device, write))
	MCFG_I8255_IN_PORTB_CB(READ8(myb3k_state, ppi_portb_r))
	MCFG_I8255_OUT_PORTC_CB(WRITE8(myb3k_state, ppi_portc_w))

	/* DMA controller */
	MCFG_DEVICE_ADD("dma", I8257, XTAL(14'318'181) / 6)
	MCFG_I8257_OUT_HRQ_CB(WRITELINE(myb3k_state, hrq_w))
	MCFG_I8257_OUT_TC_CB(WRITELINE(myb3k_state, tc_w))
	MCFG_I8257_IN_MEMR_CB(READ8(myb3k_state, dma_memory_read_byte))
	MCFG_I8257_OUT_MEMW_CB(WRITE8(myb3k_state, dma_memory_write_byte))
	MCFG_I8257_IN_IOR_0_CB(READ8(myb3k_state, io_dack0_r))
	MCFG_I8257_IN_IOR_1_CB(READ8(myb3k_state, io_dack1_r))
	MCFG_I8257_IN_IOR_2_CB(READ8(myb3k_state, io_dack2_r))
	MCFG_I8257_IN_IOR_3_CB(READ8(myb3k_state, io_dack3_r))
	MCFG_I8257_OUT_IOW_0_CB(WRITE8(myb3k_state, io_dack0_w))
	MCFG_I8257_OUT_IOW_1_CB(WRITE8(myb3k_state, io_dack1_w))
	MCFG_I8257_OUT_IOW_2_CB(WRITE8(myb3k_state, io_dack2_w))
	MCFG_I8257_OUT_IOW_3_CB(WRITE8(myb3k_state, io_dack3_w))
	MCFG_I8257_OUT_DACK_0_CB(WRITELINE(myb3k_state, dack0_w))
	MCFG_I8257_OUT_DACK_1_CB(WRITELINE(myb3k_state, dack1_w))
	MCFG_I8257_OUT_DACK_2_CB(WRITELINE(myb3k_state, dack2_w))
	MCFG_I8257_OUT_DACK_3_CB(WRITELINE(myb3k_state, dack3_w))

	/* Timer */
	MCFG_DEVICE_ADD("pit", PIT8253, 0)
	MCFG_PIT8253_CLK0(XTAL(14'318'181) / 12.0) /* TIMINT straight into IRQ0 */
	MCFG_PIT8253_OUT0_HANDLER(DEVWRITELINE("pic", pic8259_device, ir0_w))
	MCFG_PIT8253_CLK1(XTAL(14'318'181) / 12.0) /* speaker if port c bit 5 is low */
	MCFG_PIT8253_OUT1_HANDLER(WRITELINE(myb3k_state, pit_out1_changed))
	//  MCFG_PIT8253_CLK2(XTAL(14'318'181) / 12.0) /* ANDed with port c bit 6 but marked as "not use"*/
	//  MCFG_PIT8253_OUT2_HANDLER(WRITELINE(myb3k_state, pit_out2_changed))

	/* Video controller */
	MCFG_MC6845_ADD("crtc", H46505, "screen", XTAL(14'318'181) / 16) /* Main crystal divided by 16 through a 74163 4 bit counter */
	MCFG_MC6845_SHOW_BORDER_AREA(false)
	MCFG_MC6845_CHAR_WIDTH(8)
	MCFG_MC6845_UPDATE_ROW_CB(myb3k_state, crtc_update_row)

	/* ISA8+ Expansion bus */
	MCFG_DEVICE_ADD("isa", ISA8, 0)
	MCFG_ISA8_CPU(":maincpu")
	MCFG_ISA_OUT_IRQ2_CB(DEVWRITELINE("pic", pic8259_device, ir2_w))
	MCFG_ISA_OUT_IRQ3_CB(DEVWRITELINE("pic", pic8259_device, ir3_w))
	MCFG_ISA_OUT_IRQ4_CB(DEVWRITELINE("pic", pic8259_device, ir4_w))
	MCFG_ISA_OUT_IRQ5_CB(WRITELINE(myb3k_state, isa_irq5_w)) // Jumper J4 selectable
	MCFG_ISA_OUT_IRQ6_CB(DEVWRITELINE("pic", pic8259_device, ir6_w))
	MCFG_ISA_OUT_IRQ7_CB(WRITELINE(myb3k_state, isa_irq7_w)) // Jumper J5 selectable
	//MCFG_ISA_OUT_DRQ0_CB(DEVWRITELINE("dma", i8257_device, dreq0_w)) // Part of ISA16 but not ISA8 standard but implemented on ISA8 B8 (SRDY) on this motherboard
	MCFG_ISA_OUT_DRQ1_CB(DEVWRITELINE("dma", i8257_device, dreq1_w))
	MCFG_ISA_OUT_DRQ2_CB(DEVWRITELINE("dma", i8257_device, dreq2_w))
	MCFG_ISA_OUT_DRQ3_CB(DEVWRITELINE("dma", i8257_device, dreq3_w))
	MCFG_ISA8_SLOT_ADD("isa", "isa1", stepone_isa_cards, "myb3k_fdc4711", false)
	MCFG_ISA8_SLOT_ADD("isa", "isa2", stepone_isa_cards, "myb3k_com", false)
	MCFG_ISA8_SLOT_ADD("isa", "isa3", stepone_isa_cards, nullptr, false)

	/* Centronics */
	MCFG_CENTRONICS_ADD ("centronics", centronics_devices, "printer")
	MCFG_CENTRONICS_ACK_HANDLER (WRITELINE (myb3k_state, centronics_ack_w))
	MCFG_CENTRONICS_BUSY_HANDLER (WRITELINE (myb3k_state, centronics_busy_w))
	MCFG_CENTRONICS_PERROR_HANDLER (WRITELINE (myb3k_state, centronics_perror_w))
	MCFG_CENTRONICS_SELECT_HANDLER (WRITELINE (myb3k_state, centronics_select_w))
	MCFG_CENTRONICS_OUTPUT_LATCH_ADD ("cent_data_out", "centronics")

	/* Sound */
	MCFG_SPEAKER_STANDARD_MONO("mono")
	MCFG_SOUND_ADD("speaker", SPEAKER_SOUND, 0)
	MCFG_SOUND_ROUTE(ALL_OUTPUTS, "mono", 1.00)

	/* Keyboard */
	MCFG_DEVICE_ADD("myb3k_keyboard", MYB3K_KEYBOARD, 0)
	MCFG_MYB3K_KEYBOARD_CB(PUT(myb3k_state, kbd_set_data_and_interrupt))

	/* Monitor */
	MCFG_SCREEN_ADD("screen", RASTER)
	MCFG_SCREEN_RAW_PARAMS(XTAL(14'318'181) / 3, 600, 0, 600, 400, 0, 400)
	MCFG_SCREEN_UPDATE_DEVICE("crtc", h46505_device, screen_update)
MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(myb3k_state::jb3000, myb3k)
	/* Keyboard */
	MCFG_DEVICE_REPLACE("myb3k_keyboard", JB3000_KEYBOARD, 0)
	MCFG_MYB3K_KEYBOARD_CB(PUT(myb3k_state, kbd_set_data_and_interrupt))

MACHINE_CONFIG_END

MACHINE_CONFIG_DERIVED(myb3k_state::stepone, myb3k)
  /* Keyboard */
	MCFG_DEVICE_REPLACE("myb3k_keyboard", STEPONE_KEYBOARD, 0)
	MCFG_MYB3K_KEYBOARD_CB(PUT(myb3k_state, kbd_set_data_and_interrupt))

	/* software lists */
	MCFG_SOFTWARE_LIST_ADD("stepone_flop_list", "stepone_flop")
MACHINE_CONFIG_END

/* ROM definitions, ROM area is 8 x 8Kb and can be populated with 2732 mask ROMs or 2764s */
ROM_START( myb3k )
	ROM_REGION( 0x10000, "ipl", ROMREGION_ERASEFF )
	ROM_LOAD( "myb3kchrg-v2.07.bin", 0xc000, 0x2000, CRC(1e43e1e3) SHA1(956b580c9cbcaf2c5ff74e3ef80a5ac98c2df434))
	ROM_LOAD( "myb3kbios-v2.07.bin", 0xe000, 0x2000, CRC(c4c46cc5) SHA1(a3e186513fbe9ad0e369b481999393a3506db39e))
ROM_END

ROM_START( jb3000 )
	ROM_REGION( 0x10000, "ipl", ROMREGION_ERASEFF )
	ROM_LOAD( "jb3000chrg-v2.07.bin", 0xc000, 0x2000, NO_DUMP)
	ROM_LOAD( "jb3000bios-v2.07.bin", 0xe000, 0x2000, NO_DUMP)
ROM_END

ROM_START( stepone )
	ROM_REGION( 0x10000, "ipl", ROMREGION_ERASEFF )
	ROM_LOAD( "steponechrg-v2.07.bin", 0xc000, 0x2000, CRC(8284a391) SHA1(7203c5e9d83be37c1c195946fbee5c53b1bce391))
	ROM_LOAD( "steponebios-v2.07.bin", 0xe000, 0x2000, CRC(322c1618) SHA1(a7a3cc2af7cc9556007d98014714ba656f6e79d1))
ROM_END

//    YEAR  NAME     PARENT  COMPAT   MACHINE    INPUT  STATE           INIT   COMPANY        FULLNAME        FLAGS
COMP( 1982, myb3k,   0,      0,       myb3k,     myb3k, myb3k_state,    0,     "Matsushita",  "MyBrain 3000", 0)
COMP( 1982, jb3000,  myb3k,  0,       jb3000,    myb3k, myb3k_state,    0,     "Panasonic",   "JB-3000",      MACHINE_NOT_WORKING) // No rom dump available
COMP( 1984, stepone, myb3k,  0,       stepone,   myb3k, myb3k_state,    0,     "Ericsson",    "Step/One",     0)
