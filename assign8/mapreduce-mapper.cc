/**
 * File: mapreduce-mapper.cc
 * -------------------------
 * Presents the implementation of the MapReduceMapper class,
 * which is charged with the responsibility of pressing through
 * a supplied input file through the provided executable and then
 * splaying that output into a large number of intermediate files
 * such that all keys that hash to the same value appear in the same
 * intermediate.
 */

#include "mapreduce-mapper.h"
#include "mr-names.h"
#include "string-utils.h"
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <functional>            // for hash<string>
using namespace std;

MapReduceMapper::MapReduceMapper(const string& serverHost, unsigned short serverPort,
                                 const string& cwd, const string& executable,
                                 const string& outputPath, size_t hashCount) :
  MapReduceWorker(serverHost, serverPort, cwd, executable, outputPath), hashCount(hashCount) {}


void MapReduceMapper::map() const {
  
  while (true) {
    string name;
    if (!requestInput(name)) break;
    alertServerOfProgress("About to process \"" + name + "\".");
    string base = extractBase(name);
    string output = outputPath + "/" + changeExtension(base, "input", "mapped");
    bool success = processInput(name, output);
    mapHash(base, output);
    notifyServer(name, success);
  }

  alertServerOfProgress("Server says no more input chunks, so shutting down.");
}

void MapReduceMapper::mapHash(string& base, string& input) const {
  vector<vector<string>> buckets(hashCount);
  string bufferString;
  ifstream infile(input);
  while(getline(infile, bufferString)) {
    string justWord = bufferString.substr(0, bufferString.find(" "));
    size_t hashValue = hash<string>()(justWord);
    size_t index = hashValue % hashCount;
    string str = bufferString + "\n";
    buckets[index].push_back(str);
  }
  remove(input.c_str()); // Trash that old file

  for (size_t i = 0; i < hashCount; i++) {
    string outputo = outputPath + "/" + changeExtension(base, "input", numberToString(i)) + ".mapped";
    ofstream out(outputo);

    for (size_t j = 0; j < buckets[i].size(); j++) {
      out << buckets[i][j];
    }
    out.close();
  }
}
