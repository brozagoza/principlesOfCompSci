/**
 * File: mapreduce-reducer.cc
 * --------------------------
 * Presents the implementation of the MapReduceReducer class,
 * which is charged with the responsibility of collating all of the
 * intermediate files for a given hash number, sorting that collation,
 * grouping the sorted collation by key, and then pressing that result
 * through the reducer executable.
 *
 * See the documentation in mapreduce-reducer.h for more information.
 */

#include "mapreduce-reducer.h"
#include "mr-names.h"
using namespace std;

MapReduceReducer::MapReduceReducer(const string& serverHost, unsigned short serverPort,
                                   const string& cwd, const string& executable, const string& outputPath) : 
  MapReduceWorker(serverHost, serverPort, cwd, executable, outputPath) {}

void MapReduceReducer::reduce() const {
	while (true) {
		string name;
	    if (!requestInput(name)) break;
	    alertServerOfProgress("About to process \"" + name + "\".");
      string base = extractBase(name).substr(1); // assuming the '*' is the first char
      string outputFileName = base.substr(1); // assuming '.' is the first char
      string tempFile = outputPath + "/temp" + changeExtension(base, "mapped", "output");
      string output = outputPath + "/" + changeExtension(outputFileName, "mapped", "output");
	    string intermediateCommand = "cat " + name
        + " | sort | python /usr/class/cs110/lecture-examples/map-reduce/group-by-key.py"
        + " | /usr/class/cs110/lecture-examples/map-reduce/word-count-reducer.py > " + output;
	    if(system(intermediateCommand.c_str()) != 0) {
	    	alertServerOfProgress("Command failed");
	    }
      // bool success = processInput(tempFile, output);
      // remove(tempFile.c_str()); // Trash temp file
      // notifyServer(name, success);
	}
}
