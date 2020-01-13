#include <vector>
#include <string>
#include <iostream>
#include <stdio.h>
#include <set>
#include <list>
#include "path.h"
#include "imdb.h"
#include "imdb-utils.h"
using namespace std;

static const int kWrongArgumentCount = 1;
static const int kDatabaseNotFound = 2;

// Global variables for Database and Actors
imdb* database;
string startingActor;
string targetActor;
size_t maxSearchDepth = 7;

/**
 * Method: BFS
 * ---------------------
 * Performs Breadth First Search utilizing the 3 global variables and the supplied 'films' vector. Runs through each film's
 * acting cast to see if any actor is the targetActor. Checks 'visitedFilms' and 'visitedActors' to ensure no repeats.
 * 
 * @param pathQueue - BFS queue for traversal. Only purpose in this function is to append to it.
 * @param visitedFilms - set the contains previously visited films. Checked if visited when attempting insert.
 * @param visitedActors - set the contains previously visited actors. Checked if visited when attempting insert.
 * @param films - film collection to search through for targetActor
 * @param workingPath - path used to create a new starting path with an unvisited actor and add to the pathQueue
 *
 * @return true/false - whether the targetActor was found in the supplied films collection
 */ 
bool BFS(list<path>& pathQueue, set<film>& visitedFilms, set<string>& visitedActors, vector<film>& films, path workingPath) {
    // If exceeding search depth of 7, kill program
    if (workingPath.getLength() >= maxSearchDepth) {
        cout << "No path between those two people could be found." << endl;
        return true;
    }

    for(vector<film>::iterator filmItr = films.begin(); filmItr != films.end(); filmItr++) {
        pair<set<film>::iterator, bool> filmRet;

        filmRet = visitedFilms.insert(*filmItr);

        if (filmRet.second == false) {
            continue; // Film has already been visited
        }

        vector<string> actors;
        database->getCast(*filmItr, actors);

        for (vector<string>::iterator actorItr = actors.begin(); actorItr != actors.end(); actorItr++) {
            pair<set<string>::iterator, bool> actorRet;

            actorRet = visitedActors.insert(*actorItr);
            
            if (actorRet.second == false) {
                continue; // Actor has already been visited
            }

            path newPath = workingPath;
            newPath.addConnection(*filmItr, *actorItr);

            if (targetActor == *actorItr) {
                cout << newPath << endl;
                return true;
            }

            pathQueue.push_back(newPath);
        }
    }
    return false;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <actor1>" << " <actor2>" << endl;
        return kWrongArgumentCount;
    }

    imdb db(kIMDBDataDirectory);
    if (!db.good()) {
        cerr << "Data directory not found!  Aborting..." << endl; 
        return kDatabaseNotFound;
    }

    // Global variables popualted
    database = &db;
    startingActor = argv[1];
    targetActor = argv[2];

    vector<film> credits;
    vector<film> targetCredits; // Only used to see if targetActor is in database

    if (!database->getCredits(startingActor, credits) || !database->getCredits(targetActor, targetCredits)) {
        cout << "No path between those two people could be found." << endl;
        return 0;
    }

    // Variables used for BFS search
    list<path> pathQueue;
    set<string> visitedActors;
    set<film> visitedFilms;

    // Initalize for first pass through
    visitedActors.insert(startingActor);
    path initialPath(startingActor);
    pathQueue.push_back(initialPath);

    while (!pathQueue.empty()) {
        path workingPath = pathQueue.front();
        pathQueue.pop_front();

        vector<film> actorFilms;
        db.getCredits(workingPath.getLastPlayer(), actorFilms);

        if (BFS(pathQueue, visitedFilms, visitedActors, actorFilms, workingPath)) {
            break;
        }
    }
    return 0;
}
