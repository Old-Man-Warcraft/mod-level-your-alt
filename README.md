# mod-level-your-alt

**Level Your Alt** is an AzerothCore module that lets players boost an alternate character to a level milestone (60, 70, or 80) that they have already reached on another character on the same account.

## Features

- **Account-wide unlock detection** — boost tiers are available as soon as any character on the account reaches that level naturally. No separate unlock table required.
- **Gossip NPC** — talk to "Level Your Alt" NPCs placed in Stormwind and Orgrimmar (or anywhere you choose).
- **Configurable gold costs** — each tier (60 / 70 / 80) can be made free or cost a configurable amount of gold.
- **Tier hierarchy** — reaching 70 unlocks both 60 and 70 boosts; reaching 80 unlocks all three.
- **Gear rewards by class + faction** — reward items are defined in a SQL table (`mod_lya_rewards`), so you can curate full class-specific gear sets without recompiling.
- **Bag + mail delivery** — items are placed into the player's bags; overflow is mailed if bags are full.
- **Weapon skill sync** — existing weapon skills are raised to the max for the new level (configurable).

## Requirements

- AzerothCore WotLK 3.3.5a
- MySQL / MariaDB

## Installation

1. Place (or clone) this folder under `modules/`:
   ```
   cd path/to/azerothcore/modules
   git clone <repo-url> mod-level-your-alt
   ```

2. Re-run CMake and rebuild:
   ```
   cd /root/azerothcore-wotlk
   ./acore.sh compiler all
   ```

3. Apply the world-database SQL files (run once against `acore_world`):
   ```
   mysql -u root -p acore_world < modules/mod-level-your-alt/data/sql/db-world/base/mod_lya_rewards_table.sql
   mysql -u root -p acore_world < modules/mod-level-your-alt/data/sql/db-world/base/mod_lya_npc.sql
   ```

4. Copy the config dist to your server's `etc/` folder:
   ```
   cp modules/mod-level-your-alt/conf/level_your_alt.conf.dist <server>/etc/level_your_alt.conf
   ```
   Edit `level_your_alt.conf` to set costs, enable/disable tiers, etc.

5. Restart the worldserver.

## Configuration

All options live in `level_your_alt.conf`:

| Key | Default | Description |
|-----|---------|-------------|
| `LevelYourAlt.Enable` | `1` | Enable/disable the module |
| `LevelYourAlt.NpcEntry` | `900001` | creature_template entry of the gossip NPC |
| `LevelYourAlt.Allow60` | `1` | Enable level-60 boosts |
| `LevelYourAlt.Allow70` | `1` | Enable level-70 boosts |
| `LevelYourAlt.Allow80` | `1` | Enable level-80 boosts |
| `LevelYourAlt.Cost60` | `0` | Gold cost for a level-60 boost |
| `LevelYourAlt.Cost70` | `0` | Gold cost for a level-70 boost |
| `LevelYourAlt.Cost80` | `0` | Gold cost for a level-80 boost |
| `LevelYourAlt.MailOverflowItems` | `1` | Mail gear that doesn't fit in bags |
| `LevelYourAlt.LearnWeaponSkills` | `1` | Raise existing weapon skills to new-level max |

## Reward items

Items are stored in the `mod_lya_rewards` table in `acore_world`:

| Column | Description |
|--------|-------------|
| `target_level` | `60`, `70`, or `80` |
| `class_mask` | Bitmask of classes (`1 << class_id`). `4094` = all classes |
| `team_id` | `0` = Alliance, `1` = Horde, `2` = Both |
| `item_id` | item_template entry |
| `item_count` | How many to give |
| `sort_order` | Lower values are delivered first |
| `description` | Human-readable note (not used in-game) |

**Example** — add a level-80 Warrior sword for both factions:
```sql
INSERT INTO `mod_lya_rewards` (`target_level`, `class_mask`, `team_id`, `item_id`, `item_count`, `description`)
VALUES (80, 2, 2, 49623, 1, 'Warrior sword - Rimefang Claw');
```

## NPC placement

Two spawns are created by the SQL:
- **Stormwind** — Trade District near the fountain (map 0)
- **Orgrimmar** — Valley of Strength (map 1)

To move or add spawns, edit `mod_lya_npc.sql` or insert directly into the `creature` table.

## Unlock logic

A boost tier is **available** if:
1. The config key `LevelYourAlt.Allow<tier>` is `1`.
2. At least one character on the same account has `level >= tier` in the `characters` table.
3. The player is **not** already at or above the requested tier.
4. The player is **not** in combat, flight, a battleground, or an arena.
5. The player has enough gold (if a cost is configured).

## Safety notes

- A player who is already at or above the target level cannot boost.
- Boosts cannot be applied in combat, in flight, in a battleground, or in an arena.
- The module deducts gold **before** applying the boost. If the boost fails after deduction (e.g., a coding error), the gold is lost. Keep this in mind during testing.
- The `mod_lya_rewards` query crosses database boundaries (`acore_world` queried from a `CharacterDatabase` connection). Make sure your DB user has SELECT permission on `acore_world`.
