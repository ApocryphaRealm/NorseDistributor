# Changelog

## 1.0.0 - 2026-11-18 - working

### Added
- First release. It contains everything built and tested in 0.1.0, listed here and kept as history below:
- Filters come from the shared NorsePatcher/include/NorseFilters.hpp, which adds Tag:Name (NorseTagger tags, tested in game) to the field filters.
- Config files follow NorseManifest: files of switched-off packs are skipped, the rest are read in pack load order, loose files last; the framework ships its own manifest (package/NorsePacks).
- Rule files in `distributions\*.ini`: `List = value | names | folders | field filters | chance`, `[Class]` sections (default CombatUnit), a leading `-` on the value removes it.
- Filters: names with `*` wildcards and `-` exclusions, folder text, `Field:text` on any field (faction via Profiles, archetype), a fixed per-unit-type chance.
- Applied at every map load; console `nd.reload`, `nd.apply`, `nd.dump <object path>`. One log line per change and a summary per rule; rules already satisfied change nothing.
- [Unit] sections: rules for live units, the player's party too (not built from CombatUnit assets) - a When line (NorseConditions) picks the units, list fields of each UnitComponent are edited, then InitializePassives + UpdateActiveAbilities re-arm them; 2 s after each map load and by nd.apply. Tested: docs/LIVE-LAYER-TEST.txt.
- main.dll carries a Windows version resource (file and product version 1.0.0, shown in its Properties > Details).

### Changed
- Asset references it writes are recorded and re-imported at a map load if their asset is no longer loaded (soft references skipped).

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

### Added (2026-11-18)
- [Unit] sections: rules for live units, the player's party too (not built from CombatUnit assets) - a When line (NorseConditions) picks the units, list fields of each UnitComponent are edited, then InitializePassives + UpdateActiveAbilities re-arm them; 2 s after each map load and by nd.apply. Tested: docs/LIVE-LAYER-TEST.txt.
