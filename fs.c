
/* Project 2 rules:
 *		ALL data must be stored on the simulated hard drive!
 *			- No global variables
 *			- No static variables
 *			- No other contrived method to preserve data between function calls
 */

/*
 * This is the only file you need to modify.  You might want to add printing or 
 * changes stuff in tester.c, but the program should work at the end with the 
 * initial versions of all files except this one.  Function stubs are included
 * so that the project will compile as-received.
 */

#include "fs.h"
#include "drive.h"
#include "fat.h"
#include "dir.h"
#include "mem_utils.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int store_fs(struct fs *the_fs)
{
    size_t size = sizeof(struct fs);
    char *data = malloc(sizeof(char) * size);
    memcpy(data, the_fs, size);
    
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


int load_fs(struct fs *the_fs)
{
    size_t size = sizeof(struct fs);
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
    memcpy(the_fs, data, size);
    free(data);
    return 0;
}


int fdelete(char* fn){
    struct fs *the_fs = malloc(sizeof(struct fs));
    int ret = 0;
    load_fs(the_fs);
    if(rem_dir_ent(&the_fs->root_dir, fn)) {
        ret = NOT_FOUND;
    };

    store_fs(the_fs);
    free(the_fs);
	return ret;
}


int load(char* fn, void* data, size_t ds){
    struct fs *the_fs = malloc(sizeof(struct fs));
    int ret = 5;
    load_fs(the_fs);
    struct dir_ent file_ent;
    if(get_dir_ent(&the_fs->root_dir, fn, &file_ent)) {
        ret = NOT_FOUND;
        goto error;
    }

    unsigned short addr = file_ent.file_addr;
    unsigned short next_addr = 0;
    size_t offset = 0;
    int cyl, sect;
    while(!get_next_addr(&the_fs->the_fat, addr, &next_addr) && offset < ds) {
        cyl = to_cylinder_number(addr);
        sect = to_sector_number(addr);
        read_sector(cyl, sect, data + offset);
        offset += BYTES_PER_SECTOR;
        addr = next_addr;
    }
    if(offset >= ds) {
        printf("looks like data cant hold the whole file!\n");
        goto error;
    }
    cyl = to_cylinder_number(addr);
    sect = to_sector_number(addr);
    char leftover[BYTES_PER_SECTOR];
    read_sector(cyl, sect, leftover);
    size_t to_read = (ds - offset > BYTES_PER_SECTOR) ? BYTES_PER_SECTOR-1 : ds - offset;
    memcpy(data + offset, leftover, to_read);
    ret = 0;

error:
    free(the_fs);
    return ret;
}


int save(char* fn, void* data, size_t ds){
    struct fs *the_fs = malloc(sizeof(struct fs));
    int ret = 5;
    load_fs(the_fs);
    struct dir_ent file_ent;
    unsigned short n = ds / BYTES_PER_SECTOR + 1;
    unsigned short *free_sectors = malloc(sizeof(unsigned short) * n);
    
    if(!get_dir_ent(&the_fs->root_dir, fn, &file_ent)) {
        ret = NAME_CONFLICT;
        goto error;
    };
    
    if(getn_free_sectors(&the_fs->the_fat, n, free_sectors)) {
        //printf("Looks like there aren't %d sectors available!\n", n);
        ret = NO_SPACE;
        goto error;
    }

    // now we actually store our file
    size_t offset = 0;
    unsigned short addr, next_addr;
    int cyl, sect;
    addr = free_sectors[0];
    // set the initial root_dir entry
    if(set_dir_ent(&the_fs->root_dir, fn, addr)) {
        printf("So we couldn't find an available space in our root_dir\n");
        ret = NO_SPACE;
        goto error;
    }

    for(int i = 0; i < n-1; ++i) {
        addr = free_sectors[i];
        next_addr = free_sectors[i+1];
        cyl = to_cylinder_number(addr);
        sect = to_sector_number(addr);
        if(set_fat_entry_value(&the_fs->the_fat, addr, next_addr)) {
            printf("Apparently %d is not a valid address!\n", next_addr);
            goto error;
        }

        if(write_sector(cyl, sect, data + offset)) {
            printf("Something went wrong with write_sector\n");
            goto error;
        }
        offset += BYTES_PER_SECTOR;
    }
    char leftover[BYTES_PER_SECTOR];
    memcpy(leftover, data + offset, ds - offset);
    addr = free_sectors[n-1];
    cyl = to_cylinder_number(addr);
    sect = to_sector_number(addr);
    if(set_fat_entry_value(&the_fs->the_fat, addr, END_OF_FILE)) {
        printf("We couldn't set the last addr to END_OF_FILE!\n");
        goto error;
    }

    if(write_sector(cyl, sect, leftover)) {
        printf("Something went wrong with writing the last bit of data!\n");
        goto error;
    }
    ret = 0;

error:
    store_fs(the_fs);
    free(the_fs);
    free(free_sectors);
	return ret;
}


void format() {
    // first we clear all the space in the drive
    int cyl, sect;
    char blank[BYTES_PER_SECTOR] = "";
    for(cyl = 0; cyl < CYLINDERS; ++cyl) {
        for(sect = 0; sect < SECTORS_PER_CYLINDER; ++sect) {
            int err = write_sector(cyl, sect, blank);
            if(err == BAD_CYLINDER) {
                printf("Bad cylinder: %d\n", cyl);

            } else if(err == BAD_SECTOR) {
                printf("Bad sector: %d\n", sect);
            }
        }
    }

    // now we set up the initial fat
    //struct fat *initial_fat = malloc(FAT_SIZE);
    //struct dir *root_dir = malloc(sizeof(struct dir));
    struct fs *init_fs = malloc(sizeof(struct fs));
    for(int i = 0; i < TOTAL_SECTORS; ++i) {
        init_fs->the_fat.table[i] = EMPTY;
    }
    memset(&init_fs->root_dir, 0, sizeof(struct dir));
    store_fs(init_fs);
    free(init_fs);
}


void mem_map()
{
    int cyl, sect;
    int blank[BYTES_PER_SECTOR+1] = {0};
    for(cyl = 1; cyl < CYLINDERS; ++cyl) {
        for(sect = 0; sect < SECTORS_PER_CYLINDER; ++sect) {
            char data[BYTES_PER_SECTOR];
            read_sector(cyl, sect, data);
            printf("Cylinder: %d\nSector: %d\n\tData: %s\n\n", cyl, sect, data);
        }
    }
}




























