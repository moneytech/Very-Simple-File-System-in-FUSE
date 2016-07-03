/*
  gcc -Wall vsfs_rev03.c diskHandler_rev01.c `pkg-config fuse --cflags --libs` -o vsfs
*/

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "ourFS.h"


int disk_ready;

FILE *disk_file;
FILE *log_file;

extern int my_disk_log (int func);
extern int my_disk_write (const void *buf, int block_no);
extern int my_disk_read (void *buf, int block_no);

struct inode
{
	unsigned int 	inode_number;
	mode_t				mode;
	nlink_t 			nlink;
	unsigned int 	block_cnt;
	off_t 				size;
	unsigned int 	block[15];
	char 					full_path[128];
  // char garbage[28];
};
typedef struct inode inode;

struct dirent {
  unsigned int   d_ino;       /* Inode number */
  off_t          d_offset;
  unsigned int   d_valid;
  unsigned char  d_type;      /* Type of file; not supported by all filesystem types */
  char           d_name[128]; /* Null-terminated filename */
};
typedef struct dirent dirent;

#define inode_per_block     (int)(BLOCK_SIZE / (int)sizeof(inode))     // inode per block = 42 // sizeof inode: 96
#define dirent_per_block  (int)(BLOCK_SIZE / (int)sizeof(dirent))   //dirent per block = 26 // sizeof dirent: 152
#define num_inode 1024
#define blk_per_inode 8
#define first_data_block 26
#define first_inode_block 1
#define total_data_blocks (num_inode * blk_per_inode)

struct blk_inode_array {
  inode inode_array[inode_per_block];
};
typedef struct blk_inode_array blk_inode_array;

struct blk_dirent_array {
  dirent dirent_array[dirent_per_block];
};
typedef struct blk_dirent_array blk_dirent_array;


struct superblock {
  unsigned char inode_bitmap[num_inode];
  unsigned char dblk_bitmap[total_data_blocks];
};
typedef struct superblock superblock;


inode get_inode(const char *path)
{
	inode _inode;
	dirent _dirent;
	char deli[2] = "/";
	char *current_token;
	int blk_it,size_it;
	char local_block[BLOCK_SIZE];
	int num_de_read = 0;
  int inode_found = 0;
	unsigned int local_inode_num=1;
	char* _path = strdup(path);
	fprintf (log_file, "get_inode with path: %s\n", path);
	fflush(log_file);
// read root inode block and then read root inode from the block
	my_disk_read((void*)local_block, first_inode_block);
	memcpy ((void *)&_inode, (void *)local_block, sizeof(_inode));

	current_token = strtok (_path, deli);
  while (current_token != NULL)
  {
    // it is not NULL second time. when it should be NULL
    fprintf (log_file, "get_inode while with token: %s with length: %d\n", current_token, (int)(strlen(current_token)));
    fflush(log_file);
    //current_token = strtok (NULL, "/");
    blk_it = 0;
    for (blk_it = 0; blk_it <  _inode.block_cnt; blk_it++)
    {

      fprintf (log_file, "get_inode read block %d blk_cnt: %d\n", blk_it,  _inode.block_cnt);
      fflush(log_file);

			my_disk_read((void*)local_block, _inode.block[blk_it]); // read data block of the inode
      for (size_it = 0; (size_it < dirent_per_block) && (num_de_read < (int)_inode.size); size_it++)
      {
        fprintf (log_file, "get_inode read directory entry %d\n", size_it);
        fflush(log_file);

				memcpy ((void *)&_dirent, (void *)&local_block[sizeof(_dirent) * (size_it % dirent_per_block)],  sizeof(_dirent) );

				if(_dirent.d_valid == 0)
					continue;
				num_de_read++;
        fprintf (log_file, "get_inode d_name: '%s'\n", _dirent.d_name);
        fflush(log_file);
				fprintf (log_file, "get_inode d_ino: '%d'\n", _dirent.d_ino);
        fflush(log_file);
        fprintf (log_file, "get_inode current_token: '%s'\n", current_token);
        fflush(log_file);
        if (strcmp(_dirent.d_name, current_token) == 0)
        {
          fprintf (log_file, "get_inode directory: '%s' inode_number: %d\n", current_token,  _dirent.d_ino);
          fflush(log_file);
            inode_found = 1;
            local_inode_num =  _dirent.d_ino;
            break;
        }
      // if (size_it % (BLOCK_SIZE/dirent_size) == 0)
      //   blk_it++;
      }
      fprintf (log_file, "get_inode break from inner for loop inode_found: %d!\n", inode_found);
      fflush(log_file);
      if(inode_found)
      {
				num_de_read = 0;
        fprintf (log_file, "get_inode inside IF statement: inode_found: %d!\n", inode_found);
        fflush(log_file);
        break;
      }
    }

    fprintf (log_file, "get_inode out of IF: inode_found: %d!\n", inode_found);
    fflush(log_file);
    if(!inode_found)
    {
      //Error: We shouldn't be here.
      fprintf (log_file, "get_inode No directory found!\n");
      fflush(log_file);
      // return -ENOENT;
    }
    fprintf (log_file, "get_inode break here\n");
    fflush(log_file);
		my_disk_read((void*)local_block, (local_inode_num / inode_per_block) + 1);
		memcpy ((void *)&_inode, (void *)&local_block[sizeof(_inode) * (local_inode_num % inode_per_block)], sizeof(_inode));


    current_token = strtok (NULL, deli); // after /foo, it should be zero, but it's not
    if (current_token == NULL)
    {

      fprintf (log_file, "NULL returned \n");
      fflush(log_file);
    }
  }

	return _inode;
}



