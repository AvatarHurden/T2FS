
#ifndef __apidisk_h__
#define __apidisk_h__
#define SECTOR_SIZE 256
int read_sector (unsigned int sector, char *buffer);
int write_sector (unsigned int sector, char *buffer);
#endif
