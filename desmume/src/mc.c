#include <stdlib.h>
#include "debug.h"
#include "types.h"
#include "mc.h"

#define FW_CMD_READ             0x3
#define FW_CMD_WRITEDISABLE     0x4
#define FW_CMD_READSTATUS       0x5
#define FW_CMD_WRITEENABLE      0x6
#define FW_CMD_PAGEWRITE        0xA

#define BM_CMD_AUTODETECT       0xFF
#define BM_CMD_WRITESTATUS      0x1
#define BM_CMD_WRITELOW         0x2
#define BM_CMD_READLOW          0x3
#define BM_CMD_WRITEDISABLE     0x4
#define BM_CMD_READSTATUS       0x5
#define BM_CMD_WRITEENABLE      0x6
#define BM_CMD_WRITEHIGH        0xA
#define BM_CMD_READHIGH         0xB

/* FLASH*/
#define COMM_PAGE_WRITE		0x0A
#define COMM_PAGE_ERASE		0xDB
#define COMM_SECTOR_ERASE	0xD8
#define COMM_CHIP_ERASE		0xC7
#define CARDFLASH_READ_BYTES_FAST	0x0B    /* Not used*/
#define CARDFLASH_DEEP_POWDOWN		0xB9    /* Not used*/
#define CARDFLASH_WAKEUP			0xAB    /* Not used*/

void mc_init(memory_chip_t *mc, int type)
{
        mc->com = 0;
        mc->addr = 0;
        mc->addr_shift = 0;
        mc->data = NULL;
        mc->size = 0;
        mc->write_enable = FALSE;
        mc->writeable_buffer = FALSE;
        mc->type = type;
        mc->autodetectsize = 0;
                               
        switch(mc->type)
        {
           case MC_TYPE_EEPROM1:
              mc->addr_size = 1;
              break;
           case MC_TYPE_EEPROM2:
           case MC_TYPE_FRAM:
              mc->addr_size = 2;
              break;
           case MC_TYPE_FLASH:
              mc->addr_size = 3;
              break;
           default: break;
        }
}

u8 *mc_alloc(memory_chip_t *mc, u32 size)
{
	u8 *buffer;
	buffer = malloc(size);
	
	if(!buffer) { return NULL; }
        mc->data = buffer;
        mc->size = size;
        mc->writeable_buffer = TRUE;
}

void mc_free(memory_chip_t *mc)
{
        if(mc->data) free(mc->data);
        mc_init(mc, 0);
}

void mc_reset_com(memory_chip_t *mc)
{
   if (mc->type == MC_TYPE_AUTODETECT && mc->com == BM_CMD_AUTODETECT)
   {
      u32 i;
      u32 addr, size;

      if (mc->autodetectsize == (32768+2))
      {
         // FRAM
         addr = (mc->autodetectbuf[0] << 8) | mc->autodetectbuf[1];
         mc->type = MC_TYPE_FRAM;
         mc->size = MC_SIZE_256KBITS;
      }
      else if (mc->autodetectsize == (256+3))
      {
         // Flash
         addr = (mc->autodetectbuf[0] << 16) |
                    (mc->autodetectbuf[1] << 8) |
                     mc->autodetectbuf[2];
         mc->type = MC_TYPE_FLASH;
         mc->size = MC_SIZE_2MBITS;
      }
      else if (mc->autodetectsize == (128+2))
      {
         // 512 Kbit EEPROM
         addr = (mc->autodetectbuf[0] << 8) | mc->autodetectbuf[1];
         mc->type = MC_TYPE_EEPROM2;
         mc->size = MC_SIZE_512KBITS;
      }
      else if (mc->autodetectsize == (32+2) || mc->autodetectsize == (20+2) || mc->autodetectsize == (10+2))
      {
         // 64 Kbit EEPROM
         addr = (mc->autodetectbuf[0] << 8) | mc->autodetectbuf[1];
         mc->type = MC_TYPE_EEPROM2;
         mc->size = MC_SIZE_64KBITS;
      }
      else if (mc->autodetectsize == (16+1) || mc->autodetectsize == (8+1) || mc->autodetectsize == (1+1))
      {
         // 4 Kbit EEPROM
         addr = mc->autodetectbuf[0];
         mc->type = MC_TYPE_EEPROM1;
         mc->size = MC_SIZE_4KBITS;
      }
      else
      {
         // Assume it's a Flash non-page write 
         LOG("Flash detected(guessed). autodetectsize = %d\n", mc->autodetectsize);
         addr = (mc->autodetectbuf[0] << 16) |
                    (mc->autodetectbuf[1] << 8) |
                     mc->autodetectbuf[2];
         mc->type = MC_TYPE_FLASH;
         mc->size = MC_SIZE_2MBITS;
      }

      size = mc->autodetectsize;
      mc_realloc(mc, mc->type, mc->size);
      memcpy(mc->data+addr, mc->autodetectbuf+mc->addr_size, size-mc->addr_size);
      mc->autodetectsize = 0;
      mc->write_enable = FALSE;

      // Generate file
      if ((mc->fp = fopen(mc->filename, "wb+")) != NULL)
         fwrite((void *)mc->data, 1, mc->size, mc->fp);
   }
   else if (mc->com == BM_CMD_WRITELOW)
   {      
      if (mc->fp)
      {
         fseek(mc->fp, 0, SEEK_SET);
         fwrite((void *)mc->data, 1, mc->size, mc->fp); // fix me
      }
      mc->write_enable = FALSE;
   }

   mc->com = 0;
}

