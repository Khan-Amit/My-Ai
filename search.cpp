#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cctype>
#include <memory>

using namespace std;

// ------------------------------------------------------------
// 1. Document structure
// ------------------------------------------------------------
struct Document {
    int id;
    string title;
    string content;
    string category;
    // Term frequency map (built during indexing)
    unordered_map<string, int> tf;
    double norm; // for cosine similarity (optional)
};

// ------------------------------------------------------------
// 2. Ternary Search Tree (TST) node
// ------------------------------------------------------------
struct TSTNode {
    char ch;
    bool isEnd;                    // marks end of a word
    shared_ptr<TSTNode> left, mid, right;
    // Store document IDs where this word appears
    vector<int> docIds;

    TSTNode(char c) : ch(c), isEnd(false), left(nullptr), mid(nullptr), right(nullptr) {}
};

class TernarySearchTree {
public:
    shared_ptr<TSTNode> root;

    TernarySearchTree() : root(nullptr) {}

    // Insert a word with document ID
    void insert(const string& word, int docId) {
        root = insert(root, word, 0, docId);
    }

    // Search for a word and return document IDs
    vector<int> search(const string& word) {
        auto node = search(root, word, 0);
        if (node && node->isEnd) return node->docIds;
        return {};
    }

    // Prefix search – return all words with prefix (for autocomplete)
    vector<string> prefixSearch(const string& prefix) {
        vector<string> results;
        auto node = search(root, prefix, 0);
        if (node) collectWords(node, prefix, results);
        return results;
    }

private:
    // Recursive insert
    shared_ptr<TSTNode> insert(shared_ptr<TSTNode> node, const string& word, int pos, int docId) {
        char c = word[pos];
        if (!node) node = make_shared<TSTNode>(c);

        if (c < node->ch) node->left = insert(node->left, word, pos, docId);
        else if (c > node->ch) node->right = insert(node->right, word, pos, docId);
        else {
            if (pos + 1 == word.length()) {
                node->isEnd = true;
                // Avoid duplicate docId in this word's list
                if (find(node->docIds.begin(), node->docIds.end(), docId) == node->docIds.end())
                    node->docIds.push_back(docId);
            } else {
                node->mid = insert(node->mid, word, pos + 1, docId);
            }
        }
        return node;
    }

    // Recursive search
    shared_ptr<TSTNode> search(shared_ptr<TSTNode> node, const string& word, int pos) {
        if (!node) return nullptr;
        char c = word[pos];
        if (c < node->ch) return search(node->left, word, pos);
        else if (c > node->ch) return search(node->right, word, pos);
        else {
            if (pos + 1 == word.length()) return node;
            return search(node->mid, word, pos + 1);
        }
    }

    // Collect all words under a node (for prefix)
    void collectWords(shared_ptr<TSTNode> node, string prefix, vector<string>& results) {
        if (!node) return;
        collectWords(node->left, prefix, results);
        if (node->isEnd) results.push_back(prefix + node->ch);
        collectWords(node->mid, prefix + node->ch, results);
        collectWords(node->right, prefix, results);
    }
};

// ------------------------------------------------------------
// 3. Search Engine class
// ------------------------------------------------------------
class SearchEngine {
private:
    vector<Document> docs;
    TernarySearchTree tst;
    unordered_map<string, int> documentFrequency; // DF: number of docs containing term

    // Helper: tokenise and lowercase
    vector<string> tokenise(const string& text) {
        vector<string> tokens;
        stringstream ss(text);
        string word;
        while (ss >> word) {
            // remove punctuation and lowercase
            string clean;
            for (char c : word) {
                if (isalnum(c)) clean += tolower(c);
            }
            if (!clean.empty()) tokens.push_back(clean);
        }
        return tokens;
    }

public:
    // Index documents
    void index(const vector<Document>& inputDocs) {
        docs = inputDocs;
        // Build term frequency per document and update DF
        for (auto& doc : docs) {
            auto tokens = tokenise(doc.title + " " + doc.content);
            for (const string& w : tokens) {
                doc.tf[w]++;
                // Insert into TST with doc ID
                tst.insert(w, doc.id);
            }
        }
        // Compute document frequency
        for (const auto& doc : docs) {
            for (const auto& pair : doc.tf) {
                const string& term = pair.first;
                // Count how many docs contain this term (we can query TST's docIds)
                auto docIds = tst.search(term);
                if (!docIds.empty()) {
                    // Count unique docs (already unique because insert avoids duplicates)
                    documentFrequency[term] = docIds.size();
                }
            }
        }
    }

    // Search query, return top-k ranked documents
    vector<pair<int, double>> search(const string& query, int topK = 5) {
        auto tokens = tokenise(query);
        if (tokens.empty()) return {};

        // For each document, compute score = sum over terms of (tf * idf)
        unordered_map<int, double> scoreMap;
        for (const auto& term : tokens) {
            auto docIds = tst.search(term);
            if (docIds.empty()) continue;

            double idf = log((double)docs.size() / (documentFrequency[term] + 1.0) + 1.0);
            for (int docId : docIds) {
                // find document
                for (const auto& doc : docs) {
                    if (doc.id == docId) {
                        int tf = doc.tf.count(term) ? doc.tf.at(term) : 0;
                        scoreMap[docId] += tf * idf;
                        break;
                    }
                }
            }
        }

        // Convert to vector and sort
        vector<pair<int, double>> results(scoreMap.begin(), scoreMap.end());
        sort(results.begin(), results.end(),
             [](const auto& a, const auto& b) { return a.second > b.second; });

        if (results.size() > topK) results.resize(topK);
        return results;
    }

    // Print a document summary
    void printDoc(int docId) {
        for (const auto& d : docs) {
            if (d.id == docId) {
                cout << "[" << d.id << "] " << d.title << " (" << d.category << ")\n";
                cout << "   " << d.content.substr(0, 100) << "...\n";
                return;
            }
        }
    }
};

// ------------------------------------------------------------
// 4. Main – demonstration
// ------------------------------------------------------------
int main() {
    // Create a small corpus (you can extend to read from file)
    vector<Document> corpus = {
        {1, "Python Machine Learning", "Learn Python for data science and ML", "tech"},
        {2, "World News Today", "Breaking news from around the world", "news"},
        {3, "Philosophy of Search", "How search engines shape our thinking", "philosophy"},
        {4, "C++ Programming Guide", "Master C++ with practical examples", "tech"},
        {5, "Climate Change Report", "Latest IPCC findings on global warming", "science"}
    };

    SearchEngine engine;
    engine.index(corpus);

    cout << "=== My-Ai Search Engine (Ternary Core) ===\n";
    cout << "Enter query (or 'exit' to quit):\n";

    string query;
    while (true) {
        cout << "\n> ";
        getline(cin, query);
        if (query == "exit") break;

        auto results = engine.search(query, 3);
        if (results.empty()) {
            cout << "No results found.\n";
        } else {
            cout << "Top results:\n";
            for (const auto& [docId, score] : results) {
                engine.printDoc(docId);
                cout << "   Score: " << score << "\n\n";
            }
        }
    }
    return 0;
}
