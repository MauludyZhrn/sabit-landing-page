// ============================================================================
// SABIT MOTION DESIGN SYSTEM & ARCHITECTURE (V2 — PHASE 3)
// Precision Observatory Motion Engine
// Built with GSAP, ScrollTrigger, and Responsive matchMedia.
// Section-specific cinematic experiences:
//   - Astronomical Geometry: Coordinated vector line-drawing & MABIMS spotlight
//   - Authentic Evidence Showcase: Aperture mask reveal, sequential hotspots, & 3D tilt
//   - Interactive Workflow: Dual-axis scroll-linked progress track & stage nodes
//   - Research Roadmap: Branching SVG timeline spine, milestone nodes, & distinct status beacons
//   - Technical & Download: Precision micro-interactions, tactile buttons, & WCAG 2.2.2 controls
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
  easeSpring: 'back.out(1.8)',
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

      // 4. Evidence Section: Cinematic Screenshot Mask Reveal, Sequential Hotspots & 3D Tilt
      const screenshotStage = document.querySelector<HTMLElement>('.screenshot-stage');
      if (screenshotStage) {
        const evidenceTl = gsap.timeline({
          scrollTrigger: {
            trigger: screenshotStage,
            start: 'top 82%',
            once: true
          }
        });

        evidenceTl
          .from(screenshotStage, {
            opacity: 0.8,
            y: 28,
            scale: 0.985,
            duration: MOTION_TOKENS.durCinematic,
            ease: MOTION_TOKENS.easeGentle,
            onComplete: () => {
              screenshotStage.classList.add('stage-lit');
            }
          })
          .fromTo('.ui-hotspot',
            { scale: 0, opacity: 0 },
            {
              scale: 1,
              opacity: 1,
              duration: 0.45,
              stagger: 0.08,
              ease: MOTION_TOKENS.easeSpring
            },
            '-=0.35'
          );

        initScreenshot3DTilt();
      }

      // 5. Workflow: Horizontal Progress Track & Sequential Stage Nodes
      const workflowTimeline = document.querySelector<HTMLElement>('.workflow-timeline');
      const desktopFill = document.querySelector<HTMLElement>('.workflow-desktop-fill');
      const steps = gsap.utils.toArray<HTMLElement>('.workflow-step');
      const desktopNodes = gsap.utils.toArray<HTMLElement>('.workflow-desktop-nodes .workflow-node');

      if (workflowTimeline && desktopFill) {
        ScrollTrigger.create({
          trigger: workflowTimeline,
          start: 'top 75%',
          end: 'bottom 60%',
          scrub: 0.5,
          onUpdate: (self) => {
            const p = self.progress;
            desktopFill.style.width = `${Math.max(15, p * 100)}%`;

            // Sequential step highlighting based on progress quartile
            const activeIdx = Math.min(Math.floor(p * steps.length), steps.length - 1);
            steps.forEach((step, idx) => {
              const isActive = idx === activeIdx;
              const isPast = idx < activeIdx;
              step.classList.toggle('active', isActive);
              step.classList.toggle('completed', isPast);
            });

            desktopNodes.forEach((node, idx) => {
              node.classList.toggle('active', idx === activeIdx);
              node.classList.toggle('completed', idx < activeIdx);
            });
          }
        });

        // Entrance stagger for workflow cards
        gsap.from(steps, {
          opacity: 0.8,
          y: 22,
          duration: MOTION_TOKENS.durStandard,
          stagger: 0.10,
          ease: MOTION_TOKENS.easeGentle,
          scrollTrigger: {
            trigger: workflowTimeline,
            start: 'top 82%',
            once: true
          }
        });
      }

      // 6. Roadmap: Branching SVG Connector Line & Progressive Area Cards
      const roadmapBox = document.querySelector<HTMLElement>('.roadmap-timeline-box');
      const connectorLines = document.querySelectorAll<SVGGeometryElement>(
        '.roadmap-stem-line, .roadmap-spine-line, .roadmap-branch-line'
      );
      const connectorCircles = document.querySelectorAll<SVGElement>('.roadmap-node-circle');

      if (roadmapBox) {
        // Pre-measure connector paths
        connectorLines.forEach(line => {
          try {
            const len = line.getTotalLength();
            line.style.strokeDasharray = `${len}`;
            line.style.strokeDashoffset = `${len}`;
          } catch {
            line.style.strokeDashoffset = '0';
          }
        });

        const roadmapTl = gsap.timeline({
          scrollTrigger: {
            trigger: roadmapBox,
            start: 'top 82%',
            once: true
          }
        });

        // Step A: Baseline Card Entrance
        roadmapTl.from('.roadmap-baseline-card', {
          opacity: 0.8,
          y: 20,
          duration: MOTION_TOKENS.durStandard,
          ease: MOTION_TOKENS.easeGentle
        });

        // Step B: Draw connecting SVG branches
        connectorLines.forEach((line) => {
          roadmapTl.to(line, {
            strokeDashoffset: 0,
            duration: 0.4,
            ease: 'power1.out'
          }, '-=0.2');
        });

        // Step C: Pop in junction dots
        if (connectorCircles.length) {
          roadmapTl.fromTo(connectorCircles,
            { scale: 0, transformOrigin: 'center center' },
            { scale: 1, duration: 0.35, stagger: 0.05, ease: MOTION_TOKENS.easeSpring },
            '-=0.25'
          );
        }

        // Step D: Stagger reveal the 4 Exploration Cards with subtle architectural bias
        const roadmapCards = gsap.utils.toArray<HTMLElement>('.roadmap-item-card');
        roadmapCards.forEach((card, idx) => {
          const xOffset = idx % 2 === 0 ? -12 : 12;
          roadmapTl.from(card, {
            opacity: 0.8,
            x: xOffset,
            y: 18,
            duration: MOTION_TOKENS.durStandard,
            ease: MOTION_TOKENS.easeGentle
          }, `-=${idx === 0 ? 0.2 : 0.35}`);
        });
      }

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

      // Evidence reveal
      gsap.from('.screenshot-stage', {
        opacity: 0.85,
        y: 20,
        duration: MOTION_TOKENS.durStandard,
        scrollTrigger: {
          trigger: '.screenshot-stage',
          start: 'top 85%',
          once: true
        }
      });

      // Workflow cards reveal
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

      // Roadmap cards reveal
      gsap.from('.roadmap-baseline-card, .roadmap-item-card', {
        opacity: 0.8,
        y: 16,
        duration: MOTION_TOKENS.durFast,
        stagger: MOTION_TOKENS.staggerTight,
        scrollTrigger: {
          trigger: '.roadmap-timeline-box',
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

      // Vertical workflow progress bar linking to native scroll
      const mobileTrack = document.querySelector<HTMLElement>('.workflow-timeline');
      const mobileFill = document.querySelector<HTMLElement>('.workflow-progress-fill');
      if (mobileTrack && mobileFill) {
        gsap.fromTo(mobileFill,
          { scaleY: 0 },
          {
            scaleY: 1,
            ease: 'none',
            scrollTrigger: {
              trigger: mobileTrack,
              start: 'top 80%',
              end: 'bottom 60%',
              scrub: 0.5
            }
          }
        );
      }
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
// 8. Application Showcase Interactive Hotspots & Bidirectional Card Sync
// ---------------------------------------------------------------------------
function initScreenshotHotspots(): void {
  const hotspots = document.querySelectorAll<HTMLButtonElement>('.ui-hotspot');
  const detailCards = document.querySelectorAll<HTMLElement>('.inspector-card');

  function activateSubsystem(targetId: string | null): void {
    if (!targetId) return;

    hotspots.forEach(h => {
      const isMatch = h.getAttribute('data-target') === targetId;
      h.classList.toggle('active', isMatch);
      h.setAttribute('aria-expanded', isMatch ? 'true' : 'false');
    });

    detailCards.forEach(card => {
      const isMatch = card.getAttribute('id') === targetId;
      card.classList.toggle('active', isMatch);
      card.setAttribute('aria-selected', isMatch ? 'true' : 'false');
      if (isMatch) {
        card.focus?.({ preventScroll: true });
        gsap.fromTo(card,
          { scale: 0.98, opacity: 0.85 },
          { scale: 1.0, opacity: 1.0, duration: 0.35, ease: MOTION_TOKENS.easeGentle }
        );

        // On mobile viewports, scroll card into view gently if below fold
        if (window.innerWidth < 768) {
          card.scrollIntoView({ behavior: 'smooth', block: 'nearest' });
        }
      }
    });
  }

  // Hotspot click & keyboard activation
  hotspots.forEach(hotspot => {
    hotspot.addEventListener('click', () => {
      const targetId = hotspot.getAttribute('data-target');
      activateSubsystem(targetId);
    });

    hotspot.addEventListener('keydown', (e: KeyboardEvent) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        const targetId = hotspot.getAttribute('data-target');
        activateSubsystem(targetId);
      }
    });
  });

  // Card click & keyboard activation
  detailCards.forEach(card => {
    card.addEventListener('click', () => {
      const cardId = card.getAttribute('id');
      activateSubsystem(cardId);
    });

    card.addEventListener('keydown', (e: KeyboardEvent) => {
      if (e.key === 'Enter' || e.key === ' ') {
        e.preventDefault();
        const cardId = card.getAttribute('id');
        activateSubsystem(cardId);
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
