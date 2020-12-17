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
static u_int8_t *BITMAP;
static int bm_size;
static int token_border;
static int bit_border;

void print_bitmap(){

	int i;
	for (i=0; i<bm_size; i++){
		if (i%SFS_BLOCKSIZE == 0){
			printf("Bitmap Block %d ==============================\n", i/SFS_BLOCKSIZE);
			printf("Byte index\tHexa\tBit(LSB-MSB)\n");
		}

		printf("\t%d\t%x\t", i%SFS_BLOCKSIZE, BITMAP[i]);

		// convert to binary
		int binary[8];
		bzero(binary, sizeof(binary));
		int temp = BITMAP[i];
		int index = 0;
		for(;;){
			binary[index++] = temp % 2;
			temp /= 2;
			if (temp == 0) break;
		}

		// binary print
		int j;
		for (j=0; j<8; j++)
			printf("%d", binary[j]);
		printf("\n");

	}

}

u_int32_t take_free_block(){
	
	int token_num, bit_num=-1;
	for (token_num=0; token_num<bm_size; token_num++){
		if ( (token_num != token_border) && (BITMAP[token_num] == 255) )
			continue;

		int i;
		if (token_num == token_border){	// crossed the token_border
			for (i=0; i<bit_border; i++){
				if (!BIT_CHECK(BITMAP[token_num], i)){
					bit_num = i;
					BIT_SET(BITMAP[token_num], i);	// mark as in use
					// write bitmap back to disk
					for (i=0; i<SFS_BITBLOCKS(spb.sp_nblocks); i++){
						disk_write( &BITMAP[i*SFS_BLOCKSIZE], i+2);
					}
					break;
				}
			}
			if (bit_num == -1){
				return 0;	// no more free block
			}
			break;
		} else{	// in token_border (free block must here)
			int bflag=0;
			for (i=0; i<8; i++){
				if (!BIT_CHECK(BITMAP[token_num], i)){
					bit_num = i;
					BIT_SET(BITMAP[token_num], i);	// mark as in use
					// write bitmap back to disk
					for (i=0; i<SFS_BITBLOCKS(spb.sp_nblocks); i++){
						disk_write( &BITMAP[i*SFS_BLOCKSIZE], i+2);
					}
					bflag = 1;
					break;
				}
			}
			if (bflag)
				break;
		}
	}
	if (bit_num == -1){
		return 0;	// no more free block
	}

	return (token_num * 8) + bit_num;	// free block number
}

void release_block(u_int32_t blockno){
	/*
		n in -> n/8 token, n%8 shift_nbit
	*/

	// convert
	int token_num = blockno/8;
	int shift_nbit = blockno%8;

	BIT_CLEAR(BITMAP[token_num], shift_nbit);	// clear target bit

	int i;
	for (i=0; i<SFS_BITBLOCKS(spb.sp_nblocks); i++){
		disk_write( &BITMAP[i*SFS_BLOCKSIZE], i+2);
	}
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

	bm_size = sizeof(u_int8_t) * SFS_BLOCKSIZE * SFS_BITBLOCKS(spb.sp_nblocks);	// set bitmap size
	BITMAP = (u_int8_t*)malloc(bm_size);	// allocate bitmap loading space
	token_border = (SFS_BITMAPSIZE(spb.sp_nblocks) / 8) + 1;
	bit_border = SFS_BITMAPSIZE(spb.sp_nblocks) % 8;
}

void sfs_umount() {

	if( sd_cwd.sfd_ino !=  SFS_NOINO )
	{
		//umount
		disk_close();
		printf("%s, unmounted\n", spb.sp_volname);
		bzero(&spb, sizeof(struct sfs_super));
		sd_cwd.sfd_ino = SFS_NOINO;

		//remove bitmap loading space
		bm_size = 0;
		token_border = 0;
		bit_border = 0;
		free(BITMAP);
	}
}

