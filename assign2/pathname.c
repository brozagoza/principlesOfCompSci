
#include "pathname.h"
#include "directory.h"
#include "inode.h"
#include "diskimg.h"
#include "direntv6.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int pathname_lookup(struct unixfilesystem *fs, const char *pathname) {
  char mutablePathname[strlen(pathname)+1];
  strcpy(mutablePathname, pathname);
  char delimiter[2] = "/"; // Slash used for delimeter in UNIX pathnames
  char* token;

  token = strtok(mutablePathname, delimiter);

  // If no strings result, assume ROOT
  if (token == NULL) {
    return ROOT_INUMBER;
  }

  int current_dirinumber = ROOT_INUMBER;

  while (token != NULL) {
    struct direntv6 current_dirent;

    // Function returns negative value on failure
    if (directory_findname(fs, token, current_dirinumber, &current_dirent) < 0) {
      break;
    }

    token = strtok(NULL, delimiter);

    // If NULL, iNumber found
    if (token == NULL) {
      return current_dirent.d_inumber;
    }

    // Continue traversing down the path
    current_dirinumber = current_dirent.d_inumber;
  }

  return -1;
}

