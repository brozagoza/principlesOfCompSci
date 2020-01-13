/**
 * File: request-handler.cc
 * ------------------------
 * Provides the implementation for the HTTPRequestHandler class.
 */

#include "request-handler.h"
#include "response.h"
#include <cstring>
#include <iostream>
#include <socket++/sockstream.h> // for sockbuf, iosockstream
#include <sstream>
using namespace std;

static const string kDefaultProtocol = "HTTP/1.0";

HTTPRequestHandler::HTTPRequestHandler(): forwardToProxy(false) {
  handlers["GET"] = &HTTPRequestHandler::handleGETRequest;
  handlers["POST"] = &HTTPRequestHandler::handlePOSTRequest;
  handlers["HEAD"] = &HTTPRequestHandler::handleHEADRequest;

  // Blacklist magic
  blackList.addToBlacklist("blocked-domains.txt");
}

void HTTPRequestHandler::serviceRequest(const pair<int, string>& connection) throw() {
  sockbuf sb(connection.first);
  iosockstream ss(&sb);
  try {
    HTTPRequest request;
    request.setForwardToProxy(forwardToProxy); // Indicates to request whether we'll be forwarding to another proxy
    request.ingestRequestLine(ss);

    // ingestHeader returns -1 on proxyChaining dectection
    if (request.ingestHeader(ss, connection.second) == -1) {
      HTTPResponse response;
      response.setResponseCode(504);
      response.setProtocol(kDefaultProtocol);
      response.setPayload("Chained proxies may not lead through a cycle.");
      ss << response;
      ss.flush();
      return;
    }

    request.ingestPayload(ss);
    auto found = handlers.find(request.getMethod());
    if (found == handlers.cend()) throw UnsupportedMethodExeption(request.getMethod());
    (this->*(found->second))(request, ss);
  } catch (const HTTPBadRequestException &bre) {
    handleBadRequestError(ss, bre.what());
  } catch (const UnsupportedMethodExeption& ume) {
    handleUnsupportedMethodError(ss, ume.what());
  } catch (...) {}
}

/* Returns client socket integer. Takes care if forwarding to proxy or not. */
int HTTPRequestHandler::getClientSocket(const HTTPRequest& request, class iosockstream& ss) {
  int client = kClientSocketError;
  if (forwardToProxy) {
    client = createClientSocket(forwardServer, forwardPort);

    if (client == kClientSocketError) {
      string errStr = "This proxy is configured to forward to another proxy, and that second proxy (";
      errStr += forwardServer;
      errStr += ") is refusing connections.";
      handleBadRequestError(ss, errStr);
    }
  } else {
    client = createClientSocket(request.getServer(), request.getPort());

    if (client == kClientSocketError) {
      cerr << "Could not connect to host named \""
      << request.getServer() << "\"." << endl;
    }
  }
  return client;
}

/* Helper function that gets client, and ingests payload */
int HTTPRequestHandler::ingestAndFlush(const HTTPRequest& request, HTTPResponse& response, class iosockstream& ss) {
  int client = getClientSocket(request, ss);
  if (client == kClientSocketError) {
    return -1;
  }

  sockbuf sb(client);
  iosockstream sockStream(&sb);
  sockStream << request;
  sockStream.flush();

  response.ingestResponseHeader(sockStream);
  response.ingestPayload(sockStream);
  ss << response;
  ss.flush();

  return 0;
}

void HTTPRequestHandler::handleGETRequest(const HTTPRequest& request, class iosockstream& ss) {
  HTTPResponse response;

  // Blacklist
  if (!blackList.serverIsAllowed(request.getServer())) {
    response.setResponseCode(403);
    response.setProtocol(kDefaultProtocol);
    response.setPayload("Forbidden Content");
    ss << response;
    ss.flush();
    return;
  }

  // Cache
  if (cache.containsCacheEntry(request, response)) {
    ss << response;
    ss.flush();
    return;
  }

  if (ingestAndFlush(request, response, ss) == -1) {
    return;
  }

  // Adding to cache
  if (cache.shouldCache(request, response)) {
    cache.cacheEntry(request, response);
  }
}

void HTTPRequestHandler::handleHEADRequest(const HTTPRequest& request, class iosockstream& ss) {
    HTTPResponse response;

  // Blacklist
  if (!blackList.serverIsAllowed(request.getServer())) {
    response.setResponseCode(403);
    response.setProtocol(kDefaultProtocol);
    response.setPayload("Forbidden Content");
    ss << response;
    ss.flush();
    return;
  }

  int client = getClientSocket(request, ss);
  if (client == kClientSocketError) {
    return;
  }

  if (ingestAndFlush(request, response, ss) == -1) {
    return;
  }
}

void HTTPRequestHandler::handlePOSTRequest(const HTTPRequest& request, class iosockstream& ss) {
  HTTPResponse response;

  // Blacklist
  if (!blackList.serverIsAllowed(request.getServer())) {
    response.setResponseCode(403);
    response.setProtocol(kDefaultProtocol);
    response.setPayload("Forbidden Content");
    ss << response;
    ss.flush();
    return;
  }

  int client = getClientSocket(request, ss);
  if (client == kClientSocketError) {
    return;
  }

  if (ingestAndFlush(request, response, ss) == -1) {
    return;
  }
}

/**
 * Responds to the client with code 400 and the supplied message.
 */
void HTTPRequestHandler::handleBadRequestError(iosockstream& ss, const string& message) const {
  handleError(ss, kDefaultProtocol, 400, message);
}

/**
 * Responds to the client with code 405 and the provided message.
 */
void HTTPRequestHandler::handleUnsupportedMethodError(iosockstream& ss, const string& message) const {
  handleError(ss, kDefaultProtocol, 405, message);
}

/**
 * Generic error handler used when our proxy server
 * needs to invent a response because of some error.
 */
void HTTPRequestHandler::handleError(iosockstream& ss, const string& protocol,
				     int responseCode, const string& message) const {
  HTTPResponse response;
  response.setProtocol(protocol);
  response.setResponseCode(responseCode);
  response.setPayload(message);
  ss << response << flush;
}

void HTTPRequestHandler::setProxy(const std::string& server, unsigned short port) {
  forwardToProxy = true;
  forwardServer = server;
  forwardPort = port;
}

void HTTPRequestHandler::clearCache() {
  cache.clear();
}

void HTTPRequestHandler::setCacheMaxAge(long maxAge) {
  cache.setMaxAge(maxAge);
}