static void* vsfs_init(struct fuse_conn_info *conn)
{
	int counter;
	char tmp;
	superblock  _sb;
	inode root_inode;
	blk_inode_array root_inode_array;
	blk_dirent_array root_dirent_array;
	char local_block[BLOCK_SIZE];

	my_disk_log (INIT);
  	if (disk_ready == 0)
  	{
    		fprintf (log_file, "vsfs_init called disk is not ready\n");
    		fflush(log_file);

	   	for (counter = 0; counter < 2000000; counter++)
	    	{
	      		tmp = (char)(0);
	      		fwrite (&tmp, 1, 1, disk_file);
	    	}
		fprintf (log_file, "Disk initialized!\n");
    		fflush(log_file);
    		for (counter = 0; counter < num_inode; counter++)
    		{
      			_sb.inode_bitmap[counter] = 0;
    		}
		fprintf (log_file, "inode bitmap array initialized!\n");
    		fflush(log_file);
    		for (counter = 0; counter < total_data_blocks; counter++)
    		{
      			_sb.dblk_bitmap[counter] = 0;
    		}
		fprintf (log_file, "disk blocks bitmap array initialized!\n");
    		fflush(log_file);

		_sb.inode_bitmap[0] = 1; //for root inode
		_sb.dblk_bitmap[0] = 1;
// -----------------Newly added -------------
		_sb.inode_bitmap[1] = 1; //for foo inode
		_sb.dblk_bitmap[1] = 1;

		_sb.inode_bitmap[2] = 1; //for hello inode
		_sb.dblk_bitmap[2] = 1;
// ----------------------------------------
		fprintf (log_file, "superblock bitmaps set!\n");
		fflush(log_file);
    // Write in a superblock
		fprintf (log_file, "size of superblock: %d!\n", (int)sizeof(_sb));
		fflush(log_file);

    		memcpy ((void *)local_block, (void *)&_sb, sizeof(_sb));
		fprintf (log_file, "Calling disk write!\n");
    		fflush(log_file);
    		my_disk_write((void*)local_block, 0);
		fprintf (log_file, "Disk Write Returned!\n");
    		fflush(log_file);

				// Write a root inode
    		root_inode.inode_number = 0;
    		root_inode.mode = S_IFDIR | 0755;
    		root_inode.nlink = 3;
				root_inode.block_cnt = 1;
				root_inode.size = 3;
				root_inode.block[0] = first_data_block;
				strcpy(root_inode.full_path, "/");
				// put inode in to inode array
    		root_inode_array.inode_array[0] = root_inode;

//-------------------- Newly Added ----------------------------
				// directory in root's inode "foo"
				root_inode.inode_number = 1;
    		root_inode.mode = S_IFDIR | 0755;
    		root_inode.nlink = 2;
				root_inode.block_cnt = 1;
				root_inode.size = 3;
				root_inode.block[0] = first_data_block + 1; //27
				strcpy(root_inode.full_path, "/.Trash");
				// put inode in to inode array
    		root_inode_array.inode_array[1] = root_inode;

				// file's inode inside directory "hello"
				root_inode.inode_number = 2;
    		root_inode.mode = S_IFREG | 0444;
    		root_inode.nlink = 1;
				root_inode.block_cnt = 1;
				root_inode.size = 5;
				root_inode.block[0] = first_data_block + 2; //28
				strcpy(root_inode.full_path, "/.Trash/hello");
				// put inode in to inode array
    		root_inode_array.inode_array[2] = root_inode;
// -------------------------------------------------------------------

    		memcpy ((void *)local_block, (void *)&root_inode_array, sizeof(root_inode_array));
    		my_disk_write((void*)local_block, first_inode_block);

    		root_dirent_array.dirent_array[0].d_ino = 0;
    		root_dirent_array.dirent_array[0].d_offset = 0;
    		root_dirent_array.dirent_array[0].d_valid = 1;
    		root_dirent_array.dirent_array[0].d_type = 0;
    		strcpy(root_dirent_array.dirent_array[0].d_name, ".");

    		root_dirent_array.dirent_array[1].d_ino = 0;
    		root_dirent_array.dirent_array[1].d_offset = 0;
    		root_dirent_array.dirent_array[1].d_valid = 1;
    		root_dirent_array.dirent_array[1].d_type = 0;
    		strcpy(root_dirent_array.dirent_array[1].d_name, "..");

				for (counter = 2; counter < dirent_per_block; counter++)
    		{
      			root_dirent_array.dirent_array[counter].d_valid = 0;
    		}

// ------------------------ newly added --------------------------------
    		root_dirent_array.dirent_array[2].d_ino = 1;
    		root_dirent_array.dirent_array[2].d_offset = 0;
    		root_dirent_array.dirent_array[2].d_valid = 1;
    		root_dirent_array.dirent_array[2].d_type = 0;
    		strcpy(root_dirent_array.dirent_array[2].d_name, ".Trash");
// --------------------------------------------------------------------


    		memcpy ((void *)local_block, (void *)&root_dirent_array, sizeof(root_dirent_array));
    		my_disk_write((void*)local_block, first_data_block);


// ------------------------ newly added --------------------------------
				root_dirent_array.dirent_array[0].d_ino = 1;
				root_dirent_array.dirent_array[0].d_offset = 0;
				root_dirent_array.dirent_array[0].d_valid = 1;
				root_dirent_array.dirent_array[0].d_type = 0;
				strcpy(root_dirent_array.dirent_array[0].d_name, ".");

				root_dirent_array.dirent_array[1].d_ino = 0;
				root_dirent_array.dirent_array[1].d_offset = 0;
				root_dirent_array.dirent_array[1].d_valid = 1;
				root_dirent_array.dirent_array[1].d_type = 0;
				strcpy(root_dirent_array.dirent_array[1].d_name, "..");

				// file directory entry
				root_dirent_array.dirent_array[2].d_ino = 2;
    		root_dirent_array.dirent_array[2].d_offset = 0;
    		root_dirent_array.dirent_array[2].d_valid = 1;
    		root_dirent_array.dirent_array[2].d_type = 0; // TODO: type must be set
    		strcpy(root_dirent_array.dirent_array[2].d_name, "hello");

				for (counter = 3; counter < dirent_per_block; counter++)
				{
						root_dirent_array.dirent_array[counter].d_valid = 0;
				}

    		memcpy ((void *)local_block, (void *)&root_dirent_array, sizeof(root_dirent_array));
    		my_disk_write((void*)local_block, first_data_block+1); //block 27: Will be written in directoy's data block
// ---------------------------------------------------------------------------
    		fprintf (log_file, "vsfs_init called disk is ready now\n");
    		fflush(log_file);
  	}
  	else
  	{
    		fprintf (log_file, "vsfs_init called disk is ready\n");
    		fflush(log_file);
  	}
	//my_disk_log(INIT);
	return NULL;
}

