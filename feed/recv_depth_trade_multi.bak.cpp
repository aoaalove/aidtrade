#include <ixwebsocket/IXWebSocket.h>
#include <nlohmann/json.hpp>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>
#include <mutex>
#include <format>
#include <fstream>
#include "spsc.hpp"

using json = nlohmann::json;

// 심볼별 depth 저장소
struct DepthBook {
	std::vector<std::vector<std::string>> bids;
	std::vector<std::vector<std::string>> asks;
};

struct Price {
	std::string price;
	std::string qty;
};

struct Depth {
	Price	asks[5];
	Price	bids[5];
};

struct RawData
{
	std::string symbol;
	std::string type;	// depth, trade
	std::string timestamp;
	Price	trade;
	Depth	depth; 	
};

std::unordered_map<std::string, DepthBook> depth_books;
std::mutex depth_mtx;

// depth 출력
void print_depth(const std::string& symbol, const DepthBook& book) {
	auto asks_sorted = book.asks;
	std::sort(asks_sorted.begin(), asks_sorted.end(),
			[](auto& a, auto& b) { return std::stod(a[0]) > std::stod(b[0]); });

	// bid sort desc
	auto bids_sorted = book.bids;
	std::sort(bids_sorted.begin(), bids_sorted.end(),
			[](auto& a, auto& b) { return std::stod(a[0]) > std::stod(b[0]); });
	std::cout << "[DEPTH] " 
			<< symbol 
			<< " "
			<< std::setw(15) << std::setprecision(5) << std:: right << bids_sorted[0][1]  
			<< "|"
			<< std::setw(15) << std::setprecision(5) << std::right << bids_sorted[0][0]  
			<< " : " 
			<< std::setw(15) << std::setprecision(5) << std:: right << asks_sorted[0][0]  
			<< "|"
			<< std::setw(15) << std::setprecision(5) << std::right << bids_sorted[0][1]
			<< std::endl;
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

void print_depth_all(const std::string& symbol, const DepthBook& book, 
		size_t depth_count = 1) {
	//  std::lock_guard<std::mutex> lock(depth_mtx);

	// std::cout << "\033[H\033[J"; // 화면 clear
	if (depth_count == 1) {
		print_depth(symbol, book);
		return;
	}
		
	std::cout << "-----------------------------------------------------\n";
	std::cout << std::format("[DEPTH] {} (Top {} Bids(Q|P)/Asks(P|Q))\n", symbol, depth_count);
	std::cout << "-----------------------------------------------------\n";

		// ask sort  desc
	auto asks_sorted = book.asks;
	std::sort(asks_sorted.begin(), asks_sorted.end(),
			[](auto& a, auto& b) { return std::stod(a[0]) > std::stod(b[0]); });

	for (size_t i = 0; i < std::min<size_t>(depth_count, asks_sorted.size()); i++) {
		std::cout << "\t\t  " << std::setw(15) << std::right << asks_sorted[i][0] 
			<< " | " 
			<< std::setw(15) << std::right << asks_sorted[i][1] << "\n";
	}

	// bid sort desc
	auto bids_sorted = book.bids;
	std::sort(bids_sorted.begin(), bids_sorted.end(),
			[](auto& a, auto& b) { return std::stod(a[0]) > std::stod(b[0]); });

	for (size_t i = 0; i < std::min<size_t>(depth_count, bids_sorted.size()); i++) {
		std::cout << std::setw(15) << std::right << bids_sorted[i][1] 
			<< " | " 
			<< std::setw(15) << std::right << bids_sorted[i][0] << "\n";
	}

	std::cout << "-----------------------------------------------------\n";
}

struct Subscriptions {
	std::string					symbol;
	std::vector<std::string>	streams;
};

int main(int argc, char* argv[]) {
	ParseArg pa(argc, argv);
	std::string config_file = "./config.json";
	std::fstream	fs(config_file);
	if (!fs.is_open()) {
		std::cout << std::format("{} file not found", config_file);	
		return -1;
	}
	nlohmann::json js_config;
	fs >> js_config;
	std::cout << js_config.dump() << std::endl;
	bool print_depth = js_config["print_depth"].get<bool>();
	bool print_trade = js_config["print_trade"].get<bool>();
	std::string url = js_config["binance"]["url"].get<std::string>();
	size_t	depth_count = js_config["depth_count"].get<size_t>();	
	std::vector<Subscriptions>	subs;
	for (const auto& item : js_config["binance"]["subscriptions"])
	{
		Subscriptions s;
		s.symbol = std::move(item["symbol"].get<std::string>());	
		s.streams = std::move(item["streams"].get<std::vector<std::string>>());
		subs.push_back(std::move(s));
	}
	
	ix::WebSocket ws;
	ws.setUrl(url);

	ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
			if (msg->type == ix::WebSocketMessageType::Open) {
				std::cout << "✅ Connected to Binance multi-stream\n";

				// 여러 종목 depth + trade subscribe
				json sub_msg = {
					{"method", "SUBSCRIBE"},
					{"params", 
						{
						}
					},
					{"id", 1}
				};
				for (const auto& sub : subs)
				{
					for (const auto& stream : sub.streams)
						sub_msg["params"].push_back(sub.symbol + "@" + stream); 
				}

				ws.send(sub_msg.dump());
			} else if (msg->type == ix::WebSocketMessageType::Message) {
				try {
					auto js = json::parse(msg->str);

					if (!js.contains("stream") || !js.contains("data"))
						return;

					std::string stream = js["stream"];
					size_t	pos = stream.find('@');
					std::string symbol = stream.substr(0, pos);
					stream = stream.substr(pos+1);
					auto data = js["data"];
					//std::cout << data.dump(4) << std::endl;


					// ---------------- Depth 업데이트 ----------------
					if (stream == "depth5@100ms")  {
						// event == "depthUpdate" or (data.contains("bids") && data.contains("asks"))) 
						std::lock_guard<std::mutex> lock(depth_mtx);

						auto& book = depth_books[symbol];

						auto bids_update = data.value("bids", std::vector<std::vector<std::string>>{});
						auto asks_update = data.value("asks", std::vector<std::vector<std::string>>{});

						// Bid 업데이트
						for (auto& bid : bids_update) {
							auto& price = bid[0];
							auto& qty = bid[1];
							auto it = std::find_if(book.bids.begin(), book.bids.end(),
									[&](auto& x) { return x[0] == price; });
							if (it != book.bids.end()) {
								if (qty != "0") (*it)[1] = qty;
								else book.bids.erase(it);
							} else if (qty != "0") {
								book.bids.push_back({price, qty});
							}
						}

						// Ask 업데이트
						for (auto& ask : asks_update) {
							auto& price = ask[0];
							auto& qty = ask[1];
							auto it = std::find_if(book.asks.begin(), book.asks.end(),
									[&](auto& x) { return x[0] == price; });
							if (it != book.asks.end()) {
								if (qty != "0") (*it)[1] = qty;
								else book.asks.erase(it);
							} else if (qty != "0") {
								book.asks.push_back({price, qty});
							}
						}

						if (print_depth)
							print_depth_all(symbol, book, depth_count);
					} else {
						std::string symbol2 = data.value("s", "");
						std::string event  = data.value("e", "");
						if (event == "trade") {
							std::string price = data.value("p", "");
							std::string qty   = data.value("q", "");
							bool isBuyerMaker = data.value("m", false);

							if (print_trade)
							{
								std::cout << "[TRADE] " << symbol
									<< " P: " << price
									<< " Q: "   << qty
									<< " : " << (isBuyerMaker ? "SELL" : "BUY")
									<< std::endl;
							}
						}
					}
					// ---------------- Trade ----------------
				} catch (const std::exception& e) {
					std::cerr << "❌ JSON parse error: " << e.what() << std::endl;
				}
			}
			else if (msg->type == ix::WebSocketMessageType::Error) {
				std::cerr << "❌ WebSocket Error: " << msg->errorInfo.reason << std::endl;
			}
			});

			ws.start();

			// 메인 쓰레드 keep alive
			while (true)
				std::this_thread::sleep_for(std::chrono::seconds(1));

			return 0;
	}

