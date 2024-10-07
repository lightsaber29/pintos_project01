#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/flags.h"
#include "intrinsic.h"

#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/input.h"
#include "lib/string.h"
#include "threads/palloc.h"

void syscall_entry (void);
void syscall_handler (struct intr_frame *);
void check_pointer (void *pointer);
tid_t fork (const char *thread_name, struct intr_frame *f);
/* System call.
 *
 * Previously system call services was handled by the interrupt handler
 * (e.g. int 0x80 in linux). However, in x86-64, the manufacturer supplies
 * efficient path for requesting the system call, the `syscall` instruction.
 *
 * The syscall instruction works by reading the values from the the Model
 * Specific Register (MSR). For the details, see the manual. */

#define MSR_STAR 0xc0000081         /* Segment selector msr */
#define MSR_LSTAR 0xc0000082        /* Long mode SYSCALL target */
#define MSR_SYSCALL_MASK 0xc0000084 /* Mask for the eflags */

void
syscall_init (void) {
	write_msr(MSR_STAR, ((uint64_t)SEL_UCSEG - 0x10) << 48  |
			((uint64_t)SEL_KCSEG) << 32);
	write_msr(MSR_LSTAR, (uint64_t) syscall_entry);

	/* The interrupt service rountine should not serve any interrupts
	 * until the syscall_entry swaps the userland stack to the kernel
	 * mode stack. Therefore, we masked the FLAG_FL. */
	write_msr(MSR_SYSCALL_MASK,
			FLAG_IF | FLAG_TF | FLAG_DF | FLAG_IOPL | FLAG_AC | FLAG_NT);
}

/* The main system call interface */
void
syscall_handler (struct intr_frame *f UNUSED) {
	// TODO: Your implementation goes here.
	// printf ("system call!\n");
	// thread_exit ();
	int syscall_num = f->R.rax;
	switch (syscall_num) {
		case SYS_HALT:
			halt();
			break;
		case SYS_EXIT:
			exit(f->R.rdi);
			break;
		case SYS_FORK:
			f->R.rax = fork(f->R.rdi, f);
			break;
		case SYS_EXEC:
			f->R.rax = exec(f->R.rdi);
			break;
		case SYS_WAIT:
			f->R.rax = wait(f->R.rdi);
			break;
		case SYS_CREATE:
			f->R.rax = create(f->R.rdi, f->R.rsi);
			break;
		case SYS_REMOVE:
			f->R.rax = remove(f->R.rdi);
			break;
		case SYS_OPEN:
			f->R.rax = open(f->R.rdi);
			break;
		case SYS_FILESIZE:
			f->R.rax = filesize(f->R.rdi);
			break;
		case SYS_READ:
			f->R.rax = read(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_WRITE:
			f->R.rax = write(f->R.rdi, f->R.rsi, f->R.rdx);
			break;
		case SYS_SEEK:
			seek(f->R.rdi, f->R.rsi);
			break;
		case SYS_TELL:
			f->R.rax = tell(f->R.rdi);
			break;
		case SYS_CLOSE:
			close(f->R.rdi);
			break;
		default:
			break;
	}
}

void halt (void) {
	power_off();
}

void exit (int status) {
	struct thread *cur = thread_current();
	cur->exit_status = status;
	printf("%s: exit(%d)\n", cur->name, status);
	thread_exit();
}

tid_t fork (const char *thread_name, struct intr_frame *f) {
	// check_pointer(thread_name);
	// if (!strcmp(thread_name, ""))
	// 	return TID_ERROR;
	return (tid_t) process_fork(thread_name, f);
}

int exec (const char *cmd_line) {
	check_pointer(cmd_line);
	if (!strcmp(cmd_line, ""))
		exit(-1);

	char *cl_copy;

	cl_copy = palloc_get_page(0);
	if (cl_copy == NULL)
		exit(-1);
	strlcpy (cl_copy, cmd_line, strlen(cmd_line)+1);
	
	if (process_exec(cl_copy) == -1)
		exit(-1);
}

int wait (pid_t pid) {
	return process_wait(pid);
}

bool create (const char *file, unsigned initial_size) {
	check_pointer(file);
	if (!strcmp(file, "")) {
		exit(-1);
	}
	if (strlen(file) > 15) {
		return false;
	}
	struct file *file_;
	if ((file_ = filesys_open(file)) != NULL) {
		file_close(file_);
		return false;
	}
	return filesys_create(file, initial_size);
}

bool remove (const char *file) {
	check_pointer(file);
	if (!strcmp(file, ""))
		exit(-1);
	return filesys_remove(file);
}

int open (const char *file) {
	check_pointer(file);
	tid_t *tid = thread_current()->tid;
	struct file *file_;
	if ((file_ = filesys_open(file)) == NULL) {
		return -1;
	}
	int fd;
	fd = create_open_file_table(file_);
	return fd;
}

int filesize (int fd) {
	return (int) file_length(thread_current()->fdt[fd]);
}

int read (int fd, void *buffer, unsigned size) {
	check_pointer(buffer);
	tid_t *tid = thread_current()->tid;
	// if (!strcmp(buffer, ""))
	// 	exit(-1);
	if (fd == 0) {
		return input_getc();
	} else if (fd == 1) {
		return -1;
	} else {
		struct file *file = thread_current()->fdt[fd];
		int byte;
		if (file == NULL)
			return -1;
		// lock_acquire(&file->lock);
		byte = file_read(file, buffer, size);
		// lock_release(&file->lock);
		return byte;
	}
}

int write (int fd, const void *buffer, unsigned size) {
	check_pointer(buffer);
	// if (!strcmp(buffer, ""))
	// 	exit(-1);
	if (fd == 0) {
		return -1;
	} else if (fd == 1) {
		putbuf(buffer, size);
		return size;
	} else {
		struct file *file = thread_current()->fdt[fd];
		int byte;
		if (file == NULL)
			return -1;
		// lock_acquire(&file->lock);
		byte = file_write(file, buffer, size);
		// lock_release(&file->lock);
		return byte;
	}
}

void seek (int fd, unsigned position) {
	struct file *file = thread_current()->fdt[fd];
	file_seek(file, position);
}

unsigned tell (int fd) {
	struct file *file = thread_current()->fdt[fd];
	if (file == NULL)
		return -1;
	return file->pos;
}

void close (int fd) {
	struct thread *cur = thread_current();
	tid_t *tid = cur->tid;
	struct file *file = cur->fdt[fd];
	file_close(file);
	thread_current()->fdt[fd] = NULL;
}

void check_pointer (void *pointer) {
	if (is_kernel_vaddr(pointer))
		exit(-1);
	if (pointer == NULL)
		exit(-1);
	if (pml4_get_page(thread_current()->pml4, pointer) == NULL)
		exit(-1);
}
