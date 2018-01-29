#include "vm/page.h"
#include "vm/frame.h"
#include <stdio.h>
#include "threads/thread.h"
#include "threads/pte.h"
#include "threads/interrupt.h"
#include "filesys/file.h"
#include <stddef.h>
#include <stdbool.h>
#include "threads/vaddr.h"
#include <hash.h>
#include <stdint.h>
#include "userprog/syscall.h"
#include "threads/malloc.h"
#include <stdlib.h>
#include <string.h>
#include "userprog/pagedir.h"
unsigned spte_hash_func(const struct hash_elem *e, void *aux UNUSED){
    struct sup_pte *spte = hash_entry(e, struct sup_pte, elem);

    return hash_int((int)(spte->upage));
}

bool spte_hash_less_func(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED){

    struct sup_pte *spte_a = hash_entry(a, struct sup_pte, elem);
    struct sup_pte *spte_b = hash_entry(b, struct sup_pte, elem);

    return spte_a->upage < spte_b->upage;

}
struct sup_pte* find_spte(void* uaddr){

    struct hash *sup_pt;
    struct sup_pte tmp_spte, *spte;
    struct hash_elem *e;

    sup_pt = &thread_current()->supple_page_table;
    (&tmp_spte)->upage = uaddr;

    e = hash_find(sup_pt, &(&tmp_spte)->elem);

    if(e==NULL){
        return NULL;
    }

    spte = hash_entry(e, struct sup_pte, elem);


    return spte;

}
void sup_pt_init(struct hash *supple_page_table){

    struct sup_pte *spte;
    uint8_t *from, *to, *upage;
    from = 0x00000000;
    to   = PHYS_BASE;

    hash_init(supple_page_table, spte_hash_func, spte_hash_less_func, NULL);
    /*  for(upage = from; upage < to ; upage += PGSIZE){
        spte = malloc(sizeof(struct sup_pte));
    
        spte->upage = (void*)upage;
        spte->loc = LOC_NONE;

        hash_insert(supple_page_table, &spte->elem);

    }
*/

}

bool save_segment_to_spte(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable){

    struct thread *t = thread_current();
    struct hash *sup_pt = &t->supple_page_table; 
    struct hash_elem *old;
    struct sup_pte *spte, *old_spte;


    //file_seek (file, ofs);
    while(read_bytes > 0 || zero_bytes > 0){

//        printf("saved upage = %p\n",upage);
        spte = malloc(sizeof(struct sup_pte));
        
        size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
        size_t page_zero_bytes = PGSIZE - page_read_bytes;

        spte->upage = (void*)upage;
//        printf("saved upage = %p\n",upage);
        if(page_zero_bytes == PGSIZE){
            spte->loc = LOC_ZERO;
        }
        else{
            spte->loc = LOC_FILE;
            spte->file = file;
            spte->ofs = ofs;

        }
        
        spte->read_bytes = page_read_bytes;
        spte->zero_bytes = page_zero_bytes;
        spte->writable = writable;
        
        old = hash_insert(sup_pt, &spte->elem);
       
        //이런 경우가 있으면 안됨..
        if(old!=NULL){
            PANIC("old!=NULL");
            old_spte = hash_entry(old, struct sup_pte,elem);
            free(old_spte);
        }

        /* advance */
        read_bytes -= page_read_bytes;
        zero_bytes -= page_zero_bytes;
        upage += PGSIZE;
        ofs += page_read_bytes;
    }

    return true;

}
bool load_segment_from_spte(struct sup_pte *spte){
    uint8_t *knpage;
    struct thread *t = thread_current();

   // printf("%p %u\n", spte->upage, spte->ofs);
  //  printf("%u %u\n", spte->read_bytes, spte->zero_bytes);

    if(spte->loc == LOC_ZERO)
        knpage = allocate_frame(PAL_USER | PAL_ZERO);
    else{
        knpage = allocate_frame(PAL_USER);
        lock_acquire(&syscall_lock);
        if(file_read_at(spte->file, knpage, spte->read_bytes,spte->ofs) != (int)spte->read_bytes){
            free_frame(knpage);
            lock_release(&syscall_lock);
            return false;
        }
        lock_release(&syscall_lock);

        memset (knpage + spte->read_bytes, 0, spte->zero_bytes);
    }

//    printf("knpage = %p \n", knpage);
    if(pagedir_get_page(t->pagedir, spte->upage)==NULL){
        if(pagedir_set_page(t->pagedir, spte->upage, knpage, spte->writable)){      
            add_fte_upage(knpage, spte->upage);
            spte->loc = LOC_FRAME;
            return true;
        }
    }

    //여기까지 왔으면 실패한 것임
    //free_frame(knpage);
    PANIC("load_segment_from_spte 실패하였음");
    return false;

}

bool is_stack_access(void *fault_addr,void* esp){

    //when accessed by PUSH inst(push : 4byte)
    if(fault_addr+4 == esp)
        return true;
    //when accessed by PUSHA inst(pusha : 32byte)
    if(fault_addr+32 == esp)
        return true;
    
    //stack limit 도달하지 않았고, 
    //esp보다도 fault_addr이 위쪽(stack안쪽)에 위치하는 경우
    //manual 49p q&a 참고
    if( PHYS_BASE - esp < STACK_LIMIT && esp <= fault_addr)
        return true;

    return false;
}

void hash_free_frames(struct hash_elem *e, void *aux){

    struct sup_pte *spte;
    struct thread * t = thread_current();
    void * frame;
    
    spte = hash_entry(e, struct sup_pte, elem);
    if(spte->loc == LOC_FRAME){
        frame = pagedir_get_page(t->pagedir,spte->upage);
        free_frame(frame);
    }
}


bool grow_stack(){

    uint8_t *kpage;
    bool success= false;
    struct thread* t = thread_current();
    void *upage = t->lowest_spage - PGSIZE;
    struct sup_pte *spte;
    struct hash *sup_pt = &t->supple_page_table;

    kpage = allocate_frame(PAL_USER | PAL_ZERO);

    if(kpage !=NULL){
       if(pagedir_get_page(t->pagedir,upage)==NULL){
           if(pagedir_set_page (t->pagedir,upage,kpage,true)){
               add_fte_upage(kpage, upage);
               t->lowest_spage -= PGSIZE;
            
               spte = malloc(sizeof(struct sup_pte));
               spte->upage = upage;
               spte->loc = LOC_FRAME;
               spte->writable = true;
               hash_insert(sup_pt, &spte->elem);

               return true;
           }
       }
    }

    return false;
}
