NorseDistributor rule files
===========================

Every *.ini file in this folder is read, in file-name order. Each rule adds an entry to (or removes one from) a list
field of every loaded unit type that passes its filters. Nothing in the game's own files is changed: remove a rule
file and the unit types are as shipped at the next start.

One rule per line, SPID-style. Everything after the value is optional; leave a filter empty (or write NONE) to skip it:

    List = value | names | folders | field filters | chance

    [CombatUnit]          ; optional: the asset class the rules below apply to (CombatUnit is the default)
    ActiveAbilities = /Game/Norse/Core/Data/Abilitites/EnemyAbilities/ShieldSlam_Enemy.ShieldSlam_Enemy | Outcast_*,Basic_Archer | EnemyTypes
    PassiveAbilities = /Game/Norse/Core/Data/Abilitites/EnemyAbilities/Passive/Commander.Commander | | | Profiles:Brigands
    ActiveAbilities = -/Game/Norse/Core/Data/Abilitites/Alert.Alert | Basic_Archer      ; a leading - removes
    PassiveAbilities = /Game/Norse/Core/Data/Abilitites/EnemyAbilities/Passive/DropWeaponOnDeath.DropWeaponOnDeath | Basic_* | EnemyTypes | | 50

List       a list field of the unit type: ActiveAbilities, PassiveAbilities, Traits, UtilityItem.
value      the asset to add, by its full object path.
names      the unit type's name; * is a wildcard; -name excludes. Several separated by commas.
folders    text in the unit type's path (EnemyTypes, AlliedUnits, Basic, Outcasts ...); -text excludes.
fields     Field:text - that field's value contains the text, e.g. Profiles:Brigands (the faction) or
           Archetype:Basic; -Field:text excludes.
chance     0-100, rolled once per unit type and the same every start; 100 (the default) = always.

A rule that is already true ("already so" in the log) changes nothing, so rules are safe to run again.

Finding names: in-game console, nd.dump <object path> writes every field of a unit type to dumps\<name>.txt beside
NorseDistributor.ini. Console: nd.reload (re-read these files and apply), nd.apply.
The log is NorseDistributor.log beside NorseDistributor.ini: one line per change, and a summary per rule.

Applies to unit types built from unit assets (enemies and allied NPC types). The player's own party is not built from
those assets yet; that comes with a later version.
