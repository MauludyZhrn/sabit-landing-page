// ============================================================================
// SABIT MOTION DESIGN SYSTEM & ARCHITECTURE (V2)
// Precision Observatory Motion Engine
// Built with GSAP, ScrollTrigger, and Responsive matchMedia.
// Provides cinematic section reveals, staggered headlines, interactive 3D perspective
// tilts, timeline scrubbing, selenographic HUD synchronizations, and WCAG 2.2.2 controls.
// ============================================================================

import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

gsap.registerPlugin(ScrollTrigger);

// ---------------------------------------------------------------------------
// 1. Motion Design Tokens & Configuration
// ---------------------------------------------------------------------------
export const MOTION_TOKENS = {
  // Scientific Editorial Easings
  easeEditorial: 'cubic-bezier(0.16, 1, 0.3, 1)',
  easeGentle: 'power2.out',
  easeDramatic: 'power3.out',
  easeLinear: 'none',

  // Restrained Durations (seconds)
  durMicro: 0.22,
  durFast: 0.35,
  durStandard: 0.65,
  durCinematic: 0.95,

  // Stagger Intervals
  staggerTight: 0.06,
  staggerStandard: 0.10,
  staggerGenerous: 0.15,

  // Responsive Breakpoints
  mediaDesktop: '(min-width: 1024px)',
  mediaTablet: '(min-width: 768px) and (max-width: 1023px)',
  mediaMobile: '(max-width: 767px)',
} as const;

export interface MotionSystemController {
  toggleMotion: () => boolean;
  isMotionPaused: () => boolean;
  refresh: () => void;
  destroy: () => void;
}

