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
#include <boost/lockfree/spsc_queue.hpp>
#include "marketfeed.hpp"

using json = nlohmann::json;

struct DepthBook 
{
	bool bid1{};
	std::vector<std::vector<std::string>> bids;
	bool ask1{};
	std::vector<std::vector<std::string>> asks;
};

struct Config
{
	Config() = default;
	Config(nlohmann::json js) { parse(js); }
	void parse(nlohmann::json js)
	{
		url = js["binance"]["url"].get<std::string>();
		print_depth = js["print_depth"].get<bool>();
		print_trade = js["print_trade"].get<bool>();
		depth_count = js["depth_count"].get<size_t>();	
		for (const auto& item : js["binance"]["subscriptions"])
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

struct RawFeed {
	RawFeed() = default;
	RawFeed(nlohmann::json&& js)
	{
		auto now = std::chrono::system_clock::now();
		auto t = std::chrono::system_clock::to_time_t(now);
		std::tm local_tm = *std::localtime(&t);
		auto micro = std::chrono::duration_cast<std::chrono::microseconds>(
				now.time_since_epoch()).count() % 1'000'000;
		std::ostringstream oss;
		oss << std::put_time(&local_tm, "%Y%m%d%H%M%S");
		timestamp = std::format("{}.{:06}", oss.str(), micro);
		data = std::move(js);
	}

	std::string	timestamp;
	nlohmann::json 	data;
};


boost::lockfree::spsc_queue<RawFeed, boost::lockfree::capacity<1024>> _queue;
Config _config;
std::unordered_map<std::string, DepthBook> depth_books;

void print_depth(const std::string& timestamp, const std::string& symbol, const DepthBook& book) 
{
	if (!book.bid1 and !book.ask1)
		return;
	size_t	asize = book.asks.size()-1;
/*
width + alignment + float digit
double x = 3.14159;

std::cout << std::format("|{:10.2f}|", x) << '\n'; // width 10, right
std::cout << std::format("|{:<10.2f}|", x) << '\n'; // width 10, left
std::cout << std::format("|{:^10.2f}|", x) << '\n'; // width 10, center
*/
	std::cout << timestamp << ":[DEPTH] " 
			<< symbol 
			<< " "
			<< std::setw(15) << std::right << book.bids[0][1]  
			<< "|"
			<< std::setw(15) << std::right << book.bids[0][0]  
			<< " : " 
			<< std::setw(15) << std::left << book.asks[asize][0]  
			<< "|"
			<< std::setw(15) << std::right << book.asks[asize][1]
			<< std::endl;
}

void print_depth_all(const std::string& timestamp, const std::string& symbol, const DepthBook& book, size_t depth_count) 
{
	if (depth_count == 1) 
	{
		print_depth(timestamp, symbol, book);
		return;
	}
		
	std::cout << "-----------------------------------------------------\n";
	std::cout << std::format("{}:[DEPTH] {} (Top {} Bids(Q|P)/Asks(P|Q))\n", 
			timestamp, symbol, depth_count);
	std::cout << "-----------------------------------------------------\n";

	for (size_t i = 0; i < std::min<size_t>(depth_count, book.asks.size()); i++) 
	{
		std::cout << "\t\t  " << std::setw(15) << std::right << book.asks[i][0] 
			<< " | " 
			<< std::setw(15) << std::right << book.asks[i][1] << "\n";
	}

	for (size_t i = 0; i < std::min<size_t>(depth_count, book.bids.size()); i++) 
	{
		std::cout << std::setw(15) << std::right << book.bids[i][1] 
			<< " | " 
			<< std::setw(15) << std::right << book.bids[i][0] << "\n";
	}

	std::cout << "-----------------------------------------------------\n";
}

void on_ws_message(const ix::WebSocketMessagePtr& msg)
{
	try 
	{
		auto js = json::parse(msg->str);

		if (!js.contains("stream") || !js.contains("data"))
			return;

		RawFeed	feed(std::move(js));
		_queue.push(std::move(feed));
	} 
	catch (const std::exception& e) 
	{
		std::cerr << "[X]JSON parse error: " << e.what() << std::endl;
	}
}

void print(std::vector<std::vector<std::string>>& depths)
{
	for (const auto& d : depths)
	{
		for (const auto& p : d)
		{
			std::cout << p << ",";
		}
		std::cout << std::endl;
	}
}

void on_depth(const std::string& stream, const std::string& symbol, const RawFeed& feed)
{
	if (stream.compare(0, 5, "depth") != 0) 
	{
		std::cout << "not match" << std::endl;
		return; 
	}
	auto& data = feed.data["data"];
	auto& book = depth_books[symbol];
	auto bids = data.value("bids", std::vector<std::vector<std::string>>{});
	bool bidpass = false, askpass = false;

	book.bid1 = false;
	if (bids == book.bids)
		bidpass = true;
	else
	{
		if (book.bids.size() and bids.size())
		{
			if (book.bids[0][0] != bids[0][0] or book.bids[0][1] != bids[0][1])
				book.bid1 = true;
		}
		book.bids = std::move(bids);
	}

	auto asks = data.value("asks", 
			std::vector<std::vector<std::string>>{});

	book.ask1 = false;
	if (asks == book.asks)
		askpass = true;
	else
	{
		if (book.asks.size() and asks.size())
		{
			size_t	index = book.asks.size()-1;
			if (book.asks[index][0] != asks[0][0] or 
					book.asks[index][1] != asks[0][1])
				book.ask1 = true;
		}
		book.asks = std::move(asks);
	}

	std::sort(book.asks.begin(), book.asks.end(), 
			[](const std::vector<std::string>& a, 
				const std::vector<std::string>& b) {
				return a[0] > b[0];
			});

	if (bidpass and askpass)
	{
		std::cout << "no changed" << std::endl;
		return;
	}

	if (_config.print_depth)
		print_depth_all(feed.timestamp, symbol, book, _config.depth_count);
}

std::string to_lower(const std::string& input) {
    std::string result = input;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    return result;
}

void on_trade(const std::string& stream, const std::string& symbol, const RawFeed& feed)
{
	auto& data = feed.data["data"];
	std::string symbol2 = data.value("s", "");
	symbol2 = to_lower(symbol2);
	std::string event  = data.value("e", "");
	if (symbol != symbol2 or stream != event) 
	{
		std::cout << "not matched:" << symbol <<"," << symbol2 << "," << stream <<","<< event<< std::endl;
		return;
	}

	std::string price = data.value("p", "");
	std::string qty   = data.value("q", "");
	bool isBuyerMaker = data.value("m", false);

	if (_config.print_trade)
	{
		std::cout << feed.timestamp << ":[TRADE] " << symbol
			<< std::format(" {:>4} {} {:>15}\n", (isBuyerMaker ? "\033[31mSELL\033[0m" : "\033[32mBUY\033[0m"), std::format("\033[1;34m{}\033[0m", price), qty);
	}
}

int main(int argc, char* argv[]) 
{
	// parse arguments
	ParseArg pa(argc, argv);

	// read config.json
	std::string config_file = "./config.json";
	std::fstream	fs(config_file);

	if (!fs.is_open()) 
	{
		std::cout << std::format("{} file not found", config_file);	
		return -1;
	}

	nlohmann::json js_config;
	fs >> js_config;
	_config.parse(js_config);	
	ix::WebSocket ws;
	ws.setUrl(_config.url);

	ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
		switch (msg->type)
		{
			case ix::WebSocketMessageType::Open:
				{
					std::cout << "[O]Connected to Binance multi-stream\n";

					json sub_msg = {
						{"method", "SUBSCRIBE"},
						{"params", 
							{
							}
						},
						{"id", 1}
					};

					for (const auto& sub : _config.subs)
					{
						for (const auto& stream : sub.second)
							sub_msg["params"].push_back(sub.first + "@" + stream); 
					}

					ws.send(sub_msg.dump());
				}
				break;
			case ix::WebSocketMessageType::Message:
				on_ws_message(msg);
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
	{
		while (!_queue.empty())
		{
			RawFeed	feed;
			bool ret = _queue.pop(feed);

			if (!ret)
			{
				std::cout << "something is wrong";
				continue;
			}

			auto& js = feed.data;
			std::string stream = js["stream"];
			size_t	pos = stream.find('@');
			std::string symbol = stream.substr(0, pos);
			stream = stream.substr(pos+1);
			// ---------------- Depth update ----------------
			if (stream == "depth5@100ms")  
				on_depth(stream, symbol, feed);
			else 
				on_trade(stream, symbol, feed);
		}
		std::this_thread::yield();
	}

	return 0;
}