static int vsfs_getattr(const char *path, struct stat *stbuf)
{

	fprintf (log_file, "vsfs_getattr called with path: %s\n", path);
	fflush(log_file);
	inode _inode;
	int res = 0;


	_inode = get_inode(path);
	fprintf (log_file, "vsfs_getattr inode_number read for path: %s\n", _inode.full_path);
	fflush(log_file);

	if(strcmp(_inode.full_path, path))
	{
		fprintf (log_file, "vsfs_getattr inode NOT found for path: %s\n", path);
		fflush(log_file);
		return -ENOENT;
	}

	stbuf->st_mode = _inode.mode;
	stbuf->st_nlink = _inode.nlink;
	stbuf->st_size = _inode.size;

	fprintf (log_file, "vsfs_getattr inode_mode: %d\n", _inode.mode);
	fflush(log_file);

	fprintf (log_file, "vsfs_getattr inode_nlink: %d\n", (int)_inode.nlink);
	fflush(log_file);

	fprintf (log_file, "vsfs_getattr inode_size: %d\n", (int)_inode.size);
	fflush(log_file);

	fprintf (log_file, "vsfs_getattr done\n");
	fflush(log_file);

	return res;
}

static int vsfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{
	fprintf (log_file, "\t\t\t\t\tvsfs_readdir called with path: %s\n", path);
	fflush(log_file);
	(void) offset;
	(void) fi;
	inode _inode;
	char local_block[BLOCK_SIZE];
	int blk_it, size_it;
	dirent _dirent;
	int num_de_read;

	if (path[0] == '/' && path[1] == '.')
	{
		return -ENOENT;
	}
	else if (strcmp(path,"/autorun.inf") == 0)
	{
		return -ENOENT;
	}

	_inode  = get_inode(path);
	fprintf (log_file, "\t\t\t\t\tvsfs_readdir inode_number read: %d\n", _inode.inode_number);
	fflush(log_file);


  num_de_read = 0;
  for (blk_it = 0; blk_it <  _inode.block_cnt; blk_it++)
  {
    fprintf (log_file, "\t\t\t\t\tvsfs_readdir 2 read block %d \n", blk_it );
    fflush(log_file);

		my_disk_read((void*)local_block, _inode.block[blk_it]); // read data block of the inode
    for (size_it = 0; (size_it < dirent_per_block) && (num_de_read < (int)_inode.size); size_it++)
    {
			memcpy ((void *)&_dirent, (void *)&local_block[sizeof(_dirent) * (size_it % dirent_per_block)],  sizeof(_dirent) );
      fprintf (log_file, "\t\t\t\t\tvsfs_readdir 2 ino: %d name: %s\n", _dirent.d_ino, _dirent.d_name);
      fflush(log_file);
			if (_dirent.d_valid == 0)
				continue;
			num_de_read++;
			// Do your stuff here

			struct stat st;
			memset(&st, 0, sizeof(st));
			st.st_ino = _dirent.d_ino;
  		st.st_mode = _inode.mode;
  		filler(buf, _dirent.d_name, NULL, 0);
      fprintf (log_file, "\t\t\t\t\tvsfs_readdir filler filled with name: %s ino: %d\n", _dirent.d_name, _dirent.d_ino);
      fflush(log_file);
    }

  }

	fprintf (log_file, "\t\t\t\t\tvsfs_readdir done\n");
	fflush(log_file);


	return 0;
}




static int vsfs_open(const char *path, struct fuse_file_info *fi)
{
	inode _inode;
	fprintf (log_file, "vsfs_open called with path: %s\n", path);
	fflush(log_file);

	_inode = get_inode(path);
	if (strcmp(path, _inode.full_path) == 0)
	{
		return 0;
	}
	else
		return -1;


	return 0;
}

static int vsfs_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{

	inode _inode;
	char local_block[BLOCK_SIZE];

	fprintf (log_file, "vsfs_read called with path: %s and size %d and offset: %d\n", path, (int)size, (int)offset);
	fflush(log_file);

	_inode = get_inode(path);


	my_disk_read((void*)local_block, _inode.block[0]);
	memcpy( (void *)buf, (void *)(local_block + offset), size);

	fprintf (log_file, "vsfs_read done\n");
	fflush(log_file);


	return size;

}


static int vsfs_write(const char* path, const char *buf, size_t size, off_t offset, struct fuse_file_info* fi)
{
	inode _inode;
	blk_inode_array _inode_array;
	char local_block[BLOCK_SIZE];

	fprintf (log_file, "vsfs_write called with path: %s and size %d and offset: %d\n", path, (int)size, (int)offset);
	fflush(log_file);

	_inode = get_inode(path);

	my_disk_read((void*)local_block, (_inode.inode_number / inode_per_block) + 1);
	memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

	fprintf (log_file, "vsfs_write old size: %d\n",(int)_inode_array.inode_array[_inode.inode_number % inode_per_block].size);
	fflush(log_file);

	_inode_array.inode_array[_inode.inode_number % inode_per_block].size = size + offset ;

	memcpy((void *)local_block, (void *)&_inode_array,  sizeof(_inode_array));
	my_disk_write((void*)local_block, (_inode.inode_number / inode_per_block) + 1);

	my_disk_read((void*)local_block, _inode.block[0]);
	memcpy((void *)(local_block + offset), (void *)buf, size);
	my_disk_write((void*)local_block, _inode.block[0]);


	return size;
}

