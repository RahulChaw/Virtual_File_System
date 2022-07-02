#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <math.h>

#include "disk.h"
#include "fs.h"

#define FAT_EOC -1

struct superblock
{
	char signature[8];
	int16_t total_blocks;
	int16_t root_dir_idx;
	int16_t data_block_idx;
	int16_t total_data_blk;
	int8_t fat_blocks;
	int8_t padding[4079];
}*sb;

struct fatblock
{
	int16_t entries[2048];
}fat[10];

struct rootdirectory
{
	char file_name[16];
	int32_t file_size;
	int16_t data_blk_idx;
	int8_t padding[10];
}rd[FS_FILE_MAX_COUNT];

struct FD
{
	int fd_number;
	char filename[16];
	long offset;
	int rd_idx;
}fd_table[FS_OPEN_MAX_COUNT];

int fs_mount(const char *diskname)
{
	if(block_disk_open(diskname) == -1)
	{
		//printf("here:\n");
		return -1;
	}
	sb = (struct superblock*)malloc(sizeof(struct superblock));
	block_read(0, sb);
	int i=0;
	while(i<sb->fat_blocks)
	{
		block_read(i+1, &fat[i]);
		i++;
	}
	block_read(sb->root_dir_idx, rd);

	char empty[16] = "";
	for(int i=0; i<FS_OPEN_MAX_COUNT; i++)
	{
		memcpy(fd_table[i].filename, empty, (int)strlen(empty));
	}

	//printf("\nhere:%s\n", sb->signature);
	/* Some error checking. Ask Rahul if there is more to account for. */
	if(strncmp(sb->signature, "ECS150FS", 8)!=0 || sb->total_blocks != (int16_t)block_disk_count())
	{
		//printf("here:%d\n", strncmp(sb->signature, "ECS150FS", 8));
		//printf("names:%s, %d\n", sb->signature, (int)strlen(sb->signature));
		//printf("blk:%d, %d\n", sb->total_blocks, block_disk_count());
		return -1;
	}

	return 0;	
}

int fs_umount(void)
{
	/* Added writing back to the disk */
	if(strcmp(sb->signature, "")==0)
	{
		return -1;
	}
	for(int j=0; j<FS_OPEN_MAX_COUNT; j++)
	{
		if(strcmp(fd_table[j].filename, "")!=0)
		{
			return -1;
		}
	}
	block_write(0, sb);

	int i=0;
	while(i<sb->fat_blocks)
	{
		block_write(i+1, &fat[i]);
		i++;
	}
	block_write(sb->root_dir_idx, rd);
	
	free(sb);

	return block_disk_close();
}

