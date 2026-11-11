// Celestial Keplerian Orbital Background Canvas
// Adapted from astronomical Keplerian helical mechanics (orbital-hero-section)
// Optimized for Sabit: Runs gently in the Hero background behind the 3D Moon.

export interface Planet {
  name: string;
  a: number;       // Semi-major axis (AU)
  e: number;       // Eccentricity
  i: number;       // Inclination (degrees)
  node: number;    // Longitude of ascending node (degrees)
  peri: number;    // Argument of perihelion (degrees)
  M0: number;      // Mean anomaly at J2000 (degrees)
  color: string;
  size: number;
  glow?: number;
}

export const SOLAR_BODIES: Planet[] = [
  { name: "Mercury", a: 0.387, e: 0.205, i: 7.0, node: 48.3, peri: 29.1, M0: 174.8, color: "#e2d5c3", size: 1.8 },
  { name: "Venus",   a: 0.723, e: 0.007, i: 3.4, node: 76.7, peri: 54.8, M0: 50.1,  color: "#d8be92", size: 2.8 },
  { name: "Earth",   a: 1.000, e: 0.017, i: 0.0, node: 348.7, peri: 114.2, M0: 357.5, color: "#74b9ff", size: 3.0, glow: 1.2 },
  { name: "Mars",    a: 1.524, e: 0.093, i: 1.8, node: 49.5, peri: 286.5, M0: 19.4,  color: "#e17055", size: 2.2 },
  { name: "Jupiter", a: 5.203, e: 0.048, i: 1.3, node: 100.5, peri: 273.9, M0: 20.0,  color: "#e8b87d", size: 4.2 },
  { name: "Saturn",  a: 9.537, e: 0.054, i: 2.5, node: 113.7, peri: 339.4, M0: 317.0, color: "#ffeaa7", size: 3.8 }
];

const TAU = Math.PI * 2;
const RAD = Math.PI / 180;

function parseRGB(color: string): [number, number, number] {
  const c = color.trim();
  if (c.startsWith("#")) {
    const hex = c.slice(1);
    const full = hex.length === 3 ? hex[0] + hex[0] + hex[1] + hex[1] + hex[2] + hex[2] : hex.slice(0, 6);
    const n = parseInt(full, 16);
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
  }
  return [255, 255, 255];
}

function mulberry32(seed: number) {
  let t = seed >>> 0;
  return () => {
    t += 0x6d2b79f5;
    let x = t;
    x = Math.imul(x ^ (x >>> 15), x | 1);
    x ^= x + Math.imul(x ^ (x >>> 7), x | 61);
    return ((x ^ (x >>> 14)) >>> 0) / 4294967296;
  };
}

function solveKepler(M: number, e: number): number {
  let m = M % TAU;
  if (m < 0) m += TAU;
  let E = m + e * Math.sin(m) * (1 + e * Math.cos(m));
  for (let k = 0; k < 6; k++) {
    const step = (E - e * Math.sin(E) - m) / (1 - e * Math.cos(E));
    E -= step;
    if (Math.abs(step) < 1e-8) break;
  }
  return E;
}

export class OrbitalBackground {
  private canvas: HTMLCanvasElement;
  private ctx: CanvasRenderingContext2D;
  private container: HTMLElement;
  private reqId: number | null = null;
  private isPaused = false;
  private isVisible = true;
  private isReducedMotion = false;

  private width = 0;
  private height = 0;
  private dpr = 1;
  private simTime = 0;

  // Starfield buffers
  private starCount = 450;
  private sx = new Float64Array(0);
  private sy = new Float64Array(0);
  private sz = new Float64Array(0);
  private sMag = new Float64Array(0);
  private sPhase = new Float64Array(0);

  // Resize and Intersection Observers
  private resizeObserver: ResizeObserver | null = null;
  private intersectionObserver: IntersectionObserver | null = null;

  constructor(container: HTMLElement, canvas: HTMLCanvasElement) {
    this.container = container;
    this.canvas = canvas;
    const ctx = canvas.getContext("2d", { alpha: false });
    if (!ctx) throw new Error("Could not acquire 2D context");
    this.ctx = ctx;

    this.checkReducedMotion();
    this.init();
  }