static int vsfs_mknod(const char* path, mode_t mode, dev_t rdev)
{
	char _path[128];
	char _parent_path[128] = "";
	char *token1, *token2;
	inode _inode;
	char local_block[BLOCK_SIZE];
	int blk_it, size_it;
	dirent _dirent;
	// int num_de_read;
	superblock _superblock;
	int i = 0;
	int _new_block;
	int _new_inode;
	blk_inode_array _inode_array;
	blk_dirent_array _dirent_array;


	fprintf (log_file, "vsfs_mknod called with path: %s\n", path);
	fflush(log_file);
	fprintf (log_file, "vsfs_mknod called with mode: %x\n", mode);
	fflush(log_file);
	fprintf (log_file, "vsfs_mknod called with rdev: %d\n", (int)rdev);
	fflush(log_file);

	strcpy (_path, path);
	token1 = strtok (_path, "/");

	while (token1 != NULL)
	{
		token2 = strtok (NULL, "/");
		if (token2 != NULL)
		{
			strcat (_parent_path, "/");
			strcat (_parent_path, token1);
		}
		else
			break;
		token1 = token2;
	}
	fprintf (log_file, "vsfs_mknod called with parent path: %s\n", _parent_path);
	fflush(log_file);

	// read superblock from the disk
	my_disk_read((void*)local_block,0);
	memcpy ((void *)&_superblock, (void *)local_block,  sizeof(_superblock));
	// find the empty data block
	for (i = 0; i < total_data_blocks; i++)
	{
		if (_superblock.dblk_bitmap[i] == 0)
		{
			_new_block = i;
			_superblock.dblk_bitmap[i] = 1;
			memset((void *)local_block, '\0',BLOCK_SIZE);
			my_disk_write((void*)local_block, _new_block + first_data_block);
			break;
		}
	}

	fprintf (log_file, "\t\t\t\t\tvsfs_mknod new block %d \n", _new_block + first_data_block);
	fflush(log_file);
	// find empty inode
	for (i = 0; i < num_inode; i++)
	{
		if (_superblock.inode_bitmap[i] == 0)
		{
			_superblock.inode_bitmap[i] = 1;
			_new_inode = i;
			break;
		}
	}
	fprintf (log_file, "\t\t\t\t\tvsfs_mknod new inode %d \n", _new_inode );
	fflush(log_file);

	// write updated superblock back to disk
	memcpy ((void *)local_block, (void *)&_superblock,  sizeof(_superblock));
	my_disk_write((void*)local_block,0);

	// fetch the empty inode's block
	my_disk_read((void*)local_block, (_new_inode / inode_per_block) + 1);
	memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

// creat a new inode for new file
	_inode_array.inode_array[_new_inode % inode_per_block].inode_number = _new_inode ;
	_inode_array.inode_array[_new_inode % inode_per_block].mode = S_IFREG | 0444;
	_inode_array.inode_array[_new_inode % inode_per_block].nlink = 1;
	_inode_array.inode_array[_new_inode % inode_per_block].block_cnt = 1;
	_inode_array.inode_array[_new_inode % inode_per_block].size = 0;
	_inode_array.inode_array[_new_inode % inode_per_block].block[0] = _new_block+ first_data_block;
	strcpy(_inode_array.inode_array[_new_inode % inode_per_block].full_path, path);

	memcpy((void *)local_block, (void *)&_inode_array, sizeof(_inode_array));
	my_disk_write((void*)local_block, (_new_inode / inode_per_block) + 1);

	// get the Inode of the parent directory
	_inode = get_inode(_parent_path);

	// num_de_read = 0;
  for (blk_it = 0; blk_it <  _inode.block_cnt; blk_it++)
  {
    fprintf (log_file, "\t\t\t\t\tvsfs_mknod read block %d \n", _inode.block[blk_it] );
    fflush(log_file);

		my_disk_read((void*)local_block, _inode.block[blk_it]); // read data block of the inode
    for (size_it = 0; size_it < dirent_per_block; size_it++)
    {
			// memcpy ((void *)&_dirent, (void *)&local_block[sizeof(_dirent) * (size_it % dirent_per_block)],  sizeof(_dirent) );
			memcpy ((void *)&_dirent_array, (void *)local_block,  sizeof(_dirent_array));
			_dirent = _dirent_array.dirent_array[size_it % dirent_per_block];
			// Do your stuff

			if (_dirent.d_valid == 0)
			{
				_dirent.d_ino = _new_inode;
				_dirent.d_offset = 0;
				_dirent.d_valid = 1;
				_dirent.d_type = 0;

				fprintf (log_file, "\t\t\t\t\tvsfs_mknod token %s\n", token1);
				fflush(log_file);
				strcpy(_dirent.d_name, token1);

				fprintf (log_file, "\t\t\t\t\tvsfs_mknod dirent valid %d\n", _dirent.d_valid);
				fflush(log_file);

				// disk wrie for dirent
				_dirent_array.dirent_array[size_it % dirent_per_block] = _dirent;
				memcpy ((void *)local_block, (void *)&_dirent_array,  sizeof(_dirent_array));
				my_disk_write((void*)local_block, _inode.block[blk_it]);

				// update parent inode's size
				my_disk_read((void*)local_block, (_inode.inode_number / inode_per_block) + 1);
				memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

				_inode_array.inode_array[_inode.inode_number % inode_per_block].size++;

				memcpy((void *)local_block,(void *)&_inode_array, sizeof(_inode_array));
				my_disk_write((void*)local_block, (_inode.inode_number / inode_per_block) + 1);

				break;
			}
			else
			{
				fprintf (log_file, "\t\t\t\t\tvsfs_mknod dirent ino: %d name: %s\n",_dirent.d_ino, _dirent.d_name);
				fflush(log_file);
			}

    }

  }


	fprintf (log_file, "\t\t\t\t\tvsfs_mknod done\n");
	fflush(log_file);


	return 0;
}




