import requests

url = "https://api.binance.com/api/v3/depth"
params = {
    "symbol": "ADAUSDT",
    "limit": 5  # Top 5호가
}
resp = requests.get(url, params=params)
data = resp.json()

print("Bids (매수):")
for price, qty in data["bids"]:
    print(price, qty)

print("Asks (매도):")
for price, qty in data["asks"]:
    print(price, qty)

