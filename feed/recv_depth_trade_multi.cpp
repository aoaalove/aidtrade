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

using json = nlohmann::json;

// 심볼별 depth 저장소
struct DepthBook {
	std::vector<std::vector<std::string>> bids;
	std::vector<std::vector<std::string>> asks;
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

int main() {
	ix::WebSocket ws;
	ws.setUrl("wss://stream.binance.com:9443/stream");

	ws.setOnMessageCallback([&](const ix::WebSocketMessagePtr& msg) {
			if (msg->type == ix::WebSocketMessageType::Open) {
				std::cout << "✅ Connected to Binance multi-stream\n";

				// 여러 종목 depth + trade subscribe
				json sub_msg = {
					{"method", "SUBSCRIBE"},
					{"params", 
						{
							"adausdt@trade", "adausdt@depth5@100ms",
							"btcusdt@trade", "btcusdt@depth5@100ms",
							"ethusdt@trade", "ethusdt@depth5@100ms"
						}
					},
					{"id", 1}
				};
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

						print_depth_all(symbol, book, 5);
					} else {
						std::string symbol2 = data.value("s", "");
						std::string event  = data.value("e", "");
						if (event == "trade") {
							std::string price = data.value("p", "");
							std::string qty   = data.value("q", "");
							bool isBuyerMaker = data.value("m", false);

							std::cout << "[TRADE] " << symbol
								<< " P: " << price
								<< " Q: "   << qty
								<< " : " << (isBuyerMaker ? "SELL" : "BUY")
								<< std::endl;
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

