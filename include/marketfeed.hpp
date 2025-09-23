#pragma once
namespace aidetrade
{
	struct Price	
	{
		double price;	
		double qty;
	};

	struct Depth
	{
		Price	bids[5];
		Price	asks[5];
	};

	struct FeedBase
	{
		char symbol[10];
		char timestamp[20];
	};

	struct FeedTrade : FeedBase
	{
		Price	trade;
	};

	struct FeedDepth : FeedBase
	{
		Depth	depth;
	};
}
