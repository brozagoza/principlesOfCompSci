/**
 * File: request-handler.h
 * -----------------------
 * Defines the HTTPRequestHandler class, which fully proxies and
 * services a single client request.  
 */

#ifndef _request_handler_
#define _request_handler_

#include <utility>
#include <string>
#include <map>
#include "blacklist.h"
#include "cache.h"
#include "request.h"
#include "client-socket.h"

class HTTPRequestHandler {
 public:
  HTTPRequestHandler();
  void serviceRequest(const std::pair<int, std::string>& connection) throw();
  void clearCache();
  void setCacheMaxAge(long maxAge);
  void setProxy(const std::string& server, unsigned short port);

 private:
  HTTPBlacklist blackList;
  HTTPCache cache;
  bool forwardToProxy;
  std::string forwardServer;
  unsigned short forwardPort;

  typedef void (HTTPRequestHandler::*handlerMethod)(const HTTPRequest& request, class iosockstream& ss);
  std::map<std::string, handlerMethod> handlers;

  int getClientSocket(const HTTPRequest& request, class iosockstream& ss);
  int ingestAndFlush(const HTTPRequest& request, HTTPResponse& response, class iosockstream& ss);

  void handleGETRequest(const HTTPRequest& request, class iosockstream& ss);
  void handleHEADRequest(const HTTPRequest& request, class iosockstream& ss);
  void handlePOSTRequest(const HTTPRequest& request, class iosockstream& ss);
  
  void handleBadRequestError(class iosockstream& ss, const std::string& message) const;
  void handleUnsupportedMethodError(class iosockstream& ss, const std::string& message) const;
  void handleError(class iosockstream& ss, const std::string& protocol,
                   int responseCode, const std::string& message) const;
};

#endif
