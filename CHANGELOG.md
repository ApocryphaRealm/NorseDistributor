# Changelog

## 0.1.0 - 2026-11-17 (tested in game: TEST-RESULT-0.1.0.txt)
### Added (later, 2026-11-17)
- Filters come from the shared NorsePatcher/include/NorseFilters.hpp, which adds Tag:Name (NorseTagger tags, tested in game) to the field filters.
- Config files follow NorseManifest: files of switched-off packs are skipped, the rest are read in pack load order, loose files last; the framework ships its own manifest (package/NorsePacks).
### Added
- Rule files in `distributions\*.ini`: `List = value | names | folders | field filters | chance`, `[Class]` sections
  (default CombatUnit), a leading `-` on the value removes it.
- Filters: names with `*` wildcards and `-` exclusions, folder text, `Field:text` on any field (faction via Profiles,
  archetype), a fixed per-unit-type chance.
- Applied at every map load; console `nd.reload`, `nd.apply`, `nd.dump <object path>`. One log line per change and a
  summary per rule; rules already satisfied change nothing.

### Changed (2026-11-18, refinement)
- Asset references it writes are recorded and re-imported at a map load if their asset is no longer loaded (soft references skipped).
