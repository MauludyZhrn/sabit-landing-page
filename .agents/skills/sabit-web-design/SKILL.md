---
name: sabit-web-design
description: Designs, builds, and reviews the official Sabit product website and landing page. Use for Sabit marketing pages, feature sections, product screenshots, responsive layouts, visual identity, UX writing, and frontend implementation. Enforces a scientific, credible, restrained visual direction and prevents generic AI-generated aesthetics, emoji icons, fake product UI, and unsupported feature claims.
---

# Sabit Web Design Skill

## Product context

Sabit is a desktop application developed with C++ and Qt 6 for image processing and analysis related to hilal (crescent-moon) observation. The official website must communicate a credible scientific tool, not a generic SaaS startup.

The product's current thesis/research scope is the Basic version. Treat later or premium capabilities as future roadmap items unless the project owner explicitly confirms that they are implemented and ready to present. Never imply a capability is available merely because it appears in a proposal, mockup, discussion, or roadmap.

## Primary goals

1. Explain what Sabit does in plain, accurate language.
2. Help visitors understand the actual workflow and currently implemented capabilities.
3. Establish trust through real product evidence, clear documentation, and honest limitations.
4. Make the website feel designed specifically for astronomy, hilal observation, and scientific image analysis.
5. Deliver a fast, accessible, responsive website that works on desktop, tablet, and mobile.

## Visual direction

Use a scientific editorial direction: precise, calm, restrained, and contemporary. The site should feel closer to a well-designed scientific instrument or research software website than a trendy SaaS template.

Suggested starting palette (confirm against the actual Sabit logo and existing application UI before locking tokens):
- Background: warm near-white `#F5F4EF`
- Primary text: deep ink `#17212B`
- Dark surface: midnight navy `#111D2A`
- Secondary text: muted slate `#64717D`
- Accent: restrained lunar brass `#A9824A`
- Borders: low-contrast neutral `#D9DAD4`

Use color as a functional hierarchy. Do not apply gradients as a default decoration. A dark hero or dark feature section is acceptable when it serves a real contrast or storytelling purpose.

Typography should have a deliberate hierarchy. Prefer a highly legible sans-serif for body and interface copy, with an optional editorial display face for large headings and a monospace face for technical labels or measurements. Avoid using one oversized font treatment everywhere. Keep paragraphs readable and line lengths controlled.

Use the existing Sabit logo and brand assets when available. Do not redesign or distort the logo without explicit instruction.

## Hard prohibitions

- Never use emoji as icons, bullets, decoration, status indicators, or interface controls.
- Use Lucide icons, simple custom SVG, or text labels. Keep icon stroke, optical size, and alignment consistent.
- Do not use generic purple/blue gradient hero backgrounds, glowing blobs, glassmorphism, gratuitous blur, or floating decorative orbs.
- Avoid the default centered headline + two pill buttons + three identical feature cards composition unless research and content genuinely justify it.
- Avoid card grids for every section, excessive rounded rectangles, thick borders, nested cards, and unnecessary shadows.
- Do not fabricate testimonials, customer logos, user counts, performance benchmarks, accuracy claims, certifications, awards, or institutional partnerships.
- Do not invent product screens, charts, measurement results, workflows, supported formats, operating systems, or features. Use screenshots and facts supplied by the project owner. If unavailable, use clearly labeled illustrative diagrams rather than fake application UI.
- Do not present planned/pro features as available in the current Basic build.
- Do not use vague marketing phrases such as "revolutionary", "game-changing", "next-generation", or "unlock the power".

## Content and information architecture

Build the page around visitor questions, not around a stack of decorative sections. A useful initial structure is:

1. Header: Sabit identity, concise navigation, and one clear primary action.
2. Hero: one specific statement describing Sabit, one supporting sentence, one primary CTA, and authentic product evidence (prefer a real application screenshot).
3. Product explanation: clarify the problem Sabit addresses and its role in a hilal image-analysis workflow.
4. Current capabilities: show only verified features available in the current Basic version. Use short descriptions and evidence, not generic feature slogans.
5. Workflow: explain the real sequence from input image to processing/analysis/output only when confirmed by the actual application.
6. Product preview: real screenshots, annotated crops, or accurate diagrams. Label illustrative material clearly.
7. Technical foundation: mention C++ and Qt 6 only as factual implementation details where useful; do not turn the section into a technology-logo parade.
8. Documentation / download / contact: show only destinations and download assets that exist.
9. Footer: product identity, version or release metadata only when verified, and relevant links.

This is a starting structure, not a mandatory checklist. Remove sections that do not answer a real visitor question.

## UX and interaction rules

- Every CTA must have a real destination or a clearly implemented action.
- Keep navigation concise and use descriptive labels.
- Use semantic HTML, keyboard-accessible controls, visible focus states, sufficient contrast, and meaningful alt text.
- Provide responsive layouts designed for narrow screens; do not merely shrink the desktop layout.
- Respect reduced-motion preferences. Motion should clarify state, sequence, or hierarchy—not decorate empty space.
- Use subtle transitions and restrained scroll effects. Avoid parallax-heavy or continuously moving backgrounds.
- Optimize image formats, font loading, layout stability, and page weight.
- Include meaningful metadata, page title, description, favicon, and social preview metadata when implementing the complete site.

