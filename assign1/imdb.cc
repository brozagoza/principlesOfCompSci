#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <algorithm>
#include <string>
#include <stdio.h>
#include <string.h>
#include "imdb.h"
using namespace std;

const char *const imdb::kActorFileName = "actordata";
const char *const imdb::kMovieFileName = "moviedata";
imdb::imdb(const string& directory) {
  const string actorFileName = directory + "/" + kActorFileName;
  const string movieFileName = directory + "/" + kMovieFileName;  
  actorFile = acquireFileMap(actorFileName, actorInfo);
  movieFile = acquireFileMap(movieFileName, movieInfo);
}

bool imdb::good() const {
  return !( (actorInfo.fd == -1) || 
	    (movieInfo.fd == -1) ); 
}

imdb::~imdb() {
  releaseFileMap(actorInfo);
  releaseFileMap(movieInfo);
}

bool imdb::getCredits(const string& player, vector<film>& films) const {
  int* firstRecord = (int*)actorFile + 1;
  int* endOflastRecord = (int*)actorFile + *((int*)actorFile) + 1;

  int* result = lower_bound(firstRecord, endOflastRecord, player, [this] (const int& offset, const string& player){
    char* charName = (char*)((int*)actorFile + offset/4);
    return charName < player;
  });

  // Check if player was found in database
  if (player != (char*)((int*)actorFile + *(result)/4)) {
    return false;
  }

  // Used to keep track of the byteSize of this actor record. 
  // Incremented as we "move" through the image data.
  int byteSize = 0;

  char* name = (char*)((int*)actorFile + *(result)/4);
  byteSize += strlen(name) + 1;

  char* afterName = (char*)(name + strlen(name) + 1);
  // If name length is odd, there's an extra '\0'
  if (strlen(name) % 2 == 0) {
    byteSize++;
    afterName++;
  }

  short* movieCount = (short*)afterName;
  byteSize += sizeof(short);

  short* afterMovieCount = (short*)(movieCount + 1);

  if (byteSize % 4 != 0) {
    afterMovieCount++; // add 2 more bytes
  }

  int* movie = (int*)((short*)afterMovieCount); // Points to Movie offset Integer

  for (short i = 0; i < *(movieCount); i++) {
    char* movieName = (char*)((int*)movieFile + *(movie)/4);
    char* year = (char*)(movieName+strlen(movieName)+1);

    film f;
    f.title = movieName;
    f.year = *year + 1900;
    films.push_back(f);

    movie++; // point to next Movie offset Integer
  }
  return true; 
}

bool imdb::getCast(const film& movie, vector<string>& players) const {
  int* firstRecord = (int*)movieFile + 1;
  int* endOfLastRecord = (int*)movieFile + *((int*)movieFile) + 1;

  int* result = lower_bound(firstRecord, endOfLastRecord, movie, [this] (const int& offset, const film& movie){
    char* movieName = (char*)((int*)movieFile + offset/4);
    char* year = (char*)((char*)movieName+strlen(movieName)+1);

    film f;
    f.title = movieName;
    f.year = *year + 1900;

    return f < movie;
  });

  // Used to keep track of the byteSize of this actor record. 
  // Incremented as we "move" through the image data.
  int byteCount = 0;

  char* movieName = (char*)((int*)movieFile + *(result)/4);
  char* year = (char*)((char*)movieName+strlen(movieName)+1);

  ++year;
  byteCount += strlen(movieName) + 2; // + 2 because null and year is just one byte

  // Check if extra null
  if (byteCount % 2 != 0) {
    ++year;
    ++byteCount;
  }

  short* actorCount = (short*)((short*)year);
  year += 2; // advance pass the actorCount
  byteCount += 2;

  if (byteCount % 4 != 0) {
    year += 2;
    byteCount += 2;
  }
  int* actorOffset = (int*)year;

  for (short i = 0; i < *(actorCount); i++) {
    char* actorName = (char*)((int*)actorFile + *(actorOffset)/4);
    players.push_back(actorName);
    actorOffset++; // increment to next movie
  }

  return true; 
}

const void *imdb::acquireFileMap(const string& fileName, struct fileInfo& info) {
  struct stat stats;
  stat(fileName.c_str(), &stats);
  info.fileSize = stats.st_size;
  info.fd = open(fileName.c_str(), O_RDONLY);
  return info.fileMap = mmap(0, info.fileSize, PROT_READ, MAP_SHARED, info.fd, 0);
}

void imdb::releaseFileMap(struct fileInfo& info) {
  if (info.fileMap != NULL) munmap((char *) info.fileMap, info.fileSize);
  if (info.fd != -1) close(info.fd);
}
