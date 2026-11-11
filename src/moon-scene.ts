// Sabit 3D Moon Centerpiece — Three.js Implementation
// Powered by official NASA CGI Moon Kit (LRO color map + LOLA elevation map)
// Adheres strictly to sabit-web-design, MOTION-3D-BRIEF.md, and ANTIGRAVITY-REVISION-GUIDE.md

import * as THREE from 'three';

export interface MoonSceneOptions {
  container: HTMLElement;
  albedoUrl?: string;
  bumpUrl?: string;
  posterElement?: HTMLElement | null;
  onReady?: () => void;
  onError?: (err: Error) => void;
}

export class MoonScene {
  private container: HTMLElement;
  private posterElement: HTMLElement | null;
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

  // Interaction physics & orientation
  private isDragging = false;
  private previousMousePosition = { x: 0, y: 0 };
  private targetRotationY = 0.20; // Selenographic near-side orientation facing observer
  private targetRotationX = 0.04;
  private currentRotationY = 0.20;
  private currentRotationX = 0.04;
  private rotationVelocityY = 0;
  private rotationVelocityX = 0;

  // Very slow natural axial rotation: ~150 seconds per full 360-degree turn
  private baseRotationSpeed = 0.00040;

  private boundOnPointerDown: (e: PointerEvent) => void;
  private boundOnPointerMove: (e: PointerEvent) => void;
  private boundOnPointerUp: (e: PointerEvent) => void;
  private boundOnVisibilityChange: () => void;
  private boundOnReducedMotionChange: (e: MediaQueryListEvent) => void;

  constructor(options: MoonSceneOptions) {
    this.container = options.container;
    this.posterElement = options.posterElement || null;

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

      // 3. Renderer with performance capping (1.5 on desktop, 1.0 on mobile)
      const canvas = document.createElement('canvas');
      canvas.className = 'moon-webgl-canvas';
      canvas.setAttribute('aria-hidden', 'true');

      const isMobile = window.innerWidth < 768;
      const maxDpr = isMobile ? 1.0 : Math.min(window.devicePixelRatio || 1, 1.5);

      this.renderer = new THREE.WebGLRenderer({
        canvas,
        alpha: true,
        antialias: !isMobile,
        powerPreference: 'high-performance'
      });
      this.renderer.setPixelRatio(maxDpr);
      this.renderer.setSize(width, height);
      this.renderer.toneMapping = THREE.ACESFilmicToneMapping;
      this.renderer.toneMappingExposure = 1.02;

      this.container.appendChild(this.renderer.domElement);

      // 4. Lighting:
      // Directional Sun Light: Grazing angle (~72°) from top-right to create authentic terminator & crater relief
      const sunLight = new THREE.DirectionalLight(0xfffdf6, 2.4);
      sunLight.position.set(2.9, 1.15, 1.65);
      this.scene.add(sunLight);

      // Cosmic Earthshine / Deep Space Fill Light: Neutral soft dark navy (#131e2c)
      const ambientLight = new THREE.AmbientLight(0x131e2c, 0.18);
      this.scene.add(ambientLight);

      // Subtle Rim/Glance Light for soft celestial silhouette
      const rimLight = new THREE.DirectionalLight(0x8a9bb0, 0.35);
      rimLight.position.set(-2.5, 0.5, -1.0);
      this.scene.add(rimLight);

      // 5. Geometry: Smooth sphere
      const segments = isMobile ? 36 : 64;
      this.sphereGeometry = new THREE.SphereGeometry(1.0, segments, segments);

      // 6. NASA Textures Loading (LROC color map + LOLA elevation map)
      const textureLoader = new THREE.TextureLoader();
      const albedoPath = options.albedoUrl || '/textures/lroc_color_2k.jpg';
      const bumpPath = options.bumpUrl || '/textures/ldem_3_8bit.jpg';

      let loadedCount = 0;
      const onLoadCheck = () => {
        loadedCount++;
        if (loadedCount >= 2) {
          // Smooth crossfade: Hide static poster once 3D textures are decoded
          this.hideFallback();
          options.onReady?.();
        }
      };

      this.albedoTexture = textureLoader.load(albedoPath, onLoadCheck, undefined, () => {
        console.warn('NASA albedo texture failed to load, falling back.');
        onLoadCheck();
      });
      this.albedoTexture.colorSpace = THREE.SRGBColorSpace;
      this.albedoTexture.generateMipmaps = true;
      this.albedoTexture.minFilter = THREE.LinearMipmapLinearFilter;

      this.bumpTexture = textureLoader.load(bumpPath, onLoadCheck, undefined, () => {
        onLoadCheck();
      });
      this.bumpTexture.generateMipmaps = true;
      this.bumpTexture.minFilter = THREE.LinearMipmapLinearFilter;

      // 7. Material: Physically grounded matte lunar regolith
      // High roughness, minimal metalness (real rock / basalt)
      this.moonMaterial = new THREE.MeshStandardMaterial({
        map: this.albedoTexture,
        bumpMap: this.bumpTexture,
        bumpScale: isMobile ? 0.03 : 0.045,
        roughness: 0.94,
        metalness: 0.02,
        color: 0xffffff
      });

      this.moonMesh = new THREE.Mesh(this.sphereGeometry, this.moonMaterial);
      this.moonMesh.rotation.y = this.currentRotationY;
      this.moonMesh.rotation.x = this.currentRotationX;
      this.scene.add(this.moonMesh);

      // 8. Event Listeners for Drag-to-Rotate Interaction
      this.renderer.domElement.addEventListener('pointerdown', this.boundOnPointerDown);
      window.addEventListener('pointermove', this.boundOnPointerMove);
      window.addEventListener('pointerup', this.boundOnPointerUp);
      document.addEventListener('visibilitychange', this.boundOnVisibilityChange);

      // 9. Resize Observer
      this.resizeObserver = new ResizeObserver(entries => {
        for (const entry of entries) {
          const { width: w, height: h } = entry.contentRect;
          if (w > 0 && h > 0) this.resize(w, h);
        }
      });
      this.resizeObserver.observe(this.container);

      // 10. Intersection Observer (Pause rendering when offscreen)
      this.intersectionObserver = new IntersectionObserver(
        entries => {
          for (const entry of entries) {
            this.isVisible = entry.isIntersecting;
            if (this.isVisible && !this.isPaused && !this.reqId) {
              this.startLoop();
            }
          }
        },
        { threshold: 0.05 }
      );
      this.intersectionObserver.observe(this.container);

      this.startLoop();
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
    if (!paused && this.isVisible && !this.reqId) {
      this.startLoop();
    }
  }

