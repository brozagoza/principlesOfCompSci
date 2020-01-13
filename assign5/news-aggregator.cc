/**
 * File: news-aggregator.cc
 * --------------------------------
 * Presents the implementation of the NewsAggregator class.
 */

#include "news-aggregator.h"
#include <iostream>
#include <iomanip>
#include <getopt.h>
#include <libxml/parser.h>
#include <libxml/catalog.h>
#include <set>
#include <thread>
#include <mutex>
// you will almost certainly need to add more system header includes

// I'm not giving away too much detail here by leaking the #includes below,
// which contribute to the official CS110 staff solution.
#include "semaphore.h"
#include "rss-feed.h"
#include "rss-feed-list.h"
#include "html-document.h"
#include "html-document-exception.h"
#include "rss-feed-exception.h"
#include "rss-feed-list-exception.h"
#include "utils.h"
#include "ostreamlock.h"
#include "string-utils.h"
using namespace std;

static const size_t kMaxFeedThreadPermits = 8;
static const size_t kMaxArticleServerThreadPermits = 10;

/**
 * Factory Method: createNewsAggregator
 * ------------------------------------
 * Factory method that spends most of its energy parsing the argument vector
 * to decide what rss feed list to process and whether to print lots of
 * of logging information as it does so.
 */
static const string kDefaultRSSFeedListURL = "small-feed.xml";
NewsAggregator *NewsAggregator::createNewsAggregator(int argc, char *argv[]) {
  struct option options[] = {
    {"verbose", no_argument, NULL, 'v'},
    {"quiet", no_argument, NULL, 'q'},
    {"url", required_argument, NULL, 'u'},
    {NULL, 0, NULL, 0},
  };
  
  string rssFeedListURI = kDefaultRSSFeedListURL;
  bool verbose = false;
  while (true) {
    int ch = getopt_long(argc, argv, "vqu:", options, NULL);
    if (ch == -1) break;
    switch (ch) {
    case 'v':
      verbose = true;
      break;
    case 'q':
      verbose = false;
      break;
    case 'u':
      rssFeedListURI = optarg;
      break;
    default:
      NewsAggregatorLog::printUsage("Unrecognized flag.", argv[0]);
    }
  }
  
  argc -= optind;
  if (argc > 0) NewsAggregatorLog::printUsage("Too many arguments.", argv[0]);
  return new NewsAggregator(rssFeedListURI, verbose);
}

/**
 * Method: buildIndex
 * ------------------
 * Initalizex the XML parser, processes all feeds, and then
 * cleans up the parser.  The lion's share of the work is passed
 * on to processAllFeeds, which you will need to implement.
 */
void NewsAggregator::buildIndex() {
  if (built) return;
  built = true; // optimistically assume it'll all work out
  xmlInitParser();
  xmlInitializeCatalog();
  processAllFeeds();
  xmlCatalogCleanup();
  xmlCleanupParser();
}

/**
 * Method: queryIndex
 * ------------------
 * Interacts with the user via a custom command line, allowing
 * the user to surface all of the news articles that contains a particular
 * search term.
 */
void NewsAggregator::queryIndex() const {
  static const size_t kMaxMatchesToShow = 15;
  while (true) {
    cout << "Enter a search term [or just hit <enter> to quit]: ";
    string response;
    getline(cin, response);
    response = trim(response);
    if (response.empty()) break;
    const vector<pair<Article, int> >& matches = index.getMatchingArticles(response);
    if (matches.empty()) {
      cout << "Ah, we didn't find the term \"" << response << "\". Try again." << endl;
    } else {
      cout << "That term appears in " << matches.size() << " article"
           << (matches.size() == 1 ? "" : "s") << ".  ";
      if (matches.size() > kMaxMatchesToShow)
        cout << "Here are the top " << kMaxMatchesToShow << " of them:" << endl;
      else if (matches.size() > 1)
        cout << "Here they are:" << endl;
      else
        cout << "Here it is:" << endl;
      size_t count = 0;
      for (const pair<Article, int>& match: matches) {
        if (count == kMaxMatchesToShow) break;
        count++;
        string title = match.first.title;
        if (shouldTruncate(title)) title = truncate(title);
        string url = match.first.url;
        if (shouldTruncate(url)) url = truncate(url);
        string times = match.second == 1 ? "time" : "times";
        cout << "  " << setw(2) << setfill(' ') << count << ".) "
             << "\"" << title << "\" [appears " << match.second << " " << times << "]." << endl;
        cout << "       \"" << url << "\"" << endl;
      }
    }
  }
}

