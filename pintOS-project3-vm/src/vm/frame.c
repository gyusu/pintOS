#include "vm/frame.h"
#include "userprog/pagedir.h"
#include <stdbool.h>
#include <stddef.h>
#include "threads/init.h"
#include "threads/pte.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include <stdint.h>
#include <stdio.h>
#include "threads/synch.h"
#include <round.h>
#include <bitmap.h>
#include "threads/vaddr.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include <stdlib.h>
#include "devices/timer.h"
struct _frame_table frame_table;

void frame_table_init(size_t user_page_limit){

    /* palloc.c 의 palloc_init을 참고하여
     * 실제로 메모리 할당 가능한 user영역을 얻는다  */
    uint8_t *free_start = ptov (1024 * 1024);
    uint8_t *free_end = ptov (init_ram_pages * PGSIZE);
    size_t free_pages= (free_end - free_start) / PGSIZE;
    size_t user_pages = free_pages/2;
    size_t kernel_pages;
    if(user_pages > user_page_limit)
        user_pages = user_page_limit;
    kernel_pages = free_pages - user_pages;

    //첫 페이지(또는 더 많이+)는 init_pool에서 used_map으로 사용하므로
    //사용할 수 없다
    //그다음 같은 양 만큼 frame_table을 위한 used_map으로 사용하므로
    //사용할 수 없다.
    size_t bm_pages = DIV_ROUND_UP(bitmap_buf_size(user_pages), PGSIZE);
    user_pages -= bm_pages * 2;
   

    frame_table.free_frames = user_pages;
    frame_table.total_frames = user_pages;
    frame_table.base = free_start+(kernel_pages+bm_pages)*PGSIZE;
    palloc_get_multiple(PAL_USER, bm_pages);
    frame_table.used_map = bitmap_create_in_buf(user_pages, frame_table.base, bm_pages * PGSIZE);
    
    frame_table.base += bm_pages * PGSIZE;

    frame_table.fte = malloc(sizeof(struct frame_table_entry)*frame_table.free_frames);
    lock_init(&frame_table.lock);
}

void* allocate_frame(enum palloc_flags flags){
    void *frame=NULL;
    size_t frame_num=BITMAP_ERROR;
    void *upage;

    lock_acquire(&frame_table.lock);
    frame_num = bitmap_scan_and_flip(frame_table.used_map, 0, 1, false);
    lock_release(&frame_table.lock);
        
    //out of frame.. have to evict frame.. 
    if(frame_num == BITMAP_ERROR){
        frame_num = evict_frame();
        frame = get_frame_paddr(frame_num);
        //remove references to the frame
        uint32_t *pd = frame_table.fte[frame_num].owner_t->pagedir;
        void *upage = frame_table.fte[frame_num].upage;
        pagedir_clear_page(pd, upage);
        palloc_free_page(frame);
    }
        
    frame = palloc_get_page(flags);
        
    frame_table.fte[frame_num].frame = frame;
    frame_table.fte[frame_num].evictable = 1;
    frame_table.fte[frame_num].owner_t = thread_current();
    frame_table.free_frames -= 1;

    return frame;
}


//frame_num을 이용하여 frame의 physical address를 구한다.
void* get_frame_paddr(uint32_t frame_num){
    
    return frame_table.base + 0x1000*frame_num;

}

//frame의 physical address를 이용하여 frame_num을 구한다.
uint32_t get_frame_num(void *frame){
    return (frame - frame_table.base)/0x1000;
}

struct frame_table_entry *get_fte(void *frame){
    uint32_t frame_num = get_frame_num(frame);
    return &frame_table.fte[frame_num];
}

void add_fte_upage(void *frame, void *upage){
    uint32_t frame_num = get_frame_num(frame);
    frame_table.fte[frame_num].upage = upage;
}

void free_frame(void *frame){

    uint32_t frame_num = get_frame_num(frame);
//    printf("free frame_num = %d %p \n",frame_num, frame);
    frame_table.fte[frame_num].upage = NULL;
    frame_table.fte[frame_num].owner_t = NULL;
    frame_table.fte[frame_num].frame = NULL;

    bitmap_set_multiple(frame_table.used_map, frame_num, 1, false);
    frame_table.free_frames += 1;

//    pagedir_destroy에서 다 free 해줌
//    palloc_free_page(frame);
}
uint32_t evict_frame(){
    
    static int frame_idx = 0;
    bool success=false;
    void *upage;
    uint32_t *pte;
    struct thread *t;
    struct sup_pte *spte;
    int ref;
    uint32_t ret_idx;
    int loop=0;
    // dirty bit 인 애들은 swap될 수 있음
    while(!success){
//        frame_idx = timer_ticks()%frame_table.total_frames;
        
        upage = frame_table.fte[frame_idx].upage;
        t = frame_table.fte[frame_idx].owner_t;
        ref = frame_table.fte[frame_idx].evictable;
        spte = find_spte(upage);
        
        if(spte!=NULL){
            //second chance algorithm  
            //접근되었다면, 기회 한번 더 준다.
            if(pagedir_is_accessed(t->pagedir, upage)){
                      pagedir_set_accessed(t->pagedir, upage,false);
             }
            else{
                if(pagedir_is_dirty(t->pagedir,upage) || spte->pinned!=true){
                      spte->swap_sector = evict_to_swapdisk(&frame_table.fte[frame_idx]);
                      spte->loc = LOC_SWAP;
                      ret_idx = frame_idx;
                      success = true;
                }
                //    else{
                        //evict할 필요 없는, 즉, dirtybit  체크되지 않은 page들은 clear만 하면 됨
                        //이거 하려면 spte->loc 이거 사용한 구조를 다 바꿔야한다..

                 //   }
                    
                 //   pagedir_clear_page(t->pagedir, upage);
            
            }



        }

        //if(pagedir_is_dirty(t->pagedir, upage)){
               
/*          if(spte != NULL && pagedir_is_dirty(t->pagedir, upage)){
            if(ref == 0){
                // evict
                spte->swap_sector = evict_to_swapdisk(&frame_table.fte[frame_idx]);
                printf("evict 시\n");
                printf("upage %p\n", upage);
                printf("swap_sector %d\n", spte->swap_sector);

                spte->loc = LOC_SWAP;
                ret_idx = frame_idx;
                success = true;
            }
            else{
                frame_table.fte[frame_idx].evictable = 0;
            }
            
        }*/

        frame_idx = (frame_idx+1)%(frame_table.total_frames);
    }
    return ret_idx;
}
