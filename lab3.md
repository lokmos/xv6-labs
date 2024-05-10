# Task1

- 定义一个名为`vmprint()`的函数。它应当接收一个`pagetable_t`作为参数，并以下面描述的格式打印该页表。在`exec.c`中的`return argc`之前插入`if(p->pid==1) vmprint(p->pagetable)`，以打印第一个进程的页表。如果你通过了`pte printout`测试的`make grade`，你将获得此作业的满分。

- 现在，当您启动xv6时，它应该像这样打印输出来描述第一个进程刚刚完成`exec()`ing`init`时的页表：

  ```
  page table 0x0000000087f6e000
  ..0: pte 0x0000000021fda801 pa 0x0000000087f6a000
  .. ..0: pte 0x0000000021fda401 pa 0x0000000087f69000
  .. .. ..0: pte 0x0000000021fdac1f pa 0x0000000087f6b000
  .. .. ..1: pte 0x0000000021fda00f pa 0x0000000087f68000
  .. .. ..2: pte 0x0000000021fd9c1f pa 0x0000000087f67000
  ..255: pte 0x0000000021fdb401 pa 0x0000000087f6d000
  .. ..511: pte 0x0000000021fdb001 pa 0x0000000087f6c000
  .. .. ..510: pte 0x0000000021fdd807 pa 0x0000000087f76000
  .. .. ..511: pte 0x0000000020001c0b pa 0x0000000080007000
  ```

  第一行显示`vmprint`的参数。之后的每行对应一个PTE，包含树中指向页表页的PTE。每个PTE行都有一些“`..`”的缩进表明它在树中的深度。每个PTE行显示其在页表页中的PTE索引、PTE比特位以及从PTE提取的物理地址。不要打印无效的PTE。在上面的示例中，顶级页表页具有条目0和255的映射。条目0的下一级只映射了索引0，该索引0的下一级映射了条目0、1和2。

  您的代码可能会发出与上面显示的不同的物理地址。条目数和虚拟地址应相同。

## 提示

- 你可以将`vmprint()`放在***kernel/vm.c***中
- 使用定义在***kernel/riscv.h***末尾处的宏
- 函数`freewalk`可能会对你有所启发
- 将`vmprint`的原型定义在***kernel/defs.h***中，这样你就可以在`exec.c`中调用它了
- 在你的`printf`调用中使用`%p`来打印像上面示例中的完成的64比特的十六进制PTE和地址

## 实现

- 在***kernel/defs.h***中声明函数

  - ```c
    int vmprint(pagetable_t);

- 修改***kernel/exec.c***中的`exec`函数，在`exec.c`中的`return argc`之前插入`if(p->pid==1) vmprint(p->pagetable)`，以打印第一个进程的页表。

  - ```c
    //zyk: output pagetable info
      if(p->pid==1)
        vmprint(p->pagetable);
    
      return argc; // this ends up in a0, the first argument to main(argc, argv)

- 实现`vmprint`，参考`freewalk`函数，把释放页面的过程修改为打印页面的过程

  - ```c
    int
    vmprint_helper(pagetable_t pagetable, int depth)
    {
      // there are 2^9 = 512 PTEs in a page table.
      for(int i = 0; i < 512; i++){
        pte_t pte = pagetable[i];
        if(pte & PTE_V){
          printf("..");
          for(int j = 0; j < depth; j++){
            printf(" ..");
          }
          printf("%d: pte %p pa %p\n", i, pte, PTE2PA(pte));
          if ((pte & (PTE_R|PTE_W|PTE_X)) == 0){
            // this PTE points to a lower-level page table.
            uint64 child = PTE2PA(pte);
            vmprint_helper((pagetable_t)child, depth+1);
          }
        } else if(pte & PTE_V){
          panic("vmprint: leaf");
        }
      }
      return 0;
    }
    
    int 
    vmprint(pagetable_t pagetable)
    {
      printf("page table %p\n", pagetable);
      vmprint_helper(pagetable, 0);
      return 0;
    }
    ```

## 结果

![image-20240509170110492](assets/image-20240509170110492.png)



