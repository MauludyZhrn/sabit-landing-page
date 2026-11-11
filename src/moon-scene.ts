// Sabit 3D Moon Centerpiece — Three.js Implementation (Optimized Phase 4)
// Powered by official NASA CGI Moon Kit (LRO color map + LOLA elevation map)
// Features: Grazing solar key light (~72° terminator), Earthshine directional fill,
// celestial limb rim light, regolith roughness, adaptive DPR/geometry,
// intersection-based GPU pausing, throttled telemetry HUD, and WebGL context-loss resilience.

import * as THREE from 'three';

export interface MoonSceneOptions {
  container: HTMLElement;
  albedoUrl?: string;
  bumpUrl?: string;
  posterElement?: HTMLElement | null;
  coordsElement?: HTMLElement | null;
  onReady?: () => void;
  onError?: (err: Error) => void;
}

export class MoonScene {
  private container: HTMLElement;
  private posterElement: HTMLElement | null;
  private coordsElement: HTMLElement | null;
  private renderer: THREE.WebGLRenderer | null = null;
  private scene: THREE.Scene | null = null;
  private camera: THREE.PerspectiveCamera | null = null;
  private moonMesh: THREE.Mesh | null = null;
  private sphereGeometry: THREE.SphereGeometry | null = null;
  private moonMaterial: THREE.MeshStandardMaterial | null = null;
  private albedoTexture: THREE.Texture | null = null;
  private bumpTexture: THREE.Texture | null = null;

  private reqId: number | null = null;
  private isVisible = true;
  private isPaused = false;
  private isReducedMotion = false;
  private resizeObserver: ResizeObserver | null = null;
  private intersectionObserver: IntersectionObserver | null = null;
  private resizeRafId: number | null = null;

  // Interaction physics & orientation
  private isDragging = false;
  private previousMousePosition = { x: 0, y: 0 };
  private targetRotationY = 0.20; // Selenographic near-side facing observer
  private targetRotationX = 0.04;
  private currentRotationY = 0.20;
  private currentRotationX = 0.04;
  private rotationVelocityY = 0;
  private rotationVelocityX = 0;

  // Mouse Parallax on Desktop
  private mouseParallaxX = 0;
  private mouseParallaxY = 0;
  private currentParallaxX = 0;
  private currentParallaxY = 0;

  // Scroll-linked rotation scrub
  private scrollProgress = 0;

  // Natural axial rotation: ~150 seconds per full revolution
  private baseRotationSpeed = 0.00035;

  // Throttled HUD update state
  private lastHudUpdate = 0;
  private lastLoggedCoords = '';

  private boundOnPointerDown: (e: PointerEvent) => void;
  private boundOnPointerMove: (e: PointerEvent) => void;
  private boundOnPointerUp: (e: PointerEvent) => void;
  private boundOnVisibilityChange: () => void;
  private boundOnReducedMotionChange: (e: MediaQueryListEvent) => void;
  private boundOnContextLost: ((e: Event) => void) | null = null;
  private boundOnContextRestored: (() => void) | null = null;

  constructor(options: MoonSceneOptions) {
    this.container = options.container;
    this.posterElement = options.posterElement || null;
    this.coordsElement = options.coordsElement || null;

    this.boundOnPointerDown = this.onPointerDown.bind(this);
    this.boundOnPointerMove = this.onPointerMove.bind(this);
    this.boundOnPointerUp = this.onPointerUp.bind(this);
    this.boundOnVisibilityChange = this.onVisibilityChange.bind(this);
    this.boundOnReducedMotionChange = this.onReducedMotionChange.bind(this);

    this.checkReducedMotion();
    this.init(options);
  }

  private static isWebGLAvailable(): boolean {
    try {
      const canvas = document.createElement('canvas');
      return !!(
        window.WebGLRenderingContext &&
        (canvas.getContext('webgl2') || canvas.getContext('webgl') || canvas.getContext('experimental-webgl'))
      );
    } catch {
      return false;
    }
  }

  private checkReducedMotion(): void {
    const media = window.matchMedia('(prefers-reduced-motion: reduce)');
    this.isReducedMotion = media.matches;
    if (media.addEventListener) {
      media.addEventListener('change', this.boundOnReducedMotionChange);
    }
  }

  private onReducedMotionChange(e: MediaQueryListEvent): void {
    this.isReducedMotion = e.matches;
  }

