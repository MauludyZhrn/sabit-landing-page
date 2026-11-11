# Design Slop: Before and After Examples

These examples show how to replace generic visual decoration with clearer hierarchy and product-specific communication.

## 1. Thick Rounded Callout

### Slop

```html
<aside class="rounded-2xl border-4 border-l-8 border-violet-500 bg-violet-50 p-6 shadow-xl">
  <h3>Pro Tip</h3>
  <p>Use our powerful solution to supercharge your workflow.</p>
</aside>
```

Problems:

- thick border fights the large radius
- multiple treatments compete: border, accent edge, tint, and shadow
- “Pro Tip” and the copy carry little information

### Better

```html
<aside class="border-t border-violet-700/20 bg-violet-700/5 px-5 py-4">
  <p class="font-medium text-violet-950">Export the original file before changing these settings.</p>
  <p class="mt-1 text-violet-950/75">The migration cannot restore field mappings that were never included in the export.</p>
</aside>
```

The quieter surface lets the concrete warning provide the emphasis.

## 2. Card Soup

### Slop

```html
<section class="grid gap-6 md:grid-cols-3">
  <div class="rounded-2xl border-2 p-6 shadow-lg">Fast</div>
  <div class="rounded-2xl border-2 p-6 shadow-lg">Powerful</div>
  <div class="rounded-2xl border-2 p-6 shadow-lg">Seamless</div>
</section>
```

Problems:

- features are forced into three equal cards
- thick rounded borders make generic words look important
- the content contains no proof or decision value

### Better

```html
<dl class="divide-y divide-neutral-950/10">
  <div class="grid gap-2 py-5 sm:grid-cols-[12rem_1fr]">
    <dt class="font-medium">Cache purge</dt>
    <dd class="text-neutral-700">Purges the affected URL after a successful deployment, not the entire site.</dd>
  </div>
  <div class="grid gap-2 py-5 sm:grid-cols-[12rem_1fr]">
    <dt class="font-medium">Rollback</dt>
    <dd class="text-neutral-700">Keeps the previous package available until the live checksum matches.</dd>
  </div>
</dl>
```

The description list communicates real behavior without manufacturing three “feature cards.”

## 3. Generic Gradient Hero

### Slop

```html
<header class="relative overflow-hidden bg-linear-to-br from-violet-600 via-fuchsia-500 to-cyan-400 py-32 text-center text-white">
  <div class="absolute inset-0 backdrop-blur-3xl"></div>
  <h1>Revolutionize Your Digital Journey</h1>
  <p>Unlock powerful tools designed to elevate your workflow.</p>
  <a href="#" class="rounded-full bg-white px-8 py-4 text-violet-700 shadow-2xl">Get Started</a>
</header>
```

Problems:

- the visuals communicate a trend, not the product
- the copy could belong to any company
- the CTA does not predict what happens

### Better

```html
<header class="py-20">
  <div class="mx-auto grid max-w-6xl items-center gap-12 px-6 lg:grid-cols-[5fr_7fr]">
    <div>
      <p class="text-sm font-medium text-blue-700">WordPress form operations</p>
      <h1 class="mt-3 text-5xl font-semibold tracking-tight text-neutral-950">Ship forms without rebuilding validation every time.</h1>
      <p class="mt-5 max-w-xl text-lg text-neutral-700">Create reusable fields, validate submissions on the server, and inspect failed entries from one WordPress-native workflow.</p>
      <a href="/demo/" class="mt-7 inline-flex bg-blue-700 px-4 py-3 font-medium text-white">Open the form demo</a>
    </div>
    <img src="/media/form-builder.webp" alt="Form builder showing server-side validation rules" class="border border-neutral-950/10" />
  </div>
</header>
```

The page now identifies the audience, problem, mechanism, and next action.

## 4. Nested Rounded Panels

### Slop

```html
<div class="rounded-3xl border-2 bg-gray-50 p-6">
  <div class="rounded-2xl border-2 bg-white p-6">
    <div class="rounded-xl border-2 bg-white p-4">Metric</div>
  </div>
</div>
```

### Better