# Task2

- 你的第一项工作是修改内核来让每一个进程在内核中执行时使用它自己的内核页表的副本。修改`struct proc`来为每一个进程维护一个内核页表，修改调度程序使得切换进程时也切换内核页表。对于这个步骤，每个进程的内核页表都应当与现有的的全局内核页表完全一致。如果你的`usertests`程序正确运行了，那么你就通过了这个实验。

## 提示

- 在`struct proc`中为进程的内核页表增加一个字段
- 为一个新进程生成一个内核页表的合理方案是实现一个修改版的`kvminit`，这个版本中应当创造一个新的页表而不是修改`kernel_pagetable`。你将会考虑在`allocproc`中调用这个函数
- 确保每一个进程的内核页表都关于该进程的内核栈有一个映射。在未修改的XV6中，所有的内核栈都在`procinit`中设置。你将要把这个功能部分或全部的迁移到`allocproc`中
- 修改`scheduler()`来加载进程的内核页表到核心的`satp`寄存器(参阅`kvminithart`来获取启发)。不要忘记在调用完`w_satp()`后调用`sfence_vma()`
- 没有进程运行时`scheduler()`应当使用`kernel_pagetable`
- 在`freeproc`中释放一个进程的内核页表
- 你需要一种方法来释放页表，而不必释放叶子物理内存页面。
- 调式页表时，也许`vmprint`能派上用场
- 修改XV6本来的函数或新增函数都是允许的；你或许至少需要在***kernel/vm.c***和***kernel/proc.c***中这样做（但不要修改***kernel/vmcopyin.c***, ***kernel/stats.c***, ***user/usertests.c***, 和***user/stats.c***）
- 页表映射丢失很可能导致内核遭遇页面错误。这将导致打印一段包含`sepc=0x00000000XXXXXXXX`的错误提示。你可以在***kernel/kernel.asm***通过查询`XXXXXXXX`来定位错误。

## 实现

- 首先在`proc`结构体中加入每个进程自己的内核页表项

  -  ```c
     //kernel/proc.h
     // Per-process state
     struct proc {
       struct spinlock lock;
       // ...
       // zyk: kernel page table for this process
       pagetable_t kernelpgtbl;
     };
     ```

