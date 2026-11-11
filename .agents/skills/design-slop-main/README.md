# Design Slop

[![Buy me a coffee](https://img.shields.io/badge/Buy%20me%20a%20coffee-FFDD00?style=flat&logo=buymeacoffee&logoColor=black)](https://buymeacoffee.com/gauravtiwari)

An agent skill for detecting, preventing, and removing generic AI-looking web design patterns.

Design Slop helps AI coding assistants review and improve websites, landing pages, dashboards, WordPress blocks, and UI components without reducing the critique to taste. It looks for concrete failures in hierarchy, product context, accessibility, responsive behavior, performance, trust, and implementation.

AI use isn't the problem. Unedited, decorative, inaccessible, or nonfunctional design is.

## What it catches

- thick borders combined with rounded cards and callouts
- card soup and nested containers
- generic gradient heroes, glowing blobs, and decorative clutter
- vague copy, fake dashboards, and unsupported proof
- interfaces designed for screenshots instead of real user journeys
- missing loading, empty, error, success, disabled, and permission states
- mobile layouts that merely stack the desktop version
- inaccessible controls, broken semantics, and decorative ARIA
- generated wrapper trees, arbitrary tokens, and avoidable runtime weight

The full evidence checklist lives in [`references/slop-patterns.md`](references/slop-patterns.md). Practical replacements are in [`references/before-after-examples.md`](references/before-after-examples.md).

## Install

### Claude Code

```bash
git clone https://github.com/wpgaurav/design-slop.git ~/.claude/skills/design-slop
```

### OpenAI Codex

```bash
git clone https://github.com/wpgaurav/design-slop.git ~/.codex/skills/design-slop
```

### Installer

Clone the repository and choose a target:

```bash
git clone https://github.com/wpgaurav/design-slop.git
cd design-slop
./install.sh claude
```

Supported targets:

```bash
./install.sh claude
./install.sh codex
./install.sh cursor
```

The installer copies the skill to the standard user-level directory. Set `DESIGN_SLOP_INSTALL_DIR` to install elsewhere.

### Uploadable skill

Download [`dist/design-slop.skill`](dist/design-slop.skill) and upload it to an assistant that accepts packaged skills. The file is a ZIP archive with the `.skill` extension.

### Other agents and editors

The repository is plain Markdown. Point your agent at [`SKILL.md`](SKILL.md) and tell it to load the two files in `references/` before auditing or implementing a design.

For project-level use, add this to the nearest agent instruction file:

```markdown
For web and UI design work, follow ./design-slop/SKILL.md and its references.
Repo-local brand and design-system rules take priority.
```

## Use it

Ask your agent:

> Audit this landing page with the Design Slop skill. Rank concrete findings by severity and give me the smallest durable fix for each.

> Refactor this dashboard to remove card soup, preserve useful density, and handle loading, empty, error, and long-content states.

> Review this component for thick rounded borders, generic visual treatments, accessibility issues, and mobile failure modes.

The audit format is deliberately evidence-based:

```text
[Severity] Finding
Evidence: Exact component, selector, screenshot area, or behavior.
Impact: What it costs the user or business.
Fix: The smallest durable correction.
Verify: The state, viewport, or interaction to check.
```

## Core rule

Never combine a border of `2px` or more with a rounded card, callout, panel, content box, or section container by default.

Prefer, in order:

1. whitespace and alignment
2. a subtle `1px` divider
3. a restrained tint or recessed surface
4. a semantic icon, label, or status dot
5. elevation only when the element truly floats

Focus rings, selected states that require strong contrast, and explicitly approved brutalist systems are narrow exceptions.

## Repository structure

```text
design-slop/
├── SKILL.md
├── references/
│   ├── slop-patterns.md
│   └── before-after-examples.md
├── dist/
│   └── design-slop.skill
├── scripts/
│   ├── build-skill.sh
│   ├── build_skill.py
│   └── validate.sh
├── install.sh
├── CHANGELOG.md
├── CONTRIBUTING.md
└── LICENSE
```

## Principles

- Diagnose repeated patterns and user-facing costs, not one fashionable technique.
- Remove and simplify before adding new styling.
- Prefer structural corrections over cosmetic swaps.
- Use real content, real product states, and supportable proof.
- Treat accessibility, performance, responsive behavior, and maintainability as design quality.
- Let the target project's design system and brand rules outrank this general skill.

## Development

Validate the repository:

```bash
./scripts/validate.sh
```

Rebuild the uploadable skill:

```bash
./scripts/build-skill.sh
```

## Support This Project

Design Slop gives Claude Code, Codex or Cursor a free MIT-licensed checklist of generic AI-looking design patterns, from thick borders on rounded cards to fake dashboards. I keep the checklist and the before-and-after examples current and each finding in an audit comes with evidence, a small fix and the state or viewport to check.

If it caught card soup or a missing empty state in a page your agent built, you can buy me a coffee.

<a href="https://buymeacoffee.com/gauravtiwari"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy me a coffee" height="50"></a>

When the skill gets a design wrong, an issue with your prompt and the exact finding it gave is what I need to tighten the checklist and a star on the repo is welcome too.

## License

[MIT](LICENSE) © Gaurav Tiwari
