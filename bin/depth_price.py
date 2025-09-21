import websocket
import json
import threading

# 5호가 + 최근 체결 가격 스트림 URL (ADA/USDT 기준)
depth_stream = "wss://stream.binance.com:9443/ws/adausdt@depth5@100ms"
trade_stream = "wss://stream.binance.com:9443/ws/adausdt@trade"

# Depth 메시지 처리
def on_depth_message(ws, message):
    data = json.loads(message)
    print("===== Depth 5 =====")
    print("Bids:", data["b"])
    print("Asks:", data["a"])

# Trade 메시지 처리
def on_trade_message(ws, message):
    data = json.loads(message)
    print("===== Recent Trade =====")
    print("Price:", data["p"], "Qty:", data["q"])

# 각 스트림을 별도 스레드로 실행
def run_depth():
    ws = websocket.WebSocketApp(depth_stream, on_message=on_depth_message)
    ws.run_forever()

def run_trade():
    ws = websocket.WebSocketApp(trade_stream, on_message=on_trade_message)
    ws.run_forever()

# 스레드 시작
threading.Thread(target=run_depth).start()
threading.Thread(target=run_trade).start()

