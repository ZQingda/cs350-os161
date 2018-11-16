
#include "opt-A2.h"
#if OPT_A2
#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <synch.h>
#include <mips/trapframe.h>
#include <limits.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  //(void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* MY STUFF HERE */

  /* exit, tell kids they can stop existing, tell parent I've exited */

  curproc->exit_code = _MKWAIT_EXIT(exitcode);
  curproc->exited = true;

  lock_acquire(curproc->proc_lock);
  cv_signal(curproc->exited_cv, curproc->proc_lock);
  lock_release(curproc->proc_lock);
  unsigned int num_children = array_num(curproc->child_procs);

  DEBUG(DB_SYSCALL,"Syscall: LOOP START, NUM KIDS IS: %d\n", num_children);
  for (unsigned int i = 0; i < num_children; i++) {
    DEBUG(DB_SYSCALL,"Syscall: LOOPING THROUGH KIDS, CURRENTLY KID: %d\n", i);
    struct proc * cur_child = array_get(curproc->child_procs, 0);
    lock_acquire(cur_child->proc_lock);
    cv_signal(cur_child->can_dne_cv, cur_child->proc_lock);
    lock_release(cur_child->proc_lock);
    cur_child->parent_proc = NULL;
    array_remove(curproc->child_procs, 0);
  }
  DEBUG(DB_SYSCALL,"Syscall: LOOP END\n");

  /* wait on parent to see if they've exited and I can stop existing */

  lock_acquire(curproc->proc_lock);
  while(curproc->parent_proc != NULL) {
    cv_wait(curproc->can_dne_cv, curproc->proc_lock);
  }
  lock_release(curproc->proc_lock);

  /* MY STUFF IS OVER */

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  DEBUG(DB_SYSCALL, "Getting pid: %d\n", curproc->pid);
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = curproc->pid;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  DEBUG(DB_SYSCALL, "Waiting on pid: %d\n", pid);
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }

  /* MY STUFF HERE */

  if (status == NULL) {
    return(EFAULT);
  }

  /* find which child has the passed in pid and wait on it to exit */
  struct proc * wait_child = NULL;

  for (unsigned int i = 0; i < array_num(curproc->child_procs); i++) {
    wait_child = array_get(curproc->child_procs, i);
    if (wait_child->pid != pid) {
      wait_child = NULL;
    }
  }

  if (wait_child == NULL) {
    return (ESRCH);
  }

  lock_acquire(wait_child->proc_lock);
  while(!wait_child->exited) {
    cv_wait(wait_child->exited_cv, wait_child->proc_lock);
  }
  lock_release(wait_child->proc_lock);

  exitstatus = wait_child->exit_code;
  DEBUG(DB_SYSCALL, "WAITING FINISHED EXITSTATUS IS : %d\n", exitstatus);
  /* MY STUFF IS OVER */

  /* for now, just pretend the exitstatus is 0 */
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

pid_t sys_fork(pid_t *retval, struct trapframe *tf) {
  DEBUG(DB_SYSCALL, "Forking\n");
  lock_acquire(curproc->proc_lock);
  
  struct proc *forked_proc = proc_create_runprogram(curproc->p_name);
  as_copy(curproc_getas(), &forked_proc->p_addrspace);
  array_add(curproc->child_procs, forked_proc, NULL);
  forked_proc->parent_proc = curproc;

  struct trapframe *fork_tf = kmalloc(sizeof(struct trapframe));
  memcpy(fork_tf, tf, sizeof(struct trapframe));
  
  lock_release(curproc->proc_lock);

  thread_fork(forked_proc->p_name, forked_proc, &enter_forked_process, fork_tf, 0);
  DEBUG(DB_SYSCALL, "FORKED PROCESS AT PID: %d\n", forked_proc->pid);
  *retval = forked_proc->pid;
  return(0);
}

#else
#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;
  /* for now, just include this to keep the compiler from complaining about
     an unused variable */
  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#endif
