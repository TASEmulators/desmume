#include <nds.h>

static u32 cdcCurrentPage = -1;
 
void CDC_WriteRegister(u32 reg, u32 val);

void CDC_ChangePage(u32 page)
{
	if (cdcCurrentPage == page)
		return;
 
	if (cdcCurrentPage == 0xFF)
		CDC_WriteRegister(0x7F, page);
	else
		CDC_WriteRegister(0, page);
 
	cdcCurrentPage = page;
}
 
 
void CDC_WriteRegister(u32 reg, u32 val)
{
	while(REG_SPICNT & 0x80);
 
	REG_SPICNT = 0x8A00;
	REG_SPIDATA = (reg<<1) & 0xFF;
 
	while(REG_SPICNT & 0x80);
 
	REG_SPICNT = 0x8200;
	REG_SPIDATA = val & 0xFF;
}
 
void CDC_WriteRegisterEx(u32 page, u32 reg, u32 val)
{
	CDC_ChangePage(page);
	CDC_WriteRegister(reg, val);
}
 
void CDC_WriteRegisters(u32 reg, u8* buf, u32 size)
{
	if (size == 0)
		return;
 
 
	while(REG_SPICNT & 0x80);
 
	REG_SPICNT = 0x8A00;
	REG_SPIDATA = (reg<<1) & 0xFF;
 
	while(REG_SPICNT & 0x80);
 
	while(size-- > 1)
	{
		REG_SPIDATA = 0;
 
		while(REG_SPICNT & 0x80);
 
		*buf++ = REG_SPIDATA;
	}
 
	REG_SPICNT = 0x8200;
	REG_SPIDATA = 0;
 
	while(REG_SPICNT & 0x80);
 
	*buf++ = REG_SPIDATA;
}
 
void CDC_WriteRegistersEx(u32 page, u32 reg, u8* buf, u32 size)
{
	CDC_ChangePage(page);
	CDC_WriteRegisters(reg, buf, size);
}
 
u32 CDC_ReadRegister(u32 reg)
{
	while(REG_SPICNT & 0x80);
 
	REG_SPICNT = 0x8A00;
	REG_SPIDATA = ((reg<<1) | 1) & 0xFF;
 
	while(REG_SPICNT & 0x80);
 
	REG_SPICNT = 0x8200;
	REG_SPIDATA = 0;
 
	while(REG_SPICNT & 0x80);
 
	return REG_SPIDATA;
}
 
u32 CDC_ReadRegisterEx(u32 page, u32 reg)
{
	CDC_ChangePage(page);
	return CDC_ReadRegister(reg);
}
 
 
void CDC_ReadRegisters(u32 reg, u8* buf, u32 size)
{
	if (size == 0)
		return;
 
 
	while(REG_SPICNT & 0x80);
 
	REG_SPICNT = 0x8A00;
	REG_SPIDATA = ((reg<<1)|1) & 0xFF;
 
	while(REG_SPICNT & 0x80);
 
	while(size-- > 1)
	{
		REG_SPIDATA = 0;
 
		while(REG_SPICNT & 0x80);
 
		*buf++ = REG_SPIDATA;
	}
 
	REG_SPICNT = 0x8200;
	REG_SPIDATA = 0;
 
	while(REG_SPICNT & 0x80);
 
	*buf++ = REG_SPIDATA;
}
 
void CDC_ReadRegistersEx(u32 page, u32 reg, u8* buf, u32 size)
{
	CDC_ChangePage(page);
	CDC_ReadRegisters(reg, buf, size);
}

