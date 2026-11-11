import os
import subprocess
import time
import urllib.request
import json
import socket
import base64
import http.server
import socketserver
import threading

PORT = 5230
DIRECTORY = "dist"

def ws_connect(host, port, path):
    s = socket.socket()
    s.connect((host, port))
    key = base64.b64encode(os.urandom(16)).decode()
    req = (f"GET {path} HTTP/1.1\r\nHost: {host}:{port}\r\nUpgrade: websocket\r\n"
           f"Connection: Upgrade\r\nSec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n")
    s.sendall(req.encode())
    res = s.recv(4096)
    assert b"101 " in res, f"Handshake failed: {res[:100]}"
    return s

def ws_send(s, text):
    payload = text.encode('utf-8')
    length = len(payload)
    if length <= 125:
        header = bytes([0x81, 0x80 | length])
    elif length <= 65535:
        header = bytes([0x81, 0x80 | 126, (length >> 8) & 0xFF, length & 0xFF])
    else:
        header = bytes([0x81, 0x80 | 127]) + length.to_bytes(8, 'big')
    mask = os.urandom(4)
    masked = bytes([b ^ mask[i % 4] for i, b in enumerate(payload)])
    s.sendall(header + mask + masked)

def ws_recv(s):
    header = s.recv(2)
    if not header: return ""
    b1, b2 = header
    length = b2 & 0x7F
    if length == 126:
        length = int.from_bytes(s.recv(2), 'big')
    elif length == 127:
        length = int.from_bytes(s.recv(8), 'big')
    data = bytearray()
    while len(data) < length:
        packet = s.recv(min(length - len(data), 65536))
        if not packet: break
        data.extend(packet)
    return data.decode('utf-8', errors='ignore')

def capture_all():
    os.makedirs("artifacts/screenshots", exist_ok=True)
    for f in os.listdir("artifacts/screenshots"):
        if f.endswith(".png"):
            try:
                os.remove(os.path.join("artifacts/screenshots", f))
            except Exception:
                pass

    server = socketserver.TCPServer(("127.0.0.1", PORT), lambda *args: http.server.SimpleHTTPRequestHandler(*args, directory=DIRECTORY))
    server_thread = threading.Thread(target=server.serve_forever, daemon=True)
    server_thread.start()
    time.sleep(0.5)

    edge_paths = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"
    ]
    edge_bin = next((p for p in edge_paths if os.path.exists(p)), None)
    if not edge_bin:
        print("Edge binary not found.")
        return

    debug_port = 9555
    cmd = [
        edge_bin,
        "--headless=new",
        "--no-sandbox",
        "--enable-webgl",
        "--ignore-gpu-blocklist",
        f"--remote-debugging-port={debug_port}",
        "about:blank"
    ]
    proc = subprocess.Popen(cmd)
    time.sleep(2)

    viewports = [
        ("mobile_360", 360, 780, True),
        ("mobile_390", 390, 844, True),
        ("mobile_430", 430, 932, True),
        ("tablet_768", 768, 1024, True),
        ("desktop_1024", 1024, 768, False),
        ("desktop_1366", 1366, 768, False),
        ("desktop_1440", 1440, 900, False),
        ("desktop_1920", 1920, 1080, False)
    ]

    try:
        req = urllib.request.urlopen(f"http://127.0.0.1:{debug_port}/json")
        targets = json.loads(req.read().decode())
        page_target = next(t for t in targets if t.get('type') == 'page')
        ws_url = page_target['webSocketDebuggerUrl']
        path = ws_url.split(f":{debug_port}")[1]
        ws = ws_connect("127.0.0.1", debug_port, path)

        msg_id = 1
        ws_send(ws, json.dumps({"id": msg_id, "method": "Page.navigate", "params": {"url": f"http://127.0.0.1:{PORT}/"}}))
        time.sleep(2)

        for name, w, h, is_mobile in viewports:
            msg_id += 1
            ws_send(ws, json.dumps({
                "id": msg_id,
                "method": "Emulation.setDeviceMetricsOverride",
                "params": {
                    "width": w,
                    "height": h,
                    "deviceScaleFactor": 1,
                    "mobile": is_mobile
                }
            }))
            time.sleep(1.2)

            msg_id += 1
            shot_id = msg_id
            ws_send(ws, json.dumps({
                "id": shot_id,
                "method": "Page.captureScreenshot",
                "params": {"format": "png"}
            }))

            while True:
                resp_str = ws_recv(ws)
                if not resp_str:
                    break
                try:
                    msg = json.loads(resp_str)
                    if msg.get("id") == shot_id and "result" in msg:
                        img_data = base64.b64decode(msg["result"]["data"])
                        out_path = f"artifacts/screenshots/{name}.png"
                        with open(out_path, "wb") as f:
                            f.write(img_data)
                        print(f"Captured {name} ({w}x{h}): {len(img_data):,} bytes")
                        break
                except Exception:
                    continue

    finally:
        proc.terminate()

if __name__ == '__main__':
    capture_all()
