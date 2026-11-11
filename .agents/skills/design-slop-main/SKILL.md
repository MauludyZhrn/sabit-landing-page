---
name: design-slop
description: Detect, prevent, and remove generic AI-looking web design patterns. Use when creating, reviewing, auditing, or refining websites, landing pages, dashboards, WordPress blocks, and UI components.
user-invokable: true
args:
  - name: target
    description: The page, component, design, screenshot, or codebase to review (optional)
    required: false
---

# Design Slop

Prevent and remove web design that looks plausible at first glance but lacks hierarchy, context, restraint, or real product thinking.

AI use is not the problem. Unedited, generic, decorative, inaccessible, or nonfunctional design is the problem. Judge the artifact, not the tool that made it.

## Use This Skill For

- creating a new web page, component, dashboard, landing page, or WordPress block
- reviewing screenshots, Figma designs, HTML, CSS, Tailwind, JSX, templates, or rendered pages
- removing generic SaaS styling, card soup, decorative clutter, fake UI, or template residue
- making a design feel specific to its brand, audience, content, and task
- checking whether visual polish is hiding weak UX, accessibility, performance, or implementation

Apply this skill proactively during design work. Do not wait for the user to complain that a finished page looks AI-generated.

## Load Contract

Read these before auditing or implementing:

1. [Slop symptom catalog](references/slop-patterns.md)
2. [Before and after examples](references/before-after-examples.md)
3. The target project's nearest design system, brand files, components, and existing page patterns

When generating or editing UI, also load the environment's general design skill or guidelines when available. Repo-local design rules outrank this general skill.

## Non-Negotiable Surface Rule

**Never combine thick borders with rounded cards, callouts, panels, content boxes, or section containers.**

For this skill, a thick border means `2px` or more. This includes a thick border on only one side.

Reject patterns such as:

- `border-2 rounded-xl`
- `border-4 rounded-2xl`
- a thick colored left border on a rounded callout
- rounded cards outlined in saturated brand colors
- nested rounded panels, each with its own visible border
- heavy dark outlines used to make weak grouping look intentional

Default replacements, in order:

1. whitespace and alignment
2. a subtle `1px` divider or hairline border
3. a restrained background tint or recessed well
4. a small icon, label, or status dot when semantic emphasis is needed
5. a shadow or elevated surface only when the element truly floats

Use a thick outline only when it is functionally required, such as a focus indicator, a selected state that needs strong contrast, or an explicitly approved brutalist visual system. Do not turn that exception into a general card style.

## Core Test

A design is likely slop when several of these statements are true:

- It communicates mood more clearly than purpose.
- It contains more decoration than useful information.
- It looks familiar but not identifiable.
- The business name could be swapped without changing the page.
- It was composed for a screenshot instead of a real user journey.
- It covers only the ideal state and ignores errors, empty states, and edge cases.
- It uses cards, gradients, blur, shadows, or animation to compensate for weak hierarchy.
- It makes claims without evidence or displays data that the product cannot produce.
- It has not been tested with real content, devices, keyboard input, or realistic constraints.

Do not call a design slop based on one fashionable technique. Diagnose repeated patterns and explain the user-facing cost.

## Workflow

### 1. Establish Context

Inspect the target before proposing a visual direction:

- audience and primary task
- brand personality and existing visual language
- page purpose and conversion goal
- real content, product states, and data
- existing tokens, components, breakpoints, and interaction conventions
- accessibility, performance, and platform constraints

If the repository already answers these questions, proceed without blocking the work. Ask the user only when a missing choice would materially change the result.

### 2. Run the Slop Scan

Check every relevant category in the symptom catalog:

- surface treatment and borders
- layout and information hierarchy
- typography
- color, gradients, shadows, and effects
- imagery, icons, screenshots, and data visualizations
- copy, proof, and trust
- interaction and motion
- UX states and user journeys
- responsive behavior
- accessibility and semantics
- code quality and maintainability
- loading and runtime performance

Record concrete evidence. “Feels AI-generated” is not a useful finding.

### 3. Classify Findings

Use these levels:

- **Critical:** blocks a task, misleads users, fabricates proof, breaks accessibility, or creates a serious responsive failure
- **Major:** damages hierarchy, comprehension, trust, usability, brand specificity, or performance
- **Moderate:** creates visual noise, inconsistency, or unnecessary maintenance cost
- **Minor:** polish issue with limited user impact

Prioritize removal and simplification before adding new styling.

### 4. Replace the Pattern

For every finding, state:

1. the observed symptom
2. why it harms this specific page or user
3. what to remove
4. the simplest suitable replacement
5. how to verify the improvement

Prefer structural corrections over cosmetic swaps. For example, do not replace a purple gradient with a blue gradient when the real problem is an empty hero.

### 5. Verify the Result

When implementation and testing are in scope, check:

- the hierarchy remains clear without decorative effects
- there is one obvious primary action per page or self-contained dialog
- surfaces use the lightest separation that works
- no rounded container has a thick border
- real content does not break the composition
- mobile and tablet layouts are deliberately composed, not merely stacked
- keyboard, focus, zoom, contrast, reduced motion, and screen-reader semantics work
- empty, loading, error, success, disabled, and long-content states are handled
- images, fonts, scripts, and animation do not impose avoidable performance costs

Respect project-specific test boundaries. If the user has reserved visual testing for themselves, provide a precise visual QA checklist instead of claiming it was tested.

## Design Defaults

- Use whitespace before containers.
- Use dividers before cards.
- Use restrained cards only for independently interactive or meaningfully distinct units.
- Use a consistent, modest radius scale. Do not make every object a pill.
- Use one dominant visual idea per page.
- Keep gradients, glows, glass, blur, and 3D decoration rare and purposeful.
- Use real screenshots, real data, and real product states.
- Make the design recognizable without relying only on the logo.
- Let typography, spacing, alignment, and content create hierarchy.
- Preserve useful density in application interfaces.
- Keep decorative motion subordinate to functional feedback.
- Prefer semantic HTML and maintainable components over generated wrapper trees.

## Output Format For Audits

Lead with the verdict and the highest-impact issues. For each issue, include:

```text
[Severity] Finding
Evidence: Exact component, selector, screenshot area, or behavior.
Impact: What it costs the user or business.
Fix: The smallest durable correction.
Verify: The state, viewport, or interaction to check.
```

Do not produce a vague taste report. If no actionable slop is present, say so.

## Final Gate

Before completing design work, ask:

- Does this page look specific to this business and audience?
- Can users understand the purpose and next action in a few seconds?
- Is each card, border, effect, icon, and animation earning its place?
- Could any rounded thick border be replaced by spacing, a hairline, or a quieter surface?
- Are claims, testimonials, metrics, screenshots, and logos real and supportable?
- Does the design work with realistic content and non-ideal states?
- Is the mobile composition intentional?
- Is the result accessible, performant, and maintainable?

If any answer is no, continue refining.
