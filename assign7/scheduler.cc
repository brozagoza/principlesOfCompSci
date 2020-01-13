/**
 * File: scheduler.cc
 * ------------------
 * Presents the implementation of the HTTPProxyScheduler class.
 */

#include "scheduler.h"
#include "thread-pool.h"
#include <utility>
using namespace std;

HTTPProxyScheduler::HTTPProxyScheduler(): pool(64) {} 

void HTTPProxyScheduler::scheduleRequest(int clientfd, const string& clientIPAddress) throw () {
    pool.schedule([this, clientfd, clientIPAddress]{
    	requestHandler.serviceRequest(make_pair(clientfd, clientIPAddress));	
    });	
}
