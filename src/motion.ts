// Motion Coordinator for Sabit Landing Page
// Powered by 'motion' (https://github.com/motiondivision/motion)
// Delivers fluid, compositor-accelerated micro-interactions, spring reveals, and WCAG pause controls.

import { animate, inView } from 'motion';

export interface MotionSystemController {
  toggleMotion: () => boolean;
  isMotionPaused: () => boolean;
}

export function initMotionSystem(): MotionSystemController {
  const prefersReducedMotion = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  let userPaused = prefersReducedMotion;

  // 1. Initial Hero Entrance Animation via Motion
  if (!prefersReducedMotion) {
    initHeroEntrance();
  }

  // 2. Section Scroll Reveals via Motion inView
  initSectionScrollReveals(prefersReducedMotion);

  // 3. Interactive Screenshot Hotspot Inspector
  initScreenshotHotspots(prefersReducedMotion);

  // 4. WCAG 2.2.2 Motion Pause/Play Toggle Button
  const motionToggleBtn = document.querySelector<HTMLButtonElement>('#motion-toggle-btn');
  const motionStatusText = document.querySelector<HTMLElement>('#motion-status-text');

  function updateMotionState(paused: boolean): void {
    userPaused = paused;
    document.documentElement.classList.toggle('motion-paused', paused);
    if (motionToggleBtn && motionStatusText) {
      motionStatusText.textContent = paused ? 'Putar Animasi' : 'Jeda Animasi';
      motionToggleBtn.setAttribute('aria-label', paused ? 'Nyalakan animasi rotasi 3D dan diagram' : 'Jeda animasi rotasi 3D dan diagram');
      const iconPlay = motionToggleBtn.querySelector('.icon-play');
      const iconPause = motionToggleBtn.querySelector('.icon-pause');
      if (iconPlay && iconPause) {
        iconPlay.classList.toggle('hidden', !paused);
        iconPause.classList.toggle('hidden', paused);
      }
    }
  }

  if (motionToggleBtn) {
    motionToggleBtn.addEventListener('click', () => {
      updateMotionState(!userPaused);
      window.dispatchEvent(new CustomEvent('sabit-motion-toggle', { detail: { paused: userPaused } }));
    });
  }

  return {
    toggleMotion: () => {
      updateMotionState(!userPaused);
      return userPaused;
    },
    isMotionPaused: () => userPaused
  };
}

function initHeroEntrance(): void {
  // Staggered entrance for Hero text, actions, badges, and Moon stage
  const eyebrow = document.querySelector('.hero-eyebrow');
  const title = document.querySelector('.hero-title');
  const lead = document.querySelector('.hero-lead');
  const actions = document.querySelector('.hero-actions');
  const badges = document.querySelector('.hero-badges-capsule');
  const moonStage = document.querySelector('.hero-visual-stage');

  if (eyebrow) {
    animate(eyebrow, { opacity: [0, 1], y: [-8, 0] }, { duration: 0.5, delay: 0.1 });
  }
  if (title) {
    animate(title, { opacity: [0, 1], y: [16, 0] }, { duration: 0.65, delay: 0.2, ease: [0.16, 1, 0.3, 1] });
  }
  if (lead) {
    animate(lead, { opacity: [0, 1], y: [12, 0] }, { duration: 0.6, delay: 0.3, ease: [0.16, 1, 0.3, 1] });
  }
  if (actions) {
    animate(actions, { opacity: [0, 1], y: [10, 0] }, { duration: 0.55, delay: 0.4 });
  }
  if (badges) {
    animate(badges, { opacity: [0, 1], y: [8, 0] }, { duration: 0.55, delay: 0.5 });
  }
  if (moonStage) {
    animate(moonStage, { opacity: [0, 1], scale: [0.95, 1.0] }, { duration: 0.85, delay: 0.25, ease: [0.16, 1, 0.3, 1] });
  }
}

function initSectionScrollReveals(reducedMotion: boolean): void {
  const sections = document.querySelectorAll<HTMLElement>('.reveal-group');

  sections.forEach(section => {
    if (reducedMotion) {
      section.classList.add('is-revealed');
      return;
    }

    inView(section, () => {
      section.classList.add('is-revealed');
      animate(
        section,
        { opacity: [0, 1], y: [20, 0] },
        { duration: 0.65, ease: [0.16, 1, 0.3, 1] }
      );
    }, { margin: '0px 0px -60px 0px' });
  });
}

function initScreenshotHotspots(reducedMotion: boolean): void {
  const hotspots = document.querySelectorAll<HTMLButtonElement>('.ui-hotspot');
  const detailCards = document.querySelectorAll<HTMLElement>('.inspector-card');

  hotspots.forEach(hotspot => {
    hotspot.addEventListener('click', () => {
      const targetId = hotspot.getAttribute('data-target');

      hotspots.forEach(h => {
        const isMatch = h === hotspot;
        h.classList.toggle('active', isMatch);
        h.setAttribute('aria-expanded', isMatch ? 'true' : 'false');
        if (isMatch && !reducedMotion) {
          animate(h, { scale: [1.0, 1.22, 1.15] }, { duration: 0.3 });
        }
      });

      detailCards.forEach(card => {
        const isTarget = card.getAttribute('id') === targetId;
        card.classList.toggle('active', isTarget);
        if (isTarget) {
          card.focus?.();
          if (!reducedMotion) {
            animate(card, { opacity: [0.7, 1], y: [6, 0] }, { duration: 0.35, ease: [0.16, 1, 0.3, 1] });
          }
        }
      });
    });
  });
}
