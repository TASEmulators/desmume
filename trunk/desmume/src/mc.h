#ifndef __FW_H__
#define __FW_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "nds/serial.h"

#define MC_TYPE_EEPROM1         0x1
#define MC_TYPE_EEPROM2         0x2
#define MC_TYPE_FLASH           0x3

typedef struct
{
	u8 com;	/* persistent command actually handled */
        u32 addr;        /* current address for reading/writing */
        u8 addr_shift;   /* shift for address (since addresses are transfered by 3 bytes units) */
        u8 addr_size;    /* size of addr when writing/reading */
	
	BOOL write_enable;	/* is write enabled ? */
	
        u8 *data;       /* memory data */
        u32 size;       /* memory size */
	BOOL writeable_buffer;	/* is "data" writeable ? */
        int type; /* type of Memory */
} memory_chip_t;

#define NDS_FW_SIZE_V1 (256 * 1024)		/* size of fw memory on nds v1 */
#define NDS_FW_SIZE_V2 (512 * 1024)		/* size of fw memory on nds v2 */

void mc_init(memory_chip_t *mc, int type);    /* reset and init values for memory struct */
u8 *mc_alloc(memory_chip_t *mc, u32 size);  /* alloc mc memory */
void mc_free(memory_chip_t *mc);    /* delete mc memory */
void mc_reset_com(memory_chip_t *mc);       /* reset communication with mc */
u8 fw_transfer(memory_chip_t *mc, u8 data); /* transfer to, then receive data from firmware */
u8 bm_transfer(memory_chip_t *mc, u8 data); /* transfer to, then receive data from backup memory */

#ifdef __cplusplus
}
#endif

#endif /*__FW_H__*/

