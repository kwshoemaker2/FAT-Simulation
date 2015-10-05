#include <string.h>
#include <stdlib.h>

#include "fat.h"
#include "drive.h"


/*
 * Sets the value in struct fat_entry in struct fat at the first int to the second
 * int. the int must either be EMPTY, END_OF_FILE, or a valid address in memory.
 * 
 * Returns 0 if a valid index and value was given, and 1 otherwise
 */
int set_fat_entry_value(struct fat *the_fat, int index, int value)
{
    if(index < 0 || index >= TOTAL_SECTORS) {
        return 1;
    }

    return 0;
}


int store_fat(struct fat *the_fat)
{
    size_t size = FAT_SIZE;
    char *data = malloc(sizeof(char) * size);
    memcpy(data, the_fat, size);
    
    size_t offset = 0;
    int cur_sect = 0;
    for(int i = 0; i < size / BYTES_PER_SECTOR; ++i) {
        write_sector(0, cur_sect, data + offset);
        cur_sect++;
        offset += BYTES_PER_SECTOR;
    }
    char leftover[BYTES_PER_SECTOR];
    memcpy(leftover, data + offset, size - offset);
    write_sector(1, cur_sect, leftover);
    free(data);
    return 0;
}


int load_fat(struct fat *the_fat)
{
    size_t size = FAT_SIZE;
    char *data = malloc(sizeof(char) * size);
    
    size_t offset = 0;
    int cur_sect = 0;
    for(int i = 0; i < size / BYTES_PER_SECTOR; ++i) {
        read_sector(0, cur_sect, data + offset);
        cur_sect++;
        offset += BYTES_PER_SECTOR;
    }
    char leftover[BYTES_PER_SECTOR];
    read_sector(0, cur_sect, leftover);
    memcpy(data, leftover, size - offset);
    memcpy(the_fat, data, size);
    free(data);
    return 0;
}




































