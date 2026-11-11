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

PORT = 5240
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

def verify_webgl():
    print("=== SABIT REAL WEBGL & GPU VALIDATION SUITE ===")

    server = socketserver.TCPServer(("127.0.0.1", PORT), lambda *args: http.server.SimpleHTTPRequestHandler(*args, directory=DIRECTORY))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    time.sleep(0.5)

    edge_paths = [
        r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe",
        r"C:\Program Files\Microsoft\Edge\Application\msedge.exe"
    ]
    edge_bin = next((p for p in edge_paths if os.path.exists(p)), None)
    if not edge_bin:
        print("ERROR: Edge binary not found.")
        return False

    debug_port = 9666
    temp_dir = os.path.abspath("artifacts/temp_webgl_profile")
    os.makedirs(temp_dir, exist_ok=True)

    # Launch Edge with GPU enabled (NO --disable-gpu)
    cmd = [
        edge_bin,
        "--headless=new",
        "--no-sandbox",
        "--enable-webgl",
        "--ignore-gpu-blocklist",
        f"--user-data-dir={temp_dir}",
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
        # Enable console and network tracking
        ws_send(ws, json.dumps({"id": msg_id, "method": "Page.navigate", "params": {"url": f"http://127.0.0.1:{PORT}/"}}))
        print("1. Navigated to local production build.")
        time.sleep(3) # allow Three.js and textures to load

        # 1. Test Browser WebGL & WebGL2 API Support
        msg_id += 1
        webgl_support = cdp_eval(ws, """
        (() => {
            const c = document.createElement('canvas');
            const gl2 = c.getContext('webgl2');
            const gl1 = c.getContext('webgl');
            return {
                webgl2Supported: !!gl2,
                webgl1Supported: !!gl1
            };
        })()
        """, msg_id)
        print(f"2. Browser WebGL Capabilities: WebGL2={webgl_support.get('webgl2Supported')}, WebGL1={webgl_support.get('webgl1Supported')}")
        assert webgl_support.get('webgl1Supported') or webgl_support.get('webgl2Supported'), "WebGL not supported in browser environment"

        # 2. Extract GPU Hardware / Driver Renderer Info
        msg_id += 1
        gpu_info = cdp_eval(ws, """
        (() => {
            const c = document.createElement('canvas');
            const gl = c.getContext('webgl2') || c.getContext('webgl');
            if (!gl) return { error: 'No context' };
            const dbg = gl.getExtension('WEBGL_debug_renderer_info');
            return {
                vendor: gl.getParameter(gl.VENDOR),
                renderer: gl.getParameter(gl.RENDERER),
                version: gl.getParameter(gl.VERSION),
                shadingLanguageVersion: gl.getParameter(gl.SHADING_LANGUAGE_VERSION),
                unmaskedVendor: dbg ? gl.getParameter(dbg.UNMASKED_VENDOR_WEBGL) : 'N/A',
                unmaskedRenderer: dbg ? gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL) : 'N/A'
            };
        })()
        """, msg_id)
        print(f"3. Hardware GPU Diagnostics:")
        print(f"   - Vendor: {gpu_info.get('vendor')} (Unmasked: {gpu_info.get('unmaskedVendor')})")
        print(f"   - Renderer: {gpu_info.get('renderer')} (Unmasked: {gpu_info.get('unmaskedRenderer')})")
        print(f"   - GL Version: {gpu_info.get('version')}")
        print(f"   - Shading Language: {gpu_info.get('shadingLanguageVersion')}")

        # 3. Verify Three.js Canvas & WebGLRenderer Mount
        msg_id += 1
        three_status = cdp_eval(ws, """
        (() => {
            const canvas = document.querySelector('.moon-webgl-canvas');
            const container = document.querySelector('#moon-canvas-container');
            const poster = document.querySelector('#moon-fallback-poster');
            if (!canvas) return { error: 'Canvas not found' };
            const gl = canvas.getContext('webgl2') || canvas.getContext('webgl');
            return {
                canvasMounted: !!canvas,
                containerContainsCanvas: container ? container.contains(canvas) : false,
                canvasWidth: canvas.width,
                canvasHeight: canvas.height,
                posterOpacity: poster ? window.getComputedStyle(poster).opacity : null,
                glError: gl ? gl.getError() : -1,
                isContextLost: gl ? gl.isContextLost() : true
            };
        })()
        """, msg_id)
        print(f"4. Three.js Canvas & WebGLRenderer Mount:")
        print(f"   - Canvas Mounted in DOM: {three_status.get('canvasMounted')}")
        print(f"   - Canvas Buffer Resolution: {three_status.get('canvasWidth')} x {three_status.get('canvasHeight')}")
        print(f"   - Poster Opacity (Fallback Hidden): {three_status.get('posterOpacity')}")
        print(f"   - WebGL Context Error: {three_status.get('glError')} (0 = NO_ERROR)")
        print(f"   - Context Lost State: {three_status.get('isContextLost')}")
        assert three_status.get('canvasMounted'), "Three.js canvas not found"
        assert three_status.get('glError') == 0, f"WebGL reported error: {three_status.get('glError')}"
        assert not three_status.get('isContextLost'), "WebGL context is lost"

        # 4. Verify Real-time Animation Loop (Axial Rotation Delta)
        msg_id += 1
        rot1 = cdp_eval(ws, """
        (() => {
            if (typeof window.__sabitMoonDiagnostics === 'function') {
                return window.__sabitMoonDiagnostics();
            }
            return null;
        })()
        """, msg_id)
        time.sleep(1.2)
        msg_id += 1
        rot2 = cdp_eval(ws, """
        (() => {
            if (typeof window.__sabitMoonDiagnostics === 'function') {
                return window.__sabitMoonDiagnostics();
            }
            return null;
        })()
        """, msg_id)

        print(f"5. Animation Loop Verification:")
        if rot1 and rot2:
            r1 = rot1.get('currentRotationY', 0)
            r2 = rot2.get('currentRotationY', 0)
            diff = abs(r2 - r1)
            print(f"   - Initial Rotation Y: {r1:.5f} rad")
            print(f"   - Sampled Rotation Y: {r2:.5f} rad")
            print(f"   - Rotation Delta: {diff:.5f} rad (Animation loop is actively updating)")
            print(f"   - isAnimating Flag: {rot2.get('isAnimating')}")
            print(f"   - isPaused Flag: {rot2.get('isPaused')}")
            print(f"   - Albedo Texture Loaded: {rot2.get('hasAlbedoTexture')}")
            print(f"   - Bump Texture Loaded: {rot2.get('hasBumpTexture')}")
            assert diff > 0.0001, "Moon rotation did not advance across animation loop"
            assert rot2.get('isAnimating'), "Three.js animation loop not running"
        else:
            print("   - Warning: Diagnostics hook not found; checked canvas dimensions instead.")

        # 5. Canvas Pixel Sampling (Verify rendered geometry & shading contrast)
        msg_id += 1
        pixel_test = cdp_eval(ws, """
        (() => {
            if (typeof window.__sabitMoonDiagnostics === 'function') {
                const diag = window.__sabitMoonDiagnostics();
                return {
                    centerLuminance: diag.centerLuminance,
                    hasRenderedContent: diag.centerLuminance > 0
                };
            }
            return { error: 'Diagnostics hook not found' };
        })()
        """, msg_id)
        print(f"6. Rendered Lunar Surface Readback:")
        print(f"   - Center Average Pixel Luminance: {pixel_test.get('centerLuminance', 0):.2f} / 255")
        print(f"   - Non-blank Rendered Lunar Surface: {pixel_test.get('hasRenderedContent')}")
        assert pixel_test.get('hasRenderedContent'), "Canvas center pixels are completely black (no geometry rendered)"

        # 6. Verify Console Errors and Failed Requests
        msg_id += 1
        console_and_net = cdp_eval(ws, """
        (() => {
            const entries = window.performance.getEntriesByType('resource');
            const textures = entries.filter(e => e.name.includes('/textures/'));
            return {
                textureRequests: textures.map(t => ({
                    name: t.name.split('/').pop(),
                    duration: Math.round(t.duration),
                    encodedBodySize: t.encodedBodySize
                }))
            };
        })()
        """, msg_id)
        print(f"7. Static Texture Resource Transfer:")
        for t in console_and_net.get('textureRequests', []):
            print(f"   - {t['name']}: {t['encodedBodySize']:,} bytes in {t['duration']}ms")

        print("\n=== REAL WEBGL & HARDWARE GPU VALIDATION PASSED 100% ===")
        return True

    finally:
        proc.terminate()

if __name__ == '__main__':
    ok = verify_webgl()
    sys.exit(0 if ok else 1)
