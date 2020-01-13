#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "file.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

int directory_findname(struct unixfilesystem *fs, const char *name, int dirinumber, struct direntv6 *dirEnt) {
  // Dir must exist
  if (dirinumber < ROOT_INUMBER) {
    return -1;
  }

  struct inode node;
  inode_iget(fs, dirinumber, &node);

  // Make sure inode is Directory
  if ((node.i_mode & IFMT) != IFDIR) {
    fprintf(stderr,"iNode %d is not a directory\n", dirinumber);
    return -1;
  }

  int inodeSize = inode_getsize(&node);
  int currentBlock = 0;
  int bytesRead = 0; // Keep track of bytes read in

  uint16_t blockNumber = inode_indexlookup(fs, &node, currentBlock); // Get first block
  char buffer[DISKIMG_SECTOR_SIZE]; // change to 512 constant
  diskimg_readsector(fs->dfd, blockNumber, buffer);

  struct direntv6* tempDirEnt = (struct direntv6*)buffer;

  while (bytesRead < inodeSize) {

    // Need to advance to iNode's next block
    if (bytesRead != 0 && bytesRead % 512 == 0) {
      currentBlock++;
      blockNumber = inode_indexlookup(fs, &node, currentBlock);
      diskimg_readsector(fs->dfd, blockNumber, buffer);
      tempDirEnt = (struct direntv6*)buffer; // Point to first dirent in new block
    }

    if (strcmp(tempDirEnt->d_name, name) == 0) {
      *dirEnt = *tempDirEnt;
      return 0;
    }

    tempDirEnt++; // move to next dirent in block
    bytesRead += sizeof(struct direntv6);
  }

  return -1; // Exceeded iNode's block data
}
