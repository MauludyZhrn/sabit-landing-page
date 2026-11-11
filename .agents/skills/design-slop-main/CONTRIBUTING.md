# Contributing

Contributions are welcome when they make the skill more specific, useful, accessible, or verifiable.

## Good contributions

- a recurring slop pattern with a clear user-facing cost
- a smaller, more durable replacement for an existing pattern
- a realistic before-and-after example
- a missing accessibility, responsive, performance, or product-state check
- clearer language that keeps the audit evidence-based

Avoid adding rules based only on personal taste or declaring a technique bad in isolation. Explain the context, the harm, the replacement, and how to verify it.

## Before opening a pull request

1. Edit `SKILL.md` or the relevant file in `references/`.
2. Run `./scripts/validate.sh`.
3. Run `./scripts/build-skill.sh`.
4. Include the rebuilt `dist/design-slop.skill` when source files change.
5. Describe the user-facing problem the change solves.

By contributing, you agree that your contribution is licensed under the MIT License.
