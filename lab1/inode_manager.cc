#include "inode_manager.h"

// disk layer -----------------------------------------

disk::disk()
{
  bzero(blocks, sizeof(blocks));
}

void
disk::read_block(blockid_t id, char *buf)
{
	if (id<0 || id>=BLOCK_NUM || !buf) return;
	memcpy(buf, blocks[id], BLOCK_SIZE);
 /*
   *your lab1 code goes here.
   *if id is smaller than 0 or larger than BLOCK_NUM 
   *or buf is null, just return.
   *put the content of target block into buf.
   *hint: use memcpy
  */
}

void
disk::write_block(blockid_t id, const char *buf)
{
	if (id<0 || id>=BLOCK_NUM || !buf) return;
	memcpy(blocks[id], buf, BLOCK_SIZE); 
  /*
   *your lab1 code goes here.
   *hint: just like read_block
  */
}

// block layer -----------------------------------------

// Allocate a free disk block.
blockid_t
block_manager::alloc_block()
{
  /*
   * your lab1 code goes here.
   * note: you should mark the corresponding bit in block bitmap when alloc.
   * you need to think about which block you can start to be allocated.

   *hint: use macro IBLOCK and BBLOCK.
          use bit operation.
          remind yourself of the layout of disk.
   */
	char buf[BLOCK_SIZE];
	int id = IBLOCK(INODE_NUM) + 1;
	for (uint32_t ID=BBLOCK(id); ID<=BBLOCK(BLOCK_NUM); ID++) 
	{
		read_block(ID, buf);
		for (int i=0; i<BLOCK_SIZE; i++,id+=8)	//count block_id 
		{
			int w = ~buf[i];
			if (!(w&-w)) continue;
			buf[i] = buf[i] + (w & -w);
			write_block(ID, buf);
			while (w<-1) w>>=1, id++;
			return id;
		}
	}
  return 0;
}

void
block_manager::free_block(uint32_t id)
{
  /* 
   * your lab1 code goes here.
   * note: you should unmark the corresponding bit in the block bitmap when free.
   */
	char buf[BLOCK_SIZE];
	if (id<0 || id>BLOCK_NUM) return;
	read_block(BBLOCK(id), buf);
	int off = id % BPB;
	buf[off/8] &= ~(1<<(off%8)); 
	//use '&' to prevent it's zero before
	write_block(BBLOCK(id), buf);
}

// The layout of disk should be like this:
// |<-sb->|<-free block bitmap->|<-inode table->|<-data->|
block_manager::block_manager()
{
  d = new disk();

  // format the disk
  sb.size = BLOCK_SIZE * BLOCK_NUM;
  sb.nblocks = BLOCK_NUM;
  sb.ninodes = INODE_NUM;
}

void
block_manager::read_block(uint32_t id, char *buf)
{
  d->read_block(id, buf);
}

void
block_manager::write_block(uint32_t id, const char *buf)
{
  d->write_block(id, buf);
}

// inode layer -----------------------------------------

inode_manager::inode_manager()
{
  bm = new block_manager();
  uint32_t root_dir = alloc_inode(extent_protocol::T_DIR);
  if (root_dir != 1) {
    printf("\tim: error! alloc first inode %d, should be 1\n", root_dir);
    exit(0);
  }
}

/* Create a new file.
 * Return its inum. */
uint32_t
inode_manager::alloc_inode(uint32_t type)
{
	char buf[BLOCK_SIZE];
	inode_t *ino;
	for (uint32_t id=1; id<=INODE_NUM; id++)
	{
		bm->read_block( IBLOCK(id), buf);
		ino = (inode_t *) buf;
		if (ino->type) continue;
		ino->type = type;
		ino->size = 0;
		ino->atime = ino->mtime = ino->ctime = time(0);
		bm->write_block( IBLOCK(id), buf);
		return id;
	}
	return 0;
  /* 
   * your lab1 code goes here.
   * note: the normal inode block should begin from the 2nd inode block.
   * the 1st is used for root_dir, see inode_manager::inode_manager().
    
   * if you get some heap memory, do not forget to free it.
   */
}

void
inode_manager::free_inode(uint32_t inum)
{
  /* 
   * your lab1 code goes here.
   * note: you need to check if the inode is already a freed one;
   * if not, clear it, and remember to write back to disk.
   * do not forget to free memory if necessary.
   */
	char buf[BLOCK_SIZE];
	inode* ino;
	bm->read_block( IBLOCK(inum), buf);
	ino = (inode_t *) buf;
	if (!ino->type) return;
	ino->type = ino->size = 0;
	bm->write_block( IBLOCK(inum), buf);
}


/* Return an inode structure by inum, NULL otherwise.
 * Caller should release the memory. */
struct inode* 
inode_manager::get_inode(uint32_t inum)
{
  struct inode *ino, *ino_disk;
  char buf[BLOCK_SIZE];

  printf("\tim: get_inode %d\n", inum);

  if (inum < 0 || inum >= INODE_NUM) {
    printf("\tim: inum out of range\n");
    return NULL;
  }

  bm->read_block(IBLOCK(inum), buf);
  // printf("%s:%d\n", __FILE__, __LINE__);

  ino_disk = (struct inode*)buf + inum%IPB;
  if (ino_disk->type == 0) {
    printf("\tim: inode not exist\n");
    return NULL;
  }

  ino = (struct inode*)malloc(sizeof(struct inode));
  *ino = *ino_disk;

  return ino;
}