static int vsfs_rename(const char *from, const char *to)
{
//	int res;
	inode _inode;
	char local_block[BLOCK_SIZE];
	blk_inode_array _inode_array;
	char _parent_path[128], _path[128];
	blk_dirent_array _dirent_array;
	dirent _dirent;
	int blk_it,size_it;
	char *token1, *token2;
	inode _rename_inode;


	fprintf (log_file, "vsfs_rename called with from: %s and to %s \n", from, to);
	fflush(log_file);

	// get the inode of from
	_inode = get_inode(from);
	_rename_inode = _inode;
	my_disk_read((void*)local_block, (_inode.inode_number / inode_per_block) + 1);
	memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

	fprintf (log_file, "vsfs_rename old path: %s\n",_inode_array.inode_array[_inode.inode_number % inode_per_block].full_path);
	fflush(log_file);

	strcpy(_inode_array.inode_array[_inode.inode_number % inode_per_block].full_path, to);

	memcpy((void *)local_block, (void *)&_inode_array,  sizeof(_inode_array));
	my_disk_write((void*)local_block, (_inode.inode_number / inode_per_block) + 1);


	strcpy (_path, from);
	token1 = strtok (_path, "/");

	_parent_path[0] = 0;


		while (token1 != NULL)
		{
			token2 = strtok (NULL, "/");
			if (token2 != NULL)
			{
				strcat (_parent_path, "/");
				strcat (_parent_path, token1);
			}
			else
			{
				if (strlen(_parent_path) == 0)
				{
					strcpy (_parent_path, "/");
					// _parent_path[1] = 0;
					// _parent_path[0] = '/';
				}
				break;
			}
			token1 = token2;
		}
		fprintf (log_file, "vsfs_rename called with from's parent path: %s\n", _parent_path);
		fflush(log_file);

		_inode = get_inode(_parent_path);
		// decrease the from's parent inode's size
		my_disk_read((void*)local_block, (_inode.inode_number / inode_per_block) + 1);
		memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

		fprintf (log_file, "vsfs_rename from's parent updated size: %d\n",(int)_inode_array.inode_array[_inode.inode_number % inode_per_block].size);
		fflush(log_file);

		_inode_array.inode_array[_inode.inode_number % inode_per_block].size--;

		memcpy((void *)local_block, (void *)&_inode_array,  sizeof(_inode_array));
		my_disk_write((void*)local_block, (_inode.inode_number / inode_per_block) + 1);




		for (blk_it = 0; blk_it <  _inode.block_cnt; blk_it++)
	  {
	    fprintf (log_file, "vsfs_rename read block %d \n", _inode.block[blk_it] );
	    fflush(log_file);

			my_disk_read((void*)local_block, _inode.block[blk_it]); // read data block of the inode
	    for (size_it = 0; size_it < dirent_per_block; size_it++)
	    {

				memcpy ((void *)&_dirent_array, (void *)local_block,  sizeof(_dirent_array));
				_dirent = _dirent_array.dirent_array[size_it % dirent_per_block];
				// Do your stuff

				if ((_dirent.d_valid == 1) && (strcmp(_dirent.d_name , token1) == 0))
				{

					_dirent.d_valid = 0;

					fprintf (log_file, "vsfs_rename  from's directoy invalidated \n");
					fflush(log_file);

					// disk wrie for dirent
					_dirent_array.dirent_array[size_it % dirent_per_block] = _dirent;
					memcpy ((void *)local_block, (void *)&_dirent_array,  sizeof(_dirent_array));
					my_disk_write((void*)local_block, _inode.block[blk_it]);

					break;
				}
				else
				{
					fprintf (log_file, "vsfs_rename dirent ino: %d name: %s\n",_dirent.d_ino, _dirent.d_name);
					fflush(log_file);
				}

	    }

			_parent_path[0] = 0;
			strcpy (_path, to);
			token1 = strtok (_path, "/");

				while (token1 != NULL)
				{
					token2 = strtok (NULL, "/");
					if (token2 != NULL)
					{
						strcat (_parent_path, "/");
						strcat (_parent_path, token1);
					}
					else
					{
						if (strlen(_parent_path) == 0)
						{
							strcpy (_parent_path, "/");
							// _parent_path[1] = 0;
							// _parent_path[0] = '/';
						}
						break;
					}
					token1 = token2;
				}
			// strcpy(_dirent.d_name, token1);
			_inode = get_inode(_parent_path);

			// increase the to's parent inode's size
			my_disk_read((void*)local_block, (_inode.inode_number / inode_per_block) + 1);
			memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

			_inode_array.inode_array[_inode.inode_number % inode_per_block].size++;

			fprintf (log_file, "vsfs_rename to's updated size: %d\n",(int) _inode_array.inode_array[_inode.inode_number % inode_per_block].size);
			fflush(log_file);

			memcpy((void *)local_block, (void *)&_inode_array,  sizeof(_inode_array));
			my_disk_write((void*)local_block, (_inode.inode_number / inode_per_block) + 1);


			for (blk_it = 0; blk_it <  _inode.block_cnt; blk_it++)
			{
				fprintf (log_file, "vsfs_rename to's read block %d \n", _inode.block[blk_it] );
				fflush(log_file);

				my_disk_read((void*)local_block, _inode.block[blk_it]); // read data block of the inode
				for (size_it = 0; size_it < dirent_per_block; size_it++)
				{
					// memcpy ((void *)&_dirent, (void *)&local_block[sizeof(_dirent) * (size_it % dirent_per_block)],  sizeof(_dirent) );
					memcpy ((void *)&_dirent_array, (void *)local_block,  sizeof(_dirent_array));
					_dirent = _dirent_array.dirent_array[size_it % dirent_per_block];
					// Do your stuff

					if (_dirent.d_valid == 0)
					{

						_dirent.d_ino = _rename_inode.inode_number;
						_dirent.d_valid = 1;
						_dirent.d_type = 0;
						_dirent.d_offset = 0;
						strcpy(_dirent.d_name, token1);

						fprintf (log_file, "vsfs_rename to's from's directoy invalidated \n");
						fflush(log_file);

						// disk wrie for dirent
						_dirent_array.dirent_array[size_it % dirent_per_block] = _dirent;
						memcpy ((void *)local_block, (void *)&_dirent_array,  sizeof(_dirent_array));
						my_disk_write((void*)local_block, _inode.block[blk_it]);

						break;
					}
					else
					{
						fprintf (log_file, "vsfs_rename dirent ino: %d name: %s\n",_dirent.d_ino, _dirent.d_name);
						fflush(log_file);
					}
				}
			}
		}

	return 0;
}



