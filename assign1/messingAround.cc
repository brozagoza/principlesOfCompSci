




void BFS(list<path>& pathQueue, set<string>& visitedFilms, set<string>& visitedActors, vector<film>& credits, path workingPath) {

    for(vector<film>::iterator filmItr = credits.begin(); filmItr != credits.end(); filmItr++) {
        pair<set<film>::iterator, bool> filmRet;

        filmRet = visitedFilms.insert(*filmItr);

        // Film has already been visited
        if (filmRet.second == false) {
            continue;
        }

        vector<string> actors;
        database->getCast(*filmItr, actors);

        for (vector<string>::iterator actorItr = actors.begin(); actorItr != actors.end(); actorItr++) {
            pair<set<string>::iterator, bool> actorRet;

            // Actor has already been visited
            if (actorRet.second == false) {
                continue;
            }

            if (currentPath == NULL) {

            }

            path newPath = workingPath;
            newPath.addConnection(*filmItr, *actorItr);

            if (targetActor == *actorItr) {
                cout << newPath << endl;
                return; // TODO: maybe return false/true
            }
            pathQueue.push_back(newPath);
        }
    }
}