int fs_info(void)
{
	/* Error checking for virtual disk open */
	if(strcmp(sb->signature, "")==0) // HELP: "no underlying virtual disk was opened" same as not mounted?
	{
		return -1;
	}

	int rd_free=0;
	int fat_free=0;

	for(int i=0; i<FS_FILE_MAX_COUNT; i++)
	{
		if(strcmp(rd[i].file_name, "")==0)
		{
			rd_free++;
		}
	}

	int fat_idx = 0;
	for(int j=0; j<sb->total_data_blk; j++)
	{
		if(fat[fat_idx].entries[j-(fat_idx*(BLOCK_SIZE/2))] == 0)
		{
			fat_free++;
		}
		if(j >= (BLOCK_SIZE/2)*fat_idx)
		{
			fat_idx++;
		}
	}

	/* Prints information about the virtual disk */
	fprintf(stdout, "FS Info:\n");
	fprintf(stdout, "total_blk_count=%d\n", sb->total_blocks);
	fprintf(stdout, "fat_blk_count=%d\n", ((sb->total_data_blk * 2) / BLOCK_SIZE) + ((sb->total_data_blk*2)%BLOCK_SIZE != 0) );
	fprintf(stdout, "rdir_blk=%d\n", sb->root_dir_idx);
	fprintf(stdout, "data_blk=%d\n", sb->data_block_idx);
	fprintf(stdout, "data_blk_count=%d\n", sb->total_data_blk);
	fprintf(stdout, "fat_free_ratio=%d/%d\n", fat_free, sb->total_data_blk); // TODO: traverse to get fat_free
	fprintf(stdout, "rdir_free_ratio=%d/%d\n", rd_free, FS_FILE_MAX_COUNT); // TODO: traverse to get rdir_free
	
	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
	/* Checking for error conditions */
	if(strcmp(sb->signature, "")==0 || strcmp(filename, "")==0 || (int)strlen(filename) > FS_FILENAME_LEN)
	{
		//printf("here:%d, %d\n", strcmp(filename, ""), ((int)strlen(filename) > FS_FILENAME_LEN));
		return -1;
	}
	/* Loop tp check if the filename already exists and find the first empty spot on root directory*/
	int i = 0;
	int idx = -1;
	while(strcmp(rd[i].file_name, filename)!=0 && i<FS_FILE_MAX_COUNT)
	{
		if(strcmp(rd[i].file_name, "")==0 && idx == -1)
		{
			idx = i;
		}
		i++;
	}
	if(idx == -1 || i<FS_FILE_MAX_COUNT)
	{
		//printf("here:2\n");
		return -1;
	}
	/* If file does not exist then creatign the file in the root directory */
	memcpy(rd[idx].file_name, filename, FS_FILENAME_LEN);
	rd[idx].file_size = 0;
	rd[idx].data_blk_idx = FAT_EOC;
	//printf("Created successfully\n");
	return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	if(strcmp(sb->signature, "")==0 || strcmp(filename, "")==0 || (int)strlen(filename) > FS_FILENAME_LEN)
	{
		return -1;
	}
	int i = 0;
	void** buf = NULL;

	/* Checking if the filename exists, if not return -1 */
	while(strcmp(rd[i].file_name, filename)!=0 && i < FS_FILE_MAX_COUNT)
	{
		i++;
	}
	if(i>=FS_FILE_MAX_COUNT)
	{
		return -1;
	}
	int v=0;
	while(strcmp(fd_table[v].filename, filename)!=0 && v<FS_OPEN_MAX_COUNT)
	{
		v++;
	}
	if(v<FS_OPEN_MAX_COUNT)
	{
		return -1;
	}
	/* Overwriting all the datablocks of the file by NULL (deleting data) */
	int j = rd[i].data_blk_idx;
	int x = (j-(j%(BLOCK_SIZE/2))) / (BLOCK_SIZE/2);
	char empty_fname[16] = "";

	while(j != FAT_EOC)
	{
		block_write(j + sb->data_block_idx, buf);
		int entry_idx = j-(BLOCK_SIZE/2)*x;
		j = fat[x].entries[entry_idx];
		fat[x].entries[entry_idx] = 0;
		if(j >= (BLOCK_SIZE/2)*x)
		{
			x = (j-(j%(BLOCK_SIZE/2)))/(BLOCK_SIZE/2);
		}
	}
	/* Resetting the root directory entry */
	*rd[i].file_name = *empty_fname;
	rd[i].file_size = 0;
	rd[i].data_blk_idx = FAT_EOC;
	return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	if(strcmp(sb->signature, "")==0)
	{
		return -1;
	}
	int i=0;
	/* Printing out all the filenames in the disk */
	fprintf(stdout, "FS Ls:\n");
	while(strcmp(rd[i].file_name, "")!=0)
	{
		fprintf(stdout, "file: %s, size: %d, data_blk: %d \n", rd[i].file_name, 
			rd[i].file_size, (uint16_t)rd[i].data_blk_idx);
		i++;
	}
	return 0;
}

int fs_open(const char *filename)
{
	/* Error checking */
	if(strcmp(sb->signature, "")==0 || strcmp(filename, "")==0 || (int)strlen(filename) > FS_FILENAME_LEN)
	{
		//printf("here:1\n");
		return -1;
	}
	/* Check if file in root directory */
	int rd_idx = 0;
	//int idx = -1;
	while(strcmp(filename, rd[rd_idx].file_name)!=0 && rd_idx < FS_FILE_MAX_COUNT)
	{
		//printf("%s\t", rd[rd_idx].file_name);
/*		if(strcmp(filename, "")==0 && idx != -1)
		{
			idx = rd_idx;
		}
*/		rd_idx++;
	}
	//printf("name:%s, %d\n", filename, (int)strlen(filename));
	if(rd_idx >= FS_FILE_MAX_COUNT)
	{
		//printf("\nhere:2\n");
		return -1;
	}

	/* Find first available spot to add new file to fd_table */
	int idx = 0;
	while(strcmp(fd_table[idx].filename, "")!=0 && idx < FS_OPEN_MAX_COUNT)
	{
		idx++;
	}

	if(idx >= FS_OPEN_MAX_COUNT)
	{
		//printf("here:3\n");
		return -1;
	}

	fd_table[idx].fd_number = idx;
	memcpy(fd_table[idx].filename, filename, (int)strlen(filename));
	//*fd_table[idx].filename = *filename;
	fd_table[idx].offset = 0;
	fd_table[idx].rd_idx = rd_idx;
	return idx;
}

