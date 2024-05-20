# Task1

您的工作是实现每个CPU的空闲列表，并在CPU的空闲列表为空时进行窃取。所有锁的命名必须以“`kmem`”开头。也就是说，您应该为每个锁调用`initlock`，并传递一个以“`kmem`”开头的名称。运行`kalloctest`以查看您的实现是否减少了锁争用。要检查它是否仍然可以分配所有内存，请运行`usertests sbrkmuch`。您的输出将与下面所示的类似，在`kmem`锁上的争用总数将大大减少，尽管具体的数字会有所不同。确保`usertests`中的所有测试都通过。评分应该表明考试通过。

## 提示

- 您可以使用***kernel/param.h***中的常量`NCPU`
- 让`freerange`将所有可用内存分配给运行`freerange`的CPU。
- 函数`cpuid`返回当前的核心编号，但只有在中断关闭时调用它并使用其结果才是安全的。您应该使用`push_off()`和`pop_off()`来关闭和打开中断。
- 看看***kernel/sprintf.c***中的`snprintf`函数，了解字符串如何进行格式化。尽管可以将所有锁命名为“`kmem`”。

## 实现

- 降低锁竞争的一个思路就是细化锁的粒度，在这个实验中，把原来在整个空闲页上加锁变成对每个cpu核心维护一个自己的空闲页表并用对应的锁

- 修改`kmem`的结构，使其能够对每个cpu维护一个空闲页表

  - ```c
    // kernel/kalloc.c
    struct {
      struct spinlock lock;
      struct run *freelist;
    } kmem[NCPU];
    
    char *kmem_lock_names[] = {
      "kmem_cpu_0",
      "kmem_cpu_1",
      "kmem_cpu_2",
      "kmem_cpu_3",
      "kmem_cpu_4",
      "kmem_cpu_5",
      "kmem_cpu_6",
      "kmem_cpu_7",
    };
    ```

- 修改`kinit`，初始化每个cpu的锁

  - ```c
    // kernel/kalloc.c
    void
    kinit()
    {
      for(int i=0;i<NCPU;i++) {
        initlock(&kmem[i].lock, kmem_lock_names[i]);
      }
      freerange(end, (void*)PHYSTOP);
    }
    ```

- 修改`kfree`，释放的时候是对当前cpu所拥有的页释放

  - ```c
    // kernel/kalloc.c
    void
    kfree(void *pa)
    {
      // ...
      r = (struct run*)pa;
    
      push_off();
    
      int cpu = cpuid();
    
      acquire(&kmem[cpu].lock);
      r->next = kmem[cpu].freelist;
      kmem[cpu].freelist = r;
      release(&kmem[cpu].lock);
    
      pop_off();
    }
    ```

- 修改`kalloc`，在当前cpu的空闲页不足的时候，循环遍历其他cpu的页，并‘窃取’

  - ```c
    // kernel/kalloc.c
    void *
    kalloc(void)
    {
      struct run *r;
    
      push_off();
      int cpu = cpuid();
      acquire(&kmem[cpu].lock);
      if (!kmem[cpu].freelist) {
        int amount = 64;
        for (int i = 0; i < NCPU; ++i) {
          if (i == cpu) {
            continue;
          }
          acquire(&kmem[i].lock);
          struct run *rr = kmem[i].freelist;
          while (rr && amount) {
            kmem[i].freelist = rr->next;
            rr->next = kmem[cpu].freelist;
            kmem[cpu].freelist = rr;
            rr = kmem[i].freelist;
            amount--;
          }
          release(&kmem[i].lock);
          if (amount == 0) {
            break;
          }
        }
      }
      r = kmem[cpu].freelist;
      if(r)
        kmem[cpu].freelist = r->next;
      release(&kmem[cpu].lock);
      pop_off();
      if(r)
        memset((char*)r, 5, PGSIZE); // fill with junk
      return (void*)r;
    }
    ```

## 结果

![image-20240516161416016](assets/image-20240516161416016.png)