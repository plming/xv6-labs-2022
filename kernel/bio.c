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

#define NBUCKET 13

struct {
  struct spinlock lock;
  struct buf head;
} ht[NBUCKET];

struct {
  struct buf buf[NBUF];
} bcache;

static uint
get_hash(uint blockno) {
  return blockno % NBUCKET;
}

// Remove buf b from the current bucket.
// Then insert b into bucket head's most recently used position.
// Caller must hold both locks of bucket head and b.
static void
move_to(struct buf *head, struct buf *b) {
  b->next->prev = b->prev;
  b->prev->next = b->next;
  b->next = head->next;
  b->prev = head;
  head->next->prev = b;
  head->next = b;
}

void
binit(void)
{
  struct buf *b;
  int i;

  for(i = 0; i < NBUCKET; ++i) {
    initlock(&ht[i].lock, "bcache.bucket");
    ht[i].head.prev = &ht[i].head;
    ht[i].head.next = &ht[i].head;
  }

  // Create linked list of buffers
  struct buf *head = &ht[0].head;
  for(b = bcache.buf; b < bcache.buf + NBUF; b++){
    b->next = head->next;
    b->prev = head;
    initsleeplock(&b->lock, "buffer");
    head->next->prev = b;
    head->next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  int i;
  uint hash = get_hash(blockno);

  acquire(&ht[hash].lock);

  // Is the block already cached?
  for(b = ht[hash].head.next; b != &ht[hash].head; b = b->next){
    if(b->dev == dev && b->blockno == blockno){
      b->refcnt++;
      release(&ht[hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // Not cached. We must acquire bucket lock in order of ascending bucket number.
  // So, we find free buffer in 3 steps.
  // 1. Find free buffer in the same bucket.
  for(b = ht[hash].head.prev; b != &ht[hash].head; b = b->prev){
    if(b->refcnt == 0) {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      move_to(&ht[hash].head, b);
      release(&ht[hash].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }

  // 2. Find free buffer in the next buckets.
  for(i = hash + 1; i < NBUCKET; ++i) {
    acquire(&ht[i].lock);
    for(b = ht[i].head.prev; b != &ht[i].head; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        move_to(&ht[hash].head, b);
        release(&ht[i].lock);
        release(&ht[hash].lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&ht[i].lock);
  }
  
  // Before 3, we release the current bucket lock to avoid deadlock.
  release(&ht[hash].lock);

  // 3. Find free buffer in the previous buckets.
  for(i = 0; i < hash; ++i) {
    acquire(&ht[i].lock);
    for(b = ht[i].head.prev; b != &ht[i].head; b = b->prev){
      if(b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        acquire(&ht[hash].lock);
        move_to(&ht[hash].head, b);
        release(&ht[hash].lock);
        release(&ht[i].lock);
        acquiresleep(&b->lock);
        return b;
      }
    }
    release(&ht[i].lock);
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
  uint hash;

  if(!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  hash = get_hash(b->blockno);

  acquire(&ht[hash].lock);
  b->refcnt--;
  if (b->refcnt == 0) {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = &ht[hash].head;
    b->prev = ht[hash].head.prev;
    ht[hash].head.prev->next = b;
    ht[hash].head.prev = b;
  }
  release(&ht[hash].lock);
}

void
bpin(struct buf *b) {
  uint hash = get_hash(b->blockno);
  acquire(&ht[hash].lock);
  b->refcnt++;
  release(&ht[hash].lock);
}

void
bunpin(struct buf *b) {
  uint hash = get_hash(b->blockno);
  acquire(&ht[hash].lock);
  b->refcnt--;
  release(&ht[hash].lock);
}


