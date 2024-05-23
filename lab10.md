# Task1

可以通过多种方式调用`mmap`，但本实验只需要与内存映射文件相关的功能子集。您可以假设`addr`始终为零，这意味着内核应该决定映射文件的虚拟地址。`mmap`返回该地址，如果失败则返回`0xffffffffffffffff`。`length`是要映射的字节数；它可能与文件的长度不同。`prot`指示内存是否应映射为可读、可写，以及/或者可执行的；您可以认为`prot`是`PROT_READ`或`PROT_WRITE`或两者兼有。`flags`要么是`MAP_SHARED`（映射内存的修改应写回文件），要么是`MAP_PRIVATE`（映射内存的修改不应写回文件）。您不必在`flags`中实现任何其他位。`fd`是要映射的文件的打开文件描述符。可以假定`offset`为零（它是要映射的文件的起点）。

允许进程映射同一个`MAP_SHARED`文件而不共享物理页面。

`munmap(addr, length)`应删除指定地址范围内的`mmap`映射。如果进程修改了内存并将其映射为`MAP_SHARED`，则应首先将修改写入文件。`munmap`调用可能只覆盖`mmap`区域的一部分，但您可以认为它取消映射的位置要么在区域起始位置，要么在区域结束位置，要么就是整个区域(但不会在区域中间“打洞”)。

## 提示

- 首先，向`UPROGS`添加`_mmaptest`，以及`mmap`和`munmap`系统调用，以便让***user/mmaptest.c\***进行编译。现在，只需从`mmap`和`munmap`返回错误。我们在***kernel/fcntl.h\***中为您定义了`PROT_READ`等。运行`mmaptest`，它将在第一次`mmap`调用时失败。
- 惰性地填写页表，以响应页错误。也就是说，`mmap`不应该分配物理内存或读取文件。相反，在`usertrap`中（或由`usertrap`调用）的页面错误处理代码中执行此操作，就像在lazy page allocation实验中一样。惰性分配的原因是确保大文件的`mmap`是快速的，并且比物理内存大的文件的`mmap`是可能的。
- 跟踪`mmap`为每个进程映射的内容。定义与第15课中描述的VMA（虚拟内存区域）对应的结构体，记录`mmap`创建的虚拟内存范围的地址、长度、权限、文件等。由于xv6内核中没有内存分配器，因此可以声明一个固定大小的VMA数组，并根据需要从该数组进行分配。大小为16应该就足够了。
- 实现`mmap`：在进程的地址空间中找到一个未使用的区域来映射文件，并将VMA添加到进程的映射区域表中。VMA应该包含指向映射文件对应`struct file`的指针；`mmap`应该增加文件的引用计数，以便在文件关闭时结构体不会消失（提示：请参阅`filedup`）。运行`mmaptest`：第一次`mmap`应该成功，但是第一次访问被`mmap`的内存将导致页面错误并终止`mmaptest`。
- 添加代码以导致在`mmap`的区域中产生页面错误，从而分配一页物理内存，将4096字节的相关文件读入该页面，并将其映射到用户地址空间。使用`readi`读取文件，它接受一个偏移量参数，在该偏移处读取文件（但必须lock/unlock传递给`readi`的索引结点）。不要忘记在页面上正确设置权限。运行`mmaptest`；它应该到达第一个`munmap`。
- 实现`munmap`：找到地址范围的VMA并取消映射指定页面（提示：使用`uvmunmap`）。如果`munmap`删除了先前`mmap`的所有页面，它应该减少相应`struct file`的引用计数。如果未映射的页面已被修改，并且文件已映射到`MAP_SHARED`，请将页面写回该文件。查看`filewrite`以获得灵感。
- 理想情况下，您的实现将只写回程序实际修改的`MAP_SHARED`页面。RISC-V PTE中的脏位（`D`）表示是否已写入页面。但是，`mmaptest`不检查非脏页是否没有回写；因此，您可以不用看`D`位就写回页面。
- 修改`exit`将进程的已映射区域取消映射，就像调用了`munmap`一样。运行`mmaptest`；`mmap_test`应该通过，但可能不会通过`fork_test`。
- 修改`fork`以确保子对象具有与父对象相同的映射区域。不要忘记增加VMA的`struct file`的引用计数。在子进程的页面错误处理程序中，可以分配新的物理页面，而不是与父级共享页面。后者会更酷，但需要更多的实施工作。运行`mmaptest`；它应该通过`mmap_test`和`fork_test`。



