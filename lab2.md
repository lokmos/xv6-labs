# Task1

在本作业中，您将添加一个系统调用跟踪功能，该功能可能会在以后调试实验时对您有所帮助。您将创建一个新的`trace`系统调用来控制跟踪。它应该有一个参数，这个参数是一个整数“掩码”（mask），它的比特位指定要跟踪的系统调用。例如，要跟踪`fork`系统调用，程序调用`trace(1 << SYS_fork)`，其中`SYS_fork`是***kernel/syscall.h***中的系统调用编号。如果在掩码中设置了系统调用的编号，则必须修改xv6内核，以便在每个系统调用即将返回时打印出一行。该行应该包含进程id、系统调用的名称和返回值；您不需要打印系统调用参数。`trace`系统调用应启用对调用它的进程及其随后派生的任何子进程的跟踪，但不应影响其他进程。

## 提示

- 在***Makefile***的**UPROGS**中添加`$U/_trace`
- 运行`make qemu`，您将看到编译器无法编译***user/trace.c***，因为系统调用的用户空间存根还不存在：将系统调用的原型添加到***user/user.h***，存根添加到***user/usys.pl***，以及将系统调用编号添加到***kernel/syscall.h***，***Makefile***调用perl脚本***user/usys.pl***，它生成实际的系统调用存根***user/usys.S***，这个文件中的汇编代码使用RISC-V的`ecall`指令转换到内核。一旦修复了编译问题（*注：如果编译还未通过，尝试先`make clean`，再执行`make qemu`*），就运行`trace 32 grep hello README`；但由于您还没有在内核中实现系统调用，执行将失败。
- 在***kernel/sysproc.c***中添加一个`sys_trace()`函数，它通过将参数保存到`proc`结构体（请参见***kernel/proc.h***）里的一个新变量中来实现新的系统调用。从用户空间检索系统调用参数的函数在***kernel/syscall.c***中，您可以在***kernel/sysproc.c***中看到它们的使用示例。
- 修改`fork()`（请参阅***kernel/proc.c***）将跟踪掩码从父进程复制到子进程。
- 修改***kernel/syscall.c***中的`syscall()`函数以打印跟踪输出。您将需要添加一个系统调用名称数组以建立索引。

## 实现

- `trace`是一个系统调用，所以要为其分配新的系统调用号(`kernel/syscall.h`)，同时，要声明新的全局内核调用函数，加入`syscalls`映射表(`kernel/syscall.c`)

  - ```c
     // kernel/syscall.h
     // System call numbers
     #define SYS_fork    1
     // ...
     #define SYS_trace  22
    ```

  - ```c
    // kernel/syscall.c 
     extern uint64 sys_chdir(void);
    // ...
     extern uint64 sys_trace(void);   
    
     static uint64 (*syscalls[])(void) = {
     [SYS_fork]    sys_fork,
    // ...
     [SYS_trace]   sys_trace,  
     };

- 按照提示，修改`user/usys.pl`的跳板函数，加入`trace`的入口

  - ```perl
     # user/usys.pl
    
     entry("fork");
    # ...
     entry("trace"); 

- 修改用户态头文件，加入`trace`的定义，函数接受一个整型参数，返回一个整型

  - ```c
    // user/user.h
    // system calls
     int fork(void);
    // ...
     int trace(int);
    ```

- 修改`proc`结构体，使其能够接受新的参数来调用系统调用

  - ```c
    // kernel/proc.h
    // Per-process state
    struct proc {
      struct spinlock lock;
    // ...
    // zyk: add sys_trace mask field
      uint64 trace_mask;
    };
    ```

- 在` proc.c `中，创建新进程的时候，为新添加的 syscall_trace 附上默认值 0。

  - 

  - ```c
    // kernel/proc.c
    static struct proc*
    allocproc(void)
    {
    // ...
      p->context.ra = (uint64)forkret;
      p->context.sp = p->kstack + PGSIZE;
    // zyk: init 0
      p->trace_mask = 0; 
    
      return p;
    }
    ```

