// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000
 * Rob Taylor, Flying Pig Systems. robt@flyingpig.com.
 *
 * (C) Copyright 2004
 * ARM Ltd.
 * Philippe Robin, <philippe.robin@arm.com>
 */

/* Simple U-Boot driver for the PrimeCell PL010/PL011 UARTs */

#include <asm/global_data.h>
/* For get_bus_freq() */
#include <clock_legacy.h>
#include <dm.h>
#include <clk.h>
#include <errno.h>
#include <watchdog.h>
#include <asm/io.h>
#include <serial.h>
#include <spl.h>
#include <dm/device_compat.h>
#include <linux/compiler.h>

DECLARE_GLOBAL_DATA_PTR;

#if !CONFIG_IS_ENABLED(DM_SERIAL)
#error "DM_SERIAL must be turned on"
#endif

union Status {
	__attribute__((packed)) struct {
		uint32_t rx_avail : 1;
		uint32_t rx_irq_pending : 1;
		uint32_t rx_overrun : 1;
		uint32_t rx_overrun_irq_pending : 1;
		uint32_t combined_irq_pendig : 1;
	} bits;
	uint32_t u32;
};

union Control {
	__attribute__((packed)) struct {
		uint32_t tx_on : 1;
		uint32_t rx_on : 1;
		uint32_t rx_irq_on : 1;
		uint32_t rx_overrun_irq_on : 1;
		uint32_t rx_clear_irq : 1;
		uint32_t rx_clear_error : 1;
		uint32_t rx_clear_error_irq : 1;
	} bits;
	uint32_t u32;
};

__attribute__((packed)) struct Registers {
	uint32_t data;
	uint32_t status;
	uint32_t control;
	uint32_t foo;
};

struct Priv {
	void __iomem *regs;
};

static int pl01x_putc(struct Registers *regs, char c)
{
	/* Send the character */
	writel(c, &regs->data);

	return 0;
}

static int pl01x_getc(struct Registers *regs)
{
	unsigned int data;
	union Status s;

	s.u32 = readl(&regs->status);

	/* Wait until there is data in the FIFO */
	if (!s.bits.rx_avail)
		return -EAGAIN;

	data = readl(&regs->data);

	// /* Check for an error flag */
	// if (data & 0xFFFFFF00) {
	// 	/* Clear the error */
	// 	writel(0xFFFFFFFF, &regs->ecr);
	// 	return -1;
	// }

	return (int)data;
}

static int pl01x_tstc(struct Registers *regs)
{
	schedule();
	return ((union Status)readl(&regs->status)).bits.rx_avail;
}

static int pl01x_generic_serial_init(struct Registers *regs)
{
	union Control c = { .u32 = 0xfefebabf };
	// c.bits.tx_on = 1;
	// c.bits.rx_on = 1;
	writel(c.u32, &regs->control);
	return 0;
}

#if !CONFIG_IS_ENABLED(DM_SERIAL)
#error "DM_SERIAL must be turned on"
#else

static int ratchar_serial_getinfo(struct udevice *dev,
				  struct serial_device_info *info)
{
	return -ENOSYS;
}

int ratchar_serial_probe(struct udevice *dev)
{
	struct Priv *priv = dev_get_priv(dev);

	priv->regs = dev_read_addr_ptr(dev);
	if (!priv->regs)
		return -EINVAL;
	return pl01x_generic_serial_init((struct Registers *)priv->regs);
}

int ratchar_serial_getc(struct udevice *dev)
{
	struct Priv *priv = dev_get_priv(dev);

	return pl01x_getc((struct Registers *)priv->regs);
}

int ratchar_serial_putc(struct udevice *dev, const char ch)
{
	struct Priv *priv = dev_get_priv(dev);

	return pl01x_putc((struct Registers *)priv->regs, ch);
}

int ratchar_serial_pending(struct udevice *dev, bool input)
{
	struct Priv *priv = dev_get_priv(dev);

	if (input)
		return pl01x_tstc((struct Registers *)priv->regs);
	else
		return 1;
}

static int ratchar_serial_setbrg(struct udevice *dev, int baud)
{
	return 0;
}

static const struct dm_serial_ops ratchar_serial_ops = {
	.putc = ratchar_serial_putc,
	.pending = ratchar_serial_pending,
	.getc = ratchar_serial_getc,
	.setbrg = ratchar_serial_setbrg,
	.getinfo = ratchar_serial_getinfo,
};

#if CONFIG_IS_ENABLED(OF_REAL)
static const struct udevice_id pl01x_serial_id[] = {
	{ .compatible = "cream-pi,ratchar" },
	{}
};

#ifndef CFG_PL011_CLOCK
#define CFG_PL011_CLOCK 0
#endif

int ratchar_serial_of_to_plat(struct udevice *dev)
{
	return 0;
}
#endif

U_BOOT_DRIVER(serial_ratchar) = {
	.name = "serial_ratchar",
	.id = UCLASS_SERIAL,
#if CONFIG_IS_ENABLED(OF_REAL)
	// #error "of real"
	.of_match = of_match_ptr(pl01x_serial_id),
// .of_to_plat = of_match_ptr(pl01x_serial_of_to_plat),
#endif
	// .plat_auto = sizeof(struct pl01x_serial_plat),
	.probe = ratchar_serial_probe,
	.ops = &ratchar_serial_ops,
	.flags = DM_FLAG_PRE_RELOC,
	.priv_auto = sizeof(struct Priv),
};

DM_DRIVER_ALIAS(serial_pl01x, arm_pl011)
DM_DRIVER_ALIAS(serial_pl01x, arm_pl010)
#endif

#if defined(CONFIG_DEBUG_UART_PL010) || defined(CONFIG_DEBUG_UART_PL011)

#include <debug_uart.h>

static void _debug_uart_init(void)
{
	// #ifndef CONFIG_DEBUG_UART_SKIP_INIT
	// 	struct pl01x_regs *regs =
	// 		(struct pl01x_regs *)CONFIG_VAL(DEBUG_UART_BASE);
	// 	enum pl01x_type type;

	// 	if (IS_ENABLED(CONFIG_DEBUG_UART_PL011))
	// 		type = TYPE_PL011;
	// 	else
	// 		type = TYPE_PL010;

	// 	pl01x_generic_serial_init(regs, type);
	// 	pl01x_generic_setbrg(regs, type, CONFIG_DEBUG_UART_CLOCK,
	// 			     CONFIG_BAUDRATE);
	// #endif
}

static inline void _debug_uart_putc(int ch)
{
	// struct pl01x_regs *regs =
	// 	(struct pl01x_regs *)CONFIG_VAL(DEBUG_UART_BASE);

	// while (pl01x_putc(regs, ch) == -EAGAIN)
	// 	;
}

DEBUG_UART_FUNCS

#endif