## Implementation workflow

1. Inspect the repository before changing files. Identify its framework, scripts, assets, existing styles, and routing. Preserve the existing stack unless there is a concrete reason to change it.
2. Ask the repository for the current Sabit logo, screenshots, verified feature list, download destination, and contact/documentation links. If those assets or facts are not present, create explicit placeholders and report them; do not silently invent replacements.
3. Write a compact design brief: audience, primary visitor task, key message, page outline, art direction, typography, color tokens, and CTA destination.
4. Build the page with reusable components and a small, coherent token system. Keep dependencies lean.
5. Populate the page only with verified copy and actual assets. Keep future roadmap information separate from current functionality.
6. Run the available lint, type-check, build, and test commands. Fix errors before completion.
7. Preview at desktop (1440px), tablet (768px), and mobile (390px) widths. Inspect spacing, text wrapping, navigation, image crops, contrast, and horizontal overflow.
8. Audit the rendered page for generic AI patterns, accessibility, broken interactions, unsupported claims, and visual inconsistencies. Revise rather than merely describing issues.
9. Summarize changed files, implemented sections, tests/build results, and any remaining placeholders or facts requiring owner confirmation.

## Completion checklist

- [ ] The page feels specific to Sabit and hilal image analysis.
- [ ] No emoji are used anywhere in the interface.
- [ ] No fake screenshots, fake metrics, invented claims, or unverified capabilities are present.
- [ ] Basic/current functionality is clearly separated from future plans.
- [ ] Typography, spacing, color, and iconography form a coherent system.
- [ ] The page is responsive at desktop, tablet, and mobile sizes.
- [ ] Keyboard navigation, focus states, contrast, and semantic structure are acceptable.
- [ ] All CTAs and navigation links work.
- [ ] Build and available checks pass, or failures are explicitly reported.

## Motion design and 3D lunar centerpiece

The landing page should include purposeful motion and a convincing 3D Moon as a signature visual, especially in the hero. Treat this as a restrained scientific visualization, not a game scene or a generic space-themed template.

### 3D Moon art direction

- Build a true 3D spherical Moon with visible curvature, crater relief, surface albedo variation, and a clear day/night terminator. A flat circle, emoji, CSS sphere, or stock illustration does not satisfy this requirement.
- Use a high-quality lunar albedo texture plus a normal/bump/displacement map when available. If suitable licensed assets are unavailable, create a procedural lunar material/texture rather than silently downloading unverified assets.
- Use realistic, restrained lighting: one readable key light, subtle fill, and natural shadow falloff. Keep the surface neutral lunar gray with restrained warm highlights that harmonize with the Sabit palette. Avoid neon blue/purple glow, exaggerated halos, cartoon craters, and glossy plastic material.
- Place the Moon as a composed hero object with deliberate negative space for the headline and CTA. It may be partly cropped by the viewport if that improves composition. Add only a few fine orbital arcs, coordinate ticks, or observation marks when they communicate the astronomy theme.
- Give the Moon a very slow axial rotation (roughly one visual turn over 90–180 seconds) and a restrained lighting transition. It should feel calm and almost imperceptible, never spin like a loading icon.
- Optional pointer interaction: allow subtle drag-to-rotate on capable desktop devices. Do not add strong cursor-following parallax, forced camera movement, or motion that makes text difficult to read. On touch screens, keep interaction simple and avoid hijacking page scroll.
- Clearly treat this 3D Moon as a hero illustration. Do not imply it represents the live Moon phase, current sky conditions, real observation data, or an accurate simulation unless the project implements and verifies those calculations.

### Implementation choices

- Inspect the existing project stack first and preserve it.
- For a React-based site, use Three.js directly or React Three Fiber only when it fits the existing architecture. For a non-React site, use Three.js directly. Do not introduce a framework migration just to add 3D.
- Use GSAP/ScrollTrigger for a small number of coordinated, scroll-triggered sequences only when the project benefits from it. Use CSS transitions for simple hover/focus states. Do not add multiple overlapping animation libraries.
- Prefer a small, isolated `MoonScene`/`MoonCanvas` component with clear lifecycle cleanup. Dispose geometries, materials, and textures on unmount. Avoid leaking WebGL contexts or event listeners.
- Use the renderer's supported animation-loop API. Pause or reduce rendering when the canvas is offscreen or the document is hidden; avoid unnecessary full-frame rendering when the scene is static.
- Dynamically import or lazy-load the 3D scene so the first meaningful text and CTA render immediately. Do not block Largest Contentful Paint on the WebGL scene.
- Keep texture sizes and device pixel ratio appropriate to the device. Cap pixel ratio (for example, at 1.5 on desktop and lower on mobile), reduce texture/geometry detail on mobile, avoid expensive post-processing, and do not enable costly real-time shadows unless they materially improve the result.
- Provide a visually consistent static poster/fallback image for browsers without WebGL, low-power devices, failed context creation, and reduced-motion preferences. The content and CTA must remain fully usable without 3D.
- Respect `prefers-reduced-motion`: disable continuous rotation, camera parallax, and nonessential scroll choreography; show a stable Moon poster and make content immediately available. Never require motion to understand the page.
- Keep all motion non-blocking, keyboard-safe, and compatible with browser zoom. Do not hijack scrolling or use flashing, rapid zooms, spinning sections, or long pinned-scroll experiences.