  private init(options: MoonSceneOptions): void {
    if (!MoonScene.isWebGLAvailable()) {
      console.warn('WebGL is unavailable; displaying high-resolution static NASA lunar poster fallback.');
      this.showFallback();
      options.onError?.(new Error('WebGL unsupported'));
      return;
    }

    try {
      // 1. Scene
      this.scene = new THREE.Scene();

      // 2. Camera with cinematic focal length
      const width = this.container.clientWidth || 500;
      const height = this.container.clientHeight || 500;
      this.camera = new THREE.PerspectiveCamera(36, width / height, 0.1, 100);
      this.camera.position.set(0, 0, 3.45);

      // 3. Renderer with adaptive DPR (Mobile: 1.0, Tablet: 1.25, Desktop: 1.5 max)
      const canvas = document.createElement('canvas');
      canvas.className = 'moon-webgl-canvas';
      canvas.setAttribute('aria-hidden', 'true');
      canvas.style.touchAction = 'pan-y'; // Allow native vertical scrolling while supporting horizontal swipe rotation

      const isMobile = window.innerWidth < 768;
      const isTablet = window.innerWidth >= 768 && window.innerWidth <= 1024;
      const dpr = isMobile ? 1.0 : (isTablet ? 1.25 : Math.min(window.devicePixelRatio || 1, 1.5));

      this.renderer = new THREE.WebGLRenderer({
        canvas,
        alpha: true,
        antialias: !isMobile,
        powerPreference: 'high-performance'
      });
      this.renderer.setPixelRatio(dpr);
      this.renderer.setSize(width, height);
      this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
      this.renderer.toneMappingExposure = 1.12;

      this.container.appendChild(this.renderer.domElement);

      // Context Loss & Restoration Management
      this.boundOnContextLost = (e: Event) => {
        e.preventDefault();
        console.warn('WebGL context lost: pausing animation loop and displaying static lunar fallback.');
        this.stopLoop();
        this.showFallback();
      };
      this.boundOnContextRestored = () => {
        console.info('WebGL context restored: re-initializing Three.js lunar scene.');
        this.init(options);
      };
      canvas.addEventListener('webglcontextlost', this.boundOnContextLost);
      canvas.addEventListener('webglcontextrestored', this.boundOnContextRestored);

      // 4. Scientifically Plausible 4-Light Configuration:
      // A. Grazing Key Sunlight (~72° terminator angle) from top-right for authentic crater relief
      const sunLight = new THREE.DirectionalLight(0xfffdf7, 2.70);
      sunLight.position.set(2.95, 1.05, 1.50);
      this.scene.add(sunLight);

      // B. Earthshine Directional Fill Light (cool, deep space slate-blue facing dark hemisphere)
      const earthshineLight = new THREE.DirectionalLight(0x132032, 0.45);
      earthshineLight.position.set(-1.8, -0.4, 2.0);
      this.scene.add(earthshineLight);

      // C. Deep Cosmic Space Ambient
      const spaceAmbient = new THREE.AmbientLight(0x070c14, 0.12);
      this.scene.add(spaceAmbient);

      // D. Delicate Celestial Limb Glance/Rim Light (subtle silhouetting on dark limb)
      const rimLight = new THREE.DirectionalLight(0x4a6a8c, 0.38);
      rimLight.position.set(-2.8, 0.6, -1.3);
      this.scene.add(rimLight);

      // 5. Geometry: Adaptive segment resolution (Mobile: 40, Tablet: 60, Desktop: 80)
      const segments = isMobile ? 40 : (isTablet ? 60 : 80);
      this.sphereGeometry = new THREE.SphereGeometry(1.0, segments, segments);

      // 6. NASA Textures Loading (LROC color map + LOLA elevation map) with Anisotropic Filtering
      const textureLoader = new THREE.TextureLoader();
      const albedoPath = options.albedoUrl || '/textures/lroc_color_2k.jpg';
      const bumpPath = options.bumpUrl || '/textures/ldem_3_8bit.jpg';

      const maxAnisotropy = Math.min(
        this.renderer.capabilities.getMaxAnisotropy(),
        isMobile ? 2 : (isTablet ? 4 : 16)
      );

      let loadedCount = 0;
      const onLoadCheck = () => {
        loadedCount++;
        if (loadedCount >= 2) {
          this.hideFallback();
          options.onReady?.();
        }
      };

      this.albedoTexture = textureLoader.load(albedoPath, onLoadCheck, undefined, () => {
        onLoadCheck();
      });
      this.albedoTexture.colorSpace = THREE.SRGBColorSpace;
      this.albedoTexture.generateMipmaps = true;
      this.albedoTexture.minFilter = THREE.LinearMipmapLinearFilter;
      this.albedoTexture.anisotropy = maxAnisotropy;

      this.bumpTexture = textureLoader.load(bumpPath, onLoadCheck, undefined, () => {
        onLoadCheck();
      });
      this.bumpTexture.generateMipmaps = true;
      this.bumpTexture.minFilter = THREE.LinearMipmapLinearFilter;
      this.bumpTexture.anisotropy = maxAnisotropy;

      // 7. Material: Physically grounded matte lunar regolith (diffuse retro-reflection)
      this.moonMaterial = new THREE.MeshStandardMaterial({
        map: this.albedoTexture,
        bumpMap: this.bumpTexture,
        bumpScale: isMobile ? 0.038 : 0.054,
        roughness: 0.95,
        metalness: 0.0,
        color: 0xffffff
      });

      this.moonMesh = new THREE.Mesh(this.sphereGeometry, this.moonMaterial);
      this.moonMesh.rotation.y = this.currentRotationY;
      this.moonMesh.rotation.x = this.currentRotationX;
      this.scene.add(this.moonMesh);

      // 8. Event Listeners for Drag-to-Rotate & Desktop Parallax
      this.renderer.domElement.addEventListener('pointerdown', this.boundOnPointerDown);
      window.addEventListener('pointermove', this.boundOnPointerMove, { passive: true });
      window.addEventListener('pointerup', this.boundOnPointerUp);
      document.addEventListener('visibilitychange', this.boundOnVisibilityChange);

      // 9. Debounced Resize Observer
      this.resizeObserver = new ResizeObserver(entries => {
        for (const entry of entries) {
          const { width: w, height: h } = entry.contentRect;
          if (w > 0 && h > 0) {
            if (this.resizeRafId) cancelAnimationFrame(this.resizeRafId);
            this.resizeRafId = requestAnimationFrame(() => {
              this.resize(w, h);
              this.resizeRafId = null;
            });
          }
        }
      });
      this.resizeObserver.observe(this.container);

      // 10. Intersection Observer (Halt render loop when scrolled offscreen to conserve GPU)
      this.intersectionObserver = new IntersectionObserver(
        entries => {
          for (const entry of entries) {
            this.isVisible = entry.isIntersecting;
            if (this.isVisible && !this.isPaused && !document.hidden) {
              this.startLoop();
            } else if (!this.isVisible) {
              this.stopLoop();
            }
          }
        },
        { threshold: 0.05 }
      );
      this.intersectionObserver.observe(this.container);

      this.startLoop();
      (window as any).__sabitMoonDiagnostics = () => this.getDiagnostics();
    } catch (err) {
      console.error('Three.js initialization failed:', err);
      this.showFallback();
      options.onError?.(err instanceof Error ? err : new Error(String(err)));
    }
  }