## 实现

![User Address Space](assets/mit6s081-lab10-useraddrspace.png)

- 为了尽量使得 map 的文件使用的地址空间不要和进程所使用的地址空间产生冲突，选择将 mmap 映射进来的文件 map 到尽可能高的位置，也就是刚好在 trapframe 下面。并且若有多个 mmap 的文件，则向下生长。

  - ```c
    // kernel/memlayout.h
    
    // map the trampoline page to the highest address,
    // in both user and kernel space.
    #define TRAMPOLINE (MAXVA - PGSIZE)
    
    // map kernel stacks beneath the trampoline,
    // each surrounded by invalid guard pages.
    #define KSTACK(p) (TRAMPOLINE - ((p)+1)* 2*PGSIZE)
    
    // User memory layout.
    // Address zero first:
    //   text
    //   original data and bss
    //   fixed-size stack
    //   expandable heap
    //   ...
    //   mmapped files
    //   TRAPFRAME (p->trapframe, used by the trampoline)
    //   TRAMPOLINE (the same page as in the kernel)
    #define TRAPFRAME (TRAMPOLINE - PGSIZE)
    // MMAP 所能使用的最后一个页+1
    #define MMAPEND TRAPFRAME
    ```

- 接下来定义 vma 结构体，其中包含了 mmap 映射的内存区域的各种必要信息，比如开始地址、大小、所映射文件、文件内偏移以及权限等。并且在 proc 结构体末尾为每个进程加上 16 个 vma 空槽。

  - 

  - ```c
    // kernel/proc.h
    
    struct vma {
      int valid;
      uint64 vastart;
      uint64 sz;
      struct file *f;
      int prot;
      int flags;
      uint64 offset;
    };
    
    #define NVMA 16
    
    // Per-process state
    struct proc {
      struct spinlock lock;
    
      // p->lock must be held when using these:
      enum procstate state;        // Process state
      struct proc *parent;         // Parent process
      void *chan;                  // If non-zero, sleeping on chan
      int killed;                  // If non-zero, have been killed
      int xstate;                  // Exit status to be returned to parent's wait
      int pid;                     // Process ID
    
      // these are private to the process, so p->lock need not be held.
      uint64 kstack;               // Virtual address of kernel stack
      uint64 sz;                   // Size of process memory (bytes)
      pagetable_t pagetable;       // User page table
      struct trapframe *trapframe; // data page for trampoline.S
      struct context context;      // swtch() here to run process
      struct file *ofile[NOFILE];  // Open files
      struct inode *cwd;           // Current directory
      char name[16];               // Process name (debugging)
      struct vma vmas[NVMA];       // virtual memory areas
    };
    ```

