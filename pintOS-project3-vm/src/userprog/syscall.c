#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <stdbool.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "lib/kernel/console.h"
#include "filesys/filesys.h"
#include "userprog/pagedir.h"
#include "threads/pte.h"
#include "filesys/file.h"
static void syscall_handler (struct intr_frame *);


void
syscall_init (void) 
{
  lock_init(&syscall_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  //f->esp 에 stack pointer가 있음 
  //현재 stack pointer 가 가르키는 부분에 32bit word에
  //system call number가 저장되어 있음
  //
  //user가 보내준 addr가 NULL이면?
  //if addr>=PHYS_BASE
  //if addr is unmapped user address??
  //then kill the process with -1 exit code
  // detects and hanles invalid user pointer in the page fault handler
  // in page_fault()
  // for a page fault occurred in the kernel, set EAX to 0xffffffff and copy its former value into EIP
  int syscall_num;
  int *argv[4];
  int retval=0;


  //validate esp
  if(!check_addr(f->esp)) return;
  
  syscall_num = *((int*)(f->esp));
  

  switch(syscall_num){
      case SYS_HALT:
          sys_halt();
          break;
      case SYS_EXIT:
          if(!get_argv((int*)f->esp, argv, 1)) return;
          sys_exit(*argv[0]);
          break;
      case SYS_EXEC:
          if(!get_argv((int*)f->esp, argv, 1)) return;
          retval = sys_exec((char*)*argv[0]);
          break;
      case SYS_WAIT:
          if(!get_argv((int*)f->esp, argv, 1)) return;
          retval = sys_wait(*(pid_t*)argv[0]);
          break;
      case SYS_CREATE:
          if(!get_argv((int*)f->esp, argv, 2)) return;
          retval = sys_create((char*)*argv[0], *(unsigned*)argv[1]);
          break;
      case SYS_REMOVE:
          if(!get_argv((int*)f->esp, argv, 1)) return;
          retval = sys_remove((char*)*argv[0]);
          break;
      case SYS_OPEN:
          if(!get_argv((int*)f->esp, argv, 1)) return;
          retval = sys_open((char*)*argv[0]);
          break;
      case SYS_FILESIZE:
          if(!get_argv((int*)f->esp, argv, 1)) return;
          retval = sys_filesize(*argv[0]);
          break;
      case SYS_READ:
          if(!get_argv((int*)f->esp, argv, 3)) return;
          retval = sys_read(*argv[0],(void*)(*argv[1]), *(unsigned*)argv[2]);
          break;
      case SYS_WRITE:
          if(!get_argv((int*)f->esp, argv, 3)) return;
          retval = sys_write(*argv[0], (void*)(*argv[1]), *(unsigned*)argv[2]);
          break;
      case SYS_SEEK:
          if(!get_argv((int*)f->esp, argv, 2)) return;
          sys_seek(*argv[0], *(unsigned*)argv[1]);
          break;
      case SYS_TELL:
          if(!get_argv((int*)f->esp, argv, 1)) return;
          retval = sys_tell(*argv[0]);
          break;
      case SYS_CLOSE:
          if(!get_argv((int*)f->esp, argv, 1)) return;
          sys_close(*argv[0]);
          break;
      case SYS_PIBO:
          if(!get_argv((int*)f->esp, argv, 1)) return;
          retval = sys_pibo(*argv[0]);
          break;
      case SYS_SUM4INT:
          if(!get_argv((int*)f->esp, argv, 4)) return;
          retval = *argv[0] + *argv[1] + *argv[2] + *argv[3];
          break;
      default: break;
  }


  f->eax = retval;

}

//esp에서 argc개 읽어서 argv에 저장
int get_argv(int *esp, int** argv,int argc){
    
    int i;
          
    for(i=0; i<argc; i++){
        argv[i] = (esp)+i+1;
        if(!check_addr((void*)*argv[i]))
            return 0;
    }

    return 1;
}
/* if invalid address ,call sys_exit(-1) and return 0  
 * else return 1    */
int check_addr(const void *address){

  if(is_kernel_vaddr(address)){
      sys_exit(-1);
      return 0;
  }

  return 1;
}

void sys_exit(int status){

    struct thread* t = thread_current();
    printf("%s: exit(%d)\n", t->name, status);
    
    t->exit_code = status;

    close_all_files();
    thread_exit();    
}

int sys_write(int fd, const void *buffer, unsigned size){

    int retval;

    if(fd==0)
        return 0;

    if(get_user(buffer)==-1){
        sys_exit(-1);
        return 0;
    }

    if(fd==1){  // fd==1 : STDOUT
        putbuf(buffer, size);
        return size;
    }
    else{
        
        struct opened_file * ofp;
        ofp = get_ofp(fd);

        if(ofp==NULL)
            return 0;

        lock_acquire(&syscall_lock); 
        retval = file_write(ofp->fp, buffer, size);
        lock_release(&syscall_lock);
        
        return retval;
    }

}

int sys_read(int fd, void *buffer, unsigned size){
    
    int i = -1;
    uint8_t key;
    uint8_t *cur_buf = (uint8_t*)buffer;
    int retval;
    
    
    if(get_user(buffer)==-1){
       sys_exit(-1);
       return -1; 
    }
    if(!buffer_check(buffer)){
       sys_exit(-1);
       return -1; 
        
    }
     
    if(fd==0){      //fd==0 : STDIN
       for(i=0; i<(int)size; i++){
         key = input_getc();
         *cur_buf=key;
         cur_buf++;
       }
       return i+1;
    }
    else{
        
        struct opened_file * ofp;
        ofp = get_ofp(fd);

        if(ofp==NULL)
            return -1;
    
        lock_acquire(&syscall_lock);
        retval = file_read(ofp->fp, buffer, size);
        lock_release(&syscall_lock);

        return retval;
    }


}
pid_t sys_exec(const char *cmd_line){

  pid_t child_pid;

  struct thread *parent_thread;

  parent_thread = thread_current();


//  printf("parent_thread = [%s]\n", parent_thread->name);

  child_pid = process_execute(cmd_line);
 // child_pid = process_execute(cmd_line);

  //this make parent stop here
  //until child(tid) tries to load and get load_success value 
  sema_down(&(thread_cur_child(child_pid)->load_sema));
  
  if(child_pid==TID_ERROR)
      return -1;

  if(!(thread_cur_child(child_pid)->load_success))  
      return -1;

  return child_pid;

}
int sys_wait(pid_t pid){

   int exit_code = process_wait(pid);

   return exit_code; 
}
void sys_halt (void){

    shutdown_power_off();

}
int sys_pibo (int n){
    int a=0, b=1, temp, i;
    for(i=0; i<n; i++){
       temp = a;
       a =  b;
       b = temp + b;
    }
    return a;
}

//return type is bool
int sys_create (const char *file, unsigned initial_size){
   
    int retval;

    if(file==NULL || get_user((unsigned char *)file)==-1){
       sys_exit(-1);
       return 0; 
    }

    lock_acquire(&syscall_lock);
    retval = filesys_create(file,initial_size);
    lock_release(&syscall_lock);
    return retval;
}

//return type is bool
int sys_remove (const char *file){
    int retval;

    if(get_user((unsigned char *)file)==-1){
        sys_exit(-1);
        return -1;
    }

    lock_acquire(&syscall_lock);
    retval = filesys_remove(file);
    lock_release(&syscall_lock);

    return retval;
}
int sys_open (const char *file){

    struct opened_file * ofp;
    struct thread* t;

    if(file==NULL)
        return -1;

    if(get_user((unsigned char *)file)==-1){
        sys_exit(-1);
        return -1;
    }
    ofp = (struct opened_file*)malloc(sizeof(struct opened_file));
    if(ofp==NULL)
        return -1;

    lock_acquire(&syscall_lock);
    ofp->fp = filesys_open(file);
    lock_release(&syscall_lock);
    if(ofp->fp==NULL){
        free(ofp);
        return -1;
    }

    ofp->fd = allocate_fd();
    
    t = thread_current();
    list_push_back(&(t->opened_files), &(ofp->elem));

    return ofp->fd;
    
}
int sys_filesize (int fd){
    struct opened_file * ofp;
    int retval;
    ofp = get_ofp(fd);

    if(ofp==NULL)
        return 0;

    retval = file_length(ofp->fp);
    
    return retval;
}
void sys_seek(int fd, unsigned position){
    struct opened_file * ofp;
    ofp = get_ofp(fd);

    if(ofp!=NULL){
        file_seek(ofp->fp, position);
    }
}
unsigned sys_tell (int fd){
    struct opened_file * ofp;
    ofp = get_ofp(fd);
   
    if(ofp==NULL)
        return 0; 

    return file_tell(ofp->fp);
}
void sys_close (int fd){

    struct opened_file * ofp;
    if(fd<2)
        return;
    ofp = get_ofp(fd);
    if(ofp==NULL)
        return;
    lock_acquire(&syscall_lock);
    file_close(ofp->fp);
    lock_release(&syscall_lock);
    list_remove(&(ofp->elem));
    free(ofp);

}

//return int (mapid_t)
int sys_mmap (int fd, void *addr){
   // fd 파일 열어서
   // process의 virtual address space로 매핑
   // lazy load해야함.. 즉 spte에 저장해두고 fault 발생 시 frame에 매핑해야할 듯
   // mmaped file 자체를 backing store for the mapping으로 사용하라고 한다
    //즉, evicting a page by mmap writes it back to the file it was mapped from

}

int allocate_fd(void){
    static int fd=2;
    return fd++;
}


//ofp는 opened_file_pointer의 줄임말입니다.
struct opened_file* get_ofp(int fd){
   
    struct thread *t = thread_current();
    struct list_elem *ofp_elem;
    struct opened_file* ofp;
    
// find the opened_file pointer whose fd is FD
    for(ofp_elem = list_begin(&(t->opened_files)); ofp_elem != list_end(&(t->opened_files)); ofp_elem = list_next(ofp_elem)){

        if(list_entry(ofp_elem, struct opened_file, elem)->fd == fd)
            break;
    }

    //not found case
    if(list_entry(ofp_elem, struct opened_file, elem)->fd != fd)
        return NULL;

    ofp = list_entry(ofp_elem, struct opened_file, elem);
   
    return ofp;
    
}

void close_all_files(){
    struct thread *t = thread_current();
    struct opened_file* ofp;
    

    if(list_size(&(t->opened_files))<=0)
        return ;

    while(!list_empty(&(t->opened_files))){
        struct list_elem *ofp_elem = list_pop_front(&(t->opened_files));
        ofp = list_entry(ofp_elem, struct opened_file, elem);
        lock_acquire(&syscall_lock);
        file_close(ofp->fp);
        lock_release(&syscall_lock);
        free(ofp);
    }

}

/*  Reads a byte at user virtual address UADDR.
 *  UADDR must be below PHYS_BASE.
 *  Returns the byte value if successful, -1 if a segfault
 *  occurred */

static int
get_user (const unsigned char *uaddr){

    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
            : "=&a" (result) : "m" (*uaddr));
    return result;
}

/* Writes BYTE to user address UDST.
 * UDST must be below PHYS_BASE.
 * Returns true if successful, false if a segfault occurred */

static int
put_user (unsigned char *udst, unsigned char byte){
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
            : "=&a" (error_code) , "=m" (*udst) : "q" (byte));

    return error_code != -1;
   
}

//return 1 means writable, else 0
bool
buffer_check(void * buffer){
    struct thread* t = thread_current();
    uint32_t *pte;
    pte = lookup_page(t->pagedir,buffer, false); 
    if(pte==NULL){
        return false;
    }
    if(PTE_W & *pte)
        return true;
    else{
        return false;
    }

}