void sfs_touch(const char* path)
{

	int empty_dtre_found=0;
	int empty_direct_ptr=0;

	struct sfs_dir *modified_drtblock;
	u_int32_t origin_drtblock_no;
	u_int32_t fbn;

	struct sfs_inode ci;
	disk_read( &ci, sd_cwd.sfd_ino );

	//for consistency
	assert( ci.sfi_type == SFS_TYPE_DIR );


	// check if the path already exists
	// cwd inode direct ptr loop
	int i;
	struct sfs_dir *tempdrte;
	for (i=0; i<SFS_NDIRECT; i++){
		// if direct ptr in use,
		if (ci.sfi_direct[i]){
			struct sfs_dir cdtrb[SFS_DENTRYPERBLOCK];
			disk_read( cdtrb, ci.sfi_direct[i] );

			// cwd directory entry loop
			int j;
			for (j=0; j<SFS_DENTRYPERBLOCK; j++){
				if (!empty_dtre_found && (cdtrb[j].sfd_ino == SFS_NOINO)){	// fisrt empty directory entry found
					empty_dtre_found = 1;
					tempdrte = &cdtrb[j];
					modified_drtblock = cdtrb;
					origin_drtblock_no = ci.sfi_direct[i];
				}

				// if directory entry in use, and path already exists
				if ( (cdtrb[j].sfd_ino != SFS_NOINO) && (strcmp(cdtrb[j].sfd_name, path) == 0) ){
					error_message("touch", path, -6);
					return;
				}
			}

		} else {
			empty_direct_ptr = i;
		}
	}


	if(!empty_dtre_found && !empty_direct_ptr){	// directory full
		error_message("touch", path, -3);
		return;
	}

	// clear loaded bitmap
	bzero(BITMAP, bm_size);
	// load bitmap
	for (i=0; i<SFS_BITBLOCKS(spb.sp_nblocks); i++){
		disk_read( &BITMAP[i*SFS_BLOCKSIZE], i+2);
	}

	/* for new file i-node*/

	// path not exists
	struct sfs_inode new_inode;
	bzero(&new_inode,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	new_inode.sfi_size = 0;
	new_inode.sfi_type = SFS_TYPE_FILE;

	// print_bitmap();

	fbn = take_free_block();	// find first free block, get free block number, and mark the bitmap
	if (!fbn){	// no more free block
		error_message("touch", path, -4);
		return;
	}
	u_int32_t cifbn = fbn;


	/* for directory block (current or new) */

	if(!empty_dtre_found){
		// new direct ptr -> new directory block allocate
		struct sfs_dir new_dtrb[SFS_DENTRYPERBLOCK];
		int i;
		for(i=0; i<SFS_DENTRYPERBLOCK; i++){
			new_dtrb[i].sfd_ino = SFS_NOINO;
		}

		fbn = take_free_block();
		if (!fbn){	// no more free block
			error_message("touch", path, -4);
			return;
		}

		new_dtrb[0].sfd_ino = cifbn;
		bzero(new_dtrb[0].sfd_name, SFS_NAMELEN);
		strncpy(new_dtrb[0].sfd_name, path, SFS_NAMELEN);
		disk_write(new_dtrb, fbn);

		ci.sfi_direct[empty_direct_ptr] = fbn;	// parent direct ptr update (for new directory block)
	} else{	// found empty directory entry
		tempdrte->sfd_ino = cifbn;
		bzero(tempdrte->sfd_name, SFS_NAMELEN);
		strncpy(tempdrte->sfd_name, path, SFS_NAMELEN);
		disk_write(modified_drtblock, origin_drtblock_no);
	}

	// child i-node write back
	disk_write(&new_inode, cifbn);

	/* for parent i-node */

	ci.sfi_size += sizeof(struct sfs_dir);	// file size up (one directory entry added)
	disk_write( &ci, sd_cwd.sfd_ino );

}

void sfs_cd(const char* path)
{

	// get cwd's inode
	struct sfs_inode ci;
	disk_read( &ci, sd_cwd.sfd_ino );

	//for consistency
	assert( ci.sfi_type == SFS_TYPE_DIR );

	// if path null
	if (path == NULL){
		sd_cwd.sfd_ino = 1;
		sd_cwd.sfd_name[0] = '/';
		sd_cwd.sfd_name[1] = '\0';
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

	//for consistency
	assert( ci.sfi_type == SFS_TYPE_DIR );

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

	// errors
	// path already exists -6, directory full -3, no more free blocks -4

	// directory entry 추가시마다 inode sfi_size 증가 (부모)
	// 생성된 directory entry가 가르키는 child inode에 direct_ptr한개 및 directory block 할당
	//	+ ., .. directroy entry 할당하고 child inode에 사이즈 설정, 타입설정
	// 나머지 모든 child directory entry SFS_NOINO 초기화

	
	// find path
	// if path already exists -> -6 error
	// else (path not exists)
		// if empty parent directory entry
			// 1. set parent directory entry, writeback, 
			//   parent inode size up (1 entry up), new dir_ptr if needed, writeback
			// 2. child i-node sequence
				// 2-1. find free block -> fail: -4 error
				// 2-2. set child inode size(2 entry), type, dir_ptr, write into new block
			// 3. child directory entry sequence
				// 3-1. find free block -> fail: -4 error
				// 3-2. set directory entry (., ..)
				// 3-3. set other entry SFS_NOINO, write into new block
	
	// directory full (if not returned before this line) -> -3 error


	int empty_dtre_found=0;
	int empty_direct_ptr=0;

	struct sfs_dir *modified_drtblock;
	u_int32_t origin_drtblock_no;
	u_int32_t fbn;

	struct sfs_inode ci;
	disk_read( &ci, sd_cwd.sfd_ino );

	//for consistency
	assert( ci.sfi_type == SFS_TYPE_DIR );


	// check if the path already exists
	// cwd inode direct ptr loop
	int i;
	struct sfs_dir *tempdrte;
	for (i=0; i<SFS_NDIRECT; i++){
		// if direct ptr in use,
		if (ci.sfi_direct[i]){
			struct sfs_dir cdtrb[SFS_DENTRYPERBLOCK];
			disk_read( cdtrb, ci.sfi_direct[i] );

			// cwd directory entry loop
			int j;
			for (j=0; j<SFS_DENTRYPERBLOCK; j++){
				if (!empty_dtre_found && (cdtrb[j].sfd_ino == SFS_NOINO)){	// fisrt empty directory entry found
					empty_dtre_found = 1;
					tempdrte = &cdtrb[j];
					modified_drtblock = cdtrb;
					origin_drtblock_no = ci.sfi_direct[i];
				}

				// if directory entry in use, and path already exists
				if ( (cdtrb[j].sfd_ino != SFS_NOINO) && (strcmp(cdtrb[j].sfd_name, org_path) == 0) ){
					error_message("mkdir", org_path, -6);
					return;
				}
			}

		} else {
			empty_direct_ptr = i;
		}
	}


	if(!empty_dtre_found && !empty_direct_ptr){	// directory full
		error_message("mkdir", org_path, -3);
		return;
	}

	// clear loaded bitmap
	bzero(BITMAP, bm_size);
	// load bitmap
	for (i=0; i<SFS_BITBLOCKS(spb.sp_nblocks); i++){
		disk_read( &BITMAP[i*SFS_BLOCKSIZE], i+2);
	}


	/* for child direcory i-node*/

	// path not exists
	struct sfs_inode new_inode;
	bzero(&new_inode,SFS_BLOCKSIZE); // initalize sfi_direct[] and sfi_indirect
	new_inode.sfi_size = sizeof(struct sfs_dir) * 2;
	new_inode.sfi_type = SFS_TYPE_DIR;

	// print_bitmap();

	fbn = take_free_block();	// find first free block, get free block number, and mark the bitmap
	if (!fbn){	// no more free block
		error_message("mkdir", org_path, -4);
		return;
	}
	u_int32_t cifbn = fbn;


	/* for child directory directory block */

	struct sfs_dir new_chdtrb[SFS_DENTRYPERBLOCK];
	for(i=0; i<SFS_DENTRYPERBLOCK; i++){
		new_chdtrb[i].sfd_ino = SFS_NOINO;
	}

	fbn = take_free_block();
	if (!fbn){	// no more free block
		error_message("mkdir", org_path, -4);
		return;
	}
	u_int32_t cdfbn = fbn;

	bzero(new_chdtrb[0].sfd_name, SFS_NAMELEN);
	bzero(new_chdtrb[1].sfd_name, SFS_NAMELEN);
	strncpy(new_chdtrb[0].sfd_name, ".", SFS_NAMELEN);
	strncpy(new_chdtrb[1].sfd_name, "..", SFS_NAMELEN);
	new_chdtrb[0].sfd_ino = cifbn;
	new_chdtrb[1].sfd_ino = sd_cwd.sfd_ino;
	new_inode.sfi_direct[0] = cdfbn;



	/* for parent directory block (current or new) */

	if(!empty_dtre_found){
		// new direct ptr -> new directory block allocate
		struct sfs_dir new_dtrb[SFS_DENTRYPERBLOCK];
		int i;
		for(i=0; i<SFS_DENTRYPERBLOCK; i++){
			new_dtrb[i].sfd_ino = SFS_NOINO;
		}

		fbn = take_free_block();
		if (!fbn){	// no more free block
			error_message("mkdir", org_path, -4);
			return;
		}

		new_dtrb[0].sfd_ino = cifbn;
		bzero(new_dtrb[0].sfd_name, SFS_NAMELEN);
		strncpy(new_dtrb[0].sfd_name, org_path, SFS_NAMELEN);
		disk_write(new_dtrb, fbn);

		ci.sfi_direct[empty_direct_ptr] = fbn;	// parent direct ptr update (for new directory block)
	} else{	// found empty directory entry
		tempdrte->sfd_ino = cifbn;
		bzero(tempdrte->sfd_name, SFS_NAMELEN);
		strncpy(tempdrte->sfd_name, org_path, SFS_NAMELEN);
		disk_write(modified_drtblock, origin_drtblock_no);
	}

	// child directory directory block write back
	disk_write(new_chdtrb, cdfbn);
	// child i-node write back
	disk_write(&new_inode, cifbn);

	/* for parent i-node */

	ci.sfi_size += sizeof(struct sfs_dir);	// file size up (one directory entry added)
	disk_write( &ci, sd_cwd.sfd_ino );

}

void sfs_rmdir(const char* org_path) 
{
	printf("Not Implemented\n");
}

void sfs_mv(const char* src_name, const char* dst_name) 
{

	int srcfound=0, dstfound=0;

	struct sfs_dir *modified_drtblock;
	u_int32_t origin_drtblock_no;

	// get cwd's inode
	struct sfs_inode ci;
	disk_read( &ci, sd_cwd.sfd_ino );

	//for consistency
	assert( ci.sfi_type == SFS_TYPE_DIR );

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
						modified_drtblock = cdtrb;
						origin_drtblock_no = ci.sfi_direct[i];
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
	disk_write(modified_drtblock, origin_drtblock_no);
	return;
}

void sfs_rm(const char* path) 
{
	// get cwd's inode
	struct sfs_inode ci;
	disk_read( &ci, sd_cwd.sfd_ino );

	//for consistency
	assert( ci.sfi_type == SFS_TYPE_DIR );

	// find path
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

					// if file
					int tmpchinum;
					if (pathi.sfi_type == SFS_TYPE_FILE){
						// clear loaded bitmap
						bzero(BITMAP, bm_size);
						// load bitmap
						int k;
						for (k=0; k<SFS_BITBLOCKS(spb.sp_nblocks); k++){
							disk_read( &BITMAP[k*SFS_BLOCKSIZE], k+2);
						}
						
						/* directory entry i-node number release */
						tmpchinum = cdtrb[j].sfd_ino;
						cdtrb[j].sfd_ino = SFS_NOINO;
						disk_write(cdtrb, ci.sfi_direct[i]);
						// puts("directory entry disk updated");

						ci.sfi_size -= sizeof(struct sfs_dir);	// decrease parent size info
						disk_write(&ci, sd_cwd.sfd_ino);
						// puts("parent inode disk updated");

						/* datablock pointed by direct_ptr release */
						for (k=0; k<SFS_NDIRECT; k++){
							if (pathi.sfi_direct[k]){
								// clear the datablock
								char tempdb[SFS_BLOCKSIZE];
								disk_read(tempdb, pathi.sfi_direct[k]);
								bzero(tempdb, SFS_BLOCKSIZE);
								disk_write(tempdb, pathi.sfi_direct[k]);
								// update bitmap
								release_block(pathi.sfi_direct[k]);
								// puts("datablock(dirptr) disk released");
							}
						}

						/* indirect_ptr handle */
						if (pathi.sfi_indirect){	// if in use
							u_int32_t realblock[SFS_BLOCKSIZE/32];
							disk_read(realblock, pathi.sfi_indirect);	// get real direct_ptrs' block

							for (k=0; k<SFS_BLOCKSIZE/32; k++){
								if (realblock[k]){
									// clear the datablock
									char tempdb[SFS_BLOCKSIZE];
									disk_read(tempdb, realblock[k]);
									bzero(tempdb, SFS_BLOCKSIZE);
									disk_write(tempdb, realblock[k]);
									// update bitmap
									release_block(realblock[k]);
									// puts("datablock(indirptr) disk released");
								}
							}

							bzero(realblock, SFS_BLOCKSIZE);	// clear real block
							disk_write(realblock, pathi.sfi_indirect);
							release_block(pathi.sfi_indirect);	// update bitmap
							// puts("realblock disk released");
						}

						/* release child(target file's) i-node */
						bzero(&pathi, SFS_BLOCKSIZE);
						disk_write( &pathi, tmpchinum );
						release_block(tmpchinum);
						// puts("child inode disk released");

						return;

					} else{	// if not a file
						error_message("rm", path, -10);
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