- 实现`mmap`

  - 在进程的 16 个 vma 槽中，找到可用的空槽，并且顺便计算所有 vma 中使用到的最低的虚拟地址

  - 然后将当前文件映射到该最低地址下面的位置（vastart = vaend - sz）

  - ```c
    uint64
    sys_mmap(void)
    {
      uint64 addr, sz, offset; // 声明地址、大小和偏移量变量
      int prot, flags, fd; // 声明保护、标志和文件描述符变量
      struct file *f; // 声明文件结构体指针
    
      // 获取用户传递的参数并进行检查，若任何参数无效则返回-1
      if(argaddr(0, &addr) < 0 || argaddr(1, &sz) < 0 || argint(2, &prot) < 0
        || argint(3, &flags) < 0 || argfd(4, &fd, &f) < 0 || argaddr(5, &offset) < 0 || sz == 0)
        return -1;
      
      // 检查文件的可读可写权限是否与映射要求匹配，不匹配则返回-1
      if((!f->readable && (prot & (PROT_READ)))
         || (!f->writable && (prot & PROT_WRITE) && !(flags & MAP_PRIVATE)))
        return -1;
      
      // 将大小对齐到页大小的倍数
      sz = PGROUNDUP(sz);
    
      struct proc *p = myproc(); // 获取当前进程
      struct vma *v = 0; // 声明虚拟内存区域指针并初始化为0
      uint64 vaend = MMAPEND; // 虚拟地址结束位置（不包含）
    
      // mmaptest从未传递非零地址参数，因此这里忽略addr，找到新的未映射虚拟地址区域来映射文件
      // 实现将文件映射到陷阱框架下方，从高地址到低地址
    
      // 查找一个空闲的vma，并在此过程中计算文件映射位置
      for(int i=0; i<NVMA; i++) {
        struct vma *vv = &p->vmas[i]; // 获取当前vma
        if(vv->valid == 0) { // 如果vma无效（空闲）
          if(v == 0) {
            v = &p->vmas[i]; // 记录第一个空闲的vma
            v->valid = 1; // 标记vma为有效
          }
        } else if(vv->vastart < vaend) { // 更新虚拟地址结束位置
          vaend = PGROUNDDOWN(vv->vastart);
        }
      }
    
      if(v == 0){ // 如果没有找到空闲vma，触发panic
        panic("mmap: no free vma");
      }
      
      // 设置vma的各个属性
      v->vastart = vaend - sz; // 起始地址
      v->sz = sz; // 大小
      v->prot = prot; // 保护
      v->flags = flags; // 标志
      v->f = f; // 文件指针（假设f->type == FD_INODE）
      v->offset = offset; // 偏移量
    
      filedup(v->f); // 增加文件引用计数
    
      return v->vastart; // 返回映射起始地址
    }
    ```

