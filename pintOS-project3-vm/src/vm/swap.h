
#include "devices/block.h"
#include <bitmap.h>
#include "threads/synch.h"
#include "vm/frame.h"
struct _swap_table{

    struct bitmap *used_map;
    struct block *swap_block;
    struct lock lock;
};



void init_swap_sector_map(void);

block_sector_t get_swapdisk_sector(void);

block_sector_t evict_to_swapdisk(struct frame_table_entry *);
bool load_from_swapdisk(block_sector_t , struct frame_table_entry *);
