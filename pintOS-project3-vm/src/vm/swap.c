#include <stdio.h>
#include "swap.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "devices/block.h"
#include "userprog/syscall.h"
struct _swap_table swap_table;

void init_swap_sector_map(void){
    
    block_sector_t sectors;
    
    swap_table.swap_block = block_get_role(BLOCK_SWAP);
   
    sectors = block_size(swap_table.swap_block);
    swap_table.used_map = bitmap_create_in_buf(sectors, malloc(bitmap_buf_size(sectors)), bitmap_buf_size(sectors));

    lock_init(&swap_table.lock);
}

// get empty sector
block_sector_t get_swapdisk_sector(void){

    block_sector_t sector_idx;

    lock_acquire(&swap_table.lock);
    //8개를 찾는 이유는 sector 하나당 512byte인데,
    //page는 4096byte이기 때문
    sector_idx = bitmap_scan_and_flip (swap_table.used_map, 0, 8, false);
    lock_release(&swap_table.lock);

    // sector_idx가 bitmap_error인지 체크 필요
    // 근데 에러 체크하려면 함수 정의 수정필요
    return sector_idx;
}


// write to disk, free frame. 
block_sector_t evict_to_swapdisk(struct frame_table_entry *fte){

    block_sector_t sector = get_swapdisk_sector();
    void *buf, *buf_from, *buf_to;
    int i;

    buf_from = fte->frame;
    buf_to = fte->frame + 4096; //+ 4KB를 의미
    lock_acquire(&syscall_lock);
    //buf += 128 은 512byte 씩 이동을 의미함
    for(buf=buf_from, i=0; buf<buf_to; buf += 512, i++){
        block_write(swap_table.swap_block, sector + i, buf);
    }

    lock_release(&syscall_lock);
    return sector;
}


// read from disk, write to frame
bool load_from_swapdisk(block_sector_t sector, struct frame_table_entry *fte){

  
    void *buf, *buf_from, *buf_to;
    int i;

    buf_from = fte->frame;
    buf_to = fte->frame + 4096 ; //+ 4KB를 의미
   
    lock_acquire(&syscall_lock);
    //buf += 128 은 512byte 씩 이동을 의미함
    for(buf=buf_from, i=0; buf<buf_to; buf += 512, i++){
        block_read(swap_table.swap_block, sector+i, buf); 
    }
    lock_release(&syscall_lock);

    lock_acquire(&swap_table.lock);
    bitmap_set_multiple (swap_table.used_map, sector, 8, false);
    lock_release(&swap_table.lock);
    return true;
}



