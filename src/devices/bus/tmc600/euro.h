// license:BSD-3-Clause
// copyright-holders:Curt Coder
/**********************************************************************

    Telercas Telmac TMC-600 euro bus emulation

**********************************************************************

                              C     A
                        GND   *  1  *   GND
                         D1   *  2  *   D0
                         D3   *  3  *   D2
                         D5   *  4  *   D4
                         D7   *  5  *   D6
                        GND   *  6  *   GND
                         A1   *  7  *   A0
                         A3   *  8  *   A2
                         A5   *  9  *   A4
                         A7   * 10  *   A6
                         A9   * 11  *   A8
                        A11   * 12  *   A10
                        A13   * 13  *   A12
                        A15   * 14  *   A14
                        GND   * 15  *   GND
                        TPB   * 16  *   CLOCK
                       _MWR   * 17  *   _CLEAR
                       _MRD   * 18  *   _WAIT
                        TPA   * 19  *   _DMAO
                        SC0   * 20  *   _DMAI
                        SC1   * 21  *   _INT
                         N0   * 22  *   _EF1
                         N1   * 23  *   _EF2
                         N2   * 24  *   _EF3
                          Q   * 25  *   _EF4
                        _MI   * 26  *   _EMS
                        SP1   * 27  *   SP2
                        GND   * 28  *   GND
                        -5V   * 29  *   -5V
                       +12v   * 30  *   +12V
                        +5V   * 31  *   +5V
                        +5V   * 32  *   +5V

**********************************************************************/

#ifndef MAME_DEVICES_TMC600_EURO_H
#define MAME_DEVICES_TMC600_EURO_H

#pragma once



//**************************************************************************
//  CONSTANTS
//**************************************************************************

#define TMC600_EURO_BUS_TAG     "bus"



//**************************************************************************
//  INTERFACE CONFIGURATION MACROS
//**************************************************************************

#define MCFG_TMC600_EURO_BUS_SLOT_ADD(_tag, _slot_intf, _def_slot) \
	MCFG_DEVICE_ADD(_tag, TMC600_EURO_BUS_SLOT, 0) \
	MCFG_DEVICE_SLOT_INTERFACE(_slot_intf, _def_slot, false)



//**************************************************************************
//  TYPE DEFINITIONS
//**************************************************************************

// ======================> device_tmc600_euro_bus_card_interface

class tmc600_euro_bus_slot_t;

class device_tmc600_euro_bus_card_interface : public device_slot_card_interface
{
public:

protected:
	// construction/destruction
	device_tmc600_euro_bus_card_interface(const machine_config &mconfig, device_t &device);

	tmc600_euro_bus_slot_t  *m_slot;

	friend class tmc600_euro_bus_slot_t;
};


// ======================> tmc600_euro_bus_slot_t

class tmc600_euro_bus_slot_t : public device_t,
							   public device_slot_interface
{
public:
	// construction/destruction
	tmc600_euro_bus_slot_t(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

protected:
	// device-level overrides
	virtual void device_start() override;
	virtual void device_reset() override { if (m_card) get_card_device()->reset(); }

	device_tmc600_euro_bus_card_interface *m_card;
};


// device type definition
DECLARE_DEVICE_TYPE(TMC600_EURO_BUS_SLOT, tmc600_euro_bus_slot_t)


SLOT_INTERFACE_EXTERN( tmc600_euro_bus_cards );


typedef device_type_iterator<tmc600_euro_bus_slot_t> tmc600_euro_bus_slot_t_iterator;


#endif // MAME_DEVICES_TMC600_EURO_H
