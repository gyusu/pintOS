#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdio.h>

#include "filesys/off_t.h"
#include "devices/block.h"
#include <hash.h>
#include <stdbool.h>

#define LOC_NONE 0
#define LOC_ZERO 1
#define LOC_FILE 2
#define LOC_SWAP 4
#define LOC_FRAME 8


#define STACK_LIMIT 0x800000 //stack limit 8MB
// upage를 hash key값으로 사용
struct sup_pte{
    int loc;   //file(2) or swap slot(4) or all-zero(1) or none(0)

    bool pinned;   
    uint32_t *upage;// user virtual page, as a hash key     
    
    //for file
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;

    //for swap
    block_sector_t swap_sector;
    // swap blcok은 test에서  hda4사용, 8192sectors 총 4MB

    struct hash_elem elem;
};


unsigned spte_hash_func(const struct hash_elem *e, void *aux);
bool spte_hash_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void sup_pt_init(struct hash *supple_page_table);

struct sup_pte* find_spte(void* uaddr);
bool save_segment_to_spte(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

bool load_segment_from_spte(struct sup_pte *spte);
bool is_stack_access(void *fault_addr,void* esp);
bool grow_stack(void);
void hash_free_frames(struct hash_elem *e, void *aux);
#endif
