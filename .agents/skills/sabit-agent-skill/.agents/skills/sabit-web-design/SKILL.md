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