  private showFallback(): void {
    if (this.posterElement) {
      this.posterElement.style.opacity = '1';
      this.posterElement.style.pointerEvents = 'auto';
    }
  }

  private hideFallback(): void {
    if (this.posterElement) {
      this.posterElement.style.opacity = '0';
      this.posterElement.style.pointerEvents = 'none';
    }
  }

  public setPaused(paused: boolean): void {
    this.isPaused = paused;
    if (paused) {
      this.stopLoop();
    } else if (this.isVisible && !document.hidden) {
      this.startLoop();
    }
  }

  public getPaused(): boolean {
    return this.isPaused;
  }

  public setScrollProgress(progress: number): void {
    this.scrollProgress = Math.max(0, Math.min(1, progress));
  }

  private onVisibilityChange(): void {
    if (document.hidden) {
      this.stopLoop();
    } else if (this.isVisible && !this.isPaused) {
      this.startLoop();
    }
  }

  private onPointerDown(e: PointerEvent): void {
    if (e.pointerType === 'mouse' && e.button !== 0) return;
    this.isDragging = true;
    this.previousMousePosition = { x: e.clientX, y: e.clientY };
    if (this.renderer?.domElement) {
      this.renderer.domElement.style.cursor = 'grabbing';
      if ('setPointerCapture' in this.renderer.domElement) {
        try {
          this.renderer.domElement.setPointerCapture(e.pointerId);
        } catch {
          // ignore if unsupported
        }
      }
    }
  }

