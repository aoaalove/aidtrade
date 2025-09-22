#include <iostream>
#include <vector>

struct Price
{
	Price() = default;
	Price(std::string p, std::string q) : price{p}, qty{q} {}
	std::string price;
	std::string qty;
};

struct RawBase
{
	RawBase(std::string type) : type{std::move(type)}{}
	std::string type; // "D" : depth "T" : trade
	std::string symbol;
	std::string timestamp;
}; 

struct RawDepth : RawBase 
{ 
	RawDepth() : RawBase{"D"} {}
	std::vector<Price>	asks;
	std::vector<Price>	bids;
};

struct RawTrade : RawBase
{
	RawTrade() : RawBase{"T"} {}
	Price	trade;
};

int main()
{
	std::vector<RawBase> data;
	data.reserve(10);
	
	RawDepth	d;
	RawTrade	t;
	d.symbol = "adausd";
	d.timestamp = "20250922T101000";
	d.asks.push_back(Price("0.8920", "10.0"));
	d.asks.push_back(Price("0.8921", "11.0"));
	d.asks.push_back(Price("0.8922", "12.0"));
	d.asks.push_back(Price("0.8923", "13.0"));
	d.asks.push_back(Price("0.8924", "14.0"));
	d.bids.push_back(Price("0.8919", "20.0"));
	d.bids.push_back(Price("0.8918", "21.0"));
	d.bids.push_back(Price("0.8917", "22.0"));
	d.bids.push_back(Price("0.8916", "23.0"));
	d.bids.push_back(Price("0.8915", "24.0"));
	t.trade.price = "0.8923";
	t.trade.qty = "15.0";
	data.push_back(d);
	data.push_back(t);
}
