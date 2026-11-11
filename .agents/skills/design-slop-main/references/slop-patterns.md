# Design Slop Symptom Catalog

Use this catalog as an evidence checklist, not as a style-policing scorecard. One symptom may be an intentional choice. Several symptoms with no brand, content, or functional rationale usually indicate design slop.

## 1. Surfaces, Borders, and Containers

| Symptom | Why it is slop | Better direction |
|---|---|---|
| Thick border plus rounded container | Adds visual weight without improving grouping | Remove the container, use whitespace, or use a subtle 1px divider |
| Thick colored left border on a rounded callout | Generic AI callout signature with conflicting geometry | Use a top divider, tint, icon, label, or square-edged semantic bar |
| Every section placed in a card | Avoids making hierarchy decisions | Keep related content on the canvas and reserve cards for independent units |
| White cards on a gray page by default | Creates card soup and excessive fragmentation | Use a white canvas, subtle dividers, or a recessed section where hierarchy requires it |
| Nested rounded cards | Produces container-within-container noise | Flatten the hierarchy and use spacing or one quiet nested well |
| Saturated outlines around ordinary content | Pretends all content is equally important | Reserve strong color for actions and state |
| Different radius on every component | Reveals prompt-by-prompt assembly | Use a small, consistent radius scale |
| Pill shapes everywhere | Makes navigation, labels, buttons, and decoration indistinguishable | Reserve pills for tags, compact filters, and true capsule controls |
| Shadow on every card | Simulates importance and depth | Use elevation only for floating or draggable elements |
| Border, shadow, tint, and glow on the same element | Stacks effects because none has a clear role | Choose one surface treatment |
| Random accent strips | Adds brand color without information | Tie color to a semantic state or remove it |
| Heavy outlines used as decoration | Makes the page feel generated or cartoonishly boxed | Let typography, rhythm, and alignment define sections |

### Hard Check

For cards, panels, callouts, and content boxes:

- `border-width >= 2px` plus `border-radius > 0` is a failure by default.
- A thick border on one side still counts.
- Focus rings, selected states, and an approved brutalist system are narrow exceptions.

## 2. Layout and Hierarchy

| Symptom | Why it is slop | Better direction |
|---|---|---|
| Generic hero, three cards, testimonials, CTA sequence | Uses a template rather than the user's decision journey | Organize sections around questions, objections, proof, and action |
| Alternating image and text sections throughout | Adds variety mechanically | Choose layouts based on the relationship between content and evidence |
| Features forced into groups of three | Lets grid convenience dictate information architecture | Use the number of items the product actually needs |
| Huge empty hero | Confuses scale with impact | Lead with a concrete proposition and useful product evidence |
| Excessive vertical padding | Stretches a weak page | Use spacing tokens and content-driven rhythm |
| Every section centered | Weakens scanning and long-form readability | Center only compact, high-level statements; left-align explanatory content |
| Multiple primary CTAs | Creates competing actions | Choose one page-level primary action |
| Weak section transitions | Makes the page feel like stitched prompts | Use a coherent narrative and consistent container edges |
| Unaligned grids and containers | Breaks visual rhythm | Reuse shared grid definitions, gutters, and boundaries |
| Important details buried below decoration | Optimizes for visual reveal, not decisions | Move proof, price, requirements, and limitations closer to the claim |
| Desktop layout merely stacked on mobile | Preserves source order without mobile intent | Reorder, regroup, simplify, and resize for small-screen tasks |
| Screenshot-friendly but workflow-poor composition | Treats the page as an image | Test the actual user journey and real states |

## 3. Typography

- Headings are enormous but vague.
- A heading is split awkwardly merely to create a dramatic shape.
- Gradient words appear in several headings.
- Body text is tiny beside oversized display type.
- Everything is bold, so nothing has priority.
- Type scales change between sections without a system.
- Several unrelated font families are mixed for “personality.”
- Centered paragraphs run for more than a few lines.
- Lines are too long for comfortable reading or too narrow for natural rhythm.
- Heading levels are selected by appearance instead of document structure.
- Uppercase eyebrows, pill labels, and tiny gray subtitles repeat above every heading.
- Random words are bolded because the paragraph lacks a strong information structure.
- Orphans and bad mobile line breaks are ignored.
- Low-contrast gray is treated as automatically sophisticated.