### Motion system across the page

Use a coherent motion language rather than animating every element:

1. Hero entrance: a short, quiet reveal of headline, supporting copy, CTA, and Moon. The Moon can fade in with a small scale settle; keep text movement minimal.
2. Scroll reveals: reveal only meaningful section groups with small opacity/position changes. Trigger once where possible; do not animate every paragraph or card independently.
3. Scientific diagrams: use line-drawing or mask reveals for orbital paths, measurement guides, and phase diagrams when those graphics are actually present and accurate.
4. Product screenshots: use restrained crop/zoom or annotated focus states to guide attention. Never animate a fabricated application interface.
5. Interaction feedback: consistent short transitions for navigation, buttons, tabs, and controls, with clear hover, focus, active, and disabled states.

Set timing by purpose: micro-interactions approximately 150–250 ms; content reveals approximately 350–700 ms. Use restrained easing and avoid spring/bounce effects unless there is a clear interaction reason.

### Performance and accessibility checks

- Confirm the site renders useful content before the 3D asset has loaded.
- Confirm WebGL failure produces a stable fallback rather than a blank hero or runtime crash.
- Confirm the Moon scene pauses when offscreen/backgrounded and cleans up on unmount.
- Test reduced-motion mode and confirm there is no continuous animation in that mode.
- Test mobile layout and a low-power configuration; the page must remain readable and responsive.
- Do not encode essential information only in the canvas. Provide an accessible text equivalent when the visualization communicates meaningful information.
- Avoid autoplay audio, flashing, scroll-jacking, and forced cursor effects.

## Additional completion checklist: motion and 3D

- [ ] The hero contains a true 3D Moon with visible surface detail and realistic light/shadow.
- [ ] The Moon rotates slowly and does not distract from the main message.
- [ ] The Moon is clearly decorative unless a verified scientific simulation is implemented.
- [ ] The page uses a small, consistent set of purposeful motion patterns.
- [ ] Reduced-motion mode removes continuous and nonessential motion.
- [ ] A static fallback is available for unsupported WebGL and low-power/mobile cases.
- [ ] The 3D scene is lazy-loaded, pauses when not visible, and disposes resources correctly.
- [ ] The page remains fast, accessible, and fully usable without WebGL.


## Visual revision guidance from the Sabit reference audit

When revising the existing landing page, inspect the current screenshot and distinguish it from the supplied visual reference. The current implementation's Moon texture may look like branching veins or a black cut-out rather than lunar maria/craters. Replace the material with verified LRO/LOLA-derived texture/elevation data; do not merely enlarge the existing texture. The reference contributes depth, layered orbital composition, focal-object scale, and restrained information labels. Do not reproduce its unrelated space-mission dashboard, invented metrics, avatar, or branding.

Use NASA SVS CGI Moon Kit as the preferred source for suitable Moon color/elevation maps: https://svs.gsfc.nasa.gov/4720/. Keep web assets compact (for example, 2K color + a lightweight elevation/normal map), credit the actual data source, and avoid loading multi-gigabyte scientific files into a browser. Three.js physically based materials can combine color, normal/bump, and displacement maps; select only what the project can render efficiently.

## Roadmap section

Place Roadmap / Arah Pengembangan as the final major section before the footer. Clearly distinguish the current Basic baseline from potential future directions. Candidate topics may include image-processing workflow improvements, observation-data visualization, device interoperability, and documentation/reporting. Treat these as ideas or exploration unless the owner confirms committed work. Do not invent release dates, quarters, percentages, or active statuses. Use a restrained animated timeline with a drawn progress path and staggered milestones; keep all content readable in a vertical flow on mobile.

## Repository and installer links

Offer separate destinations for Windows installer, GitHub source repository, and GitHub Releases. Detect the real GitHub `origin` remote rather than inventing an owner/repository. Search for the owner's prepared `.exe` in the workspace and verify its filename/version/architecture. A website can serve a local copy under `public/downloads/` using a relative URL, or use an exact asset URL from a published GitHub Release. Never link to a Windows filesystem path, `#`, or an unverified release asset. GitHub's documented latest-release patterns are `https://github.com/OWNER/REPO/releases/latest` and `https://github.com/OWNER/REPO/releases/latest/download/ASSET_FILENAME.exe`. Hide unavailable CTAs or show a clear unavailable state instead of leaving broken links.