- 由于需要对映射的页实行懒加载，仅在访问到的时候才从磁盘中加载出来

  - 和懒分配的那个实验类似

  - ```c
    // kernel/trap.c
    void
    usertrap(void)
    {
      int which_dev = 0; 
    
      // ......
    
      } else if((which_dev = devintr()) != 0){
        // 处理设备中断
        // ok
      } else {
        uint64 va = r_stval(); // 获取产生陷阱的虚拟地址
        if((r_scause() == 13 || r_scause() == 15)){ // 检查是否为加载或存储页面故障
          if(!vmatrylazytouch(va)) { // 尝试进行vma惰性分配
            goto unexpected_scause; // 如果惰性分配失败，跳转到unexpected_scause标签
          }
        } else {
          unexpected_scause:
          printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid); // 打印意外的陷阱原因和进程ID
          printf("            sepc=%p stval=%p\n", r_sepc(), r_stval()); // 打印陷阱程序计数器和陷阱地址
          p->killed = 1; // 标记进程被杀死
        }
      }
    
      // ......
    
      usertrapret(); 
    }

  - ```c
    // kernel/sysfile.c
    
    // find a vma using a virtual address inside that vma.
    // 根据虚拟地址在进程的vma数组中查找包含该地址的vma。
    struct vma *findvma(struct proc *p, uint64 va) {
      for(int i = 0; i < NVMA; i++) {
        struct vma *vv = &p->vmas[i]; // 获取当前vma
        if(vv->valid == 1 && va >= vv->vastart && va < vv->vastart + vv->sz) {
          // 如果vma有效且va在当前vma范围内，返回该vma
          return vv;
        }
      }
      return 0; // 未找到则返回0
    }
    
    // finds out whether a page is previously lazy-allocated for a vma
    // and needed to be touched before use.
    // if so, touch it so it's mapped to an actual physical page and contains
    // content of the mapped file.
    // 检查页面是否之前为vma惰性分配，并在使用前需要处理。
    // 如果是，则处理该页面，使其映射到实际的物理页面并包含映射文件的内容。
    int vmatrylazytouch(uint64 va) {
      struct proc *p = myproc(); // 获取当前进程
      struct vma *v = findvma(p, va); // 查找包含va的vma
      if(v == 0) { // 如果未找到对应的vma，返回0
        return 0;
      }
    
      // printf("vma mapping: %p => %d\n", va, v->offset + PGROUNDDOWN(va - v->vastart));
    
      // allocate physical page
      // 分配物理页面
      void *pa = kalloc();
      if(pa == 0) {
        panic("vmalazytouch: kalloc"); // 分配失败则触发panic
      }
      memset(pa, 0, PGSIZE); // 将物理页面清零
    
      // read data from disk
      // 从磁盘读取数据
      begin_op();
      ilock(v->f->ip); // 加锁文件
      readi(v->f->ip, 0, (uint64)pa, v->offset + PGROUNDDOWN(va - v->vastart), PGSIZE); // 读取文件内容到物理页面
      iunlock(v->f->ip); // 解锁文件
      end_op();
    
      // set appropriate perms, then map it.
      // 设置合适的权限，然后映射页面
      int perm = PTE_U;
      if(v->prot & PROT_READ)
        perm |= PTE_R;
      if(v->prot & PROT_WRITE)
        perm |= PTE_W;
      if(v->prot & PROT_EXEC)
        perm |= PTE_X;
    
      if(mappages(p->pagetable, va, PGSIZE, (uint64)pa, perm) < 0) {
        panic("vmalazytouch: mappages"); // 映射失败则触发panic
      }
    
      return 1; // 映射成功返回1
    }
    ```

- 实现 `munmap`调用，将一个 vma 所分配的所有页释放，并在必要的情况下，将已经修改的页写回磁盘。

  - ```c
    uint64
    sys_munmap(void)
    {
      uint64 addr, sz;
    
      // 获取用户传递的参数并进行检查，若任何参数无效则返回-1
      if(argaddr(0, &addr) < 0 || argaddr(1, &sz) < 0 || sz == 0)
        return -1;
    
      struct proc *p = myproc(); // 获取当前进程
    
      struct vma *v = findvma(p, addr); // 查找包含addr的vma
      if(v == 0) { // 如果未找到对应的vma，返回-1
        return -1;
      }
    
      if(addr > v->vastart && addr + sz < v->vastart + v->sz) {
        // 尝试在内存范围内"挖一个洞"，这种情况不被允许
        return -1;
      }
    
      uint64 addr_aligned = addr; // 初始化对齐的地址为输入地址
      if(addr > v->vastart) {
        addr_aligned = PGROUNDUP(addr); // 对齐地址到页边界
      }
    
      int nunmap = sz - (addr_aligned - addr); // 计算需要取消映射的字节数
      if(nunmap < 0)
        nunmap = 0;
      
      vmaunmap(p->pagetable, addr_aligned, nunmap, v); // 自定义内存页面取消映射例程
    
      if(addr <= v->vastart && addr + sz > v->vastart) { // 如果取消映射从vma的开始位置开始
        v->offset += addr + sz - v->vastart; // 更新偏移量
        v->vastart = addr + sz; // 更新vma的起始地址
      }
      v->sz -= sz; // 减少vma的大小
    
      if(v->sz <= 0) { // 如果vma的大小小于等于0，关闭文件并标记vma无效
        fileclose(v->f);
        v->valid = 0;
      }
    
      return 0; // 成功返回0
    }
    ```

  - `vmaunmap`实现了对内存页的释放

  - ```c
    // kernel/vm.c
    #include "fcntl.h"
    #include "spinlock.h"
    #include "sleeplock.h"
    #include "file.h"
    #include "proc.h"
    
    // Remove n BYTES (not pages) of vma mappings starting from va. va must be
    // page-aligned. The mappings NEED NOT exist.
    // Also free the physical memory and write back vma data to disk if necessary.
    // 从va开始移除n字节（不是页）的vma映射。va必须对齐到页边界。映射不需要存在。
    // 还需要释放物理内存，并在必要时将vma数据写回磁盘。
    void
    vmaunmap(pagetable_t pagetable, uint64 va, uint64 nbytes, struct vma *v)
    {
      uint64 a;
      pte_t *pte;
    
      // printf("unmapping %d bytes from %p\n",nbytes, va);
    
      // borrowed from "uvmunmap"
      // 从"uvmunmap"中借用的代码
      for(a = va; a < va + nbytes; a += PGSIZE){
        if((pte = walk(pagetable, a, 0)) == 0) // 获取页表条目
          continue;
        if(PTE_FLAGS(*pte) == PTE_V) // 检查是否为叶子节点
          panic("sys_munmap: not a leaf");
        if(*pte & PTE_V){ // 如果页表条目有效
          uint64 pa = PTE2PA(*pte); // 获取物理地址
          if((*pte & PTE_D) && (v->flags & MAP_SHARED)) { // 如果页面被修改且映射为共享，则需要写回磁盘
            begin_op();
            ilock(v->f->ip); // 加锁文件
            uint64 aoff = a - v->vastart; // 计算相对于内存区域起始位置的偏移量
            if(aoff < 0) { // 如果第一页不是完整的4k页
              writei(v->f->ip, 0, pa + (-aoff), v->offset, PGSIZE + aoff);
            } else if(aoff + PGSIZE > v->sz){  // 如果最后一页不是完整的4k页
              writei(v->f->ip, 0, pa, v->offset + aoff, v->sz - aoff);
            } else { // 完整的4k页
              writei(v->f->ip, 0, pa, v->offset + aoff, PGSIZE);
            }
            iunlock(v->f->ip); // 解锁文件
            end_op();
          }
          kfree((void*)pa); // 释放物理内存
          *pte = 0; // 清除页表条目
        }
      }
    }
    ```

- 在`riscv.h`中添加 dirty bit 的宏定义

  - ```c
    // kernel/riscv.h
    
    #define PTE_V (1L << 0) // valid
    #define PTE_R (1L << 1)
    #define PTE_W (1L << 2)
    #define PTE_X (1L << 3)
    #define PTE_U (1L << 4) // 1 -> user can access
    #define PTE_G (1L << 5) // global mapping
    #define PTE_A (1L << 6) // accessed
    #define PTE_D (1L << 7) // dirty