- 然后修改`kvminit`，使其接受一个页表作为参数（原来是直接使用全局的内核页表）；具体方法把原来的`kvminit`复制过来再做修改即可

  - ```c
    // kernel/vm.c
    // zyk: a new kvminit for single process
    pagetable_t
    kvminit_new()
    {
      pagetable_t pgtbl = (pagetable_t) kalloc();
      memset(pgtbl, 0, PGSIZE);
    
      // uart registers
      kvmmap(pgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
    
      // virtio mmio disk interface
      kvmmap(pgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
    
      // CLINT
      kvmmap(pgtbl, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
    
      // PLIC
      kvmmap(pgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
    
      // map kernel text executable and read-only.
      kvmmap(pgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
    
      // map kernel data and the physical RAM we'll make use of.
      kvmmap(pgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
    
      // map the trampoline for trap entry/exit to
      // the highest virtual address in the kernel.
      kvmmap(pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
    
      return pgtbl;
    }

  - 其中依赖函数`kvmmap`，修改该函数，同样的使其能够接受不同进程的页表作为参数，并替换其中原来使用全局内核页表的代码

    - ```c
      void
      kvmmap(pagetable_t pgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
      {
        if(mappages(pgtbl, va, sz, pa, perm) != 0)
          panic("kvmmap");
      }

  - 函数`kvmpa`会在之后使用，用于将内核逻辑地址转换为物理地址，也要修改

    - ```c
      uint64
      kvmpa(pagetable_t pgtbl, uint64 va)
      {
        uint64 off = va % PGSIZE;
        pte_t *pte;
        uint64 pa;
      
        pte = walk(pgtbl, va, 0);
        if(pte == 0)
          panic("kvmpa");
        if((*pte & PTE_V) == 0)
          panic("kvmpa");
        pa = PTE2PA(*pte);
        return pa+off;
      }

  - 最后，原来的`kvminit`也要修改，全局的内核页表依然需要，但要用新的初始化函数给其赋值

    - ```c
      void
      kvminit()
      {
        kernel_pagetable = kvminit_newpgtbl(); 
      }
      
      ```

- 现在可以创建进程间相互独立的内核页表了，但是还有一个东西需要处理：内核栈。 原本的 xv6 设计中，所有处于内核态的进程都共享同一个页表，即意味着共享同一个地址空间。由于 xv6 支持多核/多进程调度，同一时间可能会有多个进程处于内核态，所以需要对所有处于内核态的进程创建其独立的内核态内的栈，也就是内核栈，供给其内核态代码执行过程。

- xv6 在启动过程中，会在 procinit() 中为所有可能的 64 个进程位都预分配好内核栈 kstack，具体为在高地址空间里，每个进程使用一个页作为 kstack，并且两个不同 kstack 中间隔着一个无映射的 guard page 用于检测栈溢出错误。具体参考 [xv6 book](https://pdos.csail.mit.edu/6.S081/2020/xv6/book-riscv-rev1.pdf) 的 Figure 3.3。

- 在 xv6 原来的设计中，内核页表本来是只有一个的，所有进程共用，所以需要为不同进程创建多个内核栈，并 map 到不同位置（见 `procinit()` 和 `KSTACK` 宏）。而我们的新设计中，每一个进程都会有自己独立的内核页表，并且每个进程也只需要访问自己的内核栈，而不需要能够访问所有 64 个进程的内核栈。所以可以将所有进程的内核栈 map 到其**各自内核页表内的固定位置**（不同页表内的同一逻辑地址，指向不同物理内存）。

- 修改`procinit`函数，原先该函数为进程分配内核栈，现在不在这里完成，等到`allocproc`函数时再进行

  - ```c
    // kernel/proc.c
    void
    procinit(void)
    {
      struct proc *p;
      
      initlock(&pid_lock, "nextpid");
      for(p = proc; p < &proc[NPROC]; p++) {
          initlock(&p->lock, "proc");
    
          // Allocate a page for the process's kernel stack.
          // Map it high in memory, followed by an invalid
          // guard page.
          // char *pa = kalloc();
          // if(pa == 0)
          //   panic("kalloc");
          // uint64 va = KSTACK((int) (p - proc));
          // kvmmap(va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
          // p->kstack = va;
      }
      kvminithart();
    }
    ```

- 根据提示，修改`allocproc`，在创建进程的时候，为其分配独立的内核页表和内核栈

  - ```c
    // kernel/proc.c
    static struct proc*
    allocproc(void)
    {
      struct proc *p;
      // ...
    
    found:
      // ...
      }
    
      // zyk: create kernel pagetable for process
     // 创建页表
      p->kernelpgtbl = kvminit_new();
      char *pa = kalloc();
      if(pa == 0)
        panic("kalloc");
      // 固定内核栈逻辑地址
      uint64 va = KSTACK((int)0);
      kvmmap(p->kernelpgtbl, va, (uint64)pa, PGSIZE, PTE_R | PTE_W);
      p->kstack = va;
    
      // ...
    
      return p;
    }

- 以上完成了内核页表的创建，随后要在用户进程进入内核态时使用自己的页表，修改`scheduler`

  - ```c
    // kernel/proc.c
    void
    scheduler(void)
    {
      struct proc *p;
      struct cpu *c = mycpu();
      
      c->proc = 0;
      for(;;){
        // Avoid deadlock by ensuring that devices can interrupt.
        intr_on();
        
        int found = 0;
        for(p = proc; p < &proc[NPROC]; p++) {
          acquire(&p->lock);
          if(p->state == RUNNABLE) {
            // Switch to chosen process.  It is the process's job
            // to release its lock and then reacquire it
            // before jumping back to us.
            p->state = RUNNING;
            c->proc = p;
    
            // zyk: switch to process's kernel page table
            // 切换到独立的内核页表
            w_satp(MAKE_SATP(p->kernelpgtbl));
            // 清除快表缓存
            sfence_vma();
    		// 调度执行
            swtch(&c->context, &p->context);
    
            // zyk: go back to kernel page table
            kvminithart();
    
            // Process is done running for now.
            // It should have changed its p->state before coming back.
            c->proc = 0;
    
            found = 1;
          }
          release(&p->lock);
        }
    #if !defined (LAB_FS)
        if(found == 0) {
          intr_on();
          asm volatile("wfi");
        }
    #else
        ;
    #endif
      }
    }
    ```

  - 