Better direction: create hierarchy with a restrained type scale, readable measures, clear semantic headings, real content contrast, and deliberate alignment.

## 4. Color, Gradients, Shadows, and Effects

- Purple, blue, and pink gradients appear without brand or content rationale.
- Blurred glowing blobs sit behind the hero.
- Glassmorphism is applied to ordinary content.
- Decorative mesh gradients compete with text contrast.
- Every section introduces a new background treatment.
- Several accent colors lack semantic roles.
- Shadows have inconsistent direction, blur, or density.
- Dark mode is produced by inverting colors rather than designing a real palette.
- Gray text is placed on colored backgrounds instead of using a darker shade or transparency of that color.
- Pure black and pure white are overused to create false drama.
- Decorative noise, grids, particles, or spotlights are layered onto weak composition.
- Hover color changes are applied to noninteractive elements.

Better direction: define neutral, text, surface, accent, state, and focus roles. Use one dominant visual idea and let most of the interface stay quiet.

## 5. Imagery, Icons, Screenshots, and Data

- Abstract 3D objects communicate nothing about the product.
- AI-generated people, hands, devices, or objects contain visual errors.
- Text embedded in generated images is malformed.
- Product mockups contain controls that do not exist.
- A fake dashboard presents impossible or meaningless data.
- Charts are decorative rather than explanatory.
- Stock photos were selected only because their colors match.
- The same person or product changes appearance between images.
- Photography has inconsistent lighting, crops, or art direction.
- Icons mix stroke weights, families, sizes, and metaphors.
- Emoji substitute for a coherent icon system.
- Sparkles are used as the default symbol for AI.
- Screenshots do not match the live product.
- Placeholder logos, avatars, names, or values remain in production.
- Alt text describes the production method instead of the subject or evidence.

Better direction: use real product states, meaningful diagrams, consistent iconography, accurate captions, and assets that help users understand or decide.

## 6. Copy, Proof, and Brand Specificity

- “Revolutionize your workflow.”
- “Unlock the power of...”
- “Elevate your experience.”
- “Seamlessly transform...”
- “Supercharge your productivity.”
- “Built for the future.”
- “Innovation meets simplicity.”
- “Everything you need, all in one place.”
- “Take your business to the next level.”
- “Designed to empower you.”
- Benefits could describe any competitor.
- Buttons say “Learn More” or “Get Started” without predicting the result.
- The heading and paragraph repeat the same thought.
- Claims use unsupported percentages or invented precision.
- Testimonials have no verifiable name, role, company, or context.
- Logo clouds imply relationships that are not explained.
- FAQs exist only to fill a template.
- The page omits price, limitations, requirements, tradeoffs, or wrong-fit users.
- Product terminology changes between sections.
- The company name can be swapped without altering the design or copy.

Better direction: use specific tasks, named features, real constraints, supportable proof, honest limitations, and language drawn from the actual product and audience.

## 7. Interaction and Motion

- Nearly every element animates on scroll.
- Content remains hidden until a reveal animation runs.
- Parallax, cursor followers, tilt, glow, or magnetic buttons add no task value.
- Hover states appear on noninteractive content.
- Interactive elements lack hover, active, focus, disabled, or loading states.
- A carousel hides information that would be clearer in a list or grid.
- Carousels and video autoplay.
- Smooth scrolling feels delayed or overrides user control.
- Mobile interactions depend on hover.
- Motion ignores `prefers-reduced-motion`.
- Counters animate unsupported metrics.
- Fake progress indicators make a simple process feel longer.
- Modals interrupt without a user-triggered reason.
- Sticky UI consumes too much of a small viewport.

Better direction: keep motion functional, brief, interruptible, and subordinate to content. Make state changes clear without turning the interface into a demo reel.

## 8. UX and Product Thinking

- Users cannot quickly tell what the page or product does.
- The target audience is unclear.
- Features are shown without outcomes or use cases.
- Navigation labels are vague.
- Forms ask for information that is not needed.
- Error messages do not explain recovery.
- Empty, loading, error, success, disabled, and permission states are missing.
- Critical details live only in tooltips.
- Destructive actions lack clear confirmation or undo.
- Primary and secondary actions look equally important.
- The interface displays vanity metrics instead of task-relevant information.
- Search, filters, or sorting do not match real user decisions.
- Sign-up is demanded before value, trust, or requirements are clear.
- The design handles only perfect data and short English text.
- Important controls are far from the content they affect.
- The browser Back action breaks the flow.