int fs_close(int fd)
{

	/* Error checking */
	if(strcmp(sb->signature, "")==0 || fd < 0 || fd >= FS_OPEN_MAX_COUNT || strcmp(fd_table[fd].filename, "")==0)
	{
		return -1;
	}

	/* Clears contents */
	char empty[FS_FILENAME_LEN] = "";
	memcpy(fd_table[fd].filename, empty, FS_FILENAME_LEN);
	fd_table[fd].offset = 0;
	fd_table[fd].rd_idx = -1;

	return 0;
}

int fs_stat(int fd)
{
	/* Error checking */
	if(strcmp(sb->signature, "")==0 || fd < 0 || fd >= FS_OPEN_MAX_COUNT || strcmp(fd_table[fd].filename, "")==0)
	{
		return -1;
	}

	return rd[fd_table[fd].rd_idx].file_size;
}

int fs_lseek(int fd, size_t offset)
{
	/* Error checking */
	if(strcmp(sb->signature, "")==0 || fd < 0 || fd >= FS_OPEN_MAX_COUNT ||
		strcmp(fd_table[fd].filename, "")==0 || (int)offset > rd[fd_table[fd].rd_idx].file_size)
	{
		return -1;
	}

	/* Set offset */
	fd_table[fd].offset = offset;
	return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */

    if(	strcmp(sb->signature, "")==0 || fd < 0 || fd >= FS_OPEN_MAX_COUNT ||
		strcmp(fd_table[fd].filename, "")==0 || buf == NULL)
    {
        return -1;
    }

	void** bounce_buf = NULL;
	int i = fd_table[fd].rd_idx;
	int offset = fd_table[fd].offset;
    	int index = rd[i].data_blk_idx;
	if(index ==  FAT_EOC && (int)count > 0)
	{

		int x = 0;
		int current_fat = 0;
		while(fat[current_fat].entries[x] != 0 && (x+((BLOCK_SIZE/2)*current_fat)) < sb->total_data_blk)
		{
			x++;
			
			if(x == (BLOCK_SIZE/2)-1)
			{
				current_fat++;
				x = 0;
			}

		}
		if((x+((BLOCK_SIZE/2)*current_fat)) >= sb->total_data_blk)
		{
			return -1;
		}
		index = x + current_fat*(BLOCK_SIZE/2);
		fat[current_fat].entries[x] = FAT_EOC;
		rd[i].data_blk_idx = index;
	}
    	int fat_idx = (index-(index%(BLOCK_SIZE/2))) / (BLOCK_SIZE/2);
    	int size = rd[i].file_size;
	int bytes_written = 0;
	int data_blk = (offset - (offset % BLOCK_SIZE)) / BLOCK_SIZE;
	int idx_in_db = offset - (BLOCK_SIZE * data_blk);

	//printf("offset:%d \nindex:%d \nfat_idx:%d \nsize:%d \nbytes_written:%d \ndata_blk:%d \nidx_in_db:%d \n", offset, index, fat_idx, size, bytes_written, data_blk, idx_in_db);

	for(int k = 0; k < data_blk; k++)
	{
		index = fat[fat_idx].entries[index - (BLOCK_SIZE/2)*fat_idx];
		fat_idx = (index-(index%(BLOCK_SIZE/2))) / (BLOCK_SIZE/2);
	}

	bounce_buf = (void*)malloc(BLOCK_SIZE);
	
	while(count != 0)
	{
		//bounce_buf = (void*)malloc(BLOCK_SIZE);
		block_read(index+sb->data_block_idx, bounce_buf);

		if((int)count > BLOCK_SIZE - idx_in_db)
		{
			//printf("bounce:%d, buf%d, idx_in_db:%d, byte:%d\n", (int)strlen(*bounce_buf), (int)strlen(buf), idx_in_db, bytes_written);
			//printf("idx_in_db:%d, bytes_written:%d\n", idx_in_db, bytes_written);
			memcpy(bounce_buf + idx_in_db, &buf + bytes_written, BLOCK_SIZE - idx_in_db);
			block_write(index+sb->data_block_idx, bounce_buf);

			bytes_written += BLOCK_SIZE - idx_in_db;
			count -= BLOCK_SIZE - idx_in_db;
		}
		else
		{
			memcpy(bounce_buf + idx_in_db, &buf + bytes_written, count);
			block_write(index+sb->data_block_idx, bounce_buf);

			bytes_written += count;
			count -= count;
		}

		if(count != 0 && fat[fat_idx].entries[index - (BLOCK_SIZE/2)*fat_idx] == FAT_EOC)
		{
			int x = 0;
			int current_fat = 0;
			while(fat[current_fat].entries[x] != 0 && (x+((BLOCK_SIZE/2)*current_fat)) < sb->total_data_blk)
			{
				x++;
				
				if(x == (BLOCK_SIZE/2)-1)
				{
					current_fat++;
					x = 0;
				}

			}
			if((x+((BLOCK_SIZE/2)*current_fat)) >= sb->total_data_blk)
			{
				return -1;
			}
			fat[fat_idx].entries[index - (BLOCK_SIZE/2)*fat_idx] = x + current_fat*(BLOCK_SIZE/2);
			fat[current_fat].entries[x] = FAT_EOC;
		}

		index = fat[fat_idx].entries[index - (BLOCK_SIZE/2)*fat_idx];
		fat_idx = (index-(index%(BLOCK_SIZE/2))) / (BLOCK_SIZE/2);
		data_blk = ((bytes_written + offset) - ((bytes_written + offset) % BLOCK_SIZE)) / BLOCK_SIZE;
		idx_in_db = (bytes_written + offset) - (BLOCK_SIZE * data_blk);
		//free(bounce_buf);
		//bounce_buf = NULL;

		//printf("offset:%d \nindex:%d \nfat_idx:%d \nsize:%d \nbytes_written:%d \ndata_blk:%d \nidx_in_db:%d \n", offset, index, fat_idx, size, bytes_written, data_blk, idx_in_db);
	}
	free(bounce_buf);

	if(bytes_written + offset > size)
	{
		rd[i].file_size = bytes_written + offset;
	}

	fs_lseek(fd, bytes_written + offset);
	return bytes_written;
}