- 在 `sysproc.c`中实现`sys_trace`的代码

  - ```c
    // kernel/sysproc.c
    uint64
    sys_trace(void)
    {
      int mask;
    
      if(argint(0, &mask) < 0) 
        return -1;
      
      myproc()->trace_mask = mask; 
      return 0;
    }

- 修改`fork`函数，把父进程的掩码复制给子进程

  - ```c
    // kernel/proc.c
    int
    fork(void)
    {
    // ...
    
      safestrcpy(np->name, p->name, sizeof(p->name));
    
      np->trace_mask = p->trace_mask;
    
      pid = np->pid;
    
      np->state = RUNNABLE;
    
      release(&np->lock);
    
      return pid;
    }

- 在`syscall`函数中，如果系统调用的编号有效，那么打印跟踪信息；用一个字符串数组映射系统调用的名字

  - ```c
    // kernel/syscall.c
    const char *syscall_names[] = {
    [SYS_fork]    "fork",
    [SYS_exit]    "exit",
    [SYS_wait]    "wait",
    [SYS_pipe]    "pipe",
    [SYS_read]    "read",
    [SYS_kill]    "kill",
    [SYS_exec]    "exec",
    [SYS_fstat]   "fstat",
    [SYS_chdir]   "chdir",
    [SYS_dup]     "dup",
    [SYS_getpid]  "getpid",
    [SYS_sbrk]    "sbrk",
    [SYS_sleep]   "sleep",
    [SYS_uptime]  "uptime",
    [SYS_open]    "open",
    [SYS_write]   "write",
    [SYS_mknod]   "mknod",
    [SYS_unlink]  "unlink",
    [SYS_link]    "link",
    [SYS_mkdir]   "mkdir",
    [SYS_close]   "close",
    [SYS_trace]   "trace",
    };
    void
    syscall(void)
    {
      int num;
      struct proc *p = myproc();
    
      num = p->trapframe->a7;
      if(num > 0 && num < NELEM(syscalls) && syscalls[num]) { // 如果系统调用编号有效
        p->trapframe->a0 = syscalls[num](); // 通过系统调用编号，获取系统调用处理函数的指针，调用并将返回值存到用户进程的 a0 寄存器中
    	// 如果当前进程设置了对该编号系统调用的 trace，则打出 pid、系统调用名称和返回值。
        if((p->syscall_trace >> num) & 1) {
          printf("%d: syscall %s -> %d\n",p->pid, syscall_names[num], p->trapframe->a0); // syscall_names[num]: 从 syscall 编号到 syscall 名的映射表
        }
      } else {
        printf("%d %s: unknown sys call %d\n",
                p->pid, p->name, num);
        p->trapframe->a0 = -1;
      }
    }

- 系统调用的全流程如下

  - ```
    user/user.h:		用户态程序调用跳板函数 trace()
    user/usys.S:		跳板函数 trace() 使用 CPU 提供的 ecall 指令，调用到内核态
    kernel/syscall.c	到达内核态统一系统调用处理函数 syscall()，所有系统调用都会跳到这里来处理。
    kernel/syscall.c	syscall() 根据跳板传进来的系统调用编号，查询 syscalls[] 表，找到对应的内核函数并调用。
    kernel/sysproc.c	到达 sys_trace() 函数，执行具体内核操作
    ```

  - 这么繁琐的调用流程的主要目的是实现用户态和内核态的良好隔离。
  - 并且由于内核与用户进程的页表不同，寄存器也不互通，所以参数无法直接通过 C 语言参数的形式传过来，而是需要使用 argaddr、argint、argstr 等系列函数，从进程的 trapframe 中读取用户进程寄存器中的参数。
  - 同时由于页表不同，指针也不能直接互通访问（也就是内核不能直接对用户态传进来的指针进行解引用），而是需要使用 copyin、copyout 方法结合进程的页表，才能顺利找到用户态指针（逻辑地址）对应的物理内存地址。（在本 lab 第二个实验会用到）

## 结果

- ![image-20240508120154256](assets/image-20240508120154256.png)



# Task2

- 在这个作业中，您将添加一个系统调用`sysinfo`，它收集有关正在运行的系统的信息。系统调用采用一个参数：一个指向`struct sysinfo`的指针（参见***kernel/sysinfo.h***）。内核应该填写这个结构的字段：`freemem`字段应该设置为空闲内存的字节数，`nproc`字段应该设置为`state`字段不为`UNUSED`的进程数。我们提供了一个测试程序`sysinfotest`；如果输出“**sysinfotest: OK**”则通过

## 提示

- 在***Makefile***的**UPROGS**中添加`$U/_sysinfotest`
- 当运行`make qemu`时，***user/sysinfotest.c***将会编译失败，遵循和上一个作业一样的步骤添加`sysinfo`系统调用。要在***user/user.h***中声明`sysinfo()`的原型，需要预先声明`struct sysinfo`的存在：

```c
struct sysinfo;
int sysinfo(struct sysinfo *);
```

一旦修复了编译问题，就运行`sysinfotest`；但由于您还没有在内核中实现系统调用，执行将失败。

- `sysinfo`需要将一个`struct sysinfo`复制回用户空间；请参阅`sys_fstat()`(***kernel/sysfile.c***)和`filestat()`(***kernel/file.c***)以获取如何使用`copyout()`执行此操作的示例。
- 要获取空闲内存量，请在***kernel/kalloc.c***中添加一个函数
- 要获取进程数，请在***kernel/proc.c***中添加一个函数

## 实现

- 添加用户态的函数以及系统调用的声明等流程和Task1完全一致，不再赘述

- 获取空闲内存

  - 在内核头文件中声明函数，放在内存相关的函数一起

  - ```c
    // kernel/defs.h
    void*           kalloc(void);
    void            kfree(void *);
    void            kinit(void);
    uint64 			count_free_mem(void); 
    ```

  - 在`kalloc.c`中实现该函数

  - xv6 中，空闲内存页的记录方式是，将空虚内存页**本身**直接用作链表节点，形成一个空闲页链表，每次需要分配，就把链表根部对应的页分配出去。每次需要回收，就把这个页作为新的根节点，把原来的 freelist 链表接到后面。注意这里是**直接使用空闲页本身**作为链表节点，所以不需要使用额外空间来存储空闲页链表，在 kalloc() 里也可以看到，分配内存的最后一个阶段，是直接将 freelist 的根节点地址（物理地址）返回出去了：

  - ```c
    // kernel/kalloc.c
    uint64 count_free_mem(void)
    {
      struct run *r;
      uint64 count = 0;
    
      acquire(&kmem.lock); // 要先锁住内存管理结构，避免竞态条件
      r = kmem.freelist; // 空闲页面链表
      while(r)
      {
        count += PGSIZE;
        r = r->next;
      }
      release(&kmem.lock);
    
      return count;
    }

