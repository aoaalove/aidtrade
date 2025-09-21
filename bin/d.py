import websocket
import json
import requests

# -------------------------
# 1. REST API로 초기 Depth 5
# -------------------------
url = "https://api.binance.com/api/v3/depth"
params = {"symbol": "ADAUSDT", "limit": 5}
resp = requests.get(url, params=params).json()

prev_bids = resp["bids"]  # [["가격", "수량"], ...]
prev_asks = resp["asks"]

def print_depth(bids, asks):
    # 화면 맨 위로 이동 + 화면 전체 지우기
    print("\033[H\033[J", end="")

    # 정렬
    bids_sorted = sorted(bids, key=lambda x: float(x[0]), reverse=True)[:5]
    asks_sorted = sorted(asks, key=lambda x: float(x[0]), reverse=True)[:5]

    print("==== Depth 5 (Bid | Ask) ===========================================")
    # Ask 먼저
    for ask in asks_sorted:
        print(f"\t\t\t\t\t{ask[0]:>10} | {ask[1]:>15}")
    # Bid 아래쪽
    for bid in bids_sorted:
        print(f"{bid[0]:>10} | {bid[1]:>15}")
    print("====================================================================\n")

print_depth(prev_bids, prev_asks)

# -------------------------
# 2. WebSocket 스트림 (Depth 5)
# -------------------------
stream = "wss://stream.binance.com:9443/ws/adausdt@depth5@100ms"

def on_message(ws, message):
    global prev_bids, prev_asks
    data = json.loads(message)

    bids_update = data.get("bids", [])
    asks_update = data.get("asks", [])

    changed = False

    # ---------- Bid 증분 적용 ----------
    for price, qty in bids_update:
        found = False
        for i, x in enumerate(prev_bids):
            if x[0] == price:
                if qty != "0":
                    prev_bids[i][1] = qty
                else:
                    prev_bids.pop(i)
                found = True
                changed = True
                break
        if not found and qty != "0":
            prev_bids.append([price, qty])
            changed = True

    # ---------- Ask 증분 적용 ----------
    for price, qty in asks_update:
        found = False
        for i, x in enumerate(prev_asks):
            if x[0] == price:
                if qty != "0":
                    prev_asks[i][1] = qty
                else:
                    prev_asks.pop(i)
                found = True
                changed = True
                break
        if not found and qty != "0":
            prev_asks.append([price, qty])
            changed = True

    if changed:
        print_depth(prev_bids, prev_asks)

def on_open(ws):
    print("WebSocket 연결됨")

# -------------------------
# 3. WebSocket 실행
# -------------------------
ws = websocket.WebSocketApp(stream, on_message=on_message, on_open=on_open)
ws.run_forever()

