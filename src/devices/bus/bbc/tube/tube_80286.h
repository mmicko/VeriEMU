// license:BSD-3-Clause
// copyright-holders:Nigel Barnes
/**********************************************************************

    Acorn 80286 2nd Processor

**********************************************************************/


#ifndef MAME_BUS_BBC_TUBE_80286_H
#define MAME_BUS_BBC_TUBE_80286_H

#include "tube.h"
#include "cpu/i86/i286.h"
#include "machine/ram.h"
#include "machine/tube.h"

//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> bbc_tube_80286_device

class bbc_tube_80286_device :
	public device_t,
	public device_bbc_tube_interface
{
public:
	// construction/destruction
	bbc_tube_80286_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	DECLARE_READ8_MEMBER( disable_boot_rom );
	DECLARE_WRITE8_MEMBER( irq_latch_w );

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override;

	// optional information overrides
	virtual void device_add_mconfig(machine_config &config) override;
	virtual const tiny_rom_entry *device_rom_region() const override;

	virtual DECLARE_READ8_MEMBER( host_r ) override;
	virtual DECLARE_WRITE8_MEMBER( host_w ) override;

private:
	uint8_t m_irq_latch;

	IRQ_CALLBACK_MEMBER( irq_callback );

	required_device<i80286_cpu_device> m_i80286;
	required_device<tube_device> m_ula;
	required_device<ram_device> m_ram;
	required_memory_region m_bootstrap;
};


// device type definition
DECLARE_DEVICE_TYPE(BBC_TUBE_80286, bbc_tube_80286_device)


#endif /* MAME_BUS_BBC_TUBE_80286_H */
