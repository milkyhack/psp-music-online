---
name: writing-polish
description: >-
  Fix spelling, grammar, and awkward phrasing in user-facing text (README, UI
  strings, commits, docs) in Russian and English. Use when writing or editing
  prose, documentation, license summaries, or bilingual copy; when the user
  mentions typos, «ошибки в словах», proofreading, or wants text to sound
  natural for RU and EN readers.
---

# Writing polish (RU + EN)

## When this applies

Any time you create or change **user-facing prose**: README, CONTRIBUTING, UI
status strings, release notes, GitHub descriptions, comments meant for humans.

## Rules

1. **Correct language**
   - English: native-sounding, short sentences, no filler (“robust”, “seamless”, “leverage”).
   - Russian: правильная орфография и пунктуация; «ё» где уместно; без транслита («Proverka servera» запрещён).
2. **Bilingual docs**
   - Keep EN and RU sections parallel (same facts, same order).
   - Language switcher at the top: `[English](#english) · [Русский](#russian)`.
3. **Proofread before finish**
   - Re-read for typos, broken markdown anchors, wrong product facts.
   - Do not invent features.
4. **Voice**
   - Direct and concrete. Prefer tables for steps/controls.
   - No AI disclaimers in the doc body.

## Checklist (run mentally before saving)

- [ ] No translit Russian
- [ ] No misspelled product terms (Wi-Fi, Memory Stick, EBOOT, sceMp3)
- [ ] Clone URL and license match the repo
- [ ] Screenshots paths exist
- [ ] EN/RU both updated if one changed