/**
 * Private Constructor: NewsAggregator
 * -----------------------------------
 * Self-explanatory.  You may need to add a few lines of code to
 * initialize any additional fields you add to the private section
 * of the class definition.
 */
NewsAggregator::NewsAggregator(const string& rssFeedListURI, bool verbose): 
  log(verbose), rssFeedListURI(rssFeedListURI), built(false), articlePermits(24) {}

/**
 * Private Method: lockServer
 * -------------------------------
 * Given a server (create and) wait on semaphore. Max 8 threads per server.
 */
void NewsAggregator::lockServer(string& server) {
  serverLock.lock();
  unique_ptr<semaphore>& up = serverSemaphores[server];
  if (up == nullptr) {
    up.reset(new semaphore(kMaxArticleServerThreadPermits));
  }
  serverLock.unlock();
  up->wait();
}

/**
 * Private Method: unlockServer
 * -------------------------------
 * Given a server, setup signal on_thread_exit on server semaphore.
 */
void NewsAggregator::unlockServer(string& server) {
  serverLock.lock();
  auto found = serverSemaphores.find(server);
  serverLock.unlock();
  if (found == serverSemaphores.end()) {
    throw "unlock server semaphore that never locked";
  }
  unique_ptr<semaphore>& up = found->second;
  up->signal(on_thread_exit);
}

/**
 * Private Method: articleDownload
 * -------------------------------
 * Downloads a feed as passed in as the feed parameter. Parses all the articles from the
 * feed and downloads/parses them in children threads.
 *
 * @param article - Article to be parsed and downloaded
 * @param urls - set containing visited URLs
 * @param serverToArticlesMap - maps Servers to Articles to detect intersection of duplicate articles
 * @param addLock - mutex lock when adding to RSSIndex
 * @param urlLock - mutex lock when inserting into URLs set
 */
void NewsAggregator::articleDownload(Article article, set<string>& urls, 
    map<string, map<string, pair<Article, vector<string>>>>& serverToArticlesMap,
    mutex& addLock, mutex& urlLock) {

  articlePermits.signal(on_thread_exit); // semaphore to cap max threads running at one time (24)
  string newStr(article.url);
  unlockServer(newStr);
  
  urlLock.lock();
  pair<set<string>::iterator, bool> ret = urls.insert(article.url);
  urlLock.unlock();

  if (ret.second == false) {
    return; // URL is already present
  }

  log.noteSingleArticleDownloadBeginning(article);
  HTMLDocument htmldoc(article.url);

  try {
    htmldoc.parse();
  } catch (const HTMLDocumentException& e) {
    log.noteSingleArticleDownloadFailure(article);
    log.noteSingleArticleDownloadSkipped(article);
    return; // URL only gets one chance to download
  }

  addLock.lock();

  // Keep track of articles with same name on same server for intersection of words
  if (serverToArticlesMap.count(getURLServer(htmldoc.getURL())) == 0) {
    map<string, pair<Article, vector<string>>> atm;
    serverToArticlesMap.insert(pair<string, map<string, pair<Article, vector<string>>> >( getURLServer(htmldoc.getURL()), atm));
  } 
  
  auto existingMap = serverToArticlesMap.find( getURLServer(htmldoc.getURL()) );
  index.add(article, htmldoc.getTokens(), existingMap->second);

  addLock.unlock();
}

