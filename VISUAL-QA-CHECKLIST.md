# Sabit landing page — visual QA checklist

## Visual design
- [ ] The visual language feels specific to astronomy/hilal image analysis, not a copied gaming dashboard.
- [ ] Hero composition has a clear focal point and negative space.
- [ ] Language and terminology are consistent.
- [ ] The design uses no emoji.
- [ ] No fake metrics, fake product UI, unsupported feature claims, or invented testimonials.

## 3D Moon
- [ ] True 3D sphere with verified lunar color/albedo texture.
- [ ] Visible maria, highlands, craters and restrained topographic relief.
- [ ] No strange branching/radial texture patterns or fake black crater holes.
- [ ] Matte surface, natural directional light and readable terminator.
- [ ] Rotation is slow and purposeful.
- [ ] Moon visual is not described as live data or scientific simulation.
- [ ] Static fallback displays if WebGL fails.

## Motion
- [ ] Hero entrance is subtle and short.
- [ ] Scroll reveals trigger once and do not hide important content.
- [ ] Roadmap line/milestone motion works without scroll-jacking.
- [ ] Hover, focus, pressed and active states are distinct.
- [ ] `prefers-reduced-motion` disables continuous/nonessential motion.
- [ ] Canvas pauses offscreen/tab-hidden and cleans up resources.

## UX and responsive
- [ ] Desktop 1440px.
- [ ] Laptop 1024px.
- [ ] Tablet 768px.
- [ ] Mobile 390px.
- [ ] No horizontal overflow.
- [ ] Keyboard navigation, visible focus, semantic headings, alt text and contrast.
- [ ] Headline and CTA appear before WebGL finishes loading.

## Downloads
- [ ] GitHub repo destination comes from the actual origin remote or owner-provided URL.
- [ ] Releases URL opens the actual repository release page.
- [ ] Installer CTA targets the verified `.exe` or release asset.
- [ ] No active placeholder URL or `href="#"`.
- [ ] Version, architecture and file size match the actual installer.

## Build
- [ ] Lint passes.
- [ ] Typecheck passes.
- [ ] Tests pass (if available).
- [ ] Production build passes.
- [ ] Remaining blockers are reported honestly.
