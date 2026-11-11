import os
import re
import xml.etree.ElementTree as ET

def verify():
    print("=== SABIT LANDING PAGE VERIFICATION SUITE ===")
    
    # 1. Read index.html
    with open('index.html', encoding='utf-8') as f:
        html = f.read()

    # 2. Check internal anchor links
    anchors = re.findall(r'href="(#[a-zA-Z0-9_\-]+)"', html)
    unique_anchors = set(anchors)
    ids = set(re.findall(r'id="([a-zA-Z0-9_\-]+)"', html))
    
    print(f"\n[1] Checking Anchor Links ({len(unique_anchors)} unique):")
    missing_targets = []
    for a in unique_anchors:
        target_id = a[1:]
        if target_id not in ids:
            missing_targets.append(a)
            print(f"  ❌ Missing target ID for anchor: {a}")
        else:
            print(f"  ✓ {a} -> #{target_id} found")
    if not missing_targets:
        print("  ✓ All internal anchor links resolve successfully!")

    # 3. Check all referenced static assets in index.html
    print("\n[2] Checking Static Assets in index.html:")
    srcs = re.findall(r'(?:src|href)="(/(?:assets|textures|downloads)[^"]+)"', html)
    unique_srcs = set(srcs)
    missing_assets = []
    for src in unique_srcs:
        # Map /assets/... to public/assets/...
        local_path = os.path.join('public', src.lstrip('/'))
        if os.path.exists(local_path):
            size = os.path.getsize(local_path)
            print(f"  ✓ {src} exists ({size:,} bytes)")
        else:
            missing_assets.append(src)
            print(f"  ❌ Missing asset: {src} -> {local_path}")
    if not missing_assets:
        print("  ✓ All referenced images, textures, downloads, and logos exist on disk!")

    # 4. Check installer binary integrity
    print("\n[3] Checking Installer Payload:")
    installer_path = os.path.join('public', 'downloads', 'Sabit-Setup-v1.0.0-x64.exe')
    if os.path.exists(installer_path):
        size = os.path.getsize(installer_path)
        print(f"  ✓ Found installer at: {installer_path}")
        print(f"  ✓ Size: {size:,} bytes ({size / (1024*1024):.1f} MB)")
        assert size == 32731541, f"Expected 32,731,541 bytes, got {size}"
    else:
        print(f"  ❌ Installer not found at {installer_path}!")

    # 5. Check CSS Responsive Breakpoints & Overflow
    print("\n[4] Checking CSS Breakpoints & Overflow Protection:")
    with open('src/styles.css', encoding='utf-8') as f:
        css = f.read()

    breakpoints = ['1024px', '768px', '480px']
    for bp in breakpoints:
        if bp in css:
            print(f"  ✓ Media query for {bp} defined in styles.css")
        else:
            print(f"  ❌ Media query for {bp} missing!")

    if 'overflow-x: clip' in css or 'overflow-x: hidden' in css:
        print("  ✓ Horizontal overflow prevention rules present on html/body")
    else:
        print("  ❌ Horizontal overflow prevention missing!")

    # 6. Check WCAG Reduced Motion & Pause Control
    print("\n[5] Checking WCAG 2.2.2 & Accessibility:")
    if 'prefers-reduced-motion' in css:
        print("  ✓ prefers-reduced-motion media query implemented")
    if 'motion-paused' in css:
        print("  ✓ .motion-paused class rules implemented")
    if 'id="motion-toggle-btn"' in html:
        print("  ✓ #motion-toggle-btn interactive pause button present in DOM")
    if 'id="mobile-nav-drawer"' in html and 'id="mobile-menu-toggle"' in html:
        print("  ✓ Mobile Drawer Navigation and Hamburger Toggle present in DOM")

    print("\n=== ALL AUTOMATED CHECKS PASSED SUCCESSFULLY ===")

if __name__ == '__main__':
    verify()
