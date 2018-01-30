// license:BSD-3-Clause
// copyright-holders:Carl

#ifndef MAME_MACHINE_ACS8600_ICS_H_
#define MAME_MACHINE_ACS8600_ICS_H_

#pragma once



class acs8600_ics_device : public device_t
{
public:
	acs8600_ics_device(const machine_config &mconfig, const char *tag, device_t *owner, uint32_t clock);

	DECLARE_WRITE8_MEMBER(hiaddr_w);
	DECLARE_WRITE8_MEMBER(ctrl_w);
	DECLARE_READ8_MEMBER(hostram_r);
	DECLARE_WRITE8_MEMBER(hostram_w);
	DECLARE_WRITE_LINE_MEMBER(attn_w);

	template <class Object> static devcb_base &static_set_irq1_callback(device_t &device, Object &&cb) { return downcast<acs8600_ics_device &>(device).m_out_irq1_func.set_callback(std::forward<Object>(cb)); }
	template <class Object> static devcb_base &static_set_irq2_callback(device_t &device, Object &&cb) { return downcast<acs8600_ics_device &>(device).m_out_irq2_func.set_callback(std::forward<Object>(cb)); }
	static void static_set_maincpu_tag(device_t &device, const char *maincpu_tag) { downcast<acs8600_ics_device &>(device).m_maincpu_tag = maincpu_tag; }
	const tiny_rom_entry *device_rom_region() const override;

protected:
	virtual void device_start() override;
	virtual void device_add_mconfig(machine_config &config) override;

private:
	required_device<cpu_device> m_icscpu;
	devcb_write_line m_out_irq1_func;
	devcb_write_line m_out_irq2_func;
	const char *m_maincpu_tag;
	address_space *m_maincpu_mem;
	u8 m_hiaddr, m_ctrl;
};
#define MCFG_ACS8600_ICS_MAINCPU(_maincpu_tag) \
	acs8600_ics_device::static_set_maincpu_tag(*device, _maincpu_tag);

#define MCFG_ACS8600_ICS_IRQ1(_irq_line) \
	devcb = &acs8600_ics_device::static_set_irq1_callback(*device, DEVCB_##_irq_line);

#define MCFG_ACS8600_ICS_IRQ2(_irq_line) \
	devcb = &acs8600_ics_device::static_set_irq2_callback(*device, DEVCB_##_irq_line);

DECLARE_DEVICE_TYPE(ACS8600_ICS, acs8600_ics_device)

#endif