static int vsfs_unlink(const char *path)
{
	// int res;
	inode _inode;
	char _path[128];
	char _parent_path[128];
	int _data_block_no;
	int _inode_number;
	char local_block[BLOCK_SIZE];
	blk_inode_array _inode_array;
	superblock _superblock;
	char *token1, *token2;
	int blk_it, size_it;
	blk_dirent_array _dirent_array;
	dirent _dirent;

	fprintf (log_file, "vsfs_unlink called with path: %s\n", path);
	fflush(log_file);

	_inode = get_inode(path);

	_inode_number = _inode.inode_number;
	_data_block_no = _inode.block[0];

	my_disk_read((void*)local_block, 0);

	memcpy((void *)&_superblock, (void *)local_block, sizeof(_superblock));
	_superblock.inode_bitmap[_inode_number] = 0;
	_superblock.dblk_bitmap[_data_block_no] = 0;

	memcpy( (void *)local_block, (void *)&_superblock, sizeof(_superblock));
	my_disk_write((void*)local_block, 0);

	//get a parent path
	strcpy (_path, path);
	token1 = strtok (_path, "/");

	_parent_path[0] = 0;

		while (token1 != NULL)
		{
			token2 = strtok (NULL, "/");
			if (token2 != NULL)
			{
				strcat (_parent_path, "/");
				strcat (_parent_path, token1);
			}
			else
			{
				if (strlen(_parent_path) == 0)
				{
					strcpy (_parent_path, "/");
					// _parent_path[1] = 0;
					// _parent_path[0] = '/';
				}
				break;
			}
			token1 = token2;
		}
		fprintf (log_file, "vsfs_unlink called with from's parent path: %s\n", _parent_path);
		fflush(log_file);

		_inode = get_inode(_parent_path);
		// decrease the from's parent inode's size
		my_disk_read((void*)local_block, (_inode.inode_number / inode_per_block) + 1);
		memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

		fprintf (log_file, "vsfs_unlink from's parent updated size: %d\n",(int)_inode_array.inode_array[_inode.inode_number % inode_per_block].size);
		fflush(log_file);

		_inode_array.inode_array[_inode.inode_number % inode_per_block].size--;

		memcpy((void *)local_block, (void *)&_inode_array,  sizeof(_inode_array));
		my_disk_write((void*)local_block, (_inode.inode_number / inode_per_block) + 1);


		// invalidate dirent
		for (blk_it = 0; blk_it <  _inode.block_cnt; blk_it++)
	  {
	    fprintf (log_file, "vsfs_unlink read block %d \n", _inode.block[blk_it] );
	    fflush(log_file);

			my_disk_read((void*)local_block, _inode.block[blk_it]); // read data block of the inode
	    for (size_it = 0; size_it < dirent_per_block; size_it++)
	    {
				// memcpy ((void *)&_dirent, (void *)&local_block[sizeof(_dirent) * (size_it % dirent_per_block)],  sizeof(_dirent) );
				memcpy ((void *)&_dirent_array, (void *)local_block,  sizeof(_dirent_array));
				_dirent = _dirent_array.dirent_array[size_it % dirent_per_block];
				// Do your stuff

				if ((_dirent.d_valid == 1) && (strcmp(_dirent.d_name , token1) == 0))
				{

					_dirent.d_valid = 0;

					fprintf (log_file, "vsfs_unlink from's directoy invalidated \n");
					fflush(log_file);

					// disk wrie for dirent
					_dirent_array.dirent_array[size_it % dirent_per_block] = _dirent;
					memcpy ((void *)local_block, (void *)&_dirent_array,  sizeof(_dirent_array));
					my_disk_write((void*)local_block, _inode.block[blk_it]);

					break;
				}
				else
				{
					fprintf (log_file, "vsfs_unlink dirent ino: %d name: %s\n",_dirent.d_ino, _dirent.d_name);
					fflush(log_file);
				}
	    }
		}

	return 0;
}


