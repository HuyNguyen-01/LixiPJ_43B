from flask import Flask, jsonify, send_file
from flask_sock import Sock

app = Flask(__name__)
sock = Sock(app)

# Trang frontend
@app.route("/")
def index():
    return send_file("index.html")

# route chung cho ảnh
@app.route("/<path:filename>")
def static_files(filename):
    return send_file(filename)

# ===== WEBSOCKET (GIỐNG ESP32) =====
clients = []

@sock.route("/ws")
def websocket(ws):
    clients.append(ws)
    print("Client connected")

    try:
        while True:
            data = ws.receive()
            if data is None:
                break
    finally:
        clients.remove(ws)
        print("Client disconnected")

# ===== ADMIN TRIGGER (GIẢ LẬP ESP32 BẤM NÚT) =====
@app.route("/admin/spin")
def admin_spin():
    print("ADMIN -> SPIN")
    for ws in clients:
        ws.send("spin_now")
    return { "ok": True }


# API cấu hình tiền
@app.route("/money-config")
def money_config():
    return jsonify([
        { "value": 10000, "weight": 40 },
        { "value": 20000, "weight": 30 },
        { "value": 50000, "weight": 20 },
        { "value": 100000, "weight": 10 }
    ])

if __name__ == "__main__":
    app.run(
        host="192.168.137.1",
        port=5000,
        debug=True
    )