/**
 * Private Method: feedDownload
 * -------------------------------
 * Downloads a feed as passed in as the feed parameter. Parses all the articles from the
 * feed and downloads/parses them in children threads.
 *
 * @param feed - RSS feed that holds the articles to download
 * @param serverToArticlesMap - maps Servers to Articles to detect intersection of duplicate articles
 * @param urls - set containing visited URLs
 * @param addLock - mutex lock when adding to RSSIndex
 * @param urlLock - mutex lock when inserting into URLs set
 * @param permits - semaphore to cap max feed threads running at one time (8)
 */
void NewsAggregator::feedDownload(const pair<string, string> feed,
    map<string, map<string, pair<Article, vector<string>>>>& serverToArticlesMap,
    set<string>& urls, mutex& addLock, mutex& urlLock, semaphore& permits) {

  permits.signal(on_thread_exit);
  log.noteSingleFeedDownloadBeginning(feed.first);
  RSSFeed rssFeed(feed.first);

  try {
    rssFeed.parse();
  } catch (const RSSFeedException& e) {
    log.noteSingleFeedDownloadFailure(feed.first);
    log.noteSingleFeedDownloadSkipped(feed.first);
    return; // URL only gets one chance to download
  }

  log.noteAllArticlesHaveBeenScheduled(feed.second);
  const vector<Article>& articles = rssFeed.getArticles();

  vector<thread> threads; // grandchildren threads

  for (vector<Article>::const_iterator itr = articles.begin(); itr != articles.end(); itr++) {
    articlePermits.wait(); // 24 threads max downloading articles
    string newStr(itr->url);
    lockServer(newStr); // 10 threads max per server

    threads.push_back(
      thread(&NewsAggregator::articleDownload, this, *itr, ref(urls), ref(serverToArticlesMap),
        ref(addLock), ref(urlLock))
    );
  }

  for (thread& t: threads) {
    t.join();
  }

  log.noteSingleFeedDownloadEnd(feed.first);
}

/**
 * Private Method: processAllFeeds
 * -------------------------------
 * Downloads and parses the encapsulated RSSFeedList, which itself
 * leads to RSSFeeds, which themsleves lead to HTMLDocuemnts, which
 * can be collectively parsed for their tokens to build a huge RSSIndex.
 * 
 * The vast majority of your Assignment 5 work has you implement this
 * method using multithreading while respecting the imposed constraints
 * outlined in the spec.
 */
void NewsAggregator::processAllFeeds() {
  RSSFeedList rssFeedList(rssFeedListURI);

  try {
    rssFeedList.parse();
  } catch (const RSSFeedListException& e) {
    log.noteFullRSSFeedListDownloadFailureAndExit(rssFeedListURI);
  }

  // Getting the feeds URIs
  const map<string, string>& feedListMap = rssFeedList.getFeeds();
  
  // 8 threads 
  set<string> urls;  // Used so we don't try URLs more than once
  vector<thread> threads;
  semaphore feedPermits(kMaxFeedThreadPermits);
  mutex articleLock; // Used when adding to RSSIndex
  mutex urlLock;     // Used when adding to urls set

  // Used to keep track of duplicate articles on same server
  map<string, map<string, pair<Article, vector<string>>>> serverToArticlesMap;

  for (const pair<string, string>& feed: feedListMap) {
    feedPermits.wait();
    threads.push_back(
      thread(&NewsAggregator::feedDownload, this, feed, ref(serverToArticlesMap),
        ref(urls), ref(articleLock), ref(urlLock), ref(feedPermits))
    );
    log.noteSingleFeedDownloadEnd(feed.first);
  }

  for (thread& t: threads) {
    t.join();
  }

  log.noteFullRSSFeedListDownloadEnd();
}
