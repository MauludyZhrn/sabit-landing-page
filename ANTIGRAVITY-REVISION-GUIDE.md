# Sabit — Antigravity Visual Revision Guide

## 1. Audit singkat dari dua screenshot

### Kondisi landing page saat ini
- Struktur hero masih terasa seperti blok dua kolom statis: teks di kiri, objek Bulan di kanan, dengan pemisahan yang terlalu tegas.
- Background grid dan navigasi membantu memberi kesan teknis, tetapi belum membangun ruang/atmosfer yang menyatu dengan objek utama.
- Bulan memang besar, tetapi teksturnya tampak seperti pola bercabang/urat dan ada area hitam seperti lubang yang tidak meyakinkan sebagai kawah lunar. Ini membuat objek terlihat seperti bola dengan tekstur tempelan, bukan visualisasi 3D Bulan yang natural.
- Headline berbahasa Inggris sementara deskripsi berbahasa Indonesia; bahasa utama perlu konsisten.
- CTA dan metadata ada, tetapi motion, kedalaman visual, dan ritme antar-section belum terasa.

### Referensi visual kedua
Ambil prinsip berikut:
- satu objek planet besar sebagai pusat perhatian;
- kedalaman ruang melalui pencahayaan, orbit tipis, dan layer depan/belakang;
- framing yang rapih dan hierarki tipografi yang terkontrol;
- panel informasi ringkas yang melayang hanya saat membantu konteks;
- komposisi asimetris, tidak semuanya disusun sebagai kartu yang sama.

Jangan menyalin brand NOVA, avatar, menu dashboard, angka statistik fiktif, atau layout dashboard misi. Sabit adalah website perangkat lunak observasi hilal, bukan game eksplorasi luar angkasa.

## 2. Art direction yang dituju

**Nama arah:** Scientific Observatory / Lunar Editorial.

- Warna dasar: midnight navy dan charcoal, dengan teks off-white.
- Aksen: lunar brass/gold secukupnya; jangan semua tombol dan garis berwarna emas.
- Material: matte, kontras terukur, garis orbit tipis, transparansi hanya pada 1–2 panel penting.
- Tipografi: sans-serif modern yang mudah dibaca; monospace hanya untuk metadata teknis; heading besar tetapi tidak memenuhi seluruh layar.
- Hero: objek Bulan 3D memiliki ruang sendiri tetapi terhubung ke headline lewat orbit/garis koordinat yang halus.
- Section setelah hero harus berganti ritme: editorial split, diagram alur, screenshot besar, spesifikasi ringkas, kemudian roadmap.
- Gunakan screenshot aplikasi Sabit asli. Jangan menggambar ulang dashboard aplikasi sebagai ilustrasi seolah-olah itu screenshot.

## 3. Spesifikasi Bulan 3D

### Sumber data/tekstur yang disarankan
NASA Scientific Visualization Studio menyediakan CGI Moon Kit yang dibuat dari data Lunar Reconnaissance Orbiter (LRO) dan Lunar Orbiter Laser Altimeter (LOLA):

- CGI Moon Kit: https://svs.gsfc.nasa.gov/4720/
- Global SVS: https://svs.gsfc.nasa.gov/
- NASA Lunar Surface in Color: https://science.nasa.gov/resource/lunar-surface-in-color/

Untuk web, gunakan aset yang ukurannya masuk akal, misalnya color map 2K dan elevation/displacement map ringan dari CGI Moon Kit. Hindari file EXR/TIFF ratusan MB hingga GB untuk halaman landing. Jika model GLB digunakan, ukur ukuran dan optimalkan sebelum shipping.

Credit yang aman untuk dicantumkan sesuai aset yang dipakai: `Lunar data: NASA/GSFC/Arizona State University — LRO/LOLA`. Ikuti kredit spesifik yang tercantum pada halaman aset.

### Kualitas visual minimum
- Bentuk sphere 3D nyata, bukan circle CSS.
- Albedo map memperlihatkan maria gelap, highlands cerah, dan variasi permukaan yang halus.
- Height/displacement atau normal/bump map menambah relief kawah dan topografi.
- Pencahayaan directional memperlihatkan terminator alami.
- Roughness tinggi, permukaan matte; jangan terlihat seperti plastik atau logam.
- Hindari pola urat bercabang, garis radial buatan, kawah hitam besar yang terkesan berlubang, dan noise berlebihan.
- Satu rotasi visual 120–180 detik; gerakan harus terasa hampir tidak disadari.
- Objek ini ilustrasi visual dekoratif, bukan sumber data hilal aktual.

Three.js `MeshStandardMaterial` mendukung albedo `map`, `normalMap`, `bumpMap`, dan `displacementMap`; pilih kombinasi sesuai aset yang benar-benar tersedia. Dokumentasi: https://threejs.org/docs/pages/MeshStandardMaterial.html

## 4. Motion language

