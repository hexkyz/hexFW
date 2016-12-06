#ifndef LATTE_H
#define LATTE_H

#include "types.h"

#define	EXI_REG_BASE		0x0D806800

#define	LT_I2C_CLOCK		0x0D800570
#define	LT_I2C_DATA_IN		0x0D800574
#define	LT_I2C_CTRL			0x0D800578
#define	LT_I2C_DATA_OUT		0x0D80057C
#define	LT_I2C_INT_MASK		0x0D800580
#define	LT_I2C_INT_STATUS	0x0D800584

u32 kern_read(u32 addr);
void kern_write(u32 addr, u32 val);

void ppc_reset();

void exi_init();
void exi_reg(int ch, u32 device, u32 clk_freq);
void exi_poll(int ch);
void exi_reset(int ch);
void exi_rw(int ch, void *buf, u32 size, int mode);

void i2c_init(u32 clk, u32 ch);
void i2c_rw(u8 data, int last);
void i2c_int_disable(u32 mask);
void i2c_int_enable(u32 mask);
void i2c_wait();
void i2c_read(u8 slave_7bit, u8* data, u32 len);
void i2c_write(u8 slave_7bit, u8* data, u32 len);

#endif