import websocket
import json
import requests
import threading

# -------------------------
# 1. REST API로 초기 Depth 5
# -------------------------
url = "https://api.binance.com/api/v3/depth"
params = {"symbol": "ADAUSDT", "limit": 5}
resp = requests.get(url, params=params).json()

prev_bids = resp["bids"]  # [["가격", "수량"], ...]
prev_asks = resp["asks"]

last_trade_price = None  # 최근 체결 가격

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
    print("====================================================================")
    if last_trade_price:
        print(f"Last Trade Price: {last_trade_price}")
    print("====================================================================\n")

# -------------------------
# 2. Depth WebSocket
# -------------------------
depth_stream = "wss://stream.binance.com:9443/ws/adausdt@depth5@100ms"

def on_depth_message(ws, message):
    global prev_bids, prev_asks
    data = json.loads(message)

    bids_update = data.get("b", []) or data.get("bids", [])
    asks_update = data.get("a", []) or data.get("asks", [])

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

def on_depth_open(ws):
    print("Depth WebSocket 연결됨")

# -------------------------
# 3. Trade WebSocket
# -------------------------
trade_stream = "wss://stream.binance.com:9443/ws/adausdt@trade"

def on_trade_message(ws, message):
    global last_trade_price
    data = json.loads(message)
    last_trade_price = data.get("p")  # 체결 가격
    # Depth 출력 갱신 시 체결가도 같이 보여줌
    print_depth(prev_bids, prev_asks)

def on_trade_open(ws):
    print("Trade WebSocket 연결됨")

# -------------------------
# 4. WebSocket 동시에 실행
# -------------------------
def run_ws(url, on_msg, on_opn):
    ws = websocket.WebSocketApp(url, on_message=on_msg, on_open=on_opn)
    ws.run_forever()

# Depth와 Trade 각각 스레드로 실행
threading.Thread(target=run_ws, args=(depth_stream, on_depth_message, on_depth_open), daemon=True).start()
threading.Thread(target=run_ws, args=(trade_stream, on_trade_message, on_trade_open), daemon=True).start()

# 메인 스레드는 무한 대기
while True:
    pass