```html
<section class="bg-neutral-50 py-10">
  <div class="mx-auto max-w-6xl px-6">
    <dl class="grid divide-y divide-neutral-950/10 sm:grid-cols-3 sm:divide-x sm:divide-y-0">
      <!-- Metrics use alignment and dividers instead of nested cards. -->
    </dl>
  </div>
</section>
```

One recessed surface and subtle dividers are enough.

## 5. Fake Dashboard Proof

### Slop

```text
+247% productivity
10K+ happy customers
99.99% smarter decisions
```

Problems:

- numbers lack definitions, timeframes, samples, and sources
- “smarter decisions” cannot be measured as shown
- animated counters amplify unsupported claims

### Better

```text
Median form response time: 182 ms
Measured across 24,381 production submissions from 1-30 June 2026.
```

Use a metric only when the product can define, produce, and support it.

## 6. Decorative Feature Icons

### Slop

- sparkle icon for “AI-powered”
- rocket icon for “fast”
- shield icon for “secure”
- each icon uses a different family and sits inside a rounded gradient tile

### Better

- remove the icons when headings scan clearly without them
- if icons improve recognition, use one consistent family and literal metaphors
- connect “secure” to a specific control such as passkeys, audit logs, or encryption at rest

## 7. Too Many Primary Buttons

### Slop

```text
[Start Free] [Book Demo] [See Pricing] [Watch Video]
```

All four actions use filled brand-color buttons.

### Better

```text
[Start Free]  See pricing
```

Keep one filled primary action. Move supporting evidence into the page and place sales-assisted alternatives where they match user intent.

## 8. Scroll-Reveal Content

### Slop

```css
.section > * {
  opacity: 0;
  transform: translateY(40px);
}
```

JavaScript reveals every item as the user scrolls.

### Better

- render reading content visible by default
- animate only a meaningful state change
- use short distance, restrained easing, and `prefers-reduced-motion`
- ensure the page remains complete when JavaScript or animation fails

## 9. Mobile as a Collapsed Desktop

### Slop

Desktop has a six-card comparison grid. Mobile stacks six tall cards, repeats labels, and pushes the decision CTA several screens down.

### Better

- show the decision criteria first
- convert comparable attributes to a scroll-safe table or compact list
- keep the primary action near the relevant choice
- remove decorative descriptions that repeat the same benefit
- verify long names, zoom, keyboard presence, and landscape orientation

## 10. Generic Testimonial Grid

### Slop

Three equal rounded cards contain synthetic portraits and quotes such as “This changed everything!”

### Better

Use one attributable story with:

- the person's real name and role
- the original problem
- what they used
- a concrete result and timeframe
- a link or internal record supporting publication rights

If that evidence does not exist, omit the section.

## 11. Accessibility Decoration

### Slop

```html
<div role="button" tabindex="0" aria-label="Submit" class="rounded-full border-2">Send</div>
```

The element has ARIA but no native button behavior, disabled state, form semantics, or reliable keyboard activation.

### Better

```html
<button type="submit" class="bg-blue-700 px-4 py-3 font-medium text-white focus-visible:outline-2 focus-visible:outline-offset-2 focus-visible:outline-blue-600">
  Send application
</button>
```

Start with the correct native element, then style it.

## 12. Generated Wrapper Tree

### Slop

```html
<div class="section-wrapper">
  <div class="section-inner">
    <div class="content-wrapper">
      <div class="card-wrapper">
        <div class="card-inner">...</div>
      </div>
    </div>
  </div>
</div>
```

### Better

```html
<section class="py-16">
  <div class="mx-auto max-w-6xl px-6">...</div>
</section>
```

Keep wrappers only when they own layout, containment, clipping, state, or semantics.

## Replacement Decision Ladder

When a design feels weak, do not immediately add another card or effect. Ask in order:

1. Is the content concrete and correctly ordered?
2. Can spacing and alignment establish the relationship?
3. Would typography create sufficient hierarchy?
4. Is a subtle divider needed?
5. Is a recessed background needed?
6. Is this unit independently interactive or fundamentally distinct enough to deserve a card?
7. Does elevation represent actual layering?

Stop at the first treatment that solves the problem.
