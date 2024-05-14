// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

#define NUMHASH 13
char bcacheName[NUMHASH][8];

uint hash(uint dev, uint block) {
  return (dev + block) % NUMHASH;
}

struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  struct bucket hashtable[NUMHASH];
} bcache;

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  for (int i = 0; i < NUMHASH; i++) {
    snprintf(bcacheName[i], sizeof(bcacheName[i]) - 1, "bcache%d", i);
    initlock(&bcache.hashtable[i].lock, bcacheName[i]);
  }

  for (int i = 0; i < NBUF; i++) {
    initsleeplock(&bcache.buf[i].lock, "buffer");
    b = &bcache.hashtable[i % NUMHASH].head;
    bcache.buf[i].next = b->next;
    bcache.buf[i].timestamp = 0;
    b->next = &bcache.buf[i];
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;

  uint h = hash(dev, blockno);
  acquire(&bcache.hashtable[h].lock);

  for(b = &bcache.hashtable[h].head; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.hashtable[h].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.hashtable[h].lock);

  acquire(&bcache.lock);
  acquire(&bcache.hashtable[h].lock);
  for(b = &bcache.hashtable[h].head; b; b = b->next) {
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&bcache.hashtable[h].lock);
      release(&bcache.lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  release(&bcache.hashtable[h].lock);

  struct buf *prev_empty = 0;
  // -1 表示无效，即没有找到空闲的buf
  uint prev_empty_h = -1;
  for(int i = 0; i < NUMHASH; i++) {
    acquire(&bcache.hashtable[i].lock);
    int found = 0;
    for(b = &bcache.hashtable[i].head; b->next; b = b->next) {
      if(b->next->refcnt == 0 && (prev_empty == 0 || prev_empty->timestamp > b->next->timestamp)) {
        found = 1;
        prev_empty = b;
      }
    }
    if(found == 0) {
      release(&bcache.hashtable[i].lock);
    } else {
      if(prev_empty_h != -1)
        release(&bcache.hashtable[prev_empty_h].lock);
      prev_empty_h = i;
    }
  }

  if(prev_empty) {
    b = prev_empty->next;
    if(prev_empty_h != h) {
      prev_empty->next = b->next;
      release(&bcache.hashtable[prev_empty_h].lock);
      acquire(&bcache.hashtable[h].lock);
      b->next = bcache.hashtable[h].head.next;  
      bcache.hashtable[h].head.next = b;
    }
    b->dev = dev;
    b->blockno = blockno;
    b->valid = 0;
    b->refcnt = 1;
    release(&bcache.hashtable[h].lock);
    release(&bcache.lock);
    acquiresleep(&b->lock);
    return b;
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint h = hash(b->dev, b->blockno);
  acquire(&bcache.hashtable[h].lock);
  b->refcnt--;
  if (b->refcnt == 0) 
    b->timestamp = ticks;
  release(&bcache.hashtable[h].lock);
}

void
bpin(struct buf *b) {
  uint h = hash(b->dev, b->blockno);
  acquire(&bcache.hashtable[h].lock);
  b->refcnt++;
  release(&bcache.hashtable[h].lock);
}

void
bunpin(struct buf *b) {
  uint h = hash(b->dev, b->blockno);
  acquire(&bcache.hashtable[h].lock);
  b->refcnt--;
  release(&bcache.hashtable[h].lock);
}