- 获取运行的进程数

  - 声明函数

  - ```c
    // kernel/defs.h
    void            sleep(void*, struct spinlock*);
    void            userinit(void);
    int             wait(uint64);
    void            wakeup(void*);
    void            yield(void);
    int             either_copyout(int user_dst, uint64 dst, void *src, uint64 len);
    int             either_copyin(void *dst, int user_src, uint64 src, uint64 len);
    void            procdump(void);
    uint64			count_process(void); 

  - 实现

  - ```c
    // kernel/proc.c
    uint64
    count_process(void)
    {
      struct proc *p;
      uint64 count = 0;
      for(p = proc; p < &proc[NPROC]; p++){
        if(p->state != UNUSED)
          count++;
      }
      return count;
    }

- 实现`sysinfo`系统调用

  - ```c
    // kernel/sysproc.c
    // 注意要包含 sysinfo.h 的头文件
    uint64
    sys_sysinfo(void)
    {
      // 从用户态读入一个指针，作为存放 sysinfo 结构的缓冲区
      uint64 addr;
      if(argaddr(0, &addr) < 0)
        return -1;
      
      struct sysinfo sinfo;
      sinfo.freemem = count_free_mem(); // kalloc.c
      sinfo.nproc = count_process(); // proc.c
      
      // 使用 copyout，结合当前进程的页表，获得进程传进来的指针（逻辑地址）对应的物理地址
      // 然后将 &sinfo 中的数据复制到该指针所指位置，供用户进程使用。
      if(copyout(myproc()->pagetable, addr, (char *)&sinfo, sizeof(sinfo)) < 0)
        return -1;
      return 0;
    }

  - 在`user.h`提供用户态入口时，要声明一下`sysinfo`结构

  - ```c
    // user.h
    char* sbrk(int);
    int sleep(int);
    int uptime(void);
    int trace(int);
    struct sysinfo; 
    int sysinfo(struct sysinfo *);
    ```

## 结果

![image-20240508120141446](assets/image-20240508120141446.png)

# 总结

![image-20240508120252832](assets/image-20240508120252832.png)