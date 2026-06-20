#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <cctype>
#include <memory>
#include <random>
#include <chrono>
#include <iomanip>
#include "httplib.h"
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;

// ---------- Your existing SearchEngine code here (TST, Document, etc.) ----------
// (Paste your full TST + SearchEngine implementation from earlier)
// For brevity, I'll assume you have that code. If you need it, I'll re‑paste it.

// ---------- Simple session & log manager ----------
unordered_map<string, string> sessions; // token -> username
vector<json> login_logs; // for demonstration – you should write to file/db

string generate_token() {
    static const char alphanum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    string token;
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, sizeof(alphanum)-2);
    for (int i = 0; i < 64; ++i) token += alphanum[dis(gen)];
    return token;
}

void log_login(const string& username, const string& ip, bool success) {
    json entry;
    entry["timestamp"] = chrono::system_clock::now().time_since_epoch().count(); // approximate
    entry["username"] = username;
    entry["ip"] = ip;
    entry["success"] = success;
    login_logs.push_back(entry);
    cout << "LOGIN: " << username << " from " << ip << " -> " << (success ? "SUCCESS" : "FAILURE") << endl;
}

// ---------- main server ----------
int main() {
    // Hardcoded users for demo – replace with database lookup
    unordered_map<string, string> users = {
        {"seliim", "password123"},
        {"admin", "admin123"}
    };

    // Index your documents – same as before
    vector<Document> corpus = {
        // ... your documents (at least a few)
        {1, "Introducing MY-AI", "https://my-ai.dev A clean search engine.", "tech"},
        {2, "C++ TST", "https://github.com/My-Ai/tst Efficient data structure.", "dev"},
        {3, "Philosophy of Search", "https://my-ai.dev/blog Minimalist.", "philosophy"}
    };

    SearchEngine engine;
    engine.index(corpus);

    httplib::Server svr;

    // ----- Login endpoint -----
    svr.Post("/login", [&](const httplib::Request& req, httplib::Response& res) {
        try {
            json body = json::parse(req.body);
            string username = body["username"];
            string password = body["password"];
            string ip = req.remote_addr;

            auto it = users.find(username);
            bool valid = (it != users.end() && it->second == password);

            log_login(username, ip, valid);

            if (!valid) {
                res.status = 401;
                res.set_content(R"({"error":"Invalid credentials"})", "application/json");
                return;
            }

            // Generate token and store session
            string token = generate_token();
            sessions[token] = username;

            json response;
            response["token"] = token;
            response["expires_in"] = 86400; // 24h
            response["user"]["username"] = username;
            response["user"]["role"] = "free"; // you can set from DB
            res.set_content(response.dump(), "application/json");
        } catch (const exception& e) {
            res.status = 400;
            res.set_content(R"({"error":"Invalid JSON"})", "application/json");
        }
    });

    // ----- Protected search endpoint -----
    svr.Get("/search", [&](const httplib::Request& req, httplib::Response& res) {
        // Check Authorization header
        string auth = req.get_header_value("Authorization");
        if (auth.empty() || auth.find("Bearer ") != 0) {
            res.status = 401;
            res.set_content(R"({"error":"Missing or invalid token"})", "application/json");
            return;
        }
        string token = auth.substr(7); // remove "Bearer "
        if (sessions.find(token) == sessions.end()) {
            res.status = 401;
            res.set_content(R"({"error":"Invalid or expired token"})", "application/json");
            return;
        }

        // Token is valid – process the query
        string query = req.get_param_value("q");
        if (query.empty()) {
            res.status = 400;
            res.set_content(R"({"error":"No query"})", "application/json");
            return;
        }

        // Perform search (same as before)
        auto results = engine.search(query, 36);
        string overview = "Results for '" + query + "'";
        Document bestDoc = corpus[0];
        if (!results.empty()) {
            for (auto& d : corpus) if (d.id == results[0].first) { bestDoc = d; break; }
        }
        vector<Document> resultDocs;
        for (auto& p : results) {
            for (auto& d : corpus) if (d.id == p.first) { resultDocs.push_back(d); break; }
        }

        string json = build_json(overview, bestDoc, resultDocs); // your existing JSON builder
        res.set_content(json, "application/json");
    });

    // ----- (Optional) Logs endpoint – only for admin -----
    svr.Get("/logs", [&](const httplib::Request& req, httplib::Response& res) {
        // In production, check if the user is admin
        json logs = login_logs;
        res.set_content(logs.dump(), "application/json");
    });

    cout << "Server started on port 8080" << endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}
