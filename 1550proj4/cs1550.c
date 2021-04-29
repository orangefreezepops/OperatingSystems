#define FUSE_USE_VERSION 26

#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "cs1550.h"

FILE *disk;


static struct cs1550_directory_entry* get_entry_block(size_t bn);
static struct cs1550_root_directory* get_root_block(void);
static int find_directory(struct cs1550_root_directory *r, char dir[MAX_FILENAME+1]);
static int find_entry(struct cs1550_directory_entry *e, char fn[MAX_FILENAME+1]);
static int find_entry_ext(struct cs1550_directory_entry *e, char fn[MAX_FILENAME+1], char ext[MAX_EXTENSION+1]);

//read the root dir from disk
static struct cs1550_root_directory* get_root_block(){
	struct cs1550_root_directory *r;
	r = malloc(sizeof(struct cs1550_root_directory));
	//find the first block
	fseek(disk, 0, SEEK_SET);
	//put it into the root struct
	fread(r, sizeof(struct cs1550_root_directory), 1, disk);

	return r;
}

//read the dir entry
static struct cs1550_directory_entry* get_entry_block(size_t bn){
	struct cs1550_directory_entry *e;
	e = malloc(sizeof(struct cs1550_directory_entry));
	//find the block given the block number parameter
	fseek(disk, sizeof(struct cs1550_directory_entry)*bn, SEEK_SET);
	//put it into the entry struct
	fread(e, sizeof(struct cs1550_directory_entry), 1, disk);

	return e;
}

static int find_directory(struct cs1550_root_directory *r, char dir[MAX_FILENAME+1]){
	int i;
	//loop through the directories in the root
	for (i = 0; i < (int)r->num_directories; i ++){
		//check that the directory from the path is equal to
		//the current directory in the search
		if (strncmp(dir, r->directories[i].dname, MAX_FILENAME+1) == 0){
			//return index 
			return i;
		}
	}
	//failure
	return -ENOENT;
}

static int find_entry(struct cs1550_directory_entry *e, char fn[MAX_FILENAME+1]){
	int i; //loop counter
	//loop through directory entries
	for (i = 0; i < (int)e->num_files; i ++){
		//compare th filename
		if (strncmp(fn, e->files[i].fname, MAX_FILENAME+1) == 0){
			//found it
			return i;
		}
	}
	//failure 
	return -ENOENT;
}

static int find_entry_ext(struct cs1550_directory_entry *e, char fn[MAX_FILENAME+1], char ext[MAX_EXTENSION+1]){
	int i; //loop counter
	//loop through directory entries
	for (i = 0; i < (int)e->num_files; i ++){
		//compare the file name and extension
		if ((strncmp(fn, e->files[i].fname, MAX_FILENAME+1) == 0) && strncmp(ext, e->files[i].fext, MAX_EXTENSION+1) == 0){
			//found it
			//return the index
			return i;
		}
	}
	//failure 
	return -ENOENT;
}
/**
 * Called whenever the system wants to know the file attributes, including
 * simply whether the file exists or not.
 *
 * `man 2 stat` will show the fields of a `struct stat` structure.
 */