- 修改`proc.c`，添加处理vma的代码

  - 让 allocproc 初始化进程的时候，将 vma 槽都清空

  - freeproc 释放进程时，调用 vmaunmap 将所有 vma 的内存都释放，并在需要的时候写回磁盘

  - fork 时，拷贝父进程的所有 vma，但是不拷贝物理页

  - ```c
    // kernel/proc.c
    
    static struct proc*
    allocproc(void)
    {
      // ......
    
      // Clear VMAs
      // 清除虚拟内存区域（VMA）
      for(int i = 0; i < NVMA; i++) {
        p->vmas[i].valid = 0; // 将所有VMA标记为无效
      }
    
      return p; // 返回分配的进程结构
    }
    
    // free a proc structure and the data hanging from it,
    // including user pages.
    // p->lock must be held.
    // 释放进程结构及其附属数据，包括用户页。
    // 必须持有p->lock。
    static void
    freeproc(struct proc *p)
    {
      if(p->trapframe)
        kfree((void*)p->trapframe); // 释放陷阱帧
      p->trapframe = 0;
      for(int i = 0; i < NVMA; i++) {
        struct vma *v = &p->vmas[i];
        vmaunmap(p->pagetable, v->vastart, v->sz, v); // 取消映射VMA
      }
      if(p->pagetable)
        proc_freepagetable(p->pagetable, p->sz); // 释放页表
      p->pagetable = 0;
      p->sz = 0;
      p->pid = 0;
      p->parent = 0;
      p->name[0] = 0;
      p->chan = 0;
      p->killed = 0;
      p->xstate = 0;
      p->state = UNUSED; // 将进程状态设置为未使用
    }
    
    // Create a new process, copying the parent.
    // Sets up child kernel stack to return as if from fork() system call.
    // 创建一个新进程，复制父进程。
    // 设置子进程的内核栈以从fork()系统调用返回。
    int
    fork(void)
    {
      // ......
    
      // copy vmas created by mmap.
      // actual memory page as well as pte will not be copied over.
      // 复制由mmap创建的VMA。
      // 实际的内存页和页表项不会被复制。
      for(int i = 0; i < NVMA; i++) {
        struct vma *v = &p->vmas[i];
        if(v->valid) {
          np->vmas[i] = *v; // 复制VMA
          filedup(v->f); // 增加文件引用计数
        }
      }
    
      safestrcpy(np->name, p->name, sizeof(p->name)); // 复制进程名
    
      int pid = np->pid; // 获取子进程的PID
    
      np->state = RUNNABLE; // 将子进程状态设置为可运行
    
      release(&np->lock); // 释放子进程的锁
    
      return pid; // 返回子进程的PID
    }
    ```

    