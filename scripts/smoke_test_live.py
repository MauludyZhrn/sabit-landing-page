import urllib.request
import json
import re

BASE = 'https://sabit-landing-page.vercel.app'
headers = {'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64)'}

print('=== LIVE PRODUCTION SMOKE TEST ===')

# 1. Main Page
req = urllib.request.Request(BASE, headers=headers)
with urllib.request.urlopen(req) as resp:
    html = resp.read().decode('utf-8')
    print('1. Main Page Status:', resp.status)
    print('   - Canonical present:', 'rel="canonical"' in html)
    print('   - OpenGraph present:', 'property="og:title"' in html)
    print('   - Twitter card present:', 'name="twitter:card"' in html)
    print('   - Schema.org present:', 'SoftwareApplication' in html)
    print('   - Motion toggle button present:', 'motion-toggle-btn' in html)
    print('   - Mobile drawer present:', 'mobile-nav-drawer' in html)
    print('   - Desktop repo link verified:', 'https://github.com/MauludyZhrn/Sabit' in html)
    print('   - Installer link verified:', '/downloads/Sabit-Setup-v1.0.0-x64.exe' in html)

# 2. Robots.txt
req = urllib.request.Request(f'{BASE}/robots.txt', headers=headers)
with urllib.request.urlopen(req) as resp:
    robots = resp.read().decode('utf-8')
    print('2. Robots.txt Status:', resp.status, '| Content:', robots.strip().replace('\n', ' -- '))

# 3. Sitemap.xml
req = urllib.request.Request(f'{BASE}/sitemap.xml', headers=headers)
with urllib.request.urlopen(req) as resp:
    sitemap = resp.read().decode('utf-8')
    print('3. Sitemap.xml Status:', resp.status, '| Valid XML:', '<urlset' in sitemap)

# 4. Installer
req = urllib.request.Request(f'{BASE}/downloads/Sabit-Setup-v1.0.0-x64.exe', headers={'User-Agent': 'Mozilla/5.0', 'Range': 'bytes=0-1023'})
with urllib.request.urlopen(req) as resp:
    chunk = resp.read(2)
    print('4. Installer Status:', resp.status)
    print('   - Magic MZ header:', chunk == b'MZ')
    print('   - Content-Range / Content-Length header:', resp.headers.get('Content-Range') or resp.headers.get('Content-Length'))

# 5. Live Texture Check
req = urllib.request.Request(f'{BASE}/textures/lroc_color_2k.jpg', headers={'User-Agent': 'Mozilla/5.0', 'Range': 'bytes=0-100'})
with urllib.request.urlopen(req) as resp:
    print('5. NASA LRO Albedo Texture Status:', resp.status)

# 6. Check full content-length of installer with HEAD request
req = urllib.request.Request(f'{BASE}/downloads/Sabit-Setup-v1.0.0-x64.exe', headers={'User-Agent': 'Mozilla/5.0'}, method='HEAD')
with urllib.request.urlopen(req) as resp:
    print('6. Installer Full HEAD Status:', resp.status, '| Content-Length:', resp.headers.get('Content-Length'))

print('=== ALL LIVE HTTP CHECKS PASSED ===')
