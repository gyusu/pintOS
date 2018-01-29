#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "lib/kernel/console.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
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
  int *arg0, *arg1, *arg2, *arg3;
  int retval=0;

  //validate esp
  if(!check_addr(f->esp)) return;
  
  syscall_num = *((int*)(f->esp));

  switch(syscall_num){
      case SYS_HALT:
          sys_halt();
          break;
      case SYS_EXIT:
          arg0 = (int*)(f->esp)+1;
          if(!check_addr(arg0)) return;
          sys_exit(*arg0);
          break;
      case SYS_EXEC:
          arg0 = (int*)(f->esp)+1;
          if(!check_addr(arg0)) return;
          retval = sys_exec((char*)*arg0);
          break;
      case SYS_WAIT:
          arg0 = (int*)(f->esp)+1;
          if(!check_addr(arg0)) return;
          retval = sys_wait(*(pid_t*)arg0);
          break;
      case SYS_CREATE:
          arg0= (int*)(f->esp)+1;
          arg1= (int*)(f->esp)+2;
          if(!check_addr(arg0)) return;
          if(!check_addr(arg1)) return;
          retval = sys_create((char*)*arg0, *(unsigned*)arg1);
          break;
      case SYS_REMOVE:
          arg0= (int*)(f->esp)+1;
          if(!check_addr(arg0)) return;
          retval = sys_remove((char*)*arg0);
          break;
      case SYS_OPEN:
          arg0= (int*)(f->esp)+1;
          if(!check_addr(arg0)) return;
          retval = sys_open((char*)*arg0);
          break;
      case SYS_FILESIZE:
          arg0= (int*)(f->esp)+1;
          if(!check_addr(arg0)) return;
          retval = sys_filesize(*arg0);
          break;
      case SYS_READ:
          arg0= (int*)(f->esp)+1;
          arg1= (int*)(f->esp)+2;
          arg2= (int*)(f->esp)+3;
          if(!check_addr(arg0)) return;
          if(!check_addr(arg1)) return;
          if(!check_addr(arg2)) return;
          retval = sys_read(*arg0,(void*)(*arg1), *(unsigned*)arg2);
          break;
      case SYS_WRITE:
          arg0= (int*)(f->esp)+1;
          arg1= (int*)(f->esp)+2;
          arg2= (int*)(f->esp)+3;
          if(!check_addr(arg0)) return;
          if(!check_addr(arg1)) return;
          if(!check_addr(arg2)) return;
          retval = sys_write(*arg0, (void*)(*arg1), *(unsigned*)arg2);
          break;
      case SYS_SEEK:
          arg0= (int*)(f->esp)+1;
          arg1= (int*)(f->esp)+2;
          if(!check_addr(arg0)) return;
          if(!check_addr(arg1)) return;
          sys_seek(*arg0, *(unsigned*)arg1);
          break;
      case SYS_TELL:
          arg0= (int*)(f->esp)+1;
          if(!check_addr(arg0)) return;
          retval = sys_tell(*arg0);
          break;
      case SYS_CLOSE:
          arg0= (int*)(f->esp)+1;
          if(!check_addr(arg0)) return;
          sys_close(*arg0);
          break;
      case SYS_PIBO:
          arg0= (int*)(f->esp)+1;
          if(!check_addr(arg0)) return;
          retval = sys_pibo(*arg0);
          break;
      case SYS_SUM4INT:
          arg0= (int*)(f->esp)+1;
          arg1= (int*)(f->esp)+2;
          arg2= (int*)(f->esp)+3;
          arg3= (int*)(f->esp)+4;
          if(!check_addr(arg0)) return;
          if(!check_addr(arg1)) return;
          if(!check_addr(arg2)) return;
          if(!check_addr(arg3)) return;
          retval = *arg0 + *arg1 + *arg2 + *arg3;
          break;
      default: break;
  }


  f->eax = retval;

}

int check_n_addr(void *address, int argc){



}
/* if invalid address ,call sys_exit(-1) and return 0  
 * else return 1    */
int check_addr(void *address){

  uintptr_t addr = (uintptr_t)address; 

  if(is_kernel_vaddr(addr) || addr<0 ){
      sys_exit(-1);
      return 0;
  }


  return 1;
}

void sys_exit(int status){

    struct thread* t = thread_current();
    printf("%s: exit(%d)\n", t->name, status);
    
    t->exit_code = status;
 //w   printf("exit code 저장했다!!\n");
 //   sema_down(&t->die_sema);
 //   process_exit();
    thread_exit();    
}

int sys_write(int fd, const void *buffer, unsigned size){

    if(fd==1){  // fd==1 : STDOUT
        putbuf(buffer, size);
    }

    return size;
}

int sys_read(int fd, void *buffer, unsigned size){
    
    int i = -1;
    uint8_t key;
    uint8_t *cur_buf = (uint8_t*)buffer;

    if(fd==0){      //fd==0 : STDIN
       for(i=0; i<(int)size; i++){
         key = input_getc();
         *cur_buf=key;
         cur_buf++;
       }
    }

    return i+1;

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

//   printf("-----system call wait(%d) \n", pid);

   int exit_code = process_wait(pid);


//   printf("-----system call wait(%d) return!\n", pid);
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


bool sys_create (const char *file, unsigned initial_size){

}
bool sys_remove (const char *file){

}
int sys_open (const char *file){

    struct file *fp;

    fp = filesys_open(file);
    if(fp==NULL)
        return -1;

    return fp;
    
}
int sys_filesize (int fd){

}
void sys_seek(int fd, unsigned position){

}
unsigned sys_tell (int fd){

}
void sys_close (int fd){


}