void mc_realloc(memory_chip_t *mc, int type, u32 size)
{
        if(mc->data) free(mc->data);
        mc_init(mc, type);
        mc_alloc(mc, size);     
}

void mc_load_file(memory_chip_t *mc, const char* filename)
{
   long size;
   FILE* file = fopen(filename, "rb+");
   if(file == NULL)
   {
      mc->filename = strdup(filename);
      return;
   }

   fseek(file, 0, SEEK_END);
   size = ftell(file);
   fseek(file, 0, SEEK_SET);

   if (size == MC_SIZE_4KBITS)
      mc->type = MC_TYPE_EEPROM1;
   else if (size == MC_SIZE_64KBITS)
      mc->type = MC_TYPE_EEPROM2;
   else if (size == MC_SIZE_256KBITS)
      mc->type = MC_TYPE_FRAM;
   else if (size == MC_SIZE_512KBITS)
      mc->type = MC_TYPE_EEPROM2;
   else if (size >= MC_SIZE_2MBITS)
      mc->type = MC_TYPE_FLASH;

   mc->size = size;

   mc_realloc(mc, mc->type, mc->size);
   fread (mc->data, 1, mc->size, file);
   mc->fp = file;
}

u8 fw_transfer(memory_chip_t *mc, u8 data)
{
        if(mc->com == FW_CMD_READ || mc->com == FW_CMD_PAGEWRITE) /* check if we are in a command that needs 3 bytes address */
	{
                if(mc->addr_shift > 0)   /* if we got a complete address */
		{
                        mc->addr_shift--;
                        mc->addr |= data << (mc->addr_shift * 8); /* argument is a byte of address */
		}
                else    /* if we have received 3 bytes of address, proceed command */
		{
                        switch(mc->com)
			{
                                case FW_CMD_READ:
                                        if(mc->addr < mc->size)  /* check if we can read */
					{
                                                data = mc->data[mc->addr];       /* return byte */
                                                mc->addr++;      /* then increment address */
					}
					break;
					
                                case FW_CMD_PAGEWRITE:
                                        if(mc->addr < mc->size)
					{
                                                mc->data[mc->addr] = data;       /* write byte */
                                                mc->addr++;
					}
					break;
			}
			
		}
	}
        else if(mc->com == FW_CMD_READSTATUS)
	{
		return 0;
	}
	else	/* finally, check if it's a new command */
	{
		switch(data)
		{
			case 0: break;	/* nothing */
			
                        case FW_CMD_READ:    /* read command */
                                mc->addr = 0;
                                mc->addr_shift = 3;
                                mc->com = FW_CMD_READ;
				break;
				
                        case FW_CMD_WRITEENABLE:     /* enable writing */
                                if(mc->writeable_buffer) { mc->write_enable = TRUE; }
				break;
				
                        case FW_CMD_WRITEDISABLE:    /* disable writing */
                                mc->write_enable = FALSE;
				break;
				
                        case FW_CMD_PAGEWRITE:       /* write command */
                                if(mc->write_enable)
				{
                                        mc->addr = 0;
                                        mc->addr_shift = 3;
                                        mc->com = FW_CMD_PAGEWRITE;
				}
				else { data = 0; }
				break;
			
                        case FW_CMD_READSTATUS:  /* status register command */
                                mc->com = FW_CMD_READSTATUS;
				break;
				
			default:
                                LOG("Unhandled FW command: %02X\n", data);
				break;
		}
	}
	
	return data;
}	

