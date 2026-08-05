#include <stdint.h>

#define MAX_EXTENTS 16

typedef struct {
	char magic[8]; // TurtleFS
	uint32_t version; // version of the filesystem
	uint32_t bitmap_start; // start of the bitmap
	uint32_t bitmap_end; // end of the bitmap
	uint32_t entries_start;
	uint32_t entries_end;
	uint32_t total_blocks; // total blocks of device
	uint32_t data_start; // start of data
} super_blockt_t;

typedef struct {
	uint32_t start; // start of the extent
	uint32_t end; // end of the extent
} fs_extent_t;

typedef struct {
	char name[64];
	int type; // 1 means it's a file, 2 means it's a folder
	int size;
	fs_extent_t extents[MAX_EXTENTS];
	int extent_count;
	int parent; // parent folder
	int id; // own id
} fs_entry_t;

void tfs_format();
int total_sectors_mounted();
int make_dir(char* name, int parent_id);
int make_file(char* name, int parent, int size);
int file_get_size(int id);
int file_read(int id, void* buffer);
int file_write(int id, const void* buffer, int size);
int file_delete(int id);
int dir_delete(int id);
int find_entry(char* name, int parent);
int list_dir(int parent, fs_entry_t* out, int max_entries);
int get_entry_by_id(int id, fs_entry_t* out, int* out_sector, int* out_index);
int file_resize(int id, int new_size);
int tfs_mount();
