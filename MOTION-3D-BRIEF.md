SABIT — MOTION & 3D MOON DIRECTION

Visual goal
A restrained scientific-editorial landing page with a high-quality, slowly rotating 3D Moon in the hero. The visual should feel like an astronomy instrument / observatory publication, not a gaming site or generic sci-fi template.

Hero Moon
- Real 3D sphere, not a flat illustration or CSS circle.
- Detailed lunar albedo plus normal/bump relief; natural crater distribution and subtle maria.
- Neutral lunar-gray material; controlled directional lighting; readable terminator and soft shadow falloff.
- Very slow rotation: approximately one visual turn every 90–180 seconds.
- Optional subtle drag rotation on desktop; no aggressive pointer parallax.
- A few restrained orbital/coordinate marks are acceptable when they support the composition.
- The Moon is an artistic hero visual, not live lunar-phase data or a scientific simulation.

Motion language
- Short hero entrance for headline / CTA / Moon.
- Small scroll reveals for section groups, triggered once.
- Line-draw reveals for real scientific diagrams.
- Restrained screenshot focus/zoom effects only for actual Sabit screenshots.
- CSS transitions for ordinary controls; use GSAP/ScrollTrigger only if needed.
- No scroll-jacking, spinning sections, excessive parallax, bounce effects, or constant motion everywhere.

Engineering constraints
- Preserve the project's current stack.
- Use Three.js directly, or React Three Fiber only if the project already uses React and it is a good fit.
- Lazy-load the 3D scene; render the headline and CTA immediately.
- Pause rendering when offscreen or when the tab is hidden; clean up WebGL resources.
- Cap pixel ratio / texture quality and simplify on mobile.
- Provide a static fallback if WebGL is unavailable or reduced motion is requested.
- Respect prefers-reduced-motion; no continuous rotation in reduced-motion mode.
- Keep all important content accessible without the canvas.

Acceptance
The first viewport should immediately communicate what Sabit is. The Moon adds identity and atmosphere but must never compete with the product message, hide the CTA, or slow the page enough to affect usability.