int fs_read(int fd, void *buf, size_t count)
{
	
    if(	strcmp(sb->signature, "")==0 || fd < 0 || fd >= FS_OPEN_MAX_COUNT ||
		strcmp(fd_table[fd].filename, "")==0 || buf == NULL)
    {
        return -1;
    }

	void** bounce_buf = NULL;
	int i = fd_table[fd].rd_idx;
	int offset = fd_table[fd].offset;
    int index = rd[i].data_blk_idx;
    int fat_idx = (index-(index%(BLOCK_SIZE/2))) / (BLOCK_SIZE/2);
    int size = rd[i].file_size;
	int bytes_read = 0;
	int data_blk = (offset - (offset % BLOCK_SIZE)) / BLOCK_SIZE;
	int idx_in_db = offset - (BLOCK_SIZE * data_blk);
	 
	for(int k = 0; k < data_blk; k++)
	{
		index = fat[fat_idx].entries[index - (BLOCK_SIZE/2)*fat_idx];
		fat_idx = (index-(index%(BLOCK_SIZE/2))) / (BLOCK_SIZE/2);
	}

	bounce_buf = (void*)malloc(BLOCK_SIZE);
    while(index != FAT_EOC && count != 0)
    {
		block_read(index+sb->data_block_idx, bounce_buf);

		if((int)count > BLOCK_SIZE - idx_in_db)
		{
			memcpy(&buf + bytes_read, bounce_buf + idx_in_db, BLOCK_SIZE - idx_in_db);
			bytes_read += BLOCK_SIZE - idx_in_db;
			count -= (BLOCK_SIZE - idx_in_db);
		}
		else if(offset + (int)count > size)
		{
			memcpy(&buf + bytes_read, bounce_buf + idx_in_db, size - offset);
			bytes_read += (size - offset);
			count -= (size - offset);
		}
		else
		{
			memcpy(&buf + bytes_read, bounce_buf + idx_in_db, count);
			bytes_read += count;
			count -= count;
		}

		index = fat[fat_idx].entries[index - (BLOCK_SIZE/2)*fat_idx];
		fat_idx = (index-(index%(BLOCK_SIZE/2))) / (BLOCK_SIZE/2);
		data_blk = ((bytes_read + offset) - ((bytes_read + offset) % BLOCK_SIZE)) / BLOCK_SIZE;
		idx_in_db = (bytes_read + offset) - (BLOCK_SIZE * data_blk);
		
    }
	free(bounce_buf);

	fs_lseek(fd, bytes_read + offset);

    return bytes_read;
}
