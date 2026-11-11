// Astronomy & Ephemeris Geometry Diagram
// Powered by 'motion' (https://github.com/motiondivision/motion)
// Coordinated line-draw reveal for Sabit's MABIMS Hilal Observation criteria.

import { animate, inView } from 'motion';

export function initAstronomyDiagram(container: HTMLElement): void {
  const svg = container.querySelector<SVGSVGElement>('.ephemeris-svg');
  if (!svg) return;

  const paths = svg.querySelectorAll<SVGGeometryElement>('.draw-path');
  const prefersReducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  // Pre-calculate path lengths
  const pathLengths: number[] = [];
  paths.forEach((path, idx) => {
    try {
      const len = path.getTotalLength();
      pathLengths[idx] = len;
      path.style.strokeDasharray = `${len}`;
      if (prefersReducedMotion) {
        path.style.strokeDashoffset = '0';
        path.classList.add('drawn');
      } else {
        path.style.strokeDashoffset = `${len}`;
      }
    } catch {
      path.classList.add('drawn');
    }
  });

  if (prefersReducedMotion) return;

  // Trigger line-draw via Motion inView
  inView(container, () => {
    paths.forEach((path, idx) => {
      const len = pathLengths[idx] || 500;
      const delay = 0.15 + idx * 0.18;

      animate(
        path,
        { strokeDashoffset: [len, 0] },
        { duration: 0.85, delay, ease: [0.16, 1, 0.3, 1] }
      );
    });

    // Fade in labels and nodes
    const labels = svg.querySelectorAll<SVGElement>('.diag-label, .diag-node');
    labels.forEach((label, idx) => {
      const delay = 0.45 + idx * 0.10;
      animate(
        label,
        { opacity: [0, 1], y: [4, 0] },
        { duration: 0.5, delay, ease: 'easeOut' }
      );
    });
  }, { margin: '0px 0px -40px 0px' });
}
