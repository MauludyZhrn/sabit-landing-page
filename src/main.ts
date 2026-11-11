// Sabit Official Landing Page — Main Application Entry
// Orchestrates Orbital Background, 3D Moon Centerpiece, GSAP Motion Engine, and Verified Releases.

import { APP_CONFIG } from './config';
import { initMotionSystem } from './motion';
import { initAstronomyDiagram } from './astronomy-diagram';
import { OrbitalBackground } from './orbital-background';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

document.addEventListener('DOMContentLoaded', () => {
  // 1. Populate Centralized Release & Repository URLs
  populateLinks();

  // 2. Initialize Centralized Motion System (powered by GSAP & ScrollTrigger)
  const motionControl = initMotionSystem();

  // 3. Initialize Celestial Keplerian Orbital Background Canvas
  const heroSection = document.querySelector<HTMLElement>('.hero-section');
  const orbitalCanvas = document.querySelector<HTMLCanvasElement>('#orbital-hero-canvas');
  let orbitalBg: OrbitalBackground | null = null;

  if (heroSection && orbitalCanvas) {
    try {
      orbitalBg = new OrbitalBackground(heroSection, orbitalCanvas);
      if (motionControl.isMotionPaused()) {
        orbitalBg.setPaused(true);
      }
    } catch (err) {
      console.warn('Orbital background canvas initialization skipped:', err);
    }
  }

  // 4. Initialize Astronomy Geometry Diagram (Line-draw & Parameter Spotlight)
  const diagramSection = document.querySelector<HTMLElement>('#ephemeris-diagram-container');
  if (diagramSection) {
    initAstronomyDiagram(diagramSection);
  }

  // 5. Lazy-Load 3D Moon Canvas with official NASA CGI Moon Kit textures
  const moonContainer = document.querySelector<HTMLElement>('#moon-canvas-container');
  const fallbackPoster = document.querySelector<HTMLElement>('#moon-fallback-poster');
  const liveCoordsEl = document.querySelector<HTMLElement>('#moon-live-coords');

  if (moonContainer) {
    const prefersReducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

    if (prefersReducedMotion) {
      console.info('prefers-reduced-motion is active: presenting static NASA lunar poster.');
    } else {
      const load3DMoon = async () => {
        try {
          const { MoonScene } = await import('./moon-scene');
          const moonScene = new MoonScene({
            container: moonContainer,
            posterElement: fallbackPoster,
            coordsElement: liveCoordsEl,
            albedoUrl: '/textures/lroc_color_2k.jpg',
            bumpUrl: '/textures/ldem_3_8bit.jpg',
            onReady: () => {
              console.info('Sabit 3D Moon (NASA CGI Moon Kit) initialized successfully.');
            },
            onError: (err) => {
              console.warn('3D Moon fallback active:', err.message);
            }
          });

          // Sync with accessible Pause/Play control button
          window.addEventListener('sabit-motion-toggle', ((e: CustomEvent<{ paused: boolean }>) => {
            moonScene.setPaused(e.detail.paused);
            orbitalBg?.setPaused(e.detail.paused);
          }) as EventListener);

          if (motionControl.isMotionPaused()) {
            moonScene.setPaused(true);
          }

          // Link Moon orientation to scroll progress through the hero section
          if (heroSection) {
            ScrollTrigger.create({
              trigger: heroSection,
              start: 'top top',
              end: 'bottom top',
              onUpdate: (self) => {
                moonScene.setScrollProgress(self.progress);
              }
            });
          }
        } catch (err) {
          console.error('Failed to dynamically load 3D Moon module:', err);
        }
      };

      if ('requestIdleCallback' in window) {
        (window as Window & { requestIdleCallback: (cb: () => void) => void }).requestIdleCallback(() => {
          load3DMoon();
        });
      } else {
        setTimeout(load3DMoon, 80);
      }
    }
  }

  // 6. Smooth Anchor Scrolling with focus management and ScrollTrigger refresh
  document.querySelectorAll<HTMLAnchorElement>('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', e => {
      const targetId = anchor.getAttribute('href');
      if (!targetId || targetId === '#') return;
      const targetEl = document.querySelector(targetId);
      if (targetEl) {
        e.preventDefault();
        targetEl.scrollIntoView({ behavior: 'smooth' });
        targetEl.setAttribute('tabindex', '-1');
        (targetEl as HTMLElement).focus?.({ preventScroll: true });
        setTimeout(() => {
          ScrollTrigger.refresh();
        }, 350);
      }
    });
  });
});

function populateLinks(): void {
  // Direct Installer Links
  document.querySelectorAll<HTMLAnchorElement>('[data-action="download-installer"]').forEach(el => {
    el.href = APP_CONFIG.windowsInstallerUrl;
    el.setAttribute('download', APP_CONFIG.windowsInstallerFilename);
  });

  // GitHub Repo Links
  document.querySelectorAll<HTMLAnchorElement>('[data-action="github-repo"]').forEach(el => {
    el.href = APP_CONFIG.githubRepoUrl;
    el.setAttribute('target', '_blank');
    el.setAttribute('rel', 'noopener noreferrer');
  });

  // GitHub Releases Links
  document.querySelectorAll<HTMLAnchorElement>('[data-action="github-releases"]').forEach(el => {
    el.href = APP_CONFIG.githubReleasesUrl;
    el.setAttribute('target', '_blank');
    el.setAttribute('rel', 'noopener noreferrer');
  });

  // Text values
  document.querySelectorAll<HTMLElement>('[data-value="installer-filename"]').forEach(el => {
    el.textContent = APP_CONFIG.windowsInstallerFilename;
  });
  document.querySelectorAll<HTMLElement>('[data-value="installer-size"]').forEach(el => {
    el.textContent = APP_CONFIG.installerSizeLabel;
  });
  document.querySelectorAll<HTMLElement>('[data-value="installer-arch"]').forEach(el => {
    el.textContent = APP_CONFIG.installerArch;
  });
  document.querySelectorAll<HTMLElement>('[data-value="lunar-credit"]').forEach(el => {
    el.textContent = APP_CONFIG.lunarDataCredit;
  });
}
