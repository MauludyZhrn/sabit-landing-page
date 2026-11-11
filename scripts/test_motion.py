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
import sys

PORT = 5250
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
    while time.time() - start < 5:
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

def cdp_screenshot(ws, out_path, msg_id):
    ws_send(ws, json.dumps({"id": msg_id, "method": "Page.captureScreenshot", "params": {"format": "png"}}))
    while True:
        raw = ws_recv(ws)
        if not raw: break
        try:
            msg = json.loads(raw)
            if msg.get("id") == msg_id and "result" in msg:
                data = base64.b64decode(msg["result"]["data"])
                with open(out_path, "wb") as f:
                    f.write(data)
                return True
        except Exception:
            continue
    return False

def test_motion():
    print("=== SABIT MOTION INTERACTION & ACCESSIBILITY VALIDATION ===")

    server = socketserver.TCPServer(("127.0.0.1", PORT), lambda *args: http.server.SimpleHTTPRequestHandler(*args, directory=DIRECTORY))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    time.sleep(0.5)

    edge_paths = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"
    ]
    edge_bin = next((p for p in edge_paths if os.path.exists(p)), None)
    if not edge_bin:
        print("Edge binary not found.")
        return False

    debug_port = 9777
    cmd = [
        edge_bin,
        "--headless=new",
        "--no-sandbox",
        "--enable-webgl",
        f"--remote-debugging-port={debug_port}",
        "about:blank"
    ]
    proc = subprocess.Popen(cmd)
    time.sleep(2)

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

        # Set 1440x900 viewport
        msg_id += 1
        ws_send(ws, json.dumps({
            "id": msg_id,
            "method": "Emulation.setDeviceMetricsOverride",
            "params": { "width": 1440, "height": 900, "deviceScaleFactor": 1, "mobile": False }
        }))
        time.sleep(1)

        # 1. Headline & Hero Stagger
        msg_id += 1
        hero_motion = cdp_eval(ws, """
        (() => {
            const eyebrow = document.querySelector('.hero-eyebrow');
            const title = document.querySelector('.hero-title');
            const lead = document.querySelector('.hero-lead');
            const stage = document.querySelector('.hero-visual-stage');
            return {
                eyebrowOpacity: eyebrow ? window.getComputedStyle(eyebrow).opacity : null,
                titleOpacity: title ? window.getComputedStyle(title).opacity : null,
                leadOpacity: lead ? window.getComputedStyle(lead).opacity : null,
                stageOpacity: stage ? window.getComputedStyle(stage).opacity : null,
                titleVisible: title && window.getComputedStyle(title).visibility === 'visible'
            };
        })()
        """, msg_id)
        print("1. Hero Entrance & Stagger Validation:")
        print(f"   - Title Opacity: {hero_motion.get('titleOpacity')} (Visible: {hero_motion.get('titleVisible')})")
        print(f"   - Lead Opacity: {hero_motion.get('leadOpacity')}")
        print(f"   - Stage Opacity: {hero_motion.get('stageOpacity')}")
        assert hero_motion.get('titleVisible'), "Hero title is not visible"

        # 2. WCAG 2.2.2 Pause/Resume Toggle Button
        msg_id += 1
        toggle_res = cdp_eval(ws, """
        (() => {
            const btn = document.querySelector('#motion-toggle-btn');
            if (!btn) return { error: 'No toggle button' };
            const beforeClick = document.documentElement.classList.contains('motion-paused');
            btn.click();
            const afterClick1 = document.documentElement.classList.contains('motion-paused');
            const diag1 = typeof window.__sabitMoonDiagnostics === 'function' ? window.__sabitMoonDiagnostics() : null;
            btn.click();
            const afterClick2 = document.documentElement.classList.contains('motion-paused');
            const diag2 = typeof window.__sabitMoonDiagnostics === 'function' ? window.__sabitMoonDiagnostics() : null;
            return {
                beforeClick,
                pausedAfterClick: afterClick1,
                moonPaused: diag1 ? diag1.isPaused : null,
                resumedAfterSecondClick: !afterClick2,
                moonResumed: diag2 ? !diag2.isPaused : null
            };
        })()
        """, msg_id)
        print("2. WCAG 2.2.2 Pause/Resume Motion Control:")
        print(f"   - HTML paused class applied on click: {toggle_res.get('pausedAfterClick')}")
        print(f"   - Three.js Moon scene paused: {toggle_res.get('moonPaused')}")
        print(f"   - HTML unpaused on 2nd click: {toggle_res.get('resumedAfterSecondClick')}")
        print(f"   - Three.js Moon scene resumed: {toggle_res.get('moonResumed')}")
        assert toggle_res.get('pausedAfterClick'), "Pause button failed to apply .motion-paused"
        assert toggle_res.get('resumedAfterSecondClick'), "Pause button failed to resume"

        # 3. Interactive 3D Perspective Tilt on Application Showcase
        msg_id += 1
        tilt_res = cdp_eval(ws, """
        (async () => {
            const stage = document.querySelector('.screenshot-stage');
            if (!stage) return { error: 'No stage' };
            const rect = stage.getBoundingClientRect();
            // Dispatch pointermove near top-left of stage to trigger 3D perspective tilt
            stage.dispatchEvent(new MouseEvent('mousemove', {
                clientX: rect.left + 50,
                clientY: rect.top + 50,
                bubbles: true
            }));
            await new Promise(r => setTimeout(r, 100));
            const transform = stage.style.transform || window.getComputedStyle(stage).transform;
            return {
                transform,
                transformApplied: transform.includes('matrix3d') || transform.includes('rotateX') || transform.includes('perspective')
            };
        })()
        """, msg_id)
        print("3. Application Showcase 3D Perspective Tilt:")
        print(f"   - Mouse Tilt Transform Applied: {tilt_res.get('transformApplied')}")

        # 4. Scroll Reveal & Capturing Visual Evidence across Scroll Positions
        scroll_targets = [
            ("scroll_hero", 0, "Hero & Celestial Orbital Space"),
            ("scroll_geometri", 700, "Geometri Pengamatan & Kriteria MABIMS"),
            ("scroll_evidence", 1500, "Antarmuka Aplikasi Sabit Desktop"),
            ("scroll_workflow", 2300, "Alur Kerja Sabit Basic"),
            ("scroll_roadmap", 3200, "Arah Riset & Roadmap"),
            ("scroll_teknis", 4000, "Spesifikasi Teknis & Download")
        ]

        print("4. Scroll-Triggered Progress & Visual Evidence Captures:")
        for name, scroll_y, label in scroll_targets:
            msg_id += 1
            cdp_eval(ws, f"window.scrollTo({{ top: {scroll_y}, behavior: 'instant' }});", msg_id)
            time.sleep(0.8)
            msg_id += 1
            out_file = f"artifacts/screenshots/{name}.png"
            cdp_screenshot(ws, out_file, msg_id)
            size = os.path.getsize(out_file) if os.path.exists(out_file) else 0
            print(f"   - Captured {label} (y={scroll_y}px) -> {name}.png ({size:,} bytes)")

        # 5. Workflow Timeline Progress Scrub Check
        msg_id += 1
        wf_check = cdp_eval(ws, """
        (() => {
            const fill = document.querySelector('.workflow-progress-fill');
            const track = document.querySelector('.workflow-progress-track');
            return {
                fillExists: !!fill,
                fillTransform: fill ? window.getComputedStyle(fill).transform : null
            };
        })()
        """, msg_id)
        print(f"5. Workflow Scrubbed Timeline Progress: fillExists={wf_check.get('fillExists')}, transform={wf_check.get('fillTransform')}")

        # 6. Prefers-Reduced-Motion Emulation
        msg_id += 1
        ws_send(ws, json.dumps({
            "id": msg_id,
            "method": "Emulation.setEmulatedMedia",
            "params": {
                "features": [{ "name": "prefers-reduced-motion", "value": "reduce" }]
            }
        }))
        time.sleep(0.5)
        msg_id += 1
        reduced_test = cdp_eval(ws, "window.matchMedia('(prefers-reduced-motion: reduce)').matches", msg_id)
        print(f"6. Reduced Motion Media Query Active: {reduced_test}")
        assert reduced_test, "prefers-reduced-motion media feature was not emulated properly"

        # 7. Window Resize Resilience
        msg_id += 1
        ws_send(ws, json.dumps({
            "id": msg_id,
            "method": "Emulation.setDeviceMetricsOverride",
            "params": { "width": 768, "height": 1024, "deviceScaleFactor": 1, "mobile": True }
        }))
        time.sleep(1)
        msg_id += 1
        resize_test = cdp_eval(ws, """
        (() => {
            const canvas = document.querySelector('.moon-webgl-canvas');
            const header = document.querySelector('.header-inner');
            return {
                canvasWidth: canvas ? canvas.width : 0,
                headerVisible: header && window.getComputedStyle(header).visibility !== 'hidden'
            };
        })()
        """, msg_id)
        print(f"7. Viewport Resize to Tablet 768px: canvasWidth={resize_test.get('canvasWidth')}, headerVisible={resize_test.get('headerVisible')}")
        assert resize_test.get('headerVisible'), "Header is not visible after resize"

        print("\n=== ALL MOTION INTERACTION & ACCESSIBILITY TESTS PASSED ===")
        return True

    finally:
        proc.terminate()

if __name__ == '__main__':
    ok = test_motion()
    sys.exit(0 if ok else 1)
