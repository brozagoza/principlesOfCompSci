/**
 * File: rss-index.cc
 * ------------------
 * Presents the implementation of the RSSIndex class, which is
 * little more than a glorified map.
 */

#include "rss-index.h"
#include "html-document.h" // added 
#include "html-document-exception.h" // added
#include <iostream> // logging. remove later
#include <algorithm>
#include <iterator>

using namespace std;

void RSSIndex::add(const Article& article, const vector<string>& words, map<string, pair<Article, vector<string>>>& articleTitleMap) {

  // fix the overlap
  auto existingArticle = articleTitleMap.find(article.title);

  // Article Title doesn't already exist
  if (existingArticle == articleTitleMap.end()) {
    vector<string> allwords(words);

    articleTitleMap.insert(pair<string, pair<Article, vector<string>>>(article.title, std::make_pair(article, allwords) ));

    for (const string& word : words) { // iteration via for keyword, yay C++11
      index[word][article]++;
    }
    return;
  }

  // Article Title does exist. Find the overlapping words.
  vector<string>& currentWords = existingArticle->second.second;
  vector<string> incomingWords(words);
  vector<string> intersectWords;

  sort(incomingWords.begin(), incomingWords.end());
  sort(currentWords.begin(), currentWords.end());

  set_intersection(incomingWords.cbegin(), incomingWords.cend(), currentWords.cbegin(), currentWords.cend(), back_inserter(intersectWords));

  // Remove the Old Words
  for (const string& word : currentWords) {
    index[word][existingArticle->second.first]--;

    if (index[word][existingArticle->second.first] == 0) {
      index[word].erase(existingArticle->second.first); // remove the article instance
    }
  }

  // Add the New Words with the lower lexographic url Article
  if (existingArticle->second.first.url < article.url) {

    for (const string& word : intersectWords) {
      index[word][existingArticle->second.first]++;
    }
    pair<Article, vector<string>> newPair(existingArticle->second.first, intersectWords);
    existingArticle->second = newPair;
  } else {

    for (const string& word : intersectWords) {
      index[word][article]++;
    }
    pair<Article, vector<string>> newPair(article, intersectWords);
    existingArticle->second = newPair;
  }
}

static const vector<pair<Article, int> > emptyResult;
vector<pair<Article, int> > RSSIndex::getMatchingArticles(const string& word) const {
  auto indexFound = index.find(word);
  if (indexFound == index.end()) return emptyResult;
  const map<Article, int>& matches = indexFound->second;
  vector<pair<Article, int> > v;
  for (const pair<Article, int>& match: matches) v.push_back(match);
  sort(v.begin(), v.end(), [](const pair<Article, int>& one, 
                              const pair<Article, int>& two) {
   return one.second > two.second || (one.second == two.second && one.first < two.first);
  });
  return v;
}
