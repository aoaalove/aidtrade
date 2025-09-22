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
#include "parsearg.hpp"
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

void print_depth_all(const std::string& symbol, const DepthBook& book, 
		size_t depth_count = 1) {
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

struct Config
{
	Config(nlohmann::json config)
	{
		url = config["binance"]["url"].get<std::string>();
		print_depth = config["print_depth"].get<bool>();
		print_trade = config["print_trade"].get<bool>();
		depth_count = config["depth_count"].get<size_t>();	
		for (const auto& item : config["binance"]["subscriptions"])
		{
			subs.push_back({std::move(item["symbol"].get<std::string>()),
					std::move(item["streams"].get<std::vector<std::string>>())});
		}
	}

	std::string url;
	bool print_depth;
	bool print_trade;
	size_t	depth_count;
	std::vector<std::pair<std::string, std::vector<std::string>>>	subs;
};

void operOpen(ix::WebSocket& ws, const Config& config)
{
	std::cout << "[O]Connected to Binance multi-stream\n";

	// 여러 종목 depth + trade subscribe
	json sub_msg = {
		{"method", "SUBSCRIBE"},
		{"params", 
			{
			}
		},
		{"id", 1}
	};
	for (const auto& sub : config.subs)
	{
		for (const auto& stream : sub.second)
			sub_msg["params"].push_back(sub.first + "@" + stream); 
	}

	ws.send(sub_msg.dump());
}

void operMessage(const ix::WebSocketMessagePtr& msg, const Config& config)
{
	try {
		std::lock_guard<std::mutex> lock(depth_mtx);
		auto js = json::parse(msg->str);

		if (!js.contains("stream") || !js.contains("data"))
			return;

		std::string stream = js["stream"];
		size_t	pos = stream.find('@');
		std::string symbol = stream.substr(0, pos);
		stream = stream.substr(pos+1);
		auto data = js["data"];
		// ---------------- Depth update ----------------
		if (stream == "depth5@100ms")  {

			auto& book = depth_books[symbol];

			auto bids_update = data.value("bids", 
					std::vector<std::vector<std::string>>{});
			auto asks_update = data.value("asks", 
					std::vector<std::vector<std::string>>{});

			// Bid update
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

			// Ask update
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

			if (config.print_depth)
				print_depth_all(symbol, book, config.depth_count);
		} else {
			std::string symbol2 = data.value("s", "");
			std::string event  = data.value("e", "");
			if (event == "trade") {
				std::string price = data.value("p", "");
				std::string qty   = data.value("q", "");
				bool isBuyerMaker = data.value("m", false);

				if (config.print_trade)
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
		std::cerr << "[X]JSON parse error: " << e.what() << std::endl;
	}
}

int main(int argc, char* argv[]) {
	// parse arguments
	ParseArg pa(argc, argv);

	// read config.json
	std::string config_file = "./config.json";
	std::fstream	fs(config_file);
	if (!fs.is_open()) {
		std::cout << std::format("{} file not found", config_file);	
		return -1;
	}
	nlohmann::json js_config;
	fs >> js_config;
	Config config(js_config);	
	ix::WebSocket ws;
	ws.setUrl(config.url);

	ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
			switch (msg->type)
			{
				case ix::WebSocketMessageType::Open:
					operOpen(ws, config);
				break;
				case ix::WebSocketMessageType::Message:
					operMessage(msg, config);
				break;
				case ix::WebSocketMessageType::Error:
					std::cerr << "[X]WebSocket Error: " << msg->errorInfo.reason << std::endl;
				break;
				default:
				break;
			}
	});

	ws.start();

	while (true)
		std::this_thread::sleep_for(std::chrono::seconds(1));

	return 0;
}

