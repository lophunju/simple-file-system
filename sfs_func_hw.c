// OS Homework (Simple File System)
// Submission Year: 2020-2
// Student Name: Park Juhun (박주훈)
// Student Number: B511072
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* optional */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
/***********/

#include "sfs_types.h"
#include "sfs_func.h"
#include "sfs_disk.h"
#include "sfs.h"

void dump_directory();

/* BIT operation Macros */
/* a=target variable, b=bit number to act upon 0-n */
#define BIT_SET(a,b) ((a) |= (1<<(b)))
#define BIT_CLEAR(a,b) ((a) &= ~(1<<(b)))
#define BIT_FLIP(a,b) ((a) ^= (1<<(b)))
#define BIT_CHECK(a,b) ((a) & (1<<(b)))

static struct sfs_super spb;	// superblock
static struct sfs_dir sd_cwd = { SFS_NOINO }; // current working directory

/* Bitmap */
static char *BITMAP;
static int token_length;
static int token_boundary;
static int bit_boundary;
// SFS_BITMAPSIZE(nblocks) -> 비트맵 몇비트인지 -> 32로나누면 몇개의 u_int32_t인지
// SFS_BITBLOCKS(nblocks) -> 비트맵 몇블락인지 (몇개의 512바이트(SFS_BLOCKSIZE)인지)

void make_bitmap(){

	// allocate bitmap space
	// token_length = (spb.sp_nblocks / 8) + 1;
	token_length = (515 / 8) + 1;
	BITMAP = (char*)malloc(sizeof(char) * token_length);

	// initialize
	bzero(BITMAP, token_length);
	// token_boundary = spb.sp_nblocks / 8;
	// bit_boundary = spb.sp_nblocks % 8;
	token_boundary = 515 / 8;
	bit_boundary = 515 % 8;

	puts("bitmap made");
	printf("size: %d bits\n", token_length * 8);
	printf("number of 8bit token: %d\n", token_length);
	printf("token_boundary: %d token fully usable\n", token_boundary);
	printf("bit_boundary: %d bit usable after token_boundary\n", bit_boundary);
}

void remove_bitmap(){
	token_length = 0;
	token_boundary = 0;
	bit_boundary = 0;
	free(BITMAP);

	puts("bitmap removed");
}

void error_message(const char *message, const char *path, int error_code) {
	switch (error_code) {
	case -1:
		printf("%s: %s: No such file or directory\n",message, path); return;
	case -2:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -3:
		printf("%s: %s: Directory full\n",message, path); return;
	case -4:
		printf("%s: %s: No block available\n",message, path); return;
	case -5:
		printf("%s: %s: Not a directory\n",message, path); return;
	case -6:
		printf("%s: %s: Already exists\n",message, path); return;
	case -7:
		printf("%s: %s: Directory not empty\n",message, path); return;
	case -8:
		printf("%s: %s: Invalid argument\n",message, path); return;
	case -9:
		printf("%s: %s: Is a directory\n",message, path); return;
	case -10:
		printf("%s: %s: Is not a file\n",message, path); return;
	default:
		printf("unknown error code\n");
		return;
	}
}

void sfs_mount(const char* path)
{
	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;
	}

	printf("Disk image: %s\n", path);

	disk_open(path);
	disk_read( &spb, SFS_SB_LOCATION );

	printf("Superblock magic: %x\n", spb.sp_magic);

	assert( spb.sp_magic == SFS_MAGIC );
	
	printf("Number of blocks: %d\n", spb.sp_nblocks);
	printf("Volume name: %s\n", spb.sp_volname);
	printf("%s, mounted\n", spb.sp_volname);
	
	sd_cwd.sfd_ino = 1;		//init at root
	sd_cwd.sfd_name[0] = '/';
	sd_cwd.sfd_name[1] = '\0';

	make_bitmap();
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;

		remove_bitmap();
	}
}