// ---------------------------------------------------------------------------
// 2. Primary Motion System Initializer
// ---------------------------------------------------------------------------
export function initMotionSystem(): MotionSystemController {
  const prefersReduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  let userPaused = prefersReduced;
  let mm: gsap.MatchMedia | null = null;

  // A. Global Chrome & Navigation Interactions (Always active regardless of motion preference)
  initScrollProgressBar();
  initHeaderScrollState();
  initActiveNavObserver();
  initMobileDrawer();
  initScreenshotHotspots();

  // B. Progressive Animations (Skip or simplify when user prefers reduced motion)
  if (!prefersReduced) {
    mm = gsap.matchMedia();

    // -------------------------------------------------------------------------
    // DESKTOP EXPERIENCE (>= 1024px)
    // Rich 3D perspective, celestial depth parallax, and coordinated reveals
    // -------------------------------------------------------------------------
    mm.add(MOTION_TOKENS.mediaDesktop, () => {
      // 1. Hero Cinematic Stagger (Never start completely invisible)
      const heroTl = gsap.timeline({ defaults: { ease: MOTION_TOKENS.easeDramatic } });
      heroTl
        .from('.hero-eyebrow', { opacity: 0.3, y: -10, duration: MOTION_TOKENS.durFast })
        .from('.hero-title', { opacity: 0.3, y: 18, duration: MOTION_TOKENS.durStandard }, '-=0.2')
        .from('.hero-lead', { opacity: 0.3, y: 12, duration: MOTION_TOKENS.durStandard }, '-=0.3')
        .from('.hero-actions .btn-primary, .hero-actions .btn-secondary-dark, .hero-actions .hero-action-tertiary', {
          opacity: 0.4,
          y: 10,
          duration: MOTION_TOKENS.durFast,
          stagger: MOTION_TOKENS.staggerTight
        }, '-=0.25')
        .from('.hero-badges-capsule .badge-item', {
          opacity: 0.4,
          y: 8,
          duration: MOTION_TOKENS.durFast,
          stagger: MOTION_TOKENS.staggerTight
        }, '-=0.2')
        .from('.hero-visual-stage', {
          opacity: 0.85,
          scale: 0.96,
          duration: MOTION_TOKENS.durStandard,
          ease: MOTION_TOKENS.easeGentle
        }, '-=0.4');

      // 2. Hero Celestial Parallax Scrub
      gsap.to('.hero-visual-stage', {
        scrollTrigger: {
          trigger: '.hero-section',
          start: 'top top',
          end: 'bottom top',
          scrub: 1.0
        },
        y: 45,
        ease: MOTION_TOKENS.easeLinear
      });

      gsap.to('.hero-celestial-grid', {
        scrollTrigger: {
          trigger: '.hero-section',
          start: 'top top',
          end: 'bottom top',
          scrub: 1.0
        },
        y: -30,
        ease: MOTION_TOKENS.easeLinear
      });

      // 3. Staggered Section Headers Reveal
      initSectionHeaderReveals();

      // 4. Application Showcase (Evidence) Window Reveal & 3D Tilt
      gsap.from('.screenshot-stage', {
        opacity: 0.85,
        y: 28,
        duration: MOTION_TOKENS.durCinematic,
        ease: MOTION_TOKENS.easeGentle,
        scrollTrigger: {
          trigger: '.screenshot-stage',
          start: 'top 85%',
          once: true
        }
      });

      initScreenshot3DTilt();

      // 5. Workflow Scrubbed Connected Timeline Line
      gsap.fromTo('.workflow-progress-fill',
        { scaleY: 0 },
        {
          scaleY: 1,
          ease: MOTION_TOKENS.easeLinear,
          transformOrigin: 'top center',
          scrollTrigger: {
            trigger: '.workflow-timeline',
            start: 'top 75%',
            end: 'bottom 85%',
            scrub: 0.6
          }
        }
      );

      // Sequential active workflow step highlight
      const steps = gsap.utils.toArray<HTMLElement>('.workflow-step');
      steps.forEach((step, idx) => {
        gsap.from(step, {
          opacity: 0.8,
          y: 20,
          duration: MOTION_TOKENS.durStandard,
          delay: idx * 0.1,
          scrollTrigger: {
            trigger: '.workflow-timeline',
            start: 'top 80%',
            once: true
          }
        });

        ScrollTrigger.create({
          trigger: step,
          start: 'top 75%',
          end: 'bottom 25%',
          onEnter: () => step.classList.add('active'),
          onEnterBack: () => step.classList.add('active'),
          onLeave: () => step.classList.remove('active'),
          onLeaveBack: () => step.classList.remove('active')
        });
      });

      // 6. Roadmap Section Progressive Entrance
      gsap.from('.roadmap-baseline-card', {
        opacity: 0.8,
        y: 20,
        duration: MOTION_TOKENS.durStandard,
        ease: MOTION_TOKENS.easeGentle,
        scrollTrigger: {
          trigger: '.roadmap-timeline-box',
          start: 'top 85%',
          once: true
        }
      });

      const roadmapCards = gsap.utils.toArray<HTMLElement>('.roadmap-item-card');
      roadmapCards.forEach((card, idx) => {
        // Subtle lateral bias based on column for architectural feel
        const xOffset = idx % 2 === 0 ? -12 : 12;
        gsap.from(card, {
          opacity: 0.8,
          x: xOffset,
          y: 16,
          duration: MOTION_TOKENS.durStandard,
          delay: idx * MOTION_TOKENS.staggerStandard,
          ease: MOTION_TOKENS.easeGentle,
          scrollTrigger: {
            trigger: '.roadmap-grid',
            start: 'top 85%',
            once: true
          }
        });
      });

      // 7. Tech Foundation & Download Section
      gsap.from('.tech-item', {
        opacity: 0.8,
        x: -16,
        duration: MOTION_TOKENS.durStandard,
        stagger: MOTION_TOKENS.staggerTight,
        ease: MOTION_TOKENS.easeGentle,
        scrollTrigger: {
          trigger: '.tech-feature-list',
          start: 'top 85%',
          once: true
        }
      });

      gsap.from('.download-box', {
        opacity: 0.85,
        y: 22,
        duration: MOTION_TOKENS.durStandard,
        ease: MOTION_TOKENS.easeGentle,
        scrollTrigger: {
          trigger: '.download-box',
          start: 'top 85%',
          once: true
        }
      });
    });

    // -------------------------------------------------------------------------
    // TABLET EXPERIENCE (768px – 1023px)
    // Balanced transitions, restrained parallax, no hover-heavy requirements
    // -------------------------------------------------------------------------
    mm.add(MOTION_TOKENS.mediaTablet, () => {
      gsap.from('.hero-content > *', {
        opacity: 0.8,
        y: 14,
        duration: MOTION_TOKENS.durStandard,
        stagger: MOTION_TOKENS.staggerTight,
        ease: MOTION_TOKENS.easeGentle
      });

      initSectionHeaderReveals();

      gsap.from('.workflow-step', {
        opacity: 0.8,
        y: 16,
        duration: MOTION_TOKENS.durFast,
        stagger: MOTION_TOKENS.staggerTight,
        scrollTrigger: {
          trigger: '.workflow-timeline',
          start: 'top 82%',
          once: true
        }
      });

      gsap.from('.roadmap-item-card', {
        opacity: 0.8,
        y: 16,
        duration: MOTION_TOKENS.durFast,
        stagger: MOTION_TOKENS.staggerTight,
        scrollTrigger: {
          trigger: '.roadmap-grid',
          start: 'top 85%',
          once: true
        }
      });
    });

    // -------------------------------------------------------------------------
    // MOBILE EXPERIENCE (<= 767px)
    // Lightweight, fast, 100% native touch scrolling, zero layout shifts
    // -------------------------------------------------------------------------
    mm.add(MOTION_TOKENS.mediaMobile, () => {
      gsap.from('.hero-content > *', {
        opacity: 0.85,
        y: 10,
        duration: MOTION_TOKENS.durFast,
        stagger: 0.05,
        ease: MOTION_TOKENS.easeGentle
      });
      // Content remains immediately visible and responsive without heavy scrub triggers
    });

    // Refresh ScrollTrigger calculations after all static assets decode
    window.addEventListener('load', () => {
      ScrollTrigger.refresh();
    });
  }

  // C. WCAG 2.2.2 Motion Pause/Play Toggle Button
  const motionToggleBtn = document.querySelector<HTMLButtonElement>('#motion-toggle-btn');
  const motionStatusText = document.querySelector<HTMLElement>('#motion-status-text');

  function updateMotionState(paused: boolean): void {
    userPaused = paused;
    document.documentElement.classList.toggle('motion-paused', paused);

    if (paused) {
      gsap.globalTimeline.pause();
    } else {
      gsap.globalTimeline.play();
    }

    if (motionToggleBtn && motionStatusText) {
      motionStatusText.textContent = paused ? 'Putar Animasi' : 'Jeda Animasi';
      motionToggleBtn.setAttribute(
        'aria-label',
        paused ? 'Nyalakan animasi rotasi 3D dan diagram' : 'Jeda animasi rotasi 3D dan diagram'
      );
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
    isMotionPaused: () => userPaused,
    refresh: () => {
      ScrollTrigger.refresh();
    },
    destroy: () => {
      mm?.revert();
      ScrollTrigger.getAll().forEach(t => t.kill());
    }
  };
}

// ---------------------------------------------------------------------------
// 3. Section Header Staggered Reveals Helper
// ---------------------------------------------------------------------------
function initSectionHeaderReveals(): void {
  const headers = document.querySelectorAll<HTMLElement>('.section-header');
  headers.forEach(header => {
    const eyebrow = header.querySelector('.section-eyebrow');
    const title = header.querySelector('.section-title');
    const desc = header.querySelector('.section-desc');

    const elementsToAnimate = [eyebrow, title, desc].filter(Boolean) as HTMLElement[];
    if (elementsToAnimate.length === 0) return;

    gsap.from(elementsToAnimate, {
      opacity: 0.7,
      y: 14,
      duration: MOTION_TOKENS.durStandard,
      stagger: MOTION_TOKENS.staggerTight,
      ease: MOTION_TOKENS.easeGentle,
      scrollTrigger: {
        trigger: header,
        start: 'top 85%',
        once: true
      }
    });
  });
}

// ---------------------------------------------------------------------------
// 4. Scroll Progress Bar
// ---------------------------------------------------------------------------
function initScrollProgressBar(): void {
  const progressBar = document.querySelector<HTMLElement>('#scroll-progress-bar');
  if (!progressBar) return;

  let ticking = false;

  const updateProgress = () => {
    const scrollTop = window.scrollY || document.documentElement.scrollTop;
    const docHeight = document.documentElement.scrollHeight - window.innerHeight;
    const progress = docHeight > 0 ? (scrollTop / docHeight) * 100 : 0;
    progressBar.style.width = `${progress}%`;
    ticking = false;
  };

  window.addEventListener('scroll', () => {
    if (!ticking) {
      window.requestAnimationFrame(updateProgress);
      ticking = true;
    }
  }, { passive: true });
}

// ---------------------------------------------------------------------------
// 5. Sticky Header Elevation Observer
// ---------------------------------------------------------------------------
function initHeaderScrollState(): void {
  const header = document.querySelector<HTMLElement>('.site-header');
  if (!header) return;

  let isScrolled = false;

  const checkScroll = () => {
    const scrolled = window.scrollY > 30;
    if (scrolled !== isScrolled) {
      isScrolled = scrolled;
      header.classList.toggle('header-scrolled', isScrolled);
    }
  };

  window.addEventListener('scroll', checkScroll, { passive: true });
  checkScroll();
}

// ---------------------------------------------------------------------------
// 6. Active Navigation Link Indicator via IntersectionObserver
// ---------------------------------------------------------------------------
function initActiveNavObserver(): void {
  const navLinks = document.querySelectorAll<HTMLAnchorElement>('.site-nav .nav-link, .mobile-nav-link');
  const sections = document.querySelectorAll<HTMLElement>('section[id]');
  if (!sections.length || !navLinks.length) return;

  const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
      if (entry.isIntersecting) {
        const id = entry.target.getAttribute('id');
        navLinks.forEach(link => {
          const href = link.getAttribute('href');
          const isCurrent = href === `#${id}`;
          link.classList.toggle('active', isCurrent);
          if (isCurrent) {
            link.setAttribute('aria-current', 'location');
          } else {
            link.removeAttribute('aria-current');
          }
        });
      }
    });
  }, {
    rootMargin: '-20% 0px -60% 0px',
    threshold: 0
  });

  sections.forEach(sec => observer.observe(sec));
}

