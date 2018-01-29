#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <stdio.h>
#include <stddef.h>
#include "threads/palloc.h"
#include "threads/synch.h"
               
struct frame_table_entry{
    void *frame;  //실제 메모리 상의 한frame의 base 주소이다.
    struct thread *owner_t;
    // 어떤 page(virtual memory)가 이 frame을 사용하는지도 필요할 수 있다.
    // 왜냐면 evict 되었을 때 spte에 접근해서 location = swap으로 설정해줘야 하니깐!
    void *upage;

    int evictable;
};

struct _frame_table{
    uint32_t free_frames;
    uint32_t total_frames;
    void *base;
    struct frame_table_entry *fte;//array (frame_number로indexing)
    struct bitmap *used_map;
    struct lock lock;
};

void* allocate_frame(enum palloc_flags flags);
void frame_table_init(size_t user_page_limit);
void free_frame(void *frame);
void* get_frame_paddr(uint32_t frame_num);
uint32_t get_frame_num(void *frame);
struct frame_table_entry *get_fte(void *frame);
uint32_t evict_frame(void);
void add_fte_upage(void *frame, void *upage);
#endif
//RAM은 기본적으로 3968KB 임
//PHYS_BASE부터 +1MB는 건너뛴다.(왜?o)
//왜냐면 init_page_dir 때문. 
//1024*1024만큼은 프로세스의 page directory 초기화에 사용됨.
//init.c에 paging_init()에 나와있음
//그러면 2944KB 가용
//4로 나누면 736
// kernel / user pool 368 page씩 사용 가능 
// 각각 1page씩은 used_map으로 사용(palloc에서)dd
//따라서 kernel_pool은 기본적으로 PHYS_BASE + 0x00101000
//user_pool은 PHYS_BASE + 0x00271000 부터 시작됨
//거기에 frame_table의 used_map(기본적으로 1page)추가
//frame num 0  : PHYS_BASE + 0x00272000
//frame num 1  : PHYS_BASE + 0x00273000...
//frame_table.fte[frame_num].frame == PHYS_BASE + 0x00272000 + 0x1000*frame_num
//== frame_table.base + 0x1000*frame_num 
 