  private onPointerMove(e: PointerEvent): void {
    if (!this.isVisible) return;

    // 1. Desktop Mouse Parallax (only when not dragging and on wider screens)
    if (!this.isReducedMotion && window.innerWidth >= 768 && !this.isDragging) {
      const normX = (e.clientX / window.innerWidth) - 0.5;
      const normY = (e.clientY / window.innerHeight) - 0.5;
      this.mouseParallaxX = normX * 0.12;
      this.mouseParallaxY = normY * 0.08;
    }

    // 2. Drag rotation
    if (!this.isDragging) return;
    const deltaX = e.clientX - this.previousMousePosition.x;
    const deltaY = e.clientY - this.previousMousePosition.y;

    this.rotationVelocityY = deltaX * 0.0035;
    this.rotationVelocityX = deltaY * 0.0025;

    this.targetRotationY += this.rotationVelocityY;
    this.targetRotationX = Math.max(-0.35, Math.min(0.35, this.targetRotationX + this.rotationVelocityX));

    this.previousMousePosition = { x: e.clientX, y: e.clientY };
  }

  private onPointerUp(e?: PointerEvent): void {
    this.isDragging = false;
    if (this.renderer?.domElement) {
      this.renderer.domElement.style.cursor = 'grab';
      if (e && 'releasePointerCapture' in this.renderer.domElement) {
        try {
          this.renderer.domElement.releasePointerCapture(e.pointerId);
        } catch {
          // ignore
        }
      }
    }
  }

  private resize(width: number, height: number): void {
    if (!this.renderer || !this.camera) return;
    this.camera.aspect = width / height;
    this.camera.updateProjectionMatrix();
    this.renderer.setSize(width, height);
  }

  private startLoop(): void {
    if (this.reqId !== null) return;
    const animate = () => {
      this.render();
      if (this.isVisible && !this.isPaused && !document.hidden) {
        this.reqId = requestAnimationFrame(animate);
      } else {
        this.reqId = null;
      }
    };
    this.reqId = requestAnimationFrame(animate);
  }

  private stopLoop(): void {
    if (this.reqId !== null) {
      cancelAnimationFrame(this.reqId);
      this.reqId = null;
    }
  }

  private render(): void {
    if (!this.renderer || !this.scene || !this.camera || !this.moonMesh) return;

    if (!this.isReducedMotion && !this.isPaused) {
      // Natural slow axial rotation
      if (!this.isDragging) {
        this.targetRotationY += this.baseRotationSpeed;
        this.rotationVelocityY *= 0.92;
        this.rotationVelocityX *= 0.92;
      }

      // Smooth dampening to target rotation
      this.currentRotationY += (this.targetRotationY - this.currentRotationY) * 0.08;
      this.currentRotationX += (this.targetRotationX - this.currentRotationX) * 0.08;

      // Smooth mouse parallax damping
      this.currentParallaxX += (this.mouseParallaxX - this.currentParallaxX) * 0.05;
      this.currentParallaxY += (this.mouseParallaxY - this.currentParallaxY) * 0.05;

      // Apply rotation: base + drag + scroll-linked shift + parallax
      const scrollRotationShift = this.scrollProgress * 0.35;
      this.moonMesh.rotation.y = this.currentRotationY + scrollRotationShift + this.currentParallaxX;
      this.moonMesh.rotation.x = this.currentRotationX + this.currentParallaxY;

      // Subtle camera parallax tilt
      this.camera.position.x = this.currentParallaxX * 0.3;
      this.camera.position.y = -this.currentParallaxY * 0.3 - (this.scrollProgress * 0.15);
      this.camera.lookAt(0, 0, 0);

      // Throttled HUD update (~12Hz / 80ms)
      const now = performance.now();
      if (now - this.lastHudUpdate > 80) {
        this.updateLiveCoordinates();
        this.lastHudUpdate = now;
      }
    }

    this.renderer.render(this.scene, this.camera);
  }

