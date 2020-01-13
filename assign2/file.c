#include <stdio.h>
#include <assert.h>

#include "file.h"
#include "inode.h"
#include "diskimg.h"

int file_getblock(struct unixfilesystem *fs, int inumber, int blockNum, void *buf) {
  struct inode node;
  inode_iget(fs, inumber, &node);

  int blockIndex = inode_indexlookup(fs, &node, blockNum);

  if (blockIndex < 0) {
    return -1;
  }

  diskimg_readsector(fs->dfd, blockIndex, buf);

  // If reading last block of iNode, not neccessarily 512 bytes
  if ((blockNum + ROOT_INUMBER) * DISKIMG_SECTOR_SIZE > inode_getsize(&node)) {
    return inode_getsize(&node) % DISKIMG_SECTOR_SIZE;
  }

  return DISKIMG_SECTOR_SIZE;
}