// ---------------------------------------------------------------------------
// 7. Mobile Navigation Drawer (Hamburger + Slide-over + Focus Trap)
// ---------------------------------------------------------------------------
function initMobileDrawer(): void {
  const toggleBtn = document.querySelector<HTMLButtonElement>('#mobile-menu-toggle');
  const drawer = document.querySelector<HTMLElement>('#mobile-nav-drawer');
  const closeBtn = document.querySelector<HTMLButtonElement>('#mobile-nav-close');
  const overlay = document.querySelector<HTMLElement>('.mobile-nav-overlay');
  const navLinks = document.querySelectorAll<HTMLAnchorElement>('.mobile-nav-link, .mobile-nav-footer a');

  if (!toggleBtn || !drawer) return;

  function openDrawer(): void {
    drawer?.classList.add('is-open');
    drawer?.setAttribute('aria-hidden', 'false');
    toggleBtn?.setAttribute('aria-expanded', 'true');
    document.body.classList.add('drawer-locked');
    closeBtn?.focus();
  }

  function closeDrawer(): void {
    drawer?.classList.remove('is-open');
    drawer?.setAttribute('aria-hidden', 'true');
    toggleBtn?.setAttribute('aria-expanded', 'false');
    document.body.classList.remove('drawer-locked');
    toggleBtn?.focus();
  }

  toggleBtn.addEventListener('click', openDrawer);
  closeBtn?.addEventListener('click', closeDrawer);
  overlay?.addEventListener('click', closeDrawer);

  navLinks.forEach(link => {
    link.addEventListener('click', () => {
      closeDrawer();
    });
  });

  window.addEventListener('keydown', (e) => {
    if (e.key === 'Escape' && drawer.classList.contains('is-open')) {
      closeDrawer();
    }
  });
}

