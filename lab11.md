# Task1

您的工作是在***kernel/e1000.c***中完成`e1000_transmit()`和`e1000_recv()`，以便驱动程序可以发送和接收数据包。当`make grade`表示您的解决方案通过了所有测试时，您就完成了。



## 提示

首先，将打印语句添加到`e1000_transmit()`和`e1000_recv()`，然后运行`make server`和（在xv6中）`nettests`。您应该从打印语句中看到，`nettests`生成对`e1000_transmit`的调用。

**实现`e1000_transmit`的一些提示：**

- 首先，通过读取`E1000_TDT`控制寄存器，向E1000询问等待下一个数据包的TX环索引。
- 然后检查环是否溢出。如果`E1000_TXD_STAT_DD`未在`E1000_TDT`索引的描述符中设置，则E1000尚未完成先前相应的传输请求，因此返回错误。
- 否则，使用`mbuffree()`释放从该描述符传输的最后一个`mbuf`（如果有）。
- 然后填写描述符。`m->head`指向内存中数据包的内容，`m->len`是数据包的长度。设置必要的cmd标志（请参阅E1000手册的第3.3节），并保存指向`mbuf`的指针，以便稍后释放。
- 最后，通过将一加到`E1000_TDT再对TX_RING_SIZE`取模来更新环位置。
- 如果`e1000_transmit()`成功地将`mbuf`添加到环中，则返回0。如果失败（例如，没有可用的描述符来传输`mbuf`），则返回-1，以便调用方知道应该释放`mbuf`。

**实现`e1000_recv`的一些提示：**

- 首先通过提取`E1000_RDT`控制寄存器并加一对`RX_RING_SIZE`取模，向E1000询问下一个等待接收数据包（如果有）所在的环索引。
- 然后通过检查描述符`status`部分中的`E1000_RXD_STAT_DD`位来检查新数据包是否可用。如果不可用，请停止。
- 否则，将`mbuf`的`m->len`更新为描述符中报告的长度。使用`net_rx()`将`mbuf`传送到网络栈。
- 然后使用`mbufalloc()`分配一个新的`mbuf`，以替换刚刚给`net_rx()`的`mbuf`。将其数据指针（`m->head`）编程到描述符中。将描述符的状态位清除为零。
- 最后，将`E1000_RDT`寄存器更新为最后处理的环描述符的索引。
- `e1000_init()`使用mbufs初始化RX环，您需要通过浏览代码来了解它是如何做到这一点的。
- 在某刻，曾经到达的数据包总数将超过环大小（16）；确保你的代码可以处理这个问题。

您将需要锁来应对xv6可能从多个进程使用E1000，或者在中断到达时在内核线程中使用E1000的可能性。

## 实现

```c
int
e1000_transmit(struct mbuf *m)
{
  acquire(&e1000_lock); // 获取 E1000 的锁，防止多进程同时发送数据出现 race

  uint32 ind = regs[E1000_TDT]; // 下一个可用的 buffer 的下标
  struct tx_desc *desc = &tx_ring[ind]; // 获取 buffer 的描述符，其中存储了关于该 buffer 的各种信息
  // 如果该 buffer 中的数据还未传输完，则代表我们已经将环形 buffer 列表全部用完，缓冲区不足，返回错误
  if(!(desc->status & E1000_TXD_STAT_DD)) {
    release(&e1000_lock);
    return -1;
  }
  
  // 如果该下标仍有之前发送完毕但未释放的 mbuf，则释放
  if(tx_mbufs[ind]) {
    mbuffree(tx_mbufs[ind]);
    tx_mbufs[ind] = 0;
  }

  // 将要发送的 mbuf 的内存地址与长度填写到发送描述符中
  desc->addr = (uint64)m->head;
  desc->length = m->len;
  // 设置参数，EOP 表示该 buffer 含有一个完整的 packet
  // RS 告诉网卡在发送完成后，设置 status 中的 E1000_TXD_STAT_DD 位，表示发送完成。
  desc->cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;
  // 保留新 mbuf 的指针，方便后续再次用到同一下标时释放。
  tx_mbufs[ind] = m;

  // 环形缓冲区内下标增加一。
  regs[E1000_TDT] = (regs[E1000_TDT] + 1) % TX_RING_SIZE;
  
  release(&e1000_lock);
  return 0;
}

static void
e1000_recv(void)
{
  while(1) { // 每次 recv 可能接收多个包

    uint32 ind = (regs[E1000_RDT] + 1) % RX_RING_SIZE;
    
    struct rx_desc *desc = &rx_ring[ind];
    // 如果需要接收的包都已经接收完毕，则退出
    if(!(desc->status & E1000_RXD_STAT_DD)) {
      return;
    }

    rx_mbufs[ind]->len = desc->length;
    
    net_rx(rx_mbufs[ind]); // 传递给上层网络栈。上层负责释放 mbuf

    // 分配并设置新的 mbuf，供给下一次轮到该下标时使用
    rx_mbufs[ind] = mbufalloc(0); 
    desc->addr = (uint64)rx_mbufs[ind]->head;
    desc->status = 0;

    regs[E1000_RDT] = ind;
  }

}
```

## 结果

![image-20240527124601291](assets/image-20240527124601291.png)