// Astronomy & Ephemeris Geometry Diagram
// Coordinated SVG line-draw reveals, orbital nodes, and interactive MABIMS parameter spotlight.
// Engineered with GSAP ScrollTrigger, keyboard navigation, and WCAG 2.2.2 compliance.

import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

gsap.registerPlugin(ScrollTrigger);

export interface AstronomyDiagramController {
  highlightParameter: (paramIndex: number) => void;
  destroy: () => void;
}

export function initAstronomyDiagram(container: HTMLElement): AstronomyDiagramController {
  const svg = container.querySelector<SVGSVGElement>('.ephemeris-svg');
  if (!svg) {
    return { highlightParameter: () => {}, destroy: () => {} };
  }

  const paths = svg.querySelectorAll<SVGGeometryElement>('.draw-path');
  const labels = svg.querySelectorAll<SVGElement>('.diag-label, .diag-node');
  const criteriaItems = document.querySelectorAll<HTMLElement>('.criteria-item');
  const prefersReduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  // Specific diagram element handles
  const horizonPath = svg.querySelector<SVGGeometryElement>('.path-horizon');
  const sunPath = svg.querySelector<SVGGeometryElement>('.path-sun');
  const altitudePath = svg.querySelector<SVGGeometryElement>('.path-altitude');
  const elongationPath = svg.querySelector<SVGGeometryElement>('.path-elongation');
  const thresholdPath = svg.querySelector<SVGGeometryElement>('.path-threshold');

  const sunNode = svg.querySelector<SVGElement>('.diag-node-sun') || svg.querySelectorAll<SVGElement>('.diag-node')[0];
  const moonNode = svg.querySelector<SVGElement>('.diag-node-moon') || svg.querySelectorAll<SVGElement>('.diag-node')[1];
  const sunCorona = svg.querySelector<SVGElement>('.sun-corona');

  const altitudeLabel = svg.querySelector<SVGElement>('.label-altitude');
  const elongationLabel = svg.querySelector<SVGElement>('.label-elongation');

  // Pre-calculate path lengths for precision stroke drawing
  paths.forEach(path => {
    try {
      const len = path.getTotalLength();
      path.style.strokeDasharray = `${len}`;
      path.style.strokeDashoffset = prefersReduced ? '0' : `${len}`;
    } catch {
      path.style.strokeDashoffset = '0';
    }
  });

  if (prefersReduced) {
    labels.forEach(l => {
      (l as HTMLElement | SVGElement).style.opacity = '1';
    });
  }

  let ctx: gsap.Context | null = null;
  let coronaTween: gsap.core.Tween | null = null;
  let isPaused = false;

  if (!prefersReduced) {
    ctx = gsap.context(() => {
      // 1. Progressive Scientific Line Drawing Sequence
      const tl = gsap.timeline({
        scrollTrigger: {
          trigger: container,
          start: 'top 82%',
          once: true
        }
      });

      // Step A: Observer Horizon line establishes local ground reference (0° Alt)
      if (horizonPath) {
        tl.to(horizonPath, {
          strokeDashoffset: 0,
          duration: 0.75,
          ease: 'power2.out'
        });
      }

      // Step B: Sun Sunset Zenith Path & Sun node appearance
      if (sunPath) {
        tl.to(sunPath, {
          strokeDashoffset: 0,
          duration: 0.5,
          ease: 'power1.out'
        }, '-=0.25');
      }

      if (sunNode) {
        tl.fromTo(sunNode,
          { opacity: 0, scale: 0.5, transformOrigin: '180px 280px' },
          { opacity: 1, scale: 1.0, duration: 0.45, ease: 'back.out(1.5)' },
          '-=0.3'
        );
      }

      // Step C: Vertical Altitude Arc from horizon up to crescent moon position (+4° 48' 20")
      if (altitudePath) {
        tl.to(altitudePath, {
          strokeDashoffset: 0,
          duration: 0.7,
          ease: 'power2.out'
        }, '-=0.15');
      }

      // Step D: Crescent Moon celestial node reveal
      if (moonNode) {
        tl.fromTo(moonNode,
          { opacity: 0, scale: 0.4, transformOrigin: '380px 140px' },
          { opacity: 1, scale: 1.0, duration: 0.5, ease: 'back.out(1.6)' },
          '-=0.3'
        );
      }

      // Step E: Great-Circle Elongation Arc connecting Sun to Moon (Elongasi 8.24° >= 6.4°)
      if (elongationPath) {
        tl.to(elongationPath, {
          strokeDashoffset: 0,
          duration: 0.85,
          ease: 'power2.out'
        }, '-=0.2');
      }

      // Step F: MABIMS 3° minimum altitude threshold guide
      if (thresholdPath) {
        tl.to(thresholdPath, {
          strokeDashoffset: 0,
          duration: 0.45,
          ease: 'power1.out'
        }, '-=0.25');
      }

      // Step G: Staggered Appearance of Astronomical Labels
      tl.from(
        labels,
        {
          opacity: 0,
          y: 6,
          duration: 0.45,
          stagger: 0.05,
          ease: 'power2.out'
        },
        '-=0.35'
      );

      // Subtle live orbital breathing animation on the Sun's corona
      if (sunCorona) {
        coronaTween = gsap.to(sunCorona, {
          scale: 1.25,
          opacity: 0.6,
          transformOrigin: '180px 280px',
          duration: 2.2,
          repeat: -1,
          yoyo: true,
          ease: 'sine.inOut'
        });
      }
    }, container);
  }

  // 2. Interactive Parameter Spotlight & Criteria Card Synchronization
  const svgEl = svg;
  function highlightParameter(index: number): void {
    if (!svgEl) return;
    // Reset all paths and labels to baseline state
    paths.forEach(p => {
      (p as SVGElement).style.filter = '';
      (p as SVGElement).style.opacity = '0.45';
      (p as SVGElement).style.strokeWidth = '';
    });

    labels.forEach(l => {
      (l as SVGElement).style.opacity = '0.55';
    });

    if (horizonPath) (horizonPath as SVGElement).style.opacity = '0.75';

    if (index === 0) {
      // PARAMETER 01: Tinggi Hilal Hakiki (>= 3.0° MABIMS)
      if (altitudePath) {
        (altitudePath as SVGElement).style.opacity = '1';
        (altitudePath as SVGElement).style.strokeWidth = '2.8';
        (altitudePath as SVGElement).style.filter = 'drop-shadow(0 0 8px rgba(169, 130, 74, 0.95))';
      }
      if (thresholdPath) {
        (thresholdPath as SVGElement).style.opacity = '1';
        (thresholdPath as SVGElement).style.strokeWidth = '2.0';
        (thresholdPath as SVGElement).style.filter = 'drop-shadow(0 0 6px rgba(74, 222, 128, 0.8))';
      }
      if (moonNode) (moonNode as SVGElement).style.opacity = '1';
      if (altitudeLabel) {
        (altitudeLabel as SVGElement).style.opacity = '1';
        gsap.fromTo(altitudeLabel, { scale: 1.18, transformOrigin: 'left center' }, { scale: 1.0, duration: 0.35, ease: 'back.out(2)' });
      }
      svgEl.querySelectorAll('.label-mabims, .label-moon-pos').forEach(el => {
        (el as SVGElement).style.opacity = '1';
      });
    } else if (index === 1) {
      // PARAMETER 02: Sudut Elongasi Busur (>= 6.4° MABIMS)
      if (elongationPath) {
        (elongationPath as SVGElement).style.opacity = '1';
        (elongationPath as SVGElement).style.strokeWidth = '3.5';
        (elongationPath as SVGElement).style.filter = 'drop-shadow(0 0 10px rgba(229, 169, 59, 0.98))';
      }
      if (sunNode) (sunNode as SVGElement).style.opacity = '1';
      if (moonNode) (moonNode as SVGElement).style.opacity = '1';
      if (elongationLabel) {
        (elongationLabel as SVGElement).style.opacity = '1';
        gsap.fromTo(elongationLabel, { scale: 1.18, transformOrigin: 'center center' }, { scale: 1.0, duration: 0.35, ease: 'back.out(2)' });
      }
    } else {
      // PARAMETER 03: Telemetri Dual & Waktu Kalender
      paths.forEach(p => {
        (p as SVGElement).style.opacity = '1';
      });
      labels.forEach(l => {
        (l as SVGElement).style.opacity = '1';
      });
      if (sunNode) (sunNode as SVGElement).style.opacity = '1';
      if (moonNode) (moonNode as SVGElement).style.opacity = '1';
    }

    // Toggle active state and accessibility attributes on criteria cards
    criteriaItems.forEach((item, idx) => {
      const isSelected = idx === index;
      item.classList.toggle('active', isSelected);
      item.setAttribute('aria-selected', isSelected ? 'true' : 'false');
    });
  }

  // Bind mouse, touch, and keyboard interactions on criteria cards
  criteriaItems.forEach((item, idx) => {
    item.setAttribute('tabindex', '0');
    item.setAttribute('role', 'button');
    item.setAttribute('aria-selected', idx === 0 ? 'true' : 'false');

    // Hover spotlight (desktop pointer)
    item.addEventListener('mouseenter', () => highlightParameter(idx));

    // Click / Tap spotlight (desktop and mobile)
    item.addEventListener('click', () => highlightParameter(idx));

    // Keyboard navigation: Enter & Space keys
    item.addEventListener('keydown', (e: KeyboardEvent) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        highlightParameter(idx);
      }
    });
  });

  // Synchronize with WCAG 2.2.2 Motion Pause/Resume Toggle
  const onMotionToggle = (e: CustomEvent<{ paused: boolean }>) => {
    isPaused = e.detail.paused;
    if (coronaTween) {
      if (isPaused) {
        coronaTween.pause();
      } else {
        coronaTween.play();
      }
    }
  };
  window.addEventListener('sabit-motion-toggle', onMotionToggle as EventListener);

  return {
    highlightParameter,
    destroy: () => {
      window.removeEventListener('sabit-motion-toggle', onMotionToggle as EventListener);
      coronaTween?.kill();
      ctx?.revert();
    }
  };
}
