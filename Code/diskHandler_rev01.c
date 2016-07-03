#include <stdio.h>
#include "ourFS.h"

extern FILE *disk_file;
extern FILE *log_file;



int my_disk_write (const void *buf, int block_no)
{
	size_t successful_write;
	if (fseek(disk_file, BLOCK_SIZE * block_no, SEEK_SET) != 0)
	{
		fprintf (log_file, "disk write: Fseek for block #%d was not successful!\n", block_no);
		fflush (log_file);
		return -1;
	}
	fprintf (log_file, "disk write: Fseek for block #%d was successful!\n", block_no);
	fflush (log_file);
	successful_write = fwrite (buf, 1, BLOCK_SIZE, disk_file);
	if (successful_write != BLOCK_SIZE)
	{
		fprintf (log_file, "disk_write: fwrite for block #%d was not successful!\n", block_no);
		fflush (log_file);
		return -1;
	}
	fprintf (log_file, "disk_write: fwrite for block #%d was successful!\n", block_no);
	fflush (log_file);

	return 0;
}

int my_disk_read (void *buf, int block_no)
{
	size_t successful_read;
	if (fseek(disk_file, BLOCK_SIZE * block_no, SEEK_SET) != 0)
	{
		fprintf (log_file, "disk_read: Fseek for block #%d was not successful!\n", block_no);
		fflush (log_file);
		return -1;
	}
	fprintf (log_file, "disk_read: Fseek for block #%d was successful!\n", block_no);
	fflush (log_file);
	successful_read = fread (buf, 1, BLOCK_SIZE, disk_file);
	if (successful_read != BLOCK_SIZE)
	{
		fprintf (log_file, "disk_read: fread for block #%d was not successful!\n", block_no);
		fflush (log_file);
		return -1;
	}
	fprintf (log_file, "disk_read: fread for block #%d was successful!\n", block_no);
	fflush (log_file);
	return 0;
}

int my_disk_log (int func)
{
	switch (func){
		case (INIT):
			fprintf (log_file, "Init called!\n");
			break;
		case (DESTROY):
			fprintf (log_file, "DESTROY called!\n");
			break;
		case (GETATTR):
			fprintf (log_file, "GETATTR called!\n");
			break;
		case (READDIR):
			fprintf (log_file, "READDIR called!\n");
			break;
		case (OPEN):
			fprintf (log_file, "OPEN called!\n");
			break;
		case (READ):
			fprintf (log_file, "READ called!\n");
			break;
		case (WRITE):
			fprintf (log_file, "WRITE called!\n");
			break;
		case (MKNOD):
			fprintf (log_file, "MKNOD called!\n");
			break;
		default: fprintf (log_file, "Unknown ID: %d\n", func);
	}
	fflush (log_file);
	return 0;

}