static int vsfs_mkdir(const char *path, mode_t mode)
{

	char _path[128];
	char _parent_path[128] = "";
	char *token1, *token2;
	inode _inode;
	char local_block[BLOCK_SIZE];
	int blk_it, size_it;
	dirent _dirent;
	// int num_de_read;
	superblock _superblock;
	int i = 0;
	int _new_block;
	int _new_inode;
	blk_inode_array _inode_array;
	blk_dirent_array _dirent_array;
	blk_dirent_array *_dirent_pointer;


	fprintf (log_file, "vsfs_mkdir called with path: %s\n", path);
	fflush(log_file);
	fprintf (log_file, "vsfs_mkdir called with mode: %x\n", mode);
	fflush(log_file);

	// read superblock from the disk
	my_disk_read((void*)local_block,0);
	memcpy ((void *)&_superblock, (void *)local_block,  sizeof(_superblock));
	// find empty inode
	for (i = 0; i < num_inode; i++)
	{
		if (_superblock.inode_bitmap[i] == 0)
		{
			_superblock.inode_bitmap[i] = 1;
			_new_inode = i;
			break;
		}
	}
	fprintf (log_file, "\t\t\t\t\tvsfs_mkdir new inode %d \n", _new_inode );
	fflush(log_file);

	// find the empty data block
	for (i = 0; i < total_data_blocks; i++)
	{
		if (_superblock.dblk_bitmap[i] == 0)
		{
			_new_block = i;
			_superblock.dblk_bitmap[i] = 1;
			memset((void *)local_block, '\0',BLOCK_SIZE);
			my_disk_write((void*)local_block, _new_block + first_data_block);
			break;
		}
	}

	fprintf (log_file, "\t\t\t\t\tvsfs_mkdir new block %d \n", _new_block + first_data_block );
	fflush(log_file);

	// write updated superblock back to disk
	memcpy ((void *)local_block, (void *)&_superblock,  sizeof(_superblock));
	my_disk_write((void*)local_block,0);

	// fetch the empty inode's block
	my_disk_read((void*)local_block, (_new_inode / inode_per_block) + 1);
	memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

	// creat a new inode for new file
	_inode_array.inode_array[_new_inode % inode_per_block].inode_number = _new_inode ;
	_inode_array.inode_array[_new_inode % inode_per_block].mode = S_IFDIR | 0755;
	_inode_array.inode_array[_new_inode % inode_per_block].nlink = 2;
	_inode_array.inode_array[_new_inode % inode_per_block].block_cnt = 1;
	_inode_array.inode_array[_new_inode % inode_per_block].size = 2;
	_inode_array.inode_array[_new_inode % inode_per_block].block[0] = _new_block + first_data_block;
	strcpy(_inode_array.inode_array[_new_inode % inode_per_block].full_path, path);

	memcpy((void *)local_block, (void *)&_inode_array, sizeof(_inode_array));
	my_disk_write((void*)local_block, (_new_inode / inode_per_block) + 1);

	strcpy (_path, path);
	token1 = strtok (_path, "/");

	while (token1 != NULL)
	{
		token2 = strtok (NULL, "/");
		if (token2 != NULL)
		{
			strcat (_parent_path, "/");
			strcat (_parent_path, token1);
		}
		else
		break;
		token1 = token2;
	}
	fprintf (log_file, "vsfs_mkdir called with parent path: %s\n", _parent_path);
	fflush(log_file);

	// get the Inode of the parent directory
	_inode = get_inode(_parent_path);

	// read inode's data block
	my_disk_read((void*)local_block, _new_block + first_data_block);


	_dirent_pointer = (blk_dirent_array *) local_block;

	_dirent_pointer->dirent_array[0].d_ino = _new_inode;
	_dirent_pointer->dirent_array[0].d_offset = 0;
	_dirent_pointer->dirent_array[0].d_valid = 1;
	_dirent_pointer->dirent_array[0].d_type = 0;
	strcpy(_dirent_pointer->dirent_array[0].d_name, ".");

	_dirent_pointer->dirent_array[1].d_ino = _inode.inode_number;
	_dirent_pointer->dirent_array[1].d_offset = 0;
	_dirent_pointer->dirent_array[1].d_valid = 1;
	_dirent_pointer->dirent_array[1].d_type = 0;
	strcpy(_dirent_pointer->dirent_array[1].d_name, "..");

	my_disk_write((void*)local_block, _new_block + first_data_block);


	// num_de_read = 0;
	for (blk_it = 0; blk_it <  _inode.block_cnt; blk_it++)
	{
		fprintf (log_file, "\t\t\t\t\tvsfs_mkdir read block %d \n", _inode.block[blk_it] );
		fflush(log_file);

		my_disk_read((void*)local_block, _inode.block[blk_it]); // read data block of the inode
		for (size_it = 0; size_it < dirent_per_block; size_it++)
		{
			// memcpy ((void *)&_dirent, (void *)&local_block[sizeof(_dirent) * (size_it % dirent_per_block)],  sizeof(_dirent) );
			memcpy ((void *)&_dirent_array, (void *)local_block,  sizeof(_dirent_array));
			_dirent = _dirent_array.dirent_array[size_it % dirent_per_block];
			// Do your stuff

			if (_dirent.d_valid == 0)
			{
				_dirent.d_ino = _new_inode;
				_dirent.d_offset = 0;
				_dirent.d_valid = 1;
				_dirent.d_type = 0;

				fprintf (log_file, "\t\t\t\t\tvsfs_mkidr token %s\n", token1);
				fflush(log_file);
				strcpy(_dirent.d_name, token1);

				fprintf (log_file, "\t\t\t\t\tvsfs_mkdir dirent valid %d\n", _dirent.d_valid);
				fflush(log_file);

				// disk wrie for dirent
				_dirent_array.dirent_array[size_it % dirent_per_block] = _dirent;
				memcpy ((void *)local_block, (void *)&_dirent_array,  sizeof(_dirent_array));
				my_disk_write((void*)local_block, _inode.block[blk_it]);

				// update parent inode's size
				my_disk_read((void*)local_block, (_inode.inode_number / inode_per_block) + 1);
				memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

				_inode_array.inode_array[_inode.inode_number % inode_per_block].size++;
				_inode_array.inode_array[_inode.inode_number % inode_per_block].nlink++;

				memcpy((void *)local_block,(void *)&_inode_array, sizeof(_inode_array));
				my_disk_write((void*)local_block, (_inode.inode_number / inode_per_block) + 1);

				break;
			}
			else
			{
				fprintf (log_file, "\t\t\t\t\tvsfs_mkdir dirent ino: %d name: %s\n",_dirent.d_ino, _dirent.d_name);
				fflush(log_file);
			}

		}
	}


		fprintf (log_file, "\t\t\t\t\tvsfs_mkdir done\n");
		fflush(log_file);


		return 0;
}



static int vsfs_truncate(const char *path, off_t size)
{
	// int res;
	inode _inode;
	blk_inode_array _inode_array;
	char local_block[BLOCK_SIZE];


	fprintf (log_file, "vsfs_truncate called with path: %s and size: %d\n", path, (int)size);
	fflush(log_file);

	_inode = get_inode(path);
	my_disk_read((void*)local_block, (_inode.inode_number / inode_per_block) + 1);
	memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));


	_inode_array.inode_array[_inode.inode_number % inode_per_block].size = size;


	fprintf (log_file, "vsfs_truncate write size: %d\n", (int) _inode_array.inode_array[_inode.inode_number % inode_per_block].size);
	fflush(log_file);

	memcpy((void *)local_block, (void *)&_inode_array, sizeof(_inode_array));
	my_disk_write((void*)local_block, (_inode.inode_number / inode_per_block) + 1);



	fprintf (log_file, "vsfs_truncate done\n");
	fflush(log_file);

	return 0;
}