  private checkReducedMotion(): void {
    const media = window.matchMedia("(prefers-reduced-motion: reduce)");
    this.isReducedMotion = media.matches;
    media.addEventListener("change", e => {
      this.isReducedMotion = e.matches;
    });
  }

  private init(): void {
    this.resize();

    this.resizeObserver = new ResizeObserver(() => {
      this.resize();
    });
    this.resizeObserver.observe(this.container);

    this.intersectionObserver = new IntersectionObserver(
      entries => {
        for (const entry of entries) {
          this.isVisible = entry.isIntersecting;
          if (this.isVisible && !this.isPaused && !this.reqId) {
            this.start();
          }
        }
      },
      { threshold: 0.05 }
    );
    this.intersectionObserver.observe(this.container);

    document.addEventListener("visibilitychange", () => {
      if (document.hidden) {
        this.stop();
      } else if (this.isVisible && !this.isPaused) {
        this.start();
      }
    });

    this.start();
  }

  private resize(): void {
    const rect = this.container.getBoundingClientRect();
    this.width = Math.max(1, rect.width);
    this.height = Math.max(1, rect.height);
    this.dpr = Math.min(window.devicePixelRatio || 1, 1.5);

    this.canvas.width = Math.round(this.width * this.dpr);
    this.canvas.height = Math.round(this.height * this.dpr);
    this.ctx.setTransform(this.dpr, 0, 0, this.dpr, 0, 0);

    this.buildStars();
    if (!this.reqId) {
      this.render(this.simTime);
    }
  }

  private buildStars(): void {
    const rng = mulberry32(0x1337beef);
    const count = Math.min(600, Math.round(this.starCount * (this.width / 1200)));
    this.sx = new Float64Array(count);
    this.sy = new Float64Array(count);
    this.sz = new Float64Array(count);
    this.sMag = new Float64Array(count);
    this.sPhase = new Float64Array(count);

    for (let i = 0; i < count; i++) {
      this.sx[i] = (rng() - 0.5) * this.width * 1.5;
      this.sy[i] = (rng() - 0.5) * this.height * 1.5;
      this.sz[i] = 100 + rng() * 1200;
      this.sMag[i] = 0.4 + Math.pow(rng(), 2.2) * 1.6;
      this.sPhase[i] = rng() * TAU;
    }
  }

  public setPaused(paused: boolean): void {
    this.isPaused = paused;
    if (!paused && this.isVisible && !this.reqId) {
      this.start();
    }
  }

  public start(): void {
    if (this.reqId !== null) return;
    let lastStamp = performance.now();

    const loop = (now: number) => {
      const delta = (now - lastStamp) / 1000;
      lastStamp = now;

      if (!this.isPaused && !this.isReducedMotion) {
        // Slow celestial progression
        this.simTime += Math.min(delta, 0.1) * 0.12;
      }

      this.render(this.simTime);

      if (this.isVisible && !this.isPaused && !document.hidden && !this.isReducedMotion) {
        this.reqId = requestAnimationFrame(loop);
      } else {
        this.reqId = null;
      }
    };

    this.reqId = requestAnimationFrame(loop);
  }

  public stop(): void {
    if (this.reqId !== null) {
      cancelAnimationFrame(this.reqId);
      this.reqId = null;
    }
  }