- 最后，在进程结束后，要释放掉进程独享的资源和页表

  - 修改`freeproc`，释放进程的内核栈和页表

  - ```c
    // kernel/proc.c
    static void
    freeproc(struct proc *p)
    {
      // ...
    
      void *kstack_pa = (void *)kvmpa(p->kernelpgtbl, p->kstack);
       // 注意：此处不能使用 proc_freepagetable，因为其不仅会释放页表本身，还会把页表内所有的叶节点对应的物理页也释放掉。
      // 这会导致内核运行所需要的关键物理页被释放，从而导致内核崩溃。
      // 这里使用 kfree(p->kernelpgtbl) 也是不足够的，因为这只释放了**一级页表本身**，而不释放二级以及三级页表所占用的空间。
      
      kfree(kstack_pa);
      p->kstack = 0;
      // 递归释放进程独享的页表，释放页表本身所占用的空间，但**不释放页表指向的物理页**
      free_kernelpgtbl(p->kernelpgtbl);
      p->kernelpgtbl = 0;
      p->state = UNUSED;
    }

  - 其中=递归释放页表的函数`free_kernelpgtbl`

  - ```c
    // kernel/vm.c
    // zyk: free kernel page table
    void
    free_kernelpgtbl(pagetable_t pgtbl) {
      // there are 2^9 = 512 PTEs in a page table.
      for(int i = 0; i < 512; i++){
        pte_t pte = pgtbl[i];
        uint64 child = PTE2PA(pte);
        if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){ 
          free_kernelpgtbl((pagetable_t)child);
          pgtbl[i] = 0;
        }
      }
      kfree((void*)pgtbl); 
    }
    ```

- 最后，在调试的过程中，陷入了`kvmpa`函数，寻找发现 virtio 磁盘驱动 ***virtio_disk.c*** 中调用了 该函数，用于将虚拟地址转换为物理地址；因此也要修改为添加了`pagetable_t`参数的版本

  - ```c
    // kernel/virtio_disk.c
    #include "proc.h"
    void
    virtio_disk_rw(struct buf *b, int write)
    {
    // ......
    disk.desc[idx[0]].addr = (uint64) kvmpa(myproc()->kernelpgtbl, (uint64) &buf0); // 调用 myproc()，获取进程内核页表
    // ......
    }

## 结果

![image-20240509201118180](assets/image-20240509201118180.png)



# Task3

- 将定义在***kernel/vm.c***中的`copyin`的主题内容替换为对`copyin_new`的调用（在***kernel/vmcopyin.c***中定义）；对`copyinstr`和`copyinstr_new`执行相同的操作。为每个进程的内核页表添加用户地址映射，以便`copyin_new`和`copyinstr_new`工作。如果`usertests`正确运行并且所有`make grade`测试都通过，那么你就完成了此项作业。

## 提示