// ---------------------------------------------------------------------------
// 8. Application Showcase Interactive Hotspots
// ---------------------------------------------------------------------------
function initScreenshotHotspots(): void {
  const hotspots = document.querySelectorAll<HTMLButtonElement>('.ui-hotspot');
  const detailCards = document.querySelectorAll<HTMLElement>('.inspector-card');

  hotspots.forEach(hotspot => {
    hotspot.addEventListener('click', () => {
      const targetId = hotspot.getAttribute('data-target');

      hotspots.forEach(h => {
        const isMatch = h === hotspot;
        h.classList.toggle('active', isMatch);
        h.setAttribute('aria-expanded', isMatch ? 'true' : 'false');
      });

      detailCards.forEach(card => {
        const isTarget = card.getAttribute('id') === targetId;
        card.classList.toggle('active', isTarget);
        if (isTarget) {
          card.focus?.({ preventScroll: true });
          gsap.fromTo(card,
            { scale: 0.98, opacity: 0.85 },
            { scale: 1.0, opacity: 1.0, duration: 0.35, ease: MOTION_TOKENS.easeGentle }
          );
        }
      });
    });
  });

  // Card clicks also activate corresponding hotspot
  detailCards.forEach(card => {
    card.addEventListener('click', () => {
      const cardId = card.getAttribute('id');
      const matchingHotspot = document.querySelector<HTMLButtonElement>(`.ui-hotspot[data-target="${cardId}"]`);
      if (matchingHotspot) {
        matchingHotspot.click();
      }
    });
  });
}

// ---------------------------------------------------------------------------
// 9. 3D Perspective Tilt on Desktop Screenshot Window
// ---------------------------------------------------------------------------
function initScreenshot3DTilt(): void {
  const stage = document.querySelector<HTMLElement>('.screenshot-stage');
  if (!stage || window.innerWidth < 1024) return;

  stage.addEventListener('mousemove', (e: MouseEvent) => {
    const rect = stage.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const rotX = ((y / rect.height) - 0.5) * -4.0;
    const rotY = ((x / rect.width) - 0.5) * 4.5;

    gsap.to(stage, {
      rotateX: rotX,
      rotateY: rotY,
      duration: 0.45,
      ease: 'power1.out',
      transformPerspective: 1200
    });
  });

  stage.addEventListener('mouseleave', () => {
    gsap.to(stage, {
      rotateX: 0,
      rotateY: 0,
      duration: 0.6,
      ease: MOTION_TOKENS.easeGentle
    });
  });
}
