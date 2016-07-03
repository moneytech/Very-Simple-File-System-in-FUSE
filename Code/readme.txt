1- Use the following command to compile the file system:
gcc -Wall vsfs_rev03.c diskHandler_rev01.c `pkg-config fuse --cflags --libs` -o <executable file name>

2- Use the following command to mount the file system into the prefered mount point
<executable file name> <mount point> -d <disk image address> -l <log file address>
if the disk image address or log file not provided, it would use the default name an locations.

3- When done, use the following command to unmount the filesystem
fusermount -u <mount point>