Better direction: map the main journey, alternative routes, edge cases, recovery, and clear next actions before decorating the page.

## 9. Responsive Design

- Horizontal scrolling appears unexpectedly.
- Tables are clipped instead of adapted.
- Fixed elements cover content or controls.
- Mobile headings remain desktop-sized.
- Cards become extremely tall after stacking.
- Navigation disappears without an equivalent mobile route.
- Images crop out the subject.
- Decorative elements overlap text.
- Tap targets are small or crowded.
- Content order becomes illogical after columns collapse.
- Tablet widths expose awkward empty columns.
- Landscape orientation breaks the layout.
- Modals exceed the viewport or fight the keyboard.
- Primary actions move below long decorative content.

Better direction: compose for mobile, tablet, and desktop tasks. Test realistic content, zoom, keyboard presence, and orientation changes.

## 10. Accessibility and Semantics

- Text or controls fail contrast requirements.
- Color alone communicates state.
- Form labels are missing or replaced by placeholders.
- Focus indicators are absent, clipped, or low contrast.
- Keyboard focus order differs from visual order.
- A dialog does not trap and restore focus correctly.
- Custom controls lack names, roles, states, or keyboard behavior.
- Buttons are implemented as links or generic containers.
- Heading order is invalid.
- Icons have no accessible name when meaning is not visible in text.
- Touch targets are too small.
- Zoom causes clipping, overlap, or loss of functionality.
- Video lacks captions and audio starts automatically.
- Error messages are not programmatically associated with fields.
- Decorative images receive noisy alt text while meaningful images receive generic alt text.
- ARIA is added syntactically but does not match behavior.

Better direction: start with semantic HTML, visible labels, keyboard operation, robust focus, sufficient contrast, and tests with zoom and assistive technology.

## 11. Code and Maintainability

- Basic layout requires a large JavaScript dependency.
- Nearly identical components are duplicated with different names.
- Components are named `Section1`, `Card2`, or other generator placeholders.
- Deep wrapper trees exist only to support effects.
- Arbitrary dimensions, colors, radii, spacing, and z-index values replace tokens.
- Conflicting utilities and repeated `!important` rules accumulate.
- Static styling is scattered across inline attributes.
- Invalid HTML, duplicate IDs, empty links, or dead controls remain.
- Generated code imports nonexistent packages or calls imaginary APIs.
- Placeholder routes and environment-specific URLs are hardcoded.
- Forms look complete but do not submit or validate.
- Comments explain obvious markup but not important architectural choices.
- Design-system components are recreated rather than reused.
- The implementation is polished in one state but brittle under content changes.

Better direction: remove unnecessary layers, reuse project components and tokens, use semantic names, and make structure understandable without reverse-engineering generated noise.

## 12. Performance, SEO, and Trust

- A static page ships a large client-side bundle.
- Oversized images are rendered at thumbnail dimensions.
- Decorative video or animation delays meaningful content.
- Too many fonts, weights, icon sets, trackers, and animation libraries are loaded.
- Below-the-fold media is eager-loaded while the hero image is incorrectly lazy-loaded.
- Layout shifts as fonts, images, or generated widgets load.
- Metadata, canonical URLs, Open Graph images, or structured data describe a different page.
- Fake reviews, partner logos, usage notifications, awards, scarcity, or countdowns create false trust.
- Security, privacy, integration, or performance claims lack support.
- Product screenshots, prices, or features contradict the live product.
- Contact, legal, refund, shipping, or ownership information is missing where users need it.

Better direction: ship only what the page uses, preserve truthful metadata and proof, and make commercial conditions clear before conversion.

## Overall Verdict Signals

The strongest signals are combinations:

- generic visual language plus generic copy
- polished happy path plus missing real states
- excessive cards plus weak information hierarchy
- synthetic proof plus vague claims
- dramatic hero plus no concrete product evidence
- thick rounded borders plus arbitrary accent colors
- responsive stacking plus broken mobile task flow
- accessibility attributes plus unusable keyboard behavior
- attractive screenshots plus brittle or nonexistent implementation

The goal is not minimalism for its own sake. The goal is specific, useful, credible, accessible, maintainable design with every visual decision doing a job.
