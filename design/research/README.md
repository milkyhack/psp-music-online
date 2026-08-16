# Design Research Index / Индекс дизайн-исследований

**Project:** Music Online PSP — commercial-grade exhibition player UI  
**Phase:** 1 — Device & lineage research (no implementation)  
**Audience:** Concept Team → Art Director → Moodboards (Phase 2)

## Documents

| File | Purpose |
|------|---------|
| [PHASE1_DEVICE_RESEARCH.md](./PHASE1_DEVICE_RESEARCH.md) | Полный research pack: lineages, 15 theme cards, cross-cutting principles, anti-patterns, open questions |

## Status

| Phase | Status |
|-------|--------|
| Phase 1 — Device research | **Done** (this folder) |
| Phase 2 — Moodboards / material boards | Not started — inputs listed inside Phase 1 doc |
| Phase 3 — Spec / implementation | Out of scope for Research Team |

## Quick map of the 15 themes

1. Sony Walkman Premium  
2. Winamp Classic  
3. Cassette Deck  
4. MiniDisc  
5. CD Player  
6. GameBoy  
7. GameBoy Color  
8. DOS  
9. Matrix  
10. Cyberpunk  
11. CRT TV  
12. PS2 Browser  
13. PSP XMB  
14. Dreamcast  
15. Arcade  

## Repo note (read-only observation)

Current `psp/src/theme.*` already names these 15 skins and stores **palette + composition enum + viz mode**. Visual execution in `ui.c` is still largely flat fills / simple shells. Phase 1 research is written to **replace that language** with industrial / collectible-skin criteria — not to patch code.