  private updateLiveCoordinates(): void {
    if (!this.coordsElement || !this.moonMesh) return;

    // Convert rotation angles to selenographic sub-observer coordinates
    const degY = ((this.moonMesh.rotation.y * 180 / Math.PI) % 360 + 360) % 360;
    const degX = (this.moonMesh.rotation.x * 180 / Math.PI);

    let lonStr = '';
    if (degY <= 180) {
      lonStr = `${degY.toFixed(1)}° E`;
    } else {
      lonStr = `${(360 - degY).toFixed(1)}° W`;
    }

    const latStr = `${degX >= 0 ? '+' : ''}${degX.toFixed(1)}°`;
    const newCoords = `SUB-OBS: ${latStr} ${lonStr} • TERM: 71.8°`;

    if (newCoords !== this.lastLoggedCoords) {
      this.coordsElement.textContent = newCoords;
      this.lastLoggedCoords = newCoords;
    }
  }

  public getDiagnostics() {
    const gl = this.renderer?.getContext();
    let unmaskedVendor = 'unknown';
    let unmaskedRenderer = 'unknown';
    if (gl) {
      const ext = gl.getExtension('WEBGL_debug_renderer_info');
      if (ext) {
        unmaskedVendor = gl.getParameter(ext.UNMASKED_VENDOR_WEBGL) || 'unknown';
        unmaskedRenderer = gl.getParameter(ext.UNMASKED_RENDERER_WEBGL) || 'unknown';
      }
    }
    let centerLuminance = 0;
    if (this.renderer && this.scene && this.camera && this.moonMesh && gl) {
      this.renderer.render(this.scene, this.camera);
      const px = new Uint8Array(4);
      const midX = Math.floor(gl.drawingBufferWidth / 2);
      const midY = Math.floor(gl.drawingBufferHeight / 2);
      gl.readPixels(midX, midY, 1, 1, gl.RGBA, gl.UNSIGNED_BYTE, px);
      centerLuminance = Math.round((px[0] * 0.299 + px[1] * 0.587 + px[2] * 0.114) * 100) / 100;
    }
    return {
      webglVersion: gl && typeof WebGL2RenderingContext !== 'undefined' && gl instanceof WebGL2RenderingContext ? 2 : (gl ? 1 : 0),
      unmaskedVendor,
      unmaskedRenderer,
      anisotropy: this.albedoTexture?.anisotropy || 1,
      roughness: this.moonMaterial?.roughness || 0,
      metalness: this.moonMaterial?.metalness || 0,
      bumpScale: this.moonMaterial?.bumpScale || 0,
      lightCount: this.scene ? this.scene.children.filter(c => c instanceof THREE.Light).length : 0,
      toneMapping: this.renderer ? this.renderer.toneMapping : null,
      toneMappingExposure: this.renderer ? this.renderer.toneMappingExposure : null,
      isAnimating: this.reqId !== null,
      isPaused: this.isPaused,
      isVisible: this.isVisible,
      currentRotationY: this.currentRotationY,
      centerLuminance,
      fallbackPosterOpacity: this.posterElement ? parseFloat(window.getComputedStyle(this.posterElement).opacity) : 0
    };
  }

  public destroy(): void {
    this.stopLoop();
    if (this.resizeRafId) cancelAnimationFrame(this.resizeRafId);
    this.resizeObserver?.disconnect();
    this.intersectionObserver?.disconnect();

    if (this.renderer?.domElement) {
      this.renderer.domElement.removeEventListener('pointerdown', this.boundOnPointerDown);
      if (this.boundOnContextLost) {
        this.renderer.domElement.removeEventListener('webglcontextlost', this.boundOnContextLost);
      }
      if (this.boundOnContextRestored) {
        this.renderer.domElement.removeEventListener('webglcontextrestored', this.boundOnContextRestored);
      }
      this.renderer.domElement.remove();
    }
    window.removeEventListener('pointermove', this.boundOnPointerMove);
    window.removeEventListener('pointerup', this.boundOnPointerUp);
    document.removeEventListener('visibilitychange', this.boundOnVisibilityChange);

    const media = window.matchMedia('(prefers-reduced-motion: reduce)');
    if (media.removeEventListener) {
      media.removeEventListener('change', this.boundOnReducedMotionChange);
    }

    this.sphereGeometry?.dispose();
    this.moonMaterial?.dispose();
    this.albedoTexture?.dispose();
    this.bumpTexture?.dispose();
    this.renderer?.dispose();
  }
}
