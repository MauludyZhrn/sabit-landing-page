// Sabit Centralized Motion & Interaction Engine
// Powered by GSAP, ScrollTrigger, and Responsive matchMedia.
// Provides cinematic reveals, 3D perspective tilts, scroll scrubbing,
// interactive hotspots, scroll progress bar, mobile drawer, and WCAG pause control.

import gsap from 'gsap';
import { ScrollTrigger } from 'gsap/ScrollTrigger';

gsap.registerPlugin(ScrollTrigger);

export interface MotionSystemController {
  toggleMotion: () => boolean;
  isMotionPaused: () => boolean;
  destroy: () => void;
}

export function initMotionSystem(): MotionSystemController {
  const prefersReduced = window.matchMedia('(prefers-reduced-motion: reduce)').matches;
  let userPaused = prefersReduced;
  let mm: gsap.MatchMedia | null = null;

  // 1. Scroll Progress Bar
  initScrollProgressBar();

  // 2. Active Navigation Section Observer
  initActiveNavObserver();

  // 3. Mobile Navigation Drawer
  initMobileDrawer();

  // 4. Interactive Screenshot Hotspots (Desktop & Mobile)
  initScreenshotHotspots();

  // 5. GSAP Responsive Timelines & ScrollTriggers
  if (!prefersReduced) {
    mm = gsap.matchMedia();

    // Desktop Viewport (1024px and above)
    mm.add('(min-width: 1024px)', () => {
      // A. Hero Staggered Entrance
      const heroTl = gsap.timeline({ defaults: { ease: 'power3.out' } });
      heroTl
        .fromTo('.hero-eyebrow', { opacity: 0, y: -10 }, { opacity: 1, y: 0, duration: 0.6, delay: 0.1 })
        .fromTo('.hero-title', { opacity: 0, y: 24 }, { opacity: 1, y: 0, duration: 0.8 }, '-=0.4')
        .fromTo('.hero-lead', { opacity: 0, y: 16 }, { opacity: 1, y: 0, duration: 0.7 }, '-=0.5')
        .fromTo('.hero-actions .btn-primary, .hero-actions .btn-secondary-dark', { opacity: 0, y: 14 }, { opacity: 1, y: 0, duration: 0.6, stagger: 0.1 }, '-=0.4')
        .fromTo('.hero-badges-capsule .badge-item', { opacity: 0, scale: 0.92, y: 8 }, { opacity: 1, scale: 1, y: 0, duration: 0.5, stagger: 0.08 }, '-=0.3')
        .fromTo('.hero-visual-stage', { opacity: 0, scale: 0.94 }, { opacity: 1, scale: 1.0, duration: 1.1, ease: 'power2.out' }, '-=0.9');

      // B. Hero Scroll Scrub (Parallax on Moon Stage & Background)
      gsap.to('.hero-visual-stage', {
        scrollTrigger: {
          trigger: '.hero-section',
          start: 'top top',
          end: 'bottom top',
          scrub: 1.0
        },
        y: 60,
        opacity: 0.65,
        ease: 'none'
      });

      // C. Application Showcase (Perspective Tilt & Clip-path reveal)
      gsap.fromTo('.screenshot-stage', 
        { clipPath: 'inset(10% 8% 10% 8% round 14px)', opacity: 0.7 },
        {
          clipPath: 'inset(0% 0% 0% 0% round 14px)',
          opacity: 1,
          duration: 1.0,
          ease: 'power3.out',
          scrollTrigger: {
            trigger: '.screenshot-stage',
            start: 'top 80%',
            once: true
          }
        }
      );

      // Desktop 3D perspective mouse tilt on screenshot window
      initScreenshot3DTilt();

      // D. Workflow Connected Timeline Progress Line
      gsap.fromTo('.workflow-progress-fill',
        { scaleY: 0 },
        {
          scaleY: 1,
          ease: 'none',
          transformOrigin: 'top center',
          scrollTrigger: {
            trigger: '.workflow-timeline',
            start: 'top 75%',
            end: 'bottom 85%',
            scrub: 0.6
          }
        }
      );

      // Sequentially activate workflow steps
      const steps = gsap.utils.toArray<HTMLElement>('.workflow-step');
      steps.forEach((step) => {
        gsap.fromTo(step,
          { opacity: 0.35, y: 20 },
          {
            opacity: 1,
            y: 0,
            duration: 0.6,
            scrollTrigger: {
              trigger: step,
              start: 'top 78%',
              onEnter: () => step.classList.add('active'),
              onLeaveBack: () => step.classList.remove('active')
            }
          }
        );
      });

      // E. Roadmap Section Progressive Reveal
      gsap.fromTo('.roadmap-baseline-card',
        { opacity: 0, y: 20 },
        {
          opacity: 1,
          y: 0,
          duration: 0.7,
          ease: 'power2.out',
          scrollTrigger: {
            trigger: '.roadmap-timeline-box',
            start: 'top 80%',
            once: true
          }
        }
      );

      gsap.fromTo('.roadmap-item-card',
        { opacity: 0, y: 24 },
        {
          opacity: 1,
          y: 0,
          duration: 0.6,
          stagger: 0.14,
          ease: 'power2.out',
          scrollTrigger: {
            trigger: '.roadmap-grid',
            start: 'top 80%',
            once: true
          }
        }
      );

      // F. Tech Features Progressive Reveal
      gsap.fromTo('.tech-item',
        { opacity: 0, x: -16 },
        {
          opacity: 1,
          x: 0,
          duration: 0.6,
          stagger: 0.12,
          ease: 'power2.out',
          scrollTrigger: {
            trigger: '.tech-feature-list',
            start: 'top 80%',
            once: true
          }
        }
      );
    });

    // Tablet Viewport (768px – 1023px)
    mm.add('(min-width: 768px) and (max-width: 1023px)', () => {
      gsap.fromTo('.hero-content > *',
        { opacity: 0, y: 16 },
        { opacity: 1, y: 0, duration: 0.6, stagger: 0.1, ease: 'power2.out' }
      );
      gsap.fromTo('.workflow-step',
        { opacity: 0, y: 16 },
        {
          opacity: 1,
          y: 0,
          stagger: 0.12,
          duration: 0.5,
          scrollTrigger: { trigger: '.workflow-timeline', start: 'top 80%', once: true }
        }
      );
    });

    // Mobile Viewport (360px – 767px)
    mm.add('(max-width: 767px)', () => {
      // Lightweight entrance without heavy scrub or pinning
      gsap.fromTo('.hero-content > *',
        { opacity: 0, y: 12 },
        { opacity: 1, y: 0, duration: 0.5, stagger: 0.08, ease: 'power2.out' }
      );

      const revealGroups = gsap.utils.toArray<HTMLElement>('.reveal-group');
      revealGroups.forEach(group => {
        gsap.fromTo(group,
          { opacity: 0, y: 16 },
          {
            opacity: 1,
            y: 0,
            duration: 0.6,
            ease: 'power2.out',
            scrollTrigger: {
              trigger: group,
              start: 'top 88%',
              once: true
            }
          }
        );
      });
    });
  }

  // 6. WCAG 2.2.2 Motion Pause/Play Toggle Button
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
    destroy: () => {
      mm?.revert();
      ScrollTrigger.getAll().forEach(t => t.kill());
    }
  };
}

