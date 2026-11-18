# NorseDistributor

SPID-like distribution for NORSE: Oath of Blood - a UE4SS v4.0.0 C++ mod. Rule files add abilities, passives, traits and
utility items to unit types (or remove them), chosen by name, folder, field content (faction, archetype) and chance.
The game's paks are never edited; the game builds each unit from its extended unit asset.

The second framework of `4. plans\norse-realtime-combat-20261116\ASSESSMENT-AND-PLAN.md` ("NorseDistributor
(SPID-like)"). It shares NorsePatcher's reflection layer (`../NorsePatcher/include/NorseReflect.hpp`) and UE4SS import
library, so each stays its own download while the code lives once.

## Rules

`ue4ss\Mods\NorseDistributor\distributions\*.ini`; one rule per line:

```ini
[CombatUnit]
ActiveAbilities = /Game/Norse/Core/Data/Abilitites/EnemyAbilities/ShieldSlam_Enemy.ShieldSlam_Enemy | Outcast_*,Basic_Archer | EnemyTypes
PassiveAbilities = /Game/Norse/Core/Data/Abilitites/EnemyAbilities/Passive/Commander.Commander | | | Profiles:Brigands
ActiveAbilities = -/Game/Norse/Core/Data/Abilitites/Alert.Alert | Basic_Archer
```

`List = value | names | folders | field filters | chance` - the author guide is `package\distributions\README.txt`;
the tested example is `docs\examples\test-first-run.ini`.

## How it works

At each map load (and on `nd.reload` / `nd.apply`) it lists every loaded asset of the rule's class (`FindAllOf`), tests
the filters (a field filter reads the field through Unreal's text export), and rewrites the list field with Unreal's
text import - the existing entries plus the new one, or minus the removed one. A rule already satisfied changes
nothing. Unit types used by a map are loaded with the map, before its units are built, so the units get the result
(first test: live `UnitComponent.ActiveAbilities` carried the added ability and lost the removed one).

Not yet: the player's party (not built from CombatUnit assets), and units spawned from assets loaded after the map.

## Build

`xmake f -p windows -a x64 -m release --toolchain=msvc -y`, then `xmake`. Needs `../NorsePatcher` beside it.