void
inode_manager::put_inode(uint32_t inum, struct inode *ino)
{
  char buf[BLOCK_SIZE];
  struct inode *ino_disk;

  printf("\tim: put_inode %d\n", inum);
  if (ino == NULL)
    return;

  bm->read_block(IBLOCK(inum), buf);
  ino_disk = (struct inode*)buf + inum%IPB;
  *ino_disk = *ino;
  bm->write_block(IBLOCK(inum), buf);
}

#define min(a,b) ((a)<(b) ? (a) : (b))

/* Get all the data of a file by inum. 
 * Return alloced data, should be freed by caller. */
void
inode_manager::read_file(uint32_t inum, char **buf_out, int *size)
{
  /*
   * your lab1 code goes here.
   * note: read blocks related to inode number inum,
   * and copy them to buf_out
   */
	char buf[BLOCK_SIZE], buf2[BLOCK_SIZE];
	inode_t *ino;

	if (inum<0 || inum>=INODE_NUM) return;
	bm->read_block(IBLOCK(inum), buf);
	ino = (inode_t *) buf;	//there's only one inode per block
	ino->atime = time(0);	//update the access time

	int sz = *size = ino->size;
	printf("[read_file]size:%d  inum:%d\n", *size, inum);
	if (sz <= 0) return;
	char *BUF = new char[sz];
	*buf_out = BUF;
	for (int i=0; i<NDIRECT && sz>0; i++)
	{
		bm->read_block(ino->blocks[i], buf2);
		memcpy(BUF+i*BLOCK_SIZE, buf2, min(sz, BLOCK_SIZE));
		sz -= BLOCK_SIZE;
	}
	if (sz <= 0) return;
	blockid_t *blocks = new blockid_t[NINDIRECT];
	bm->read_block(ino->blocks[NDIRECT], (char *) blocks);

	BUF += NDIRECT*BLOCK_SIZE;
	for (uint32_t i=0; i<NINDIRECT && sz>0; i++)
	{
		bm->read_block(blocks[i], buf2);
		memcpy(BUF+i*BLOCK_SIZE, buf2, min(sz, BLOCK_SIZE));
		sz -= BLOCK_SIZE;
	}
}

/* alloc/free blocks if needed */
void
inode_manager::write_file(uint32_t inum, const char *BUF, int size)
{
  /*
   * your lab1 code goes here.
   * note: write buf to blocks of inode inum.
   * you need to consider the situation when the size of buf 
   * is larger or smaller than the size of original inode.
   * you should free some blocks if necessary.
   */
	printf("[write_file]size:%d  inum:%d\n", size, inum);
	if (inum<0 || inum>=BLOCK_NUM) return;

	char buf[BLOCK_SIZE];
	inode_t *ino = new inode_t;
	bm->read_block(IBLOCK(inum), buf);
	ino = (inode_t *) buf;

	if ((int)ino->size != size) ino->ctime = time(0);
	ino->size = size;
	ino->atime = ino->mtime = time(0);
	
	for (int i=0; i<NDIRECT && size>0; i++) 
	{
		ino->blocks[i] = bm->alloc_block();
		bm->write_block(ino->blocks[i], BUF);
		size -= BLOCK_SIZE;
		BUF += BLOCK_SIZE;
	}
	if (size>0) ino->blocks[NDIRECT] = bm->alloc_block();
	bm->write_block(IBLOCK(inum), buf);

	if (size>0) {
		printf("[write_file]rest_size:%d\n", size);

		blockid_t *blocks = new blockid_t[NINDIRECT];
		for (uint32_t i=0; i<NINDIRECT && size>0; i++)
		{
			blocks[i] = bm->alloc_block();
			bm->write_block(blocks[i], BUF);
			BUF += BLOCK_SIZE;
			size -= BLOCK_SIZE;
		}
		bm->write_block(ino->blocks[NDIRECT], (char *)blocks);
	}
}

void
inode_manager::getattr(uint32_t inum, extent_protocol::attr &a)
{
	char buf[BLOCK_SIZE];
	inode_t *ino;
	if (inum<=0 || inum>=INODE_NUM) return;
	bm->read_block(IBLOCK(inum), buf);
	ino = (inode_t *) buf;
	a.type = ino->type;
	a.atime = ino->atime;
	a.mtime = ino->mtime;
	a.ctime = ino->ctime;
	a.size = ino->size;
  /*
   * your lab1 code goes here.
   * note: get the attributes of inode inum.
   * you can refer to "struct attr" in extent_protocol.h
   */
}

void
inode_manager::remove_file(uint32_t inum)
{
  /*
   * your lab1 code goes here
   * note: you need to consider about both the data block and inode of the file
   * do not forget to free memory if necessary.
   */
	free_inode(inum);
	char buf[BLOCK_SIZE];
	inode_t *ino;
	bm->read_block(IBLOCK(inum), buf);
	ino = (inode_t *) buf;
	
	int sz = ino->size - NDIRECT * BLOCK_SIZE;
	for (uint32_t i=0; i<=NDIRECT; i++) 
		bm->free_block(ino->blocks[i]);
	if (sz<=0) return;

	int id = ino->blocks[NDIRECT];
	blockid_t *blocks = new blockid_t[NINDIRECT];

	bm->read_block(id, (char *) blocks);
	for (uint32_t i=0; i<NINDIRECT && sz>0; i++) {
		bm->free_block(blocks[i]);
		sz -= BLOCK_SIZE;
	}
}