u8 bm_transfer(memory_chip_t *mc, u8 data)
{
        if(mc->com == BM_CMD_READLOW || mc->com == BM_CMD_WRITELOW) /* check if we are in a command that needs multiple byte address */
	{
                if(mc->addr_shift > 0)   /* if we got a complete address */
		{
                        mc->addr_shift--;
                        mc->addr |= data << (mc->addr_shift * 8); /* argument is a byte of address */
		}
                else    /* if we have received all bytes of address, proceed command */
		{
                        switch(mc->com)
			{
                                case BM_CMD_READLOW:
                                        if(mc->addr < mc->size)  /* check if we can read */
                                        {
                                                //LOG("Read Backup Memory addr %08X(%02X)\n", mc->addr, mc->data[mc->addr]);
                                                data = mc->data[mc->addr];       /* return byte */
                                                mc->addr++;      /* then increment address */
                                        }
					break;
					
                                case BM_CMD_WRITELOW:
                                        if(mc->addr < mc->size)
                                        {
                                                //LOG("Write Backup Memory addr %08X with %02X\n", mc->addr, data);
                                                mc->data[mc->addr] = data;       /* write byte */
                                                mc->addr++;
                                        }
					break;
			}
			
		}
	}
        else if(mc->com == BM_CMD_AUTODETECT)
        {
           // Store everything in a temporary
           mc->autodetectbuf[mc->autodetectsize] = data;
           mc->autodetectsize++;
           return 0;
        }
        else if(mc->com == BM_CMD_READSTATUS)
	{
                //LOG("Backup Memory Read Status: %02X\n", mc->write_enable << 1);
                return (mc->write_enable << 1);
	}
	else	/* finally, check if it's a new command */
	{
		switch(data)
		{
			case 0: break;	/* nothing */
			
                        case BM_CMD_WRITELOW:       /* write command */
                                if(mc->write_enable)
				{
                                        if(mc->type == MC_TYPE_AUTODETECT)
                                        {
                                           mc->com = BM_CMD_AUTODETECT;
                                           break;
                                        }

                                        mc->addr = 0;
                                        mc->addr_shift = mc->addr_size;
                                        mc->com = BM_CMD_WRITELOW;
				}
				else { data = 0; }
				break;

                        case BM_CMD_READLOW:    /* read command */
                                mc->addr = 0;           
                                mc->addr_shift = mc->addr_size;
                                mc->com = BM_CMD_READLOW;
				break;
								
                        case BM_CMD_WRITEDISABLE:    /* disable writing */
                                mc->write_enable = FALSE;
				break;
							
                        case BM_CMD_READSTATUS:  /* status register command */
                                mc->com = BM_CMD_READSTATUS;
				break;

                        case BM_CMD_WRITEENABLE:     /* enable writing */
                                if(mc->writeable_buffer) { mc->write_enable = TRUE; }
				break;

                        case BM_CMD_WRITEHIGH:       /* write command that's only available on ST M95040-W that I know of */
                                if(mc->write_enable)
				{
                                        if(mc->type == MC_TYPE_AUTODETECT)
                                        {
                                           mc->com = BM_CMD_AUTODETECT;
                                           break;
                                        }

                                        if (mc->type == MC_TYPE_EEPROM1)
                                           mc->addr = 0x100;
                                        else
                                           mc->addr = 0;
                                        mc->addr_shift = mc->addr_size;
                                        mc->com = BM_CMD_WRITELOW;
				}
				else { data = 0; }
				break;

                        case BM_CMD_READHIGH:    /* read command that's only available on ST M95040-W that I know of */
                                if (mc->type == MC_TYPE_EEPROM1)
                                   mc->addr = 0x100;
                                else
                                   mc->addr = 0;
                                mc->addr_shift = mc->addr_size;
                                mc->com = BM_CMD_READLOW;

				break;

			default:
                                LOG("Unhandled Backup Memory command: %02X\n", data);
				break;
		}
	}
	
	return data;
}	

