/**
 * File: request.cc
 * ----------------
 * Presents the implementation of the HTTPRequest class and
 * its friend functions as exported by request.h.
 */

#include <sstream>
#include <cstring>
#include <iostream>
#include <sstream>
#include "request.h"
#include "string-utils.h"
using namespace std;

static const string kWhiteSpaceDelimiters = " \r\n\t";
static const string kProtocolPrefix = "http://";
static const unsigned short kDefaultPort = 80;
static const string kForwardedFor = "x-forwarded-for";
void HTTPRequest::ingestRequestLine(istream& instream) throw (HTTPBadRequestException) {
  getline(instream, requestLine);
  if (instream.fail()) {
    throw HTTPBadRequestException("First line of request could not be read.");
  }

  requestLine = trim(requestLine);
  istringstream iss(requestLine);
  iss >> method >> url >> protocol;
  server = url;
  size_t pos = server.find(kProtocolPrefix);
  if (pos == 0) server.erase(0, kProtocolPrefix.size());
  pos = server.find('/');
  if (pos == string::npos) {
    // url came in as something like http://www.google.com, without the trailing /
    // in that case, least server as is (it'd be www.google.com), and manually set
    // path to be "/"
    path = "/";
  } else {
    path = server.substr(pos);
    server.erase(pos);
  }
  port = kDefaultPort;
  pos = server.find(':');
  if (pos == string::npos) return;
  port = strtol(server.c_str() + pos + 1, NULL, 0);
  server.erase(pos);
}

int HTTPRequest::ingestHeader(istream& instream, const string& clientIPAddress) {
  requestHeader.ingestHeader(instream);
  requestHeader.addHeader("x-forwarded-proto", "http");

  if (requestHeader.containsName(kForwardedFor)) {
    // Check if IP is contained in header. If so it's a proxy chain
    stringstream headerString(requestHeader.getValueAsString(kForwardedFor));
    string workingString;
    while(getline(headerString, workingString, ',')) {
      // Because of the space before the commas :(
      workingString = workingString.substr(workingString.find_first_not_of(" "));
      if (workingString == clientIPAddress) {
        return kProxyChain;
      }
    }

    // No Proxy chain. Append all forwarded proxy IPs
    workingString = string(requestHeader.getValueAsString(kForwardedFor));
    workingString += ", "; // Oh this is the space
    workingString += clientIPAddress;
    requestHeader.addHeader(kForwardedFor, workingString);
  } else {
    requestHeader.addHeader(kForwardedFor, clientIPAddress);
  }
  return 0;
}

bool HTTPRequest::forwardContainsIP(const string& clientIPAddress) {
  if (!requestHeader.containsName(kForwardedFor)) {
    return false;
  }
  stringstream headerString(requestHeader.getValueAsString(kForwardedFor));
  string workingString;
  while(getline(headerString, workingString, ',')) {
    cout << workingString << " " << clientIPAddress << endl;
    if (workingString == clientIPAddress) {
      return true;
    }
  }
  return false;
}

bool HTTPRequest::containsName(const string& name) const {
  return requestHeader.containsName(name);
}

void HTTPRequest::ingestPayload(istream& instream) {
  if (getMethod() != "POST") return;
  payload.ingestPayload(requestHeader, instream);
}

ostream& operator<<(ostream& os, const HTTPRequest& rh) {
  const string& path = rh.path;

  if (rh.forwardToProxy) {
    os << rh.method << " " << rh.url << " " << rh.protocol << "\r\n";
    os << "Host: " << rh.server << "\r\n";
    os << rh.requestHeader;
    os << "\r\n";
    return os;
  }

  os << rh.method << " " << path << " " << rh.protocol << "\r\n";
  os << rh.requestHeader;
  os << "\r\n"; // blank line not printed by request header
  os << rh.payload;
  return os;
}
