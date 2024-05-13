# Task1

- 你的首项任务是删除`sbrk(n)`系统调用中的页面分配代码（位于***sysproc.c***中的函数`sys_sbrk()`）。`sbrk(n)`系统调用将进程的内存大小增加n个字节，然后返回新分配区域的开始部分（即旧的大小）。新的`sbrk(n)`应该只将进程的大小（`myproc()->sz`）增加n，然后返回旧的大小。它不应该分配内存——因此您应该删除对`growproc()`的调用（但是您仍然需要增加进程的大小！）。

## 实现

- 就是把原先`growproc`函数中分配页面的部分去掉

- 但是，如果`n<0`表示缩小内存，此时要立即释放

- ```c
  // kernel/sysproc.c
  uint64
  sys_sbrk(void)
  {
    int addr;
    int n;
  
    if(argint(0, &n) < 0)
      return -1;
    addr = myproc()->sz;
    // if(growproc(n) < 0)
    //   return -1;
    if (n < 0) {
      uvmdealloc(myproc()->pagetable, myproc()->sz, myproc()->sz + n);
    }
    myproc()->sz += n;
    return addr;
  }
  ```



# Task2&3

- 修改***trap.c***中的代码以响应来自用户空间的页面错误，方法是新分配一个物理页面并映射到发生错误的地址，然后返回到用户空间，让进程继续执行。您应该在生成“`usertrap(): …`”消息的`printf`调用之前添加代码。你可以修改任何其他xv6内核代码，以使`echo hi`正常工作。
- 我们为您提供了`lazytests`，这是一个xv6用户程序，它测试一些可能会给您的惰性内存分配器带来压力的特定情况。修改内核代码，使所有`lazytests`和`usertests`都通过。

## 提示

- 你可以在`usertrap()`中查看`r_scause()`的返回值是否为13或15来判断该错误是否为页面错误
- `stval`寄存器中保存了造成页面错误的虚拟地址，你可以通过`r_stval()`读取
- 参考***vm.c***中的`uvmalloc()`中的代码，那是一个`sbrk()`通过`growproc()`调用的函数。你将需要对`kalloc()`和`mappages()`进行调用
- 使用`PGROUNDDOWN(va)`将出错的虚拟地址向下舍入到页面边界
- 当前`uvmunmap()`会导致系统`panic`崩溃；请修改程序保证正常运行
- 如果内核崩溃，请在***kernel/kernel.asm***中查看`sepc`
- 使用pgtbl lab的`vmprint`函数打印页表的内容
- 如果您看到错误“incomplete type proc”，请include“spinlock.h”然后是“proc.h”。
- 处理`sbrk()`参数为负的情况。
- 如果某个进程在高于`sbrk()`分配的任何虚拟内存地址上出现页错误，则终止该进程。
- 在`fork()`中正确处理父到子内存拷贝。
- 处理这种情形：进程从`sbrk()`向系统调用（如`read`或`write`）传递有效地址，但尚未分配该地址的内存。
- 正确处理内存不足：如果在页面错误处理程序中执行`kalloc()`失败，则终止当前进程。
- 处理用户栈下面的无效页面上发生的错误。

## 实现

- 修改`usertrap()`，如果为缺页异常（`(r_scause() == 13 || r_scause() == 15)`），且发生异常的地址是由于懒分配而没有映射的话，就为其分配物理内存，并在页表建立映射

  - ```c
    void
    usertrap(void)
    {
      int which_dev = 0;
      // ...
      
      if(r_scause() == 8){
        // system call
    
        if(p->killed)
          exit(-1);
    
        // sepc points to the ecall instruction,
        // but we want to return to the next instruction.
        p->trapframe->epc += 4;
    
        // an interrupt will change sstatus &c registers,
        // so don't enable until done with those registers.
        intr_on();
    
        syscall();
      } else if((which_dev = devintr()) != 0){
        // ok
      } // zyk: lazy allocation here
        else if ((r_scause() == 13 || r_scause() == 15) && uvmshouldtouch(r_stval())) {
        uvmlazytouch(r_stval());
      } 
      else {
        printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
        printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
        p->killed = 1;
      }
    ```

- 实现上面的修改需要一个判断是否为懒分配造成的缺页异常的函数`uvmshouldtouch`和一个为懒分配的页表建立映射的函数`uvmlazytouch`

  - `uvmshouldtouch`基于几个方面检测：
    1. 处于 `[0, p->sz)`地址范围之中（进程申请的内存范围）
    2. 不是栈的 guard page（具体见 xv6 book，栈页的低一页故意留成不映射，作为哨兵用于捕捉 stack overflow 错误。懒分配不应该给这个地址分配物理页和建立映射，而应该直接抛出异常）
    3. 页表项不存在

  - 

  - ```c
    // kernel/vm.c
    
    // touch a lazy-allocated page so it's mapped to an actual physical page.
    void uvmlazytouch(uint64 va) {
      struct proc *p = myproc();
      char *mem = kalloc();
      if(mem == 0) {
        // failed to allocate physical memory
        printf("lazy alloc: out of memory\n");
        p->killed = 1;
      } else {
        memset(mem, 0, PGSIZE);
        if(mappages(p->pagetable, PGROUNDDOWN(va), PGSIZE, (uint64)mem, PTE_W|PTE_X|PTE_R|PTE_U) != 0){
          printf("lazy alloc: failed to map page\n");
          kfree(mem);
          p->killed = 1;
        }
      }
      // printf("lazy alloc: %p, p->sz: %p\n", PGROUNDDOWN(va), p->sz);
    }
    
    // whether a page is previously lazy-allocated and needed to be touched before use.
    int uvmshouldtouch(uint64 va) {
      pte_t *pte;
      struct proc *p = myproc();
      
      return va < p->sz // within size of memory for the process
        && PGROUNDDOWN(va) != r_sp() // not accessing stack guard page (it shouldn't be mapped)
        && (((pte = walk(p->pagetable, va, 0))==0) || ((*pte & PTE_V)==0)); // page table entry does not exist
    }
    ```

- 因为懒分配的页在刚刚分配的时候是没有对应的映射的，多以一些函数会在遇到这些地址后`panic`，根据test的结果，逐个修改这些`panic`的函数，在遇到没有映射的页的时候直接跳过

  - ```c
    // kernel/vm.c
    // 修改这个解决了 proc_freepagetable 时的 panic
    void
    uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
    {
      uint64 a;
      pte_t *pte;
      // ...
        if((pte = walk(pagetable, a, 0)) == 0) {
          continue; 
        }
        if((*pte & PTE_V) == 0){
          continue; 
        }
    // ...
    
    ```

  - ```c
    // kernel/vm.c
    // 修改这个解决了 fork 时的 panic
    int
    uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
    {
      pte_t *pte;
      uint64 pa, i;
      uint flags;
      char *mem;
    
      for(i = 0; i < sz; i += PGSIZE){
        if((pte = walk(old, i, 0)) == 0)
          continue; 
        if((*pte & PTE_V) == 0)
          continue; 
      // ...

  - ```c
    // kernel/vm.c
    // 修改这个解决了 read/write 时的错误 (usertests 中的 sbrkarg 失败的问题)
    int
    copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
    {
      uint64 n, va0, pa0;
    
      if(uvmshouldtouch(dstva))
        uvmlazytouch(dstva);
    
      // ...
    
    }
    
    int
    copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
    {
      uint64 n, va0, pa0;
    
      if(uvmshouldtouch(srcva))
        uvmlazytouch(srcva);
    
      // ...
    
    }
    ```

## 结果

![image-20240513142258437](assets/image-20240513142258437.png)