// ---------------------------------------------------------------------------
// Scroll Progress Bar
// ---------------------------------------------------------------------------
function initScrollProgressBar(): void {
  const progressBar = document.querySelector<HTMLElement>('#scroll-progress-bar');
  if (!progressBar) return;

  window.addEventListener('scroll', () => {
    const scrollTop = window.scrollY || document.documentElement.scrollTop;
    const docHeight = document.documentElement.scrollHeight - window.innerHeight;
    const progress = docHeight > 0 ? (scrollTop / docHeight) * 100 : 0;
    progressBar.style.width = `${progress}%`;
  }, { passive: true });
}

// ---------------------------------------------------------------------------
// Active Navigation Link Indicator via IntersectionObserver
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
// Mobile Navigation Drawer (Hamburger + Slide-over + Focus Trap)
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
// Application Showcase Interactive Hotspots
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
          gsap.fromTo(card, { scale: 0.98, opacity: 0.8 }, { scale: 1.0, opacity: 1.0, duration: 0.35, ease: 'power2.out' });
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
// 3D Perspective Tilt on Desktop Screenshot Window
// ---------------------------------------------------------------------------
function initScreenshot3DTilt(): void {
  const stage = document.querySelector<HTMLElement>('.screenshot-stage');
  if (!stage || window.innerWidth < 1024) return;

  stage.addEventListener('mousemove', (e: MouseEvent) => {
    const rect = stage.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;
    const rotX = ((y / rect.height) - 0.5) * -4.5;
    const rotY = ((x / rect.width) - 0.5) * 5.0;

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
      ease: 'power2.out'
    });
  });
}