static int vsfs_rmdir(const char *path)
{
	inode _inode;
	char _path[128];
	char _parent_path[128];
	int _data_block_no;
	int _inode_number;
	char local_block[BLOCK_SIZE];
	blk_inode_array _inode_array;
	superblock _superblock;
	char *token1, *token2;
	int blk_it, size_it;
	blk_dirent_array _dirent_array;
	dirent _dirent;

	fprintf (log_file, "vsfs_rmdir called with path: %s\n", path);
	fflush(log_file);

	_inode = get_inode(path);

	_inode_number = _inode.inode_number;
	_data_block_no = _inode.block[0];

	fprintf (log_file, "vsfs_rmdir inode size: %d\n", (int)_inode.size);
	fflush(log_file);
	if(_inode.size > 2)
		return -ENOTEMPTY;

	my_disk_read((void*)local_block, 0);
	memcpy((void *)&_superblock, (void *)local_block, sizeof(_superblock));

	_superblock.inode_bitmap[_inode_number] = 0;
	_superblock.dblk_bitmap[_data_block_no] = 0;

	memcpy( (void *)local_block, (void *)&_superblock, sizeof(_superblock));
	my_disk_write((void*)local_block, 0);

	//get a parent path
	strcpy (_path, path);
	token1 = strtok (_path, "/");

	_parent_path[0] = 0;

		while (token1 != NULL)
		{
			token2 = strtok (NULL, "/");
			if (token2 != NULL)
			{
				strcat (_parent_path, "/");
				strcat (_parent_path, token1);
			}
			else
			{
				if (strlen(_parent_path) == 0)
				{
					strcpy (_parent_path, "/");
					// _parent_path[1] = 0;
					// _parent_path[0] = '/';
				}
				break;
			}
			token1 = token2;
		}
		fprintf (log_file, "vsfs_rmdir called with from's parent path: %s\n", _parent_path);
		fflush(log_file);

		_inode = get_inode(_parent_path);

		// decrease the from's parent inode's size
		my_disk_read((void*)local_block, (_inode.inode_number / inode_per_block) + 1);
		memcpy((void *)&_inode_array, (void *)local_block, sizeof(_inode_array));

		fprintf (log_file, "vsfs_rmdir from's parent updated size: %d\n",(int)_inode_array.inode_array[_inode.inode_number % inode_per_block].size);
		fflush(log_file);

		_inode_array.inode_array[_inode.inode_number % inode_per_block].size--;

		memcpy((void *)local_block, (void *)&_inode_array,  sizeof(_inode_array));
		my_disk_write((void*)local_block, (_inode.inode_number / inode_per_block) + 1);


		// invalidate dirent
		for (blk_it = 0; blk_it <  _inode.block_cnt; blk_it++)
	  {
	    fprintf (log_file, "vsfs_rmdir read block %d \n", _inode.block[blk_it] );
	    fflush(log_file);

			my_disk_read((void*)local_block, _inode.block[blk_it]); // read data block of the inode
	    for (size_it = 0; size_it < dirent_per_block; size_it++)
	    {
				// memcpy ((void *)&_dirent, (void *)&local_block[sizeof(_dirent) * (size_it % dirent_per_block)],  sizeof(_dirent) );
				memcpy ((void *)&_dirent_array, (void *)local_block,  sizeof(_dirent_array));
				_dirent = _dirent_array.dirent_array[size_it % dirent_per_block];
				// Do your stuff

				if ((_dirent.d_valid == 1) && (strcmp(_dirent.d_name , token1) == 0))
				{

					_dirent.d_valid = 0;

					fprintf (log_file, "vsfs_unlink from's directoy invalidated \n");
					fflush(log_file);

					// disk wrie for dirent
					_dirent_array.dirent_array[size_it % dirent_per_block] = _dirent;
					memcpy ((void *)local_block, (void *)&_dirent_array,  sizeof(_dirent_array));
					my_disk_write((void*)local_block, _inode.block[blk_it]);

					break;
				}
				else
				{
					fprintf (log_file, "vsfs_unlink dirent ino: %d name: %s\n",_dirent.d_ino, _dirent.d_name);
					fflush(log_file);
				}
	    }
		}

	return 0;
}


static void vsfs_destroy(void* private_data)
{
	//my_disk_log(DESTROY);
	fclose(disk_file);
	fclose(log_file);
}


static struct fuse_operations vsfs_oper = {
  .init    	= vsfs_init,
	.getattr	= vsfs_getattr,
	.readdir	= vsfs_readdir,
	.mkdir		= vsfs_mkdir,
	.rmdir    = vsfs_rmdir,
	.open			= vsfs_open,
	.unlink		= vsfs_unlink,
	.rename		= vsfs_rename,
	.read			= vsfs_read,
	.write		= vsfs_write,
	.mknod		= vsfs_mknod,
	.truncate = vsfs_truncate,
  .destroy  = vsfs_destroy
};



int main(int argc, char *argv[])
{
////////////////////////////////// Don't change this part ////////////////////////////////

	char disk_file_name[256]; // = "disk.iso";
	char log_file_name[256]; // = "log.txt";
//	char tmp;
	int counter;//, i;


	strcpy (disk_file_name, "disk.iso");
	strcpy (log_file_name, "log.txt");
  disk_ready = 1;
	for (counter = 1; counter < argc; counter++)
	{
		if (strcmp(argv[counter], "-l") == 0)
    {
      argc -= 2;
			strcpy (log_file_name, argv[++counter]);
    }
    else if (strcmp(argv[counter], "-d") == 0)
    {
      argc -= 2;
    	strcpy (disk_file_name, argv[++counter]);
    }
  }

	//argc = 2;
	printf ("disk_file_name : %s\n", disk_file_name);
	printf ("log_file_name : %s\n", log_file_name);
	disk_file = fopen (disk_file_name, "r+");
	if (disk_file == NULL)
  {
    disk_ready = 0;
		disk_file = fopen (disk_file_name, "w+");
  }

	log_file = fopen (log_file_name, "w");
	printf ("Files opened!\n");


	return fuse_main(argc, argv, &vsfs_oper, NULL);
}