  private render(t: number): void {
    const ctx = this.ctx;
    const w = this.width;
    const h = this.height;

    // 1. Deep Space Midnight Navy Base
    ctx.globalCompositeOperation = "source-over";
    ctx.fillStyle = "#090E16";
    ctx.fillRect(0, 0, w, h);

    // 2. Stars with Parallax and Gentle Twinkle
    ctx.globalCompositeOperation = "lighter";
    const cx = w * 0.72; // Focus center near right Moon area
    const cy = h * 0.46;

    for (let i = 0; i < this.sx.length; i++) {
      const z = this.sz[i];
      const scale = 400 / z;
      const x = cx + this.sx[i] * scale;
      const y = cy + this.sy[i] * scale;

      if (x < 0 || x > w || y < 0 || y > h) continue;

      const twinkle = 0.75 + 0.25 * Math.sin(t * 3.5 + this.sPhase[i]);
      const brightness = Math.min(1.0, (this.sMag[i] * scale * 1.5 * twinkle));
      const size = Math.max(0.6, Math.min(2.0, this.sMag[i] * scale * 1.8));

      ctx.fillStyle = `rgba(215, 230, 255, ${brightness.toFixed(3)})`;
      ctx.fillRect(x, y, size, size);
    }

    // 3. Subtle Keplerian Planetary Orbital Tracks (Helical & Elliptical Wakes)
    ctx.lineWidth = 1.0;
    const trailSegments = 70;
    const orbitScale = Math.min(w, h) * 0.38;

    for (let pIdx = 0; pIdx < SOLAR_BODIES.length; pIdx++) {
      const planet = SOLAR_BODIES[pIdx];
      const [r, g, b] = parseRGB(planet.color);
      const a = Math.pow(planet.a, 0.42); // compressed spacing
      const period = Math.pow(a, 1.5);
      const meanMotion = TAU / period;
      const inc = (planet.i + 14) * RAD;
      const node = (planet.node + 28) * RAD;

      ctx.beginPath();
      let first = true;

      for (let s = 0; s <= trailSegments; s++) {
        const past = (s / trailSegments) * 0.95; // past trail
        const curM = (planet.M0 * RAD) + meanMotion * (t - past * 1.8);
        const E = solveKepler(curM, planet.e);

        // Orbital plane coords
        const xo = a * (Math.cos(E) - planet.e) * orbitScale;
        const yo = a * Math.sqrt(1 - planet.e * planet.e) * Math.sin(E) * orbitScale;

        // Rotate by node and inclination
        const xRot = xo * Math.cos(node) - yo * Math.sin(node) * Math.cos(inc);
        const yRot = (xo * Math.sin(node) + yo * Math.cos(node) * Math.cos(inc)) * 0.52; // isometric tilt

        // Gentle drift along solar course
        const driftX = past * 70;
        const driftY = -past * 35;

        const screenX = cx + xRot + driftX;
        const screenY = cy + yRot + driftY;

        if (first) {
          ctx.moveTo(screenX, screenY);
          first = false;
        } else {
          ctx.lineTo(screenX, screenY);
        }
      }

      // Fading wake gradient
      const alpha = (0.16 + (pIdx === 2 ? 0.12 : 0)).toFixed(3);
      ctx.strokeStyle = `rgba(${r}, ${g}, ${b}, ${alpha})`;
      ctx.stroke();

      // Current planet position dot
      const curM = (planet.M0 * RAD) + meanMotion * t;
      const E = solveKepler(curM, planet.e);
      const xo = a * (Math.cos(E) - planet.e) * orbitScale;
      const yo = a * Math.sqrt(1 - planet.e * planet.e) * Math.sin(E) * orbitScale;
      const px = cx + (xo * Math.cos(node) - yo * Math.sin(node) * Math.cos(inc));
      const py = cy + (xo * Math.sin(node) + yo * Math.cos(node) * Math.cos(inc)) * 0.52;

      ctx.fillStyle = `rgba(${r}, ${g}, ${b}, 0.85)`;
      ctx.beginPath();
      ctx.arc(px, py, planet.size * 0.65, 0, TAU);
      ctx.fill();
    }

    // 4. Left-Side Scrim Gradient for 100% Text & CTA Readability
    // Ensures text on the left is crisp against dark navy, while the celestial canvas shines on the right
    ctx.globalCompositeOperation = "source-over";
    const scrim = ctx.createLinearGradient(0, 0, w, 0);
    scrim.addColorStop(0, "rgba(9, 14, 22, 0.94)");
    scrim.addColorStop(0.38, "rgba(10, 16, 25, 0.86)");
    scrim.addColorStop(0.68, "rgba(12, 18, 26, 0.35)");
    scrim.addColorStop(1, "rgba(12, 18, 26, 0.0)");

    ctx.fillStyle = scrim;
    ctx.fillRect(0, 0, w, h);
  }

  public destroy(): void {
    this.stop();
    this.resizeObserver?.disconnect();
    this.intersectionObserver?.disconnect();
  }
}