| Bagian | Gerak yang disarankan | Hindari |
|---|---|---|
| Header | muncul halus, state aktif jelas | navbar melompat atau blur berat |
| Hero copy | reveal pendek, stagger kecil | tiap huruf bergerak sendiri |
| Bulan | slow rotation, sedikit parallax, drag opsional | spin cepat, mouse-follow ekstrem |
| Orbit | stroke draw saat entrance | starfield padat, garis neon |
| Feature sections | reveal sekali ketika masuk viewport | semua card terus bergerak |
| Diagram workflow | line draw/step highlight | diagram palsu atau tidak akurat |
| Screenshot | crop/zoom yang sangat halus | UI palsu sebagai screenshot |
| Roadmap | garis progres + milestone stagger | horizontal carousel yang sulit dibaca |
| Buttons | hover/focus/pressed responsif | glow, bounce, scale berlebihan |

Hormati `prefers-reduced-motion`, dan pastikan konten tidak tersembunyi jika JavaScript atau animasi tidak berjalan. GSAP ScrollTrigger docs: https://gsap.com/docs/v3/Plugins/ScrollTrigger/ ; reduced-motion: https://gsap.com/docs/v3/GSAP/gsap.matchMedia/.

## 5. Roadmap yang disarankan

Roadmap menjadi section besar terakhir sebelum footer. Labeli sebagai arah pengembangan, bukan jadwal yang dijanjikan.

**Baseline — Sabit Basic**
- Tampilkan sebagai kondisi saat ini hanya jika versi dan status rilis benar-benar cocok dengan project/release.

**Arah pengembangan potensial**
1. **Workflow pengolahan citra** — eksplorasi penyempurnaan tahap dan kontrol pemrosesan.
2. **Visualisasi pengamatan** — eksplorasi penyajian data dan evaluasi yang lebih informatif.
3. **Interoperabilitas perangkat** — evaluasi dukungan alur kerja perangkat observasi.
4. **Dokumentasi dan pelaporan** — penyempurnaan dokumentasi, ekspor, atau pelaporan bila menjadi kebutuhan penelitian/pengguna.

Gunakan status `Rencana`, `Eksplorasi`, atau `Evaluasi`. Jangan menambah tanggal, kuartal, persentase, atau klaim “sedang dikerjakan” tanpa konfirmasi. Catatan: `Roadmap ini merupakan arah pengembangan dan dapat berubah mengikuti evaluasi penelitian serta kebutuhan pengguna.`

## 6. Strategi link repository dan download

Website dapat menyediakan tiga link:
- Download installer Windows (.exe)
- GitHub repository (source code)
- GitHub Releases (riwayat dan installer)

### GitHub
Antigravity diminta membaca remote `origin` dari repo yang sedang dibuka. Jika origin menunjuk ke GitHub, gunakan URL aktual tersebut. Jangan menulis `OWNER/REPO` contoh sebagai URL hidup.

GitHub mendukung pola berikut untuk latest release:
- Halaman release terbaru: `https://github.com/OWNER/REPO/releases/latest`
- Unduh asset release terbaru: `https://github.com/OWNER/REPO/releases/latest/download/ASSET_FILENAME.exe`

Referensi resmi: https://docs.github.com/en/repositories/releasing-projects-on-github/linking-to-releases

### Installer .exe lokal
- Cari file .exe yang sudah disiapkan owner pada root project atau folder `release/`, `dist/`, `installer/`, `downloads/`.
- Cocokkan nama, versi, arsitektur dan ukuran.
- Untuk hosting langsung, letakkan salinan file di `public/downloads/` dan gunakan URL relatif `/downloads/NAMA-FILE.exe`.
- Jangan menggunakan path lokal Windows seperti `C:\Users\...` pada `href` website.
- Jangan commit binary besar ke GitHub source repo secara otomatis; gunakan asset GitHub Release jika memungkinkan.
- Jika belum ditemukan/diunggah, jangan buat link palsu. Simpan sebagai TODO atau sembunyikan CTA download.

Jika stack Vite, gunakan satu konfigurasi pusat dengan `VITE_SABIT_GITHUB_REPO_URL`, `VITE_SABIT_GITHUB_RELEASES_URL`, dan `VITE_SABIT_WINDOWS_INSTALLER_URL`. Untuk framework lain, adaptasikan pola tersebut.

## 7. Definisi selesai

Landing page baru dianggap selesai bila:
- Bulan tampak seperti Bulan, bukan bola dengan tekstur abstrak.
- Orbit, pencahayaan, dan gerak punya hubungan dengan pesan observasi hilal.
- Motion memperhalus alur, bukan menambah gangguan.
- Layout lebih dinamis dan punya variasi ritme tanpa menjadi dashboard.
- Roadmap terletak sebelum footer dan jelas dibedakan dari fitur aktif.
- GitHub/installer CTA tidak mengarah ke link kosong atau placeholder.
- Tampilan responsive, fallback WebGL, reduced-motion dan aksesibilitas telah diperiksa.
- Build berhasil dan semua temuan QA telah ditangani.