  public getPaused(): boolean {
    return this.isPaused;
  }

  private onVisibilityChange(): void {
    if (document.hidden) {
      this.stopLoop();
    } else if (this.isVisible && !this.isPaused) {
      this.startLoop();
    }
  }

  private onPointerDown(e: PointerEvent): void {
    if (e.button !== 0) return;
    this.isDragging = true;
    this.previousMousePosition = { x: e.clientX, y: e.clientY };
    if (this.renderer?.domElement) {
      this.renderer.domElement.style.cursor = 'grabbing';
    }
  }

  private onPointerMove(e: PointerEvent): void {
    if (!this.isDragging) return;
    const deltaX = e.clientX - this.previousMousePosition.x;
    const deltaY = e.clientY - this.previousMousePosition.y;

    this.rotationVelocityY = deltaX * 0.0035;
    this.rotationVelocityX = deltaY * 0.0025;

    this.targetRotationY += this.rotationVelocityY;
    this.targetRotationX = Math.max(-0.35, Math.min(0.35, this.targetRotationX + this.rotationVelocityX));

    this.previousMousePosition = { x: e.clientX, y: e.clientY };
  }

  private onPointerUp(): void {
    this.isDragging = false;
    if (this.renderer?.domElement) {
      this.renderer.domElement.style.cursor = 'grab';
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
      // Natural slow planetary rotation (~150s per revolution)
      if (!this.isDragging) {
        this.targetRotationY += this.baseRotationSpeed;
        this.rotationVelocityY *= 0.92;
        this.rotationVelocityX *= 0.92;
      }

      this.currentRotationY += (this.targetRotationY - this.currentRotationY) * 0.08;
      this.currentRotationX += (this.targetRotationX - this.currentRotationX) * 0.08;

      this.moonMesh.rotation.y = this.currentRotationY;
      this.moonMesh.rotation.x = this.currentRotationX;
    }

    this.renderer.render(this.scene, this.camera);
  }

  public destroy(): void {
    this.stopLoop();

    if (this.renderer?.domElement) {
      this.renderer.domElement.removeEventListener('pointerdown', this.boundOnPointerDown);
      this.renderer.domElement.remove();
    }
    window.removeEventListener('pointermove', this.boundOnPointerMove);
    window.removeEventListener('pointerup', this.boundOnPointerUp);
    document.removeEventListener('visibilitychange', this.boundOnVisibilityChange);

    const media = window.matchMedia('(prefers-reduced-motion: reduce)');
    if (media.removeEventListener) {
      media.removeEventListener('change', this.boundOnReducedMotionChange);
    }

    this.resizeObserver?.disconnect();
    this.intersectionObserver?.disconnect();

    this.sphereGeometry?.dispose();
    this.moonMaterial?.dispose();
    this.albedoTexture?.dispose();
    this.bumpTexture?.dispose();
    this.renderer?.dispose();

    this.scene = null;
    this.camera = null;
    this.moonMesh = null;
    this.renderer = null;
  }
}