void sfs_touch(const char* path)
{
	//skeleton implementation

	struct sfs_inode si;
	disk_read( &si, sd_cwd.sfd_ino );

	//for consistency
	assert( si.sfi_type == SFS_TYPE_DIR );

	//we assume that cwd is the root directory and root directory is empty which has . and .. only
	//unused DISK2.img satisfy these assumption
	//for new directory entry(for new file), we use cwd.sfi_direct[0] and offset 2
	//becasue cwd.sfi_directory[0] is already allocated, by .(offset 0) and ..(offset 1)
	//for new inode, we use block 6 
	// block 0: superblock,	block 1:root, 	block 2:bitmap 
	// block 3:bitmap,  	block 4:bitmap 	block 5:root.sfi_direct[0] 	block 6:unused
	//
	//if used DISK2.img is used, result is not defined
	
	//buffer for disk read
	struct sfs_dir sd[SFS_DENTRYPERBLOCK];

	//block access
	disk_read( sd, si.sfi_direct[0] );

	//allocate new block
	int newbie_ino = 6;

	sd[2].sfd_ino = newbie_ino;
	strncpy( sd[2].sfd_name, path, SFS_NAMELEN );

	disk_write( sd, si.sfi_direct[0] );

	si.sfi_size += sizeof(struct sfs_dir);
	disk_write( &si, sd_cwd.sfd_ino );

	struct sfs_inode newbie;

	bzero(&newbie,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	newbie.sfi_size = 0;
	newbie.sfi_type = SFS_TYPE_FILE;

	disk_write( &newbie, newbie_ino );
}

void sfs_cd(const char* path)
{

	// if path null
	if (path == NULL){
		sd_cwd.sfd_ino = 1;
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
		return;
	}

	// if path not null
	// get cwd's inode
	struct sfs_inode ci;
	disk_read( &ci, sd_cwd.sfd_ino );

	// cwd inode direct ptr loop
	int i;
	for (i=0; i<SFS_NDIRECT; i++){
		// if direct ptr in use,
		if (ci.sfi_direct[i]){
			struct sfs_dir cdtrb[SFS_DENTRYPERBLOCK];
			disk_read( cdtrb, ci.sfi_direct[i] );

			// cwd directory entry loop
			int j;
			for (j=0; j<SFS_DENTRYPERBLOCK; j++){
				// if directory entry in use, and path found
				if ( (cdtrb[j].sfd_ino != SFS_NOINO) && (strcmp(cdtrb[j].sfd_name, path) == 0) ){
					struct sfs_inode pathi;
					disk_read( &pathi, cdtrb[j].sfd_ino );

					// if directory
					if (pathi.sfi_type == SFS_TYPE_DIR){
						// change cwd
						sd_cwd.sfd_ino = cdtrb[j].sfd_ino;
						bzero(sd_cwd.sfd_name, SFS_NAMELEN);
						strncpy(sd_cwd.sfd_name, cdtrb[j].sfd_name, SFS_NAMELEN);
						return;
					} else{	// if not a directory
						error_message("cd", path, -2);
						return;
					}
				}
			}

		}
	}

	// path not found
	error_message("cd", path, -1);
	return;
}

void sfs_ls(const char* path)
{

	// get cwd's inode
	struct sfs_inode ci;
	disk_read( &ci, sd_cwd.sfd_ino );

	// if path null
	if (path == NULL){
		// cwd inode direct ptr loop
		int i;
		for (i=0; i<SFS_NDIRECT; i++){
			// if direct ptr in use,
			if (ci.sfi_direct[i]){
				struct sfs_dir cdtrb[SFS_DENTRYPERBLOCK];
				disk_read( cdtrb, ci.sfi_direct[i] );

				// cwd directory entry loop
				int j;
				for (j=0; j<SFS_DENTRYPERBLOCK; j++){
					// if directory entry is use
					if (cdtrb[j].sfd_ino != SFS_NOINO){
						struct sfs_inode tempi;
						disk_read( &tempi, cdtrb[j].sfd_ino );

						// if directory
						if (tempi.sfi_type == SFS_TYPE_DIR){
							printf("%s/\t", cdtrb[j].sfd_name);
						} else{	// if file
							printf("%s\t", cdtrb[j].sfd_name);
						}
					}
				}

			}
		}
		printf("\n");
		return;
	}

	// if path not null
	// cwd inode direct ptr loop
	int i;
	for (i=0; i<SFS_NDIRECT; i++){
		// if direct ptr in use,
		if (ci.sfi_direct[i]){
			struct sfs_dir cdtrb[SFS_DENTRYPERBLOCK];
			disk_read( cdtrb, ci.sfi_direct[i] );

			// cwd directory entry loop
			int j;
			for (j=0; j<SFS_DENTRYPERBLOCK; j++){
				// if directory entry in use, and path found
				if ( (cdtrb[j].sfd_ino != SFS_NOINO) && (strcmp(cdtrb[j].sfd_name, path) == 0) ){
					struct sfs_inode pathi;
					disk_read( &pathi, cdtrb[j].sfd_ino );

					// if path is directory
					if (pathi.sfi_type == SFS_TYPE_DIR){
						// path inode direct ptr loop
						int k;
						for (k=0; k<SFS_NDIRECT; k++){
							// if direct ptr in use,
							if (pathi.sfi_direct[k]){
								struct sfs_dir pdtrb[SFS_DENTRYPERBLOCK];
								disk_read( pdtrb, pathi.sfi_direct[k] );

								// path directory entry loop
								int l;
								for (l=0; l<SFS_DENTRYPERBLOCK; l++){
									// if directory entry in use
									if (pdtrb[l].sfd_ino != SFS_NOINO){
										struct sfs_inode tempi;
										disk_read( &tempi, pdtrb[l].sfd_ino );

										// if directory
										if (tempi.sfi_type == SFS_TYPE_DIR){
											printf("%s/\t", pdtrb[l].sfd_name);
										} else{	// if file
											printf("%s\t", pdtrb[l].sfd_name);
										}
									}
								}

							}
						}
					} else { // if path is file
						printf("%s", cdtrb[j].sfd_name);
					}
					printf("\n");
					return;
				}

			}

		}
	}

	// path not found
	error_message("cd", path, -1);
	return;
}

void sfs_mkdir(const char* org_path) 
{
	printf("Not Implemented\n");
}

void sfs_rmdir(const char* org_path) 
{
	printf("Not Implemented\n");
}

void sfs_mv(const char* src_name, const char* dst_name) 
{

	int srcfound=0, dstfound=0;

	// get cwd's inode
	struct sfs_inode ci;
	struct sfs_dir *modified_block;
	u_int32_t origin_block_no;
	disk_read( &ci, sd_cwd.sfd_ino );

	// check invalid
	if (!strcmp(src_name, ".") || !strcmp(dst_name, ".") ){
		error_message("mv", ".", -8);
		return;
	}
	if ( !strcmp(src_name, "..") || !strcmp(dst_name, "..") ){
		error_message("mv", "..", -8);
		return;
	}

	// find src_name
	// cwd inode direct ptr loop
	int i;
	struct sfs_dir *tmpdtre;
	for (i=0; i<SFS_NDIRECT; i++){
		if (srcfound && dstfound)	// for lower stress
			break;

		// if direct ptr in use,
		if (ci.sfi_direct[i]){
			struct sfs_dir cdtrb[SFS_DENTRYPERBLOCK];
			disk_read( cdtrb, ci.sfi_direct[i] );

			// cwd directory entry loop
			int j;
			for (j=0; j<SFS_DENTRYPERBLOCK; j++){
				if (srcfound && dstfound)	// for lower stress
					break;

				// if directory entry in use
				if (cdtrb[j].sfd_ino != SFS_NOINO){
					// srcname found
					if ( !srcfound && (strcmp(cdtrb[j].sfd_name, src_name) == 0) ){
						srcfound = 1;
						tmpdtre = &cdtrb[j];
						modified_block = cdtrb;
						origin_block_no = ci.sfi_direct[i];
					}
					// dstname found
					if ( !dstfound && (strcmp(cdtrb[j].sfd_name, dst_name) == 0) ){
						dstfound = 1;
					}
				}

			}

		}
	}

	if (!srcfound) {
		error_message("mv", src_name, -1);
		return;
	}
	if (dstfound) {
		error_message("mv", dst_name, -6);
		return;
	}

	// able to change the name
	bzero(tmpdtre->sfd_name, SFS_NAMELEN);
	strncpy(tmpdtre->sfd_name, dst_name, SFS_NAMELEN);

	// write modified block on disk
	disk_write(modified_block, origin_block_no);
	return;
}

void sfs_rm(const char* path) 
{
	printf("Not Implemented\n");
}

void sfs_cpin(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void sfs_cpout(const char* local_path, const char* path) 
{
	printf("Not Implemented\n");
}

void dump_inode(struct sfs_inode inode) {
	int i;
	struct sfs_dir dir_entry[SFS_DENTRYPERBLOCK];

	printf("size %d type %d direct ", inode.sfi_size, inode.sfi_type);
	for(i=0; i < SFS_NDIRECT; i++) {
		printf(" %d ", inode.sfi_direct[i]);
	}
	printf(" indirect %d",inode.sfi_indirect);
	printf("\n");

	if (inode.sfi_type == SFS_TYPE_DIR) {
		for(i=0; i < SFS_NDIRECT; i++) {
			if (inode.sfi_direct[i] == 0) break;
			disk_read(dir_entry, inode.sfi_direct[i]);
			dump_directory(dir_entry);
		}
	}

}

void dump_directory(struct sfs_dir dir_entry[]) {
	int i;
	struct sfs_inode inode;
	for(i=0; i < SFS_DENTRYPERBLOCK;i++) {
		printf("%d %s\n",dir_entry[i].sfd_ino, dir_entry[i].sfd_name);
		disk_read(&inode,dir_entry[i].sfd_ino);
		if (inode.sfi_type == SFS_TYPE_FILE) {
			printf("\t");
			dump_inode(inode);
		}
	}
}

void sfs_dump() {
	// dump the current directory structure
	struct sfs_inode c_inode;

	disk_read(&c_inode, sd_cwd.sfd_ino);
	printf("cwd inode %d name %s\n",sd_cwd.sfd_ino,sd_cwd.sfd_name);
	dump_inode(c_inode);
	printf("\n");

}
