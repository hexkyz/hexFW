#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "svc.h"
#include "imports.h"
#include "latte.h"

// Wrapper for kernel level word read
u32 kern_read(u32 addr)
{
	return svcRW(0x00, addr, 0x00);
}

// Wrapper for kernel level word write
void kern_write(u32 addr, u32 val)
{
	svcRW(0x01, addr, val);
}

void ppc_reset()
{
	u32 resets = kern_read(0x0D800194);
	
	// Deassert RSTB_PI and PPC hard reset
	kern_write(0x0D800194, (resets & ~(0x210)));
}

void exi_init()
{
	u32 EXI0_CSR = EXI_REG_BASE + 0x00 * 0x14;
	u32 EXI1_CSR = EXI_REG_BASE + 0x01 * 0x14;
	u32 EXI2_CSR = EXI_REG_BASE + 0x02 * 0x14;
	
	// Clear all EXI channels
	kern_write(EXI0_CSR, 0x00000000);
	kern_write(EXI1_CSR, 0x00000000);
	kern_write(EXI2_CSR, 0x00000000);
	
	// Disable ROM de-scramble for EXI0 (ROMDIS)
	kern_write(EXI0_CSR, 0x00002000);
	
	// Clear EXIINT, TCINT and EXTINT (set to 1)
	kern_write(EXI0_CSR, kern_read(EXI0_CSR) | 0x80A);
	kern_write(EXI1_CSR, kern_read(EXI1_CSR) | 0x80A);
	kern_write(EXI2_CSR, kern_read(EXI2_CSR) | 0x80A);
}

void exi_reg(int ch, u32 device, u32 clk_freq)
{
	u32 EXIx_CSR = EXI_REG_BASE + ch * 0x14;
	
	// Preserve interrupts, select device and clock frequency
	// for an EXI channel and assert TCINT
	kern_write(EXIx_CSR, ((kern_read(EXIx_CSR) & 0x405) | ((0x80 << device) | (clk_freq << 0x04)) | 0x08));
}

void exi_poll(int ch)
{
	u32 EXIx_CR = EXI_REG_BASE + 0x0C + ch * 0x14;
	u32 status = kern_read(EXIx_CR);
	
	// Keep polling EXI until the operation is done
	while ((status & 0x01) != 0)
		status = kern_read(EXIx_CR);
}

void exi_reset(int ch)
{
	u32 EXIx_CSR = EXI_REG_BASE + ch * 0x14;
	
	// Preserve interrupts but discard any selected devices
	kern_write(EXIx_CSR, kern_read(EXIx_CSR) & 0x405);
}

void exi_rw(int ch, void *buf, u32 size, int mode)
{
	if (size)
	{
		// Write data
		if (mode)
		{			
			u32 EXIx_DATA = EXI_REG_BASE + 0x10 + ch * 0x14;
			kern_write(EXIx_DATA, *(u32 *)buf);
		}
		
		// Prepare TLEN and RW
		u32 tlen = (size - 0x01) << 0x04;
		u32 rw = (mode << 0x02);
		
		// Set TLEN, RW and TSTART
		u32 EXIx_CR = EXI_REG_BASE + 0x0C + ch * 0x14;
		kern_write(EXIx_CR, tlen | rw | 0x01);
		
		// Wait for reply
		exi_poll(0);
		
		// Read
		if (!mode)
		{
			u32 EXIx_DATA = EXI_REG_BASE + 0x10 + ch * 0x14;
			*(u32 *)buf = kern_read(EXIx_DATA);
		}
	}
}

void i2c_init(u32 clk, u32 ch)
{
	// Set I2C clock and channel
	kern_write(LT_I2C_CLOCK, (ch << 0x01) | (clk << 0x10) | 0x01);
	
	// Reset control register
	kern_write(LT_I2C_CTRL, 0x00);
}

void i2c_rw(u8 data, int last)
{
	// Pass data
	kern_write(LT_I2C_DATA_IN, (data & 0xFF) | ((!last) ? 0x00 : 0x100));
	
	// Set control register
	kern_write(LT_I2C_CTRL, 0x01);
}

void i2c_int_disable(u32 mask)
{
	// Disable specified interrupt
	u32 int_mask = kern_read(LT_I2C_INT_MASK);
	kern_write(LT_I2C_INT_MASK, int_mask & ~(mask));
}

void i2c_int_enable(u32 mask)
{
	kern_write(LT_I2C_INT_STATUS, mask);
	
	// Enable specified interrupt
	u32 int_mask = kern_read(LT_I2C_INT_MASK);
	kern_write(LT_I2C_INT_MASK, int_mask | mask);
}

void i2c_wait()
{
	u32 retries = 0x00001387;
	u32 status = 0x00000000;
	
	while (retries > 0)
	{
		u32 int_status = kern_read(LT_I2C_INT_STATUS);
		u32 int_mask = kern_read(LT_I2C_INT_MASK);
		
		status = int_status & int_mask;
		
		if (((status & 0x01) != 0) || ((status & 0x02) != 0))
				break;

		retries--;
	}
	
	kern_write(LT_I2C_INT_STATUS, status);
}

void i2c_read(u8 slave_7bit, u8* data, u32 len)
{
	if ((len > 0) && (len < 0x40))
	{
		// Enable read interrupt
		i2c_int_enable(0x1D);
		
		// Send slave with 8th bit 1 (read)
		u8 slave = (((slave_7bit & 0xFF) << 0x01) & 0xFE) | 0x01;
		i2c_rw(slave, 0);
		
		// Send data (empty)
		int i;
		for (i = 0; i < len; i++)
		{
			if ((i + 1) == len)
				i2c_rw(0, 1);		// Send last
			else
				i2c_rw(0, 0);		// Send normal
		}
		
		// Wait for transfer to complete
		i2c_wait();
		
		u32 reply = kern_read(LT_I2C_DATA_OUT);
		reply &= 0x00FF0000;
		
		// Read data
		for (i = 0; i < len; i++)
		{
			reply = kern_read(LT_I2C_DATA_OUT);
			data[i] = (u8)reply;
		}
	}
}

void i2c_write(u8 slave_7bit, u8* data, u32 len)
{
	if ((len > 0) && (len < 0x40))
	{
		// Enable write interrupt
		i2c_int_enable(0x1E);
		
		// Send slave with 8th bit 0 (write)
		u8 slave = (((slave_7bit & 0xFF) << 0x01) & 0xFE);
		i2c_rw(slave, 0);
		
		// Send data
		int i;
		for (i = 0; i < len; i++)
		{
			if ((i + 1) == len)
				i2c_rw(data[i], 1);		// Send last
			else
				i2c_rw(data[i], 0);		// Send normal
		}
		
		// Wait for transfer to complete
		i2c_wait();
		
		// Disable write interrupt
		i2c_int_disable(0x1E);
	}
}