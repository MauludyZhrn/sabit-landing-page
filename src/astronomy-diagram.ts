// Astronomy & Ephemeris Geometry Diagram
// Coordinated SVG line-draw reveals and interactive MABIMS parameter spotlight.

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

  // Pre-calculate path lengths
  const pathLengths = new Map<SVGGeometryElement, number>();
  paths.forEach(path => {
    try {
      const len = path.getTotalLength();
      pathLengths.set(path, len);
      path.style.strokeDasharray = `${len}`;
      path.style.strokeDashoffset = prefersReduced ? '0' : `${len}`;
    } catch {
      path.style.strokeDashoffset = '0';
    }
  });

  if (prefersReduced) {
    labels.forEach(l => { (l as SVGElement).style.opacity = '1'; });
  }

  // ScrollTrigger timeline for entering the viewport
  let ctx: gsap.Context | null = null;

  if (!prefersReduced) {
    ctx = gsap.context(() => {
      const tl = gsap.timeline({
        scrollTrigger: {
          trigger: container,
          start: 'top 80%',
          once: true
        }
      });

      // Animate SVG stroke drawing sequentially
      paths.forEach((path, idx) => {
        tl.to(
          path,
          {
            strokeDashoffset: 0,
            duration: 0.9,
            ease: 'power2.out'
          },
          idx * 0.18
        );
      });

      // Animate labels and nodes gentle entrance
      tl.from(
        labels,
        {
          opacity: 0.6,
          y: 4,
          duration: 0.5,
          stagger: 0.06,
          ease: 'power2.out'
        },
        '-=0.4'
      );
    }, container);
  }

  // Interactive parameter highlighting
  function highlightParameter(index: number): void {
    // Parameter 1: Altitude (Tinggi Hilal >= 3°) -> path index 2 & box index 4
    // Parameter 2: Elongation (Elongasi >= 6.4°) -> path index 3
    // Parameter 3: Telemetry (All coordinates)
    const altitudePath = svg?.querySelector('.path-altitude');
    const elongationPath = svg?.querySelector('.path-elongation');
    const horizonPath = svg?.querySelector('.path-horizon');
    const thresholdPath = svg?.querySelector('.path-threshold');
    const altitudeLabel = svg?.querySelector('.label-altitude');
    const elongationLabel = svg?.querySelector('.label-elongation');

    // Reset styles
    svg?.querySelectorAll('.draw-path').forEach(p => {
      (p as SVGElement).style.filter = '';
      (p as SVGElement).style.opacity = '0.75';
    });

    if (index === 0) {
      // Tinggi Hilal
      if (altitudePath) {
        (altitudePath as SVGElement).style.opacity = '1';
        (altitudePath as SVGElement).style.filter = 'drop-shadow(0 0 6px rgba(169, 130, 74, 0.9))';
      }
      if (thresholdPath) {
        (thresholdPath as SVGElement).style.opacity = '1';
      }
      if (altitudeLabel) {
        gsap.fromTo(altitudeLabel, { scale: 1.15 }, { scale: 1.0, duration: 0.35 });
      }
    } else if (index === 1) {
      // Elongasi
      if (elongationPath) {
        (elongationPath as SVGElement).style.opacity = '1';
        (elongationPath as SVGElement).style.filter = 'drop-shadow(0 0 8px rgba(229, 169, 59, 0.95))';
      }
      if (elongationLabel) {
        gsap.fromTo(elongationLabel, { scale: 1.15 }, { scale: 1.0, duration: 0.35 });
      }
    } else {
      // All
      if (horizonPath) (horizonPath as SVGElement).style.opacity = '1';
      if (altitudePath) (altitudePath as SVGElement).style.opacity = '1';
      if (elongationPath) (elongationPath as SVGElement).style.opacity = '1';
    }

    // Toggle active state on criteria cards
    criteriaItems.forEach((item, idx) => {
      item.classList.toggle('active', idx === index);
    });
  }

  // Bind click/hover on criteria cards to spotlight corresponding diagram path
  criteriaItems.forEach((item, idx) => {
    item.addEventListener('mouseenter', () => highlightParameter(idx));
    item.addEventListener('click', () => highlightParameter(idx));
  });

  return {
    highlightParameter,
    destroy: () => {
      ctx?.revert();
    }
  };
}
