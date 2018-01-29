#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
typedef int pid_t;

void syscall_init (void);

int get_argv(int *esp, int** argv,int argc);
int check_addr(const void *address);

void sys_exit(int status);
int sys_write(int fd, const void *buffer, unsigned size);
int sys_read(int fd, void *buffer, unsigned size);
pid_t sys_exec(const char *cmd_line);
int sys_wait(pid_t pid);
void sys_halt (void);
int sys_pibo (int n);

//bool 대신 int 사용
int sys_create (const char *file, unsigned initial_size);
int sys_remove (const char *file);
int sys_open (const char *file);
int sys_filesize (int fd);
void sys_seek(int fd, unsigned position);
unsigned sys_tell (int fd);
void sys_close (int fd);


int allocate_fd(void);
struct opened_file* get_ofp(int fd);
void close_all_files(void);

static int get_user(const unsigned char *uaddr);
static int put_user(unsigned char *udst, unsigned char byte);
#endif /* userprog/syscall.h */
