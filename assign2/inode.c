#include <stdio.h>
#include <assert.h>

#include "inode.h"
#include "diskimg.h"
#include "chksumfile.h"

static const int kINodeCountPerSector = 16; // 16 iNodes per Sector
static const int kUInt16CountPerSector = 256; // Count of uint16_t values per Sector
static const int kMaxINodesBeforeDoublyIndirectBlock = 1792; // Max Count of INodes in non-doubly linked blocks

int inode_iget(struct unixfilesystem *fs, int inumber, struct inode *inp) {
  int sectorAddress = (inumber / kINodeCountPerSector) + INODE_START_SECTOR;
  int inodeOffset = inumber % kINodeCountPerSector; // Offset for inode in Sector

  if (inumber % kINodeCountPerSector == 0) {
    sectorAddress--;
  }

  if (inodeOffset == 0) {
    inodeOffset = kINodeCountPerSector;
  }

  char buffer[DISKIMG_SECTOR_SIZE];
  // If successful, function returns positive integer
  if (diskimg_readsector(fs->dfd, sectorAddress, buffer) == -1) {
    return -1;
  }

  struct inode *node = (struct inode*)buffer;
  node = (struct inode*)(node + (inodeOffset-1));
  *inp = *node;

  return 0;
}

int inode_indexlookup(struct unixfilesystem *fs, struct inode *inp, int blockNum) {

  // Small Mapping (all direct blocks)
  if ((inp->i_mode & ILARG) == 0) {
    return (blockNum < 8) ? inp->i_addr[blockNum] : -1;
  }

  // Large Mapping
  char buffer[DISKIMG_SECTOR_SIZE];

  // iNode resides in Singly-Indirect Block
  if (blockNum < kMaxINodesBeforeDoublyIndirectBlock)
  {
    uint16_t indirectBlockAddress = blockNum / kUInt16CountPerSector;
    uint16_t inodeOffset = blockNum % kUInt16CountPerSector;

    // Find index of iNode inside Singly-Indirect Block
    diskimg_readsector(fs->dfd, inp->i_addr[indirectBlockAddress], buffer);
    uint16_t* inodeIndex = (uint16_t*)buffer;
    inodeIndex += inodeOffset;

    return *inodeIndex;
  }

  // iNode resides in Double-Indirect Block
  uint16_t blocksRemaining = blockNum - kMaxINodesBeforeDoublyIndirectBlock;
  uint16_t indirectBlockAddress = blocksRemaining / kUInt16CountPerSector;
  uint16_t inodeOffset = blocksRemaining % kUInt16CountPerSector;

  // Find index of Singly-Indirect Block inside Double-Indirect Block
  diskimg_readsector(fs->dfd, inp->i_addr[7], buffer);
  uint16_t* indirectBlockNumber = (uint16_t*)buffer;
  indirectBlockNumber += indirectBlockAddress;

  // Find index of iNode inside Singly-Indirect Block
  diskimg_readsector(fs->dfd, *indirectBlockNumber, buffer);
  uint16_t* inodeIndex = (uint16_t*)buffer;
  inodeIndex += inodeOffset;

  return *inodeIndex;
}

int inode_getsize(struct inode *inp) {
  return ((inp->i_size0 << 16) | inp->i_size1); 
}
