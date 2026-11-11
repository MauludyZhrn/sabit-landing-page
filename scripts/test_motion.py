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
    print("=== SABIT PHASE 3 CINEMATIC SECTION & MOTION VALIDATION ===")

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

        # 3. Geometry Diagram & MABIMS Parameter Spotlight Keyboard & Click Test
        msg_id += 1
        geom_res = cdp_eval(ws, """
        (() => {
            const svg = document.querySelector('.ephemeris-svg');
            const paths = document.querySelectorAll('.draw-path');
            const criteria = document.querySelectorAll('.criteria-item');
            if (!svg || criteria.length < 3) return { error: 'Missing diagram elements' };

            // Simulate Keyboard Enter key on Parameter 2 (Elongation)
            criteria[1].dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
            const param2Active = criteria[1].classList.contains('active');
            const param2Selected = criteria[1].getAttribute('aria-selected') === 'true';

            // Simulate Click on Parameter 1 (Altitude)
            criteria[0].click();
            const param1Active = criteria[0].classList.contains('active');

            return {
                svgFound: !!svg,
                pathsCount: paths.length,
                param2Active,
                param2Selected,
                param1Active
            };
        })()
        """, msg_id)
        print("3. Geometry & MABIMS Interactive Parameter Spotlight:")
        print(f"   - SVG Diagram found with {geom_res.get('pathsCount')} draw paths")
        print(f"   - Keyboard Enter on Param 2 activated: {geom_res.get('param2Active')} (aria-selected: {geom_res.get('param2Selected')})")
        print(f"   - Click on Param 1 activated: {geom_res.get('param1Active')}")
        assert geom_res.get('param1Active'), "Criteria item click failed to activate"

        # 4. Application Showcase Hotspots & Bidirectional Card Sync
        msg_id += 1
        hotspots_res = cdp_eval(ws, """
        (() => {
            const hotspots = document.querySelectorAll('.ui-hotspot');
            const cards = document.querySelectorAll('.inspector-card');
            if (hotspots.length !== 5 || cards.length !== 5) {
                return { error: 'Expected 5 hotspots and 5 cards', hCount: hotspots.length, cCount: cards.length };
            }

            // Test clicking hotspot 2 (Ephemeris)
            hotspots[1].click();
            const card2Active = cards[1].classList.contains('active');
            const hotspot2Active = hotspots[1].classList.contains('active');

            // Test clicking card 4 (Toolbar)
            cards[3].click();
            const card4Active = cards[3].classList.contains('active');
            const hotspot4Active = hotspots[3].classList.contains('active');

            // Test Keyboard Enter on Hotspot 5 (Telemetry)
            hotspots[4].dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', bubbles: true }));
            const card5Active = cards[4].classList.contains('active');

            return {
                hotspotCount: hotspots.length,
                cardCount: cards.length,
                card2Active,
                hotspot2Active,
                card4Active,
                hotspot4Active,
                card5Active
            };
        })()
        """, msg_id)
        print("4. Application Showcase Hotspots & Bidirectional Sync:")
        print(f"   - Hotspots count: {hotspots_res.get('hotspotCount')}, Cards count: {hotspots_res.get('cardCount')}")
        print(f"   - Clicking Hotspot 2 activates Card 2: {hotspots_res.get('card2Active')}")
        print(f"   - Clicking Card 4 activates Hotspot 4: {hotspots_res.get('hotspot4Active')}")
        print(f"   - Keyboard Enter on Hotspot 5 activates Card 5: {hotspots_res.get('card5Active')}")
        assert hotspots_res.get('card2Active'), "Hotspot click failed to activate card"
        assert hotspots_res.get('hotspot4Active'), "Card click failed to activate hotspot"
        assert hotspots_res.get('card5Active'), "Keyboard activation failed"

        # 5. Interactive 3D Perspective Tilt on Application Showcase
        msg_id += 1
        tilt_res = cdp_eval(ws, """
        (async () => {
            const stage = document.querySelector('.screenshot-stage');
            if (!stage) return { error: 'No stage' };
            const rect = stage.getBoundingClientRect();
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
        print(f"   - Mouse Tilt Transform Applied: {tilt_res.get('transformApplied')}")

        # 6. Workflow Timeline Progress & Desktop Track
        msg_id += 1
        wf_res = cdp_eval(ws, """
        (() => {
            const desktopTrack = document.querySelector('.workflow-desktop-track');
            const desktopFill = document.querySelector('.workflow-desktop-fill');
            const steps = document.querySelectorAll('.workflow-step');
            return {
                desktopTrackExists: !!desktopTrack,
                desktopFillExists: !!desktopFill,
                stepsCount: steps.length
            };
        })()
        """, msg_id)
        print("5. Workflow Timeline Observation Journey:")
        print(f"   - Desktop progress track exists: {wf_res.get('desktopTrackExists')}")
        print(f"   - Desktop progress fill exists: {wf_res.get('desktopFillExists')}")
        print(f"   - Step cards count: {wf_res.get('stepsCount')}")
        assert wf_res.get('stepsCount') == 4, "Expected 4 workflow steps"

        # 7. Roadmap Section Branching Connector & Status Categories
        msg_id += 1
        roadmap_res = cdp_eval(ws, """
        (() => {
            const connectorSvg = document.querySelector('.roadmap-connector-svg');
            const stem = document.querySelector('.roadmap-stem-line');
            const spine = document.querySelector('.roadmap-spine-line');
            const branches = document.querySelectorAll('.roadmap-branch-line');
            const baseline = document.querySelector('.roadmap-baseline-card');
            const cards = document.querySelectorAll('.roadmap-item-card');
            const currentBadge = document.querySelector('.badge-status-current');
            const exploreBadges = document.querySelectorAll('.badge-status-explore');
            const evalBadge = document.querySelector('.badge-status-eval');
            const planBadge = document.querySelector('.badge-status-plan');

            return {
                connectorExists: !!connectorSvg,
                stemExists: !!stem,
                spineExists: !!spine,
                branchesCount: branches.length,
                baselineExists: !!baseline,
                cardsCount: cards.length,
                hasCurrentStatus: currentBadge ? currentBadge.textContent.trim() : null,
                exploreCount: exploreBadges.length,
                hasEvalStatus: evalBadge ? evalBadge.textContent.trim() : null,
                hasPlanStatus: planBadge ? planBadge.textContent.trim() : null
            };
        })()
        """, msg_id)
        print("6. Research Roadmap Cinematic Spine & Badges:")
        print(f"   - Branching connector SVG exists: {roadmap_res.get('connectorExists')} ({roadmap_res.get('branchesCount')} branch lines)")
        print(f"   - Baseline card exists: {roadmap_res.get('baselineExists')}")
        print(f"   - Current status badge: '{roadmap_res.get('hasCurrentStatus')}'")
        print(f"   - Exploration areas count: {roadmap_res.get('cardsCount')} (Eksplorasi: {roadmap_res.get('exploreCount')}, Evaluasi: '{roadmap_res.get('hasEvalStatus')}', Rencana: '{roadmap_res.get('hasPlanStatus')}')")
        assert roadmap_res.get('baselineExists'), "Missing roadmap baseline card"
        assert roadmap_res.get('cardsCount') == 4, "Expected 4 roadmap exploration cards"

        # 8. Technical Foundation & Verified Downloads Integrity
        msg_id += 1
        tech_res = cdp_eval(ws, """
        (() => {
            const installerBtn = document.querySelector('[data-action="download-installer"]');
            const repoBtn = document.querySelector('[data-action="github-repo"]');
            const techItems = document.querySelectorAll('.tech-item');
            return {
                techItemsCount: techItems.length,
                installerHref: installerBtn ? installerBtn.getAttribute('href') : null,
                repoHref: repoBtn ? repoBtn.getAttribute('href') : null,
                repoTarget: repoBtn ? repoBtn.getAttribute('target') : null
            };
        })()
        """, msg_id)
        print("7. Technical Foundation & Download Integrity:")
        print(f"   - Tech items count: {tech_res.get('techItemsCount')}")
        print(f"   - Windows Installer URL: {tech_res.get('installerHref')}")
        print(f"   - Sabit Application Repo URL: {tech_res.get('repoHref')} (target: {tech_res.get('repoTarget')})")
        assert "Sabit-Setup" in str(tech_res.get('installerHref')), "Installer URL invalid"
        assert tech_res.get('repoHref') == "https://github.com/MauludyZhrn/Sabit", "Application repo URL invalid"

        # 9. Scroll-Triggered Progress & Visual Evidence Captures for All 6 Sections
        scroll_targets = [
            ("scroll_hero", 0, "Hero & Celestial Orbital Space"),
            ("scroll_geometri", 700, "Geometri Pengamatan & Kriteria MABIMS"),
            ("scroll_evidence", 1500, "Antarmuka Aplikasi Sabit Desktop"),
            ("scroll_workflow", 2300, "Alur Kerja Sabit Basic"),
            ("scroll_roadmap", 3200, "Arah Riset & Roadmap"),
            ("scroll_teknis", 4000, "Spesifikasi Teknis & Download")
        ]

        print("8. Scroll-Triggered Progress & Visual Evidence Captures:")
        for name, scroll_y, label in scroll_targets:
            msg_id += 1
            cdp_eval(ws, f"window.scrollTo({{ top: {scroll_y}, behavior: 'instant' }});", msg_id)
            time.sleep(0.8)
            msg_id += 1
            out_file = f"artifacts/screenshots/{name}.png"
            cdp_screenshot(ws, out_file, msg_id)
            size = os.path.getsize(out_file) if os.path.exists(out_file) else 0
            print(f"   - Captured {label} (y={scroll_y}px) -> {name}.png ({size:,} bytes)")

        # 10. Prefers-Reduced-Motion Emulation
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
        print(f"9. Reduced Motion Media Query Active: {reduced_test}")
        assert reduced_test, "prefers-reduced-motion media feature was not emulated properly"

        # 11. Window Resize Resilience to Tablet & Mobile
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
        print(f"10. Viewport Resize to Tablet 768px: canvasWidth={resize_test.get('canvasWidth')}, headerVisible={resize_test.get('headerVisible')}")
        assert resize_test.get('headerVisible'), "Header is not visible after resize"

        print("\n=== ALL CINEMATIC SECTION & MOTION INTERACTION TESTS PASSED ===")
        return True

    finally:
        proc.terminate()

if __name__ == '__main__':
    ok = test_motion()
    sys.exit(0 if ok else 1)