static int cs1550_getattr(const char *path, struct stat *statbuf)
{
	//root directory struct
	struct cs1550_root_directory *root;

	//file directory entry struct
	struct cs1550_directory_entry *fde;

	//declare and instnatiate the directory filename and extension
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	

	memset(directory, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(filename, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(extension, 0, sizeof(char)*(MAX_EXTENSION+1));

	//path scanned
	int pathval;
	pathval = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	// Clear out `statbuf` first -- this function initializes it.
	memset(statbuf, 0, sizeof(struct stat));

	// Check if the path is the root directory.
	if (strcmp(path, "/") == 0) {
		statbuf->st_mode = S_IFDIR | 0755;
		statbuf->st_nlink = 2;
		return 0; // no error
	}

	//Check if the path is a subdirectory.
	if (pathval == 1) {
		//get the root
		root = get_root_block();

		//find directory
		if (find_directory(root, directory) >= 0){
			//found so set the permissions
			statbuf->st_mode = S_IFDIR | 0755;
			statbuf->st_nlink = 2;
			free(root);
			//success
			return 0;
		}
		free(root);
	}

	// Check if the path is a file.
	if (pathval == 2) {

		//get root
		root = get_root_block();

		//find the matching directory - returns index
		int fd = find_directory(root, directory);
		
		//we can use fd right away because the helpers return error code
		//so if we get to this point it means were safe

		//get the block number using the returned index
		size_t blockn = root->directories[fd].n_start_block;

		//use the block number to find the entry and load it into cs1550_directory_entry struct
		fde = get_entry_block(blockn);

		//find the entry - returns index OR error
		int fe = find_entry(fde, filename);
		
		if (fe >= 0){
			//successfully found the entry so set the permissions an return success
			statbuf->st_mode = S_IFREG | 0666;	// Regular file
			statbuf->st_nlink = 1;				// Only one hard link to this file
			statbuf->st_size = fde->files[fe].fsize;	// File size
			free(root);
			return 0; // no error
		}
		free(root);
	}

	// Check if the path is a file with an extension.
	if (pathval == 3) {

		//Same process as case 2
		root = get_root_block();
		int fd = find_directory(root, directory);
		
		//get the block
		size_t blockn = root->directories[fd].n_start_block;
		fde = get_entry_block(blockn);

		//set permissions and return success
		int fe = find_entry_ext(fde, filename, extension);

		//ensure the return value is valid
		if (fe >= 0){
			statbuf->st_mode = S_IFREG | 0666;	// Regular file
			statbuf->st_nlink = 1;				// Only one hard link to this file
			statbuf->st_size = fde->files[fe].fsize;	// File size
			free(root);
			return 0; // no error
		}
		free(root);
	}
	// Otherwise, the path doesn't exist.
	return -ENOENT;
}

/**
 * Called whenever the contents of a directory are desired. Could be from `ls`,
 * or could even be when a user presses TAB to perform autocompletion.
 */
static int cs1550_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			  off_t offset, struct fuse_file_info *fi)
{
	(void)offset;
	(void)fi;

	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	
	memset(directory, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(filename, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(extension, 0, sizeof(char)*(MAX_EXTENSION+1));

	//path scanned
	int pathval;
	pathval = sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	int i; //for loops

	struct cs1550_root_directory *root;
	struct cs1550_directory_entry *entry;

	//if the path is the root or a directory
	if (strcmp(path, "/") == 0 || pathval == 1){
		//find root dir
		root = get_root_block();

		// The filler function allows us to add entries to the listing.
		filler(buf, ".", NULL, 0);
		filler(buf, "..", NULL, 0);

		//if the path is root
		if (strcmp(path, "/") == 0){
			//iterate over the entries in the root directory
			for (i = 0; i < (int)root->num_directories; i ++){
				filler(buf, root->directories[i].dname, NULL, 0);
			}
			//success
			free(root);
			return 0;
		//else if the path is a directory
		} else if (pathval == 1){
			//make sure the directory exists
			if (find_directory(root, directory) >= 0){
				//allocate space for the entry
				entry = malloc(sizeof(struct cs1550_directory_entry));

				//find the directory
				int fd = find_directory(root, directory);

				//find the block number of that directory
				size_t blockn = root->directories[fd].n_start_block;

				//seek the the block 
				fseek(disk, sizeof(struct cs1550_directory_entry)*blockn, SEEK_SET);
				fread(entry, sizeof(struct cs1550_directory_entry), 1, disk);
				
				//iterate over the entries in the file directory
				for (i = 0; i < (int)entry->num_files; i ++){
					//if it has an extension
					if (strlen(extension)> 0){
						//make a char with enough space for filename + "." + extension
						char name[MAX_FILENAME + MAX_EXTENSION + 2 + sizeof(char)];

						//copy the filename intot he new char
						strncpy(name, entry->files[i].fname, MAX_FILENAME + 1);

						//concaenate the period
						strncat(name, ".", sizeof(char)+1);

						//concatenate the extension
						strncat(name, entry->files[i].fext, MAX_EXTENSION + 1);

						//fill the buffer
						filler(buf, name, NULL, 0);

					} else {
						//just use the filename
						filler(buf, entry->files[i].fname, NULL, 0);
					}
				}
				free(root);
				free(entry);
				return 0;
			}
		}
		free(root);
	}
	//failure
	return -ENOENT;
}

/**
 * Creates a directory. Ignore `mode` since we're not dealing with permissions.
 */
static int cs1550_mkdir(const char *path, mode_t mode)
{
	(void)mode;

	struct cs1550_root_directory *root;

	//declare and instnatiate the directory filename and extension
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];

	memset(directory, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(filename, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(extension, 0, sizeof(char)*(MAX_EXTENSION+1));

	//path scanned
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	//is the directory name compliant with the length requirements
	if (strlen(directory) > MAX_FILENAME){
		return -ENAMETOOLONG;
	}

	//get the root block
	root = get_root_block();

	//validate that its not already in the root dir
	if (find_directory(root, directory) < 0){

		//check to see if the directory is filled to capacity
		if (root->num_directories >= MAX_DIRS_IN_ROOT){
			//return an error if so
			return -ENOSPC;
		}
		//otherwise
		//copy the directory name from path into the last entry in the directory
		strncpy(root->directories[root->num_directories].dname, directory, MAX_FILENAME+1);

		//last allocated block + 1 is the block number for the new directory
		size_t newb = root->last_allocated_block + 1;

		//set the new directories block num
		root->directories[root->num_directories].n_start_block = newb;
		
		//increment last allocated block
		root->last_allocated_block ++;

		//increment num directories
		root->num_directories ++;

		//seek out the beginning of the file
		//and write the root directory back to the file
		fseek(disk, 0, SEEK_SET);
		fwrite(root, sizeof(struct cs1550_root_directory), 1, disk);

		return 0;
	}
	//if it passed the other checks and still didn't return success
	// the directory already exists
	return -EEXIST;
}

/**
 * Removes a directory.
 */
static int cs1550_rmdir(const char *path)
{
	(void)path;
	return 0;
}

/**
 * Does the actual creation of a file. `mode` and `dev` can be ignored.
 */
static int cs1550_mknod(const char *path, mode_t mode, dev_t dev)
{
	(void)mode;
	(void)dev;

	//structs to do the operations
	struct cs1550_root_directory *root;
	struct cs1550_directory_entry *entry;
	struct cs1550_index_block *iblock;

	//all the fun stuff to parse and validate the path
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];

	memset(directory, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(filename, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(extension, 0, sizeof(char)*(MAX_EXTENSION+1));

	//parsing the path
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	//get the root
	root = get_root_block();

	//get the index number in the root of the directory
	int index = find_directory(root, directory);
	
	//check to make sure the directory exists
	if (index >= 0){
		//get block number for the entry
		size_t blockn = root->directories[index].n_start_block;

		//get the directory entry from the root
		entry = get_entry_block(blockn);

		//make sure the file does not exist inside the directory
		if (find_entry(entry, filename) < 0 && find_entry_ext(entry, filename, extension) < 0){

			//check to see if theres still room in the diretory
			if (entry->num_files == MAX_FILES_IN_DIR){
				//return no space error if not
				return -ENOSPC;
			}

			//copy file name from the path into the filename at the current index
			//if it has an extension
			if (strlen(extension)> 0){
				//fill with the extension
				strncpy(entry->files[entry->num_files].fname, filename, MAX_FILENAME + 1);
				strncpy(entry->files[entry->num_files].fext, extension, MAX_EXTENSION + 1);
			} else {
				//just use the filename
				strncpy(entry->files[entry->num_files].fname, filename, MAX_FILENAME + 1);
			}
			//take last allocated block from root and set it as the index block
			size_t ib = root->last_allocated_block+1;
			entry->files[entry->num_files].n_index_block = ib; 

			//increment last_allocated_block~
			root->last_allocated_block ++;

			//make space for index block
			iblock = malloc(sizeof(struct cs1550_index_block));

			//seek to and read the index block into a cs1550_index_block struct
			fseek(disk, sizeof(struct cs1550_index_block)*ib, SEEK_SET);
			fread(iblock, sizeof(struct cs1550_index_block), 1, disk);

			//write the val of the last allocated block in root to 
			//first position in new index block
			iblock->entries[0] = root->last_allocated_block+1;

			//increment last allocated block for the first
			//data block of the file
			root->last_allocated_block ++;

			//increment num files
			entry->num_files ++;

			//seek out the beginning of the file
			//and write the root directory back to the file
			fseek(disk, 0, SEEK_SET);
			fwrite(root, sizeof(struct cs1550_root_directory), 1, disk);

			//seek out the file directory block location
			//and write the entry back to the file
			fseek(disk, sizeof(struct cs1550_directory_entry)*index, SEEK_SET);
			fwrite(entry, sizeof(struct cs1550_directory_entry), 1, disk);
			
			//seek out the index block location 
			//and write the index block back to the file
			fseek(disk, sizeof(struct cs1550_index_block)*ib, SEEK_SET);
			fwrite(iblock, sizeof(struct cs1550_index_block), 1, disk);
			return 0;
		}
	}
	return -EEXIST;
}

/**
 * Deletes a file.
 */
static int cs1550_unlink(const char *path)
{
	(void)path;
	return 0;
}

/**
 * Read `size` bytes from file into `buf`, starting from `offset`.
 */
static int cs1550_read(const char *path, char *buf, size_t size, off_t offset,
		       struct fuse_file_info *fi)
{
	(void)offset;
	(void)fi;

	//all the fun stuff to parse and validate the path
	struct cs1550_root_directory *root;
	struct cs1550_directory_entry *entry;
	struct cs1550_index_block *iblock;
	struct cs1550_data_block *dblock;
	struct stat *statbuf = malloc(sizeof(struct stat));
	char directory[MAX_FILENAME + 1];
	char filename[MAX_FILENAME + 1];
	char extension[MAX_EXTENSION + 1];
	memset(directory, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(filename, 0, sizeof(char)*(MAX_FILENAME+1));
	memset(extension, 0, sizeof(char)*(MAX_EXTENSION+1));
	memset(statbuf, 0, sizeof(struct stat));
	sscanf(path, "/%[^/]/%[^.].%s", directory, filename, extension);

	//if the path exists and there is a file associated with it
	if (cs1550_getattr(path, statbuf) == 0){
		//get the root
		root = get_root_block();

		//find the location of the directory
		int index = find_directory(root, directory);

		//get the blocknumber 
		int blockn = root->directories[index].n_start_block;

		entry = malloc(sizeof(struct cs1550_directory_entry));

		//read the data into a directory entry
		fseek(disk, sizeof(struct cs1550_directory_entry)*blockn, SEEK_SET);
		fread(entry, sizeof(struct cs1550_directory_entry), 1, disk);

		//try to find the file
		int loc;
		if (strlen(extension) > 0){
			loc = find_entry_ext(entry, filename, extension);
		} else {
			loc = find_entry(entry, filename);
		}

		iblock = malloc(sizeof(struct cs1550_index_block));

		//the file was found so read the index block
		fseek(disk, sizeof(struct cs1550_index_block)*loc, SEEK_SET);
		fread(iblock, sizeof(struct cs1550_index_block), 1, disk);

		//use offset to find index in the array
		int ibindex = (offset / BLOCK_SIZE);

		dblock = malloc(sizeof(struct cs1550_data_block));

		//read the data block
		fseek(disk, sizeof(struct cs1550_data_block)*ibindex, SEEK_SET);
		fread(dblock, sizeof(struct cs1550_data_block), 1, disk);

		//use offset to find index in datablock
		//int dbindex = (offset % BLOCK_SIZE);

		//copy the datablock starting from (offset % size) into buffer
		memcpy(buf, dblock->data + offset, sizeof(struct cs1550_data_block));
	}
	size = 0;

	return size;
}

/**
 * Write `size` bytes from `buf` into file, starting from `offset`.
 */
static int cs1550_write(const char *path, const char *buf, size_t size,
			off_t offset, struct fuse_file_info *fi)
{
	(void)offset;
	(void)fi;
	(void)buf;
	(void)path;
	return size;
}

/**
 * Called when a new file is created (with a 0 size) or when an existing file
 * is made shorter. We're not handling deleting files or truncating existing
 * ones, so all we need to do here is to initialize the appropriate directory
 * entry.
 */
static int cs1550_truncate(const char *path, off_t size)
{
	(void)path;
	(void)size;
	return 0;
}

/**
 * Called when we open a file.
 */
static int cs1550_open(const char *path, struct fuse_file_info *fi)
{
	(void)fi;
	(void)path;

	// If we can't find the desired file, return an error
	return -ENOENT;

	// Otherwise, return success
	// return 0;
}

/**
 * Called when close is called on a file descriptor, but because it might
 * have been dup'ed, this isn't a guarantee we won't ever need the file
 * again. For us, return success simply to avoid the unimplemented error
 * in the debug log.
 */
static int cs1550_flush(const char *path, struct fuse_file_info *fi)
{
	(void)fi;
	(void)path;
	// Success!
	return 0;
}

/**
 * This function should be used to open and/or initialize your `.disk` file.
 */
static void *cs1550_init(struct fuse_conn_info *fi)
{
	(void)fi;
	// Add your initialization routine here.
	disk = fopen(".disk", "rb+");
	return NULL;
}

/**
 * This function should be used to close the `.disk` file.
 */
static void cs1550_destroy(void *args)
{
	(void)args;
	// Add your teardown routine here.
	fclose(disk);
}

/*
 * Register our new functions as the implementations of the syscalls.
 */
static struct fuse_operations cs1550_oper = {
	.getattr	= cs1550_getattr,
	.readdir	= cs1550_readdir,
	.mkdir		= cs1550_mkdir,
	.rmdir		= cs1550_rmdir,
	.read		= cs1550_read,
	.write		= cs1550_write,
	.mknod		= cs1550_mknod,
	.unlink		= cs1550_unlink,
	.truncate	= cs1550_truncate,
	.flush		= cs1550_flush,
	.open		= cs1550_open,
	.init		= cs1550_init,
	.destroy	= cs1550_destroy,
};

/*
 * Don't change this.
 */
int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &cs1550_oper, NULL);
}
