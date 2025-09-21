#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <optional>

using json = nlohmann::json;

std::vector<std::vector<std::string>> prev_bids;
std::vector<std::vector<std::string>> prev_asks;

void print_depth(const std::vector<std::vector<std::string>>& bids,
                 const std::vector<std::vector<std::string>>& asks) {
    std::cout << "\033[H\033[J"; // clear screen

    std::cout << "==== Depth 5 (Bid | Ask) ====\n";

    // print Ask  (desc)
    auto asks_sorted = asks;
    std::sort(asks_sorted.begin(), asks_sorted.end(),
              [](auto &a, auto &b) { return std::stod(a[0]) > std::stod(b[0]); });

    for (size_t i = 0; i < std::min<size_t>(5, asks_sorted.size()); i++) {
        std::cout << "\t\t\t\t" << asks_sorted[i][0] << " | " << asks_sorted[i][1] << "\n";
    }

    // print Bid (desc)
    auto bids_sorted = bids;
    std::sort(bids_sorted.begin(), bids_sorted.end(),
              [](auto &a, auto &b) { return std::stod(a[0]) > std::stod(b[0]); });

    for (size_t i = 0; i < std::min<size_t>(5, bids_sorted.size()); i++) {
        std::cout << bids_sorted[i][0] << " | " << bids_sorted[i][1] << "\n";
    }

    std::cout << "==============================\n";
}

class ParseArg
{
    std::string trim(const std::string& s) {
        auto start = s.begin();
        while (start != s.end() and std::isspace(*start)) ++start;
        auto end = s.end();
        do { --end; } while(std::distance(start, end) >= 0 and std::isspace(*end));
        return std::string(start, end+1);
    }
    public:
    ParseArg() = default;
    ParseArg(int argc, char* argv[]) { parse(argc, argv); }
    void parse(int argc, char* argv[]) {
        if (argc <= 0) return;
        std::string opt{};
        std::string val{};
        for (int ii = 0 ; ii < argc ; ++ii) {
            std::string temp(trim(argv[ii]));
            if (ii == 0) {
                m_app = std::move(temp);
                continue;
            }

            if (temp[0] == '-') {
                if (temp.size() < 2)
                    return;

                if (temp[1] == '-') {
                    if (temp.size() == 2)
                        continue;
                    size_t pos = temp.find('=');
                    if (pos != std::string::npos)
                        m_options.emplace(temp.substr(0, pos), temp.substr(pos+1));
                    else
                        opt = temp;
                }
                else
                    m_options.emplace(temp, "");
            } else {
                val = temp;
                if (!opt.empty()) {
                    m_options.emplace(opt, val);
                    opt.clear();
                }
            }
        }
    }
    void print() {
        for (const auto& [k, v] : m_options) {
            std::cout << "[" << k << "," << v << "], ";
        }
        std::cout << std::endl;
    }
    std::unordered_map<std::string, std::string>    m_options;
    std::string m_app;
	bool isopt(const std::string& key) {
		auto it = m_options.find(key);	
		return it != m_options.end();
	}
	std::optional<std::string> getval(const std::string& key) {
		auto it = m_options.find(key);
		if (it == m_options.end()) return std::nullopt;
		return it->second;
	}
};


int main(int argc, char* argv[]) {
	ParseArg pa;
	pa.parse(argc, argv);
	bool bprint = pa.isopt("-p");
    ix::WebSocket ws;

    // Binance depth5 stream
    std::string url = "wss://stream.binance.com:9443/ws/adausdt@depth5@100ms";
    ws.setUrl(url);

    ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            std::cout << "Binance WebSocket connected: " << url << std::endl;
        } 
        else if (msg->type == ix::WebSocketMessageType::Message) {
            try {
                auto data = json::parse(msg->str);
                auto bids_update = data.value("bids", 
						std::vector<std::vector<std::string>>{});
                auto asks_update = data.value("asks", 
						std::vector<std::vector<std::string>>{});

                bool changed = false;

                // update Bid 
                for (auto& bid : bids_update) {
                    auto& price = bid[0];
                    auto& qty = bid[1];
                    auto it = std::find_if(prev_bids.begin(), prev_bids.end(),
                                           [&](auto& x) { return x[0] == price; });
                    if (it != prev_bids.end()) {
                        if (qty != "0") 
                            (*it)[1] = qty;
                        else
                            prev_bids.erase(it);

                        changed = true;
                    } else if (qty != "0") {
                        prev_bids.push_back({price, qty});
                        changed = true;
                    }
                }

                // update Ask
                for (auto& ask : asks_update) {
                    auto& price = ask[0];
                    auto& qty = ask[1];
                    auto it = std::find_if(prev_asks.begin(), prev_asks.end(),
                                           [&](auto& x) { return x[0] == price; });
                    if (it != prev_asks.end()) {
                        if (qty != "0") 
                            (*it)[1] = qty;
                        else
                            prev_asks.erase(it);
                        changed = true;
                    } else if (qty != "0") {
                        prev_asks.push_back({price, qty});
                        changed = true;
                    }
                }

                if (changed) {
					if (bprint)
                    	print_depth(prev_bids, prev_asks);
                }

            } catch (const std::exception& e) {
                std::cerr << "[X]JSON parse error: " << e.what() << std::endl;
            }
        } 
        else if (msg->type == ix::WebSocketMessageType::Error) 
            std::cerr << "[X]WebSocket Error: " << msg->errorInfo.reason << std::endl;
    });

    ws.start();

    // main thread (kill by Ctrl + C)
    while (true) 
        std::this_thread::sleep_for(std::chrono::seconds(1));

    return 0;
}
