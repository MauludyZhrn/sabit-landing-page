import os
import subprocess
import time
import urllib.request
import json
import socket
import base64
import sys

print("=== SABIT LIVE PRODUCTION BROWSER VALIDATION WITH REAL SELECTORS ===")

edge_paths = [
    r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
    r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"
]
edge_bin = next((p for p in edge_paths if os.path.exists(p)), None)

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

def cdp_eval(ws, expression, msg_id):
    req = {
        "id": msg_id,
        "method": "Runtime.evaluate",
        "params": {
            "expression": expression,
            "returnByValue": True,
            "awaitPromise": True
        }
    }
    ws_send(ws, json.dumps(req))
    start = time.time()
    while time.time() - start < 10:
        raw = ws_recv(ws)
        if not raw: continue
        try:
            msg = json.loads(raw)
            if msg.get("id") == msg_id:
                if "result" in msg and "result" in msg["result"]:
                    val = msg["result"]["result"].get("value")
                    return val
                return msg.get("result")
        except Exception:
            continue
    return None

debug_port = 9555
cmd = [
    edge_bin,
    "--headless=new",
    "--no-sandbox",
    "--enable-webgl",
    f"--remote-debugging-port={debug_port}",
    "about:blank"
]
proc = subprocess.Popen(cmd)
time.sleep(2.5)

try:
    req = urllib.request.urlopen(f"http://127.0.0.1:{debug_port}/json")
    targets = json.loads(req.read().decode())
    page_target = next(t for t in targets if t.get('type') == 'page')
    ws_url = page_target['webSocketDebuggerUrl']
    path = ws_url.split(f":{debug_port}")[1]
    ws = ws_connect("127.0.0.1", debug_port, path)

    msg_id = 1
    target_url = "https://sabit-landing-page.vercel.app/"
    print(f"Navigating to {target_url} ...")
    ws_send(ws, json.dumps({"id": msg_id, "method": "Page.navigate", "params": {"url": target_url}}))
    time.sleep(2.0)

    # Set 1440x900 viewport
    msg_id += 1
    ws_send(ws, json.dumps({
        "id": msg_id,
        "method": "Emulation.setDeviceMetricsOverride",
        "params": { "width": 1440, "height": 900, "deviceScaleFactor": 1, "mobile": False }
    }))

    # Wait for dynamic chunk & Moon WebGL canvas
    canvas_ready = False
    for i in range(12):
        time.sleep(1.0)
        msg_id += 1
        st = cdp_eval(ws, """
        (() => {
            const canvas = document.querySelector('#moon-canvas-container canvas');
            const poster = document.querySelector('#moon-fallback-poster');
            const coords = document.querySelector('#moon-live-coords');
            return {
                hasCanvas: !!canvas,
                canvasWidth: canvas ? canvas.clientWidth : 0,
                canvasHeight: canvas ? canvas.clientHeight : 0,
                canvasClass: canvas ? canvas.className : null,
                posterOpacity: poster ? window.getComputedStyle(poster).opacity : null,
                telemetry: coords ? coords.textContent.trim() : ''
            };
        })()
        """, msg_id)
        if st and st.get('hasCanvas'):
            print(f"  ✓ Live WebGL Moon Canvas mounted after {i+1}s:")
            print(f"    - Dimensions: {st.get('canvasWidth')}x{st.get('canvasHeight')}px")
            print(f"    - Class: {st.get('canvasClass')}")
            print(f"    - Poster Opacity: {st.get('posterOpacity')}")
            print(f"    - Selenographic Telemetry: {st.get('telemetry')}")
            canvas_ready = True
            break
        else:
            print(f"  ... waiting for WebGL canvas ({i+1}s)")

    if not canvas_ready:
        print("  WebGL Canvas did not mount in time.")

finally:
    proc.terminate()
    proc.wait()

print("=== LIVE PRODUCTION BROWSER VALIDATION COMPLETE ===")