- 内核的`copyin`函数读取用户指针指向的内存。它通过将用户指针转换为内核可以直接解引用的物理地址来实现这一点。这个转换是通过在软件中遍历进程页表来执行的。在本部分的实验中，您的工作是将用户空间的映射添加到每个进程的内核页表（上一节中创建），以允许`copyin`（和相关的字符串函数`copyinstr`）直接解引用用户指针。
- 此方案依赖于用户的虚拟地址范围不与内核用于自身指令和数据的虚拟地址范围重叠。Xv6使用从零开始的虚拟地址作为用户地址空间，幸运的是内核的内存从更高的地址开始。然而，这个方案将用户进程的最大大小限制为小于内核的最低虚拟地址。内核启动后，在XV6中该地址是`0xC000000`，即PLIC寄存器的地址；请参见***kernel/vm.c***中的`kvminit()`、***kernel/memlayout.h***和文中的图3-4。您需要修改xv6，以防止用户进程增长到超过PLIC的地址。
- 先用对`copyin_new`的调用替换`copyin()`，确保正常工作后再去修改`copyinstr`
- 在内核更改进程的用户映射的每一处，都以相同的方式更改进程的内核页表。包括`fork()`, `exec()`, 和`sbrk()`.
- 不要忘记在`userinit`的内核页表中包含第一个进程的用户页表
- 用户地址的PTE在进程的内核页表中需要什么权限？(在内核模式下，无法访问设置了`PTE_U`的页面）
- 别忘了上面提到的PLIC限制

## 实现

- 首先定义一个复制页表的函数，因为在实验中需要把用户态页表映射的一部分复制给内核页表

  - 函数需要指定源页表和目的页表，然后还要指定复制开始的位置和复制的大小

  - 只拷贝页表项，不拷贝实际的物理页内存

  - 因为内核无法直接访问用户页，所以在复制的时候，用`& ~PTE_U`将页的权限设置为非用户页

  - ```c
    // kernel/vm.c
    int
    kvmcopymappings(pagetable_t src, pagetable_t dst, uint64 start, uint64 sz)
    {
      pte_t *pte;
      uint64 pa, i;
      uint flags;
    
      // PGROUNDUP: prevent re-mapping already mapped pages (eg. when doing growproc)
      for(i = PGROUNDUP(start); i < start + sz; i += PGSIZE){
        if((pte = walk(src, i, 0)) == 0)
          panic("kvmcopymappings: pte should exist");
        if((*pte & PTE_V) == 0)
          panic("kvmcopymappings: page not present");
        pa = PTE2PA(*pte);
        flags = PTE_FLAGS(*pte) & ~PTE_U;
        if(mappages(dst, i, PGSIZE, pa, flags) != 0){
          goto err;
        }
      }
    
      return 0;
    
     err:
      uvmunmap(dst, PGROUNDUP(start), (i - PGROUNDUP(start)) / PGSIZE, 0);
      return -1;
    }
    ```

- 还需要准备一个函数，用于内核页表中的程序内存映射和用户页表中的程序内存映射的同步

  - 参考`uvmdealloc`，把程序内存缩小，但这里不释放实际内存

  - ```c
    // kernel/vm.c
    uint64
    kvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
    {
      if(newsz >= oldsz)
        return oldsz;
    
      if(PGROUNDUP(newsz) < PGROUNDUP(oldsz)){
        int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
        uvmunmap(pagetable, PGROUNDUP(newsz), npages, 0);
      }
    
      return newsz;
    }
    ```

- 接下来，为映射程序内存做准备。实验中提示内核启动后，能够用于映射程序内存的地址范围是 [0,PLIC)，我们将把进程程序内存映射到其内核页表的这个范围内，首先要确保这个范围没有和其他映射冲突。
- 查阅 xv6 book 可以看到，在 PLIC 之前还有一个 CLINT（核心本地中断器）的映射，该映射会与我们要 map 的程序内存冲突。查阅 xv6 book 的 Chapter 5 以及 start.c 可以知道 CLINT 仅在内核启动的时候需要使用到，而用户进程在内核态中的操作并不需要使用到该映射。

- 所以修改 `kvminit_new`，去除 CLINT 的映射，这样进程内核页表就不会有 CLINT 与程序内存映射冲突的问题。但是由于全局内核页表也使用了 `kvminit_new` 进行初始化，并且内核启动的时候需要 CLINT 映射存在，故在 `kvminit`中，另外单独给全局内核页表映射 CLINT。

  - ```c
    // kernel/vm.c
    // zyk: a new kvminit for single process
    pagetable_t
    kvminit_new()
    {
      pagetable_t pgtbl = (pagetable_t) kalloc();
      memset(pgtbl, 0, PGSIZE);
    
      // uart registers
      kvmmap(pgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);
    
      // virtio mmio disk interface
      kvmmap(pgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
    
      // do not use CLINT
      // CLINT
      // kvmmap(pgtbl, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
    
      // PLIC
      kvmmap(pgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
    
      // map kernel text executable and read-only.
      kvmmap(pgtbl, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
    
      // map kernel data and the physical RAM we'll make use of.
      kvmmap(pgtbl, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
    
      // map the trampoline for trap entry/exit to
      // the highest virtual address in the kernel.
      kvmmap(pgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
    
      return pgtbl;
    }
    ```

  - ```c
    // kernel/vm.c
    void
    kvminit()
    {
      kernel_pagetable = kvminit_new();
      kvmmap(kernel_pagetable, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
    }

- 在 `exec`中加入检查，防止内存超出PLIC

  - ```c
    //kernel/exec.c
    int
    exec(char *path, char **argv)
    {
      // ...
        if((sz1 = uvmalloc(pagetable, sz, ph.vaddr + ph.memsz)) == 0)
          goto bad;
        if(sz1 >= PLIC) { // 添加检测，防止程序大小超过 PLIC
          goto bad;
        }
      // ...

- 同步映射，在每个修改进程用户页表的位置，要把修改同步到内核页表中

  - `fork`

  - ```c
    // kernel/proc.c
    int
    fork(void)
    {
      // ......
    
      // Copy user memory from parent to child. 
      if(uvmcopy(p->pagetable, np->pagetable, p->sz) < 0 ||
        kvmcopymappings(np->pagetable, np->kernelpgtbl, 0, p->sz) < 0){
        freeproc(np);
        release(&np->lock);
        return -1;
      }
      np->sz = p->sz;
    ```

  - `exec`

  - ```c
    //kernel/exec.c
    int
    exec(char *path, char **argv)
    {
      // ......
    
      // Save program name for debugging.
      for(last=s=path; *s; s++)
        if(*s == '/')
          last = s+1;
      safestrcpy(p->name, last, sizeof(p->name));
    
      uvmunmap(p->kernelpgtbl, 0, PGROUNDUP(oldsz)/PGSIZE, 0);
      kvmcopymappings(pagetable, p->kernelpgtbl, 0, sz);
    ```

  - `growproc`

  - ```c
    //kernel/proc.c
    int
    growproc(int n)
    {
      uint sz;
      struct proc *p = myproc();
    
      sz = p->sz;
      if(n > 0){
        uint64 newsz;
        if((newsz = uvmalloc(p->pagetable, sz, sz + n)) == 0) {
          return -1;
        }
        // 内核页表中的映射同步扩大
        if(kvmcopymappings(p->pagetable, p->kernelpgtbl, sz, n) != 0) {
          uvmdealloc(p->pagetable, newsz, sz);
          return -1;
        }
        sz = newsz;
      } else if(n < 0){
        uvmdealloc(p->pagetable, sz, sz + n);
        // 内核页表中的映射同步缩小
        sz = kvmdealloc(p->kernelpgtbl, sz, sz + n);
      }
      p->sz = sz;
      return 0;
    }
    ```

  - `userinit`

  - ```c
    // kernel/proc.c
    void
    userinit(void)
    {
      // ......
    
      // allocate one user page and copy init's instructions
      // and data into it.
      uvminit(p->pagetable, initcode, sizeof(initcode));
      p->sz = PGSIZE;
      kvmcopymappings(p->pagetable, p->kernelpgtbl, 0, p->sz); // 同步程序内存映射到进程内核页表中
    
      // ......
    }
    ```

- 替换 `copyin&copyinstr`

  - 新的函数实验已经实现好了，只需要在原来的函数中声明并调用即可

  - ```c
    // kernel/vm.c
    
    // 声明新函数原型
    int copyin_new(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len);
    int copyinstr_new(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);
    
    // 将 copyin、copyinstr 改为转发到新函数
    int
    copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
    {
      return copyin_new(pagetable, dst, srcva, len);
    }
    
    int
    copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
    {
      return copyinstr_new(pagetable, dst, srcva, max);
    }
    ```

## 结果

![image-20240510152952261](assets/image-20240510152952261.png)