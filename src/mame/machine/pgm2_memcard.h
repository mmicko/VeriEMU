// license:BSD-3-Clause
// copyright-holders:David Haywood, Miodrag Milanovic
/*********************************************************************

    pgm2_memcard.h

    PGM2 Memory card functions.
    (based on ng_memcard.h)

*********************************************************************/
#ifndef MAME_MACHINE_PGM2_MEMCARD_H
#define MAME_MACHINE_PGM2_MEMCARD_H

#pragma once


//**************************************************************************
//  INTERFACE CONFIGURATION MACROS
//**************************************************************************

#define MCFG_PGM2_MEMCARD_ADD(_tag) \
	MCFG_DEVICE_ADD(_tag, PGM2_MEMCARD, 0)

/***************************************************************************
    FUNCTION PROTOTYPES
***************************************************************************/

// ======================> pgm2_memcard_device

class pgm2_memcard_device :  public device_t,
							public device_image_interface
{
public:
	// construction/destruction
	pgm2_memcard_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	virtual iodevice_t image_type() const override { return IO_MEMCARD; }

	virtual bool is_readable()  const override { return true; }
	virtual bool is_writeable() const override { return true; }
	virtual bool is_creatable() const override { return true; }
	virtual bool must_be_loaded() const override { return false; }
	virtual bool is_reset_on_load() const override { return false; }
	virtual const char *file_extensions() const override { return "pg2,bin,mem"; }

	virtual image_init_result call_load() override;
	virtual void call_unload() override;
	virtual image_init_result call_create(int format_type, util::option_resolution *format_options) override;

	// device-level overrides
	virtual void device_start() override;

	DECLARE_READ8_MEMBER(read);
	DECLARE_WRITE8_MEMBER(write);
	DECLARE_READ8_MEMBER(read_prot);
	DECLARE_WRITE8_MEMBER(write_prot);
	DECLARE_READ8_MEMBER(read_sec);
	DECLARE_WRITE8_MEMBER(write_sec);
	void auth(uint8_t p1, uint8_t p2, uint8_t p3);

	/* returns the index of the current memory card, or -1 if none */
	int present() { return is_loaded() ? 0 : -1; }
private:
	uint8_t m_memcard_data[0x100];
	uint8_t m_protection_data[4];
	uint8_t m_security_data[4];
	bool authenticated;
};


// device type definition
DECLARE_DEVICE_TYPE(PGM2_MEMCARD, pgm2_memcard_device)


#endif // MAME_MACHINE_PGM2_MEMCARD_H
