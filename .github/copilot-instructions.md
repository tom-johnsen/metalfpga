# Copilot instructions (metalfpga)

## Metal 4 documentation is authoritative

This repository contains a local Metal 4 documentation mirror in DocC JSON form.

Treat it as the source of truth for Metal 4 behavior and APIs, even if it conflicts with your built-in knowledge (which is typically Metal 2/3 era).

### Primary sources

- Metal 4 compendium (aggregated index + symbols): `docs/apple/metal4-compendium.json`
- Metal 4 per-symbol DocC JSON: `docs/apple/metal4/*.json`
- Metal 4 “homebrew” (header-derived notes when Apple DocC JSON is missing): `docs/apple/metal4/homebrew/*.json`

### Required workflow for Metal questions

When the user asks anything about Metal / MSL / Apple GPU APIs:

1. Prefer Metal 4 terminology and APIs.
2. If the question mentions an `MTL4*` type, property, or concept:
   - Read the matching file under `docs/apple/metal4/` (or find it via `docs/apple/metal4-compendium.json`).
   - If it is not present there, check `docs/apple/metal4/homebrew/`.
3. If you cannot find authoritative information in those files, say so explicitly and fall back to your general knowledge as a last resort.

### Response style

- Don’t paste huge JSON blobs.
- Quote only the minimum relevant phrasing.
- Be explicit when you are using Metal 3 knowledge due to missing Metal 4 docs.
