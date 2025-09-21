import requests

# 비트코인/USDT 가격 요청
url = "https://api.binance.com/api/v3/ticker/price"
params = {"symbol": "BTCUSDT"}
resp = requests.get(url, params=params)

print(resp.json())

