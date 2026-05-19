-- Level Your Alt: reward item table
-- team_id: 0 = Alliance, 1 = Horde, 2 = Both
-- class_mask: bitmask of class IDs (1 << class), e.g. Warriors (1) = 2, Paladins (2) = 4, Hunters (3) = 8 ...
--   Warrior=1  -> bit 1  -> mask value 2
--   Paladin=2  -> bit 2  -> mask value 4
--   Hunter=3   -> bit 3  -> mask value 8
--   Rogue=4    -> bit 4  -> mask value 16
--   Priest=5   -> bit 5  -> mask value 32
--   DK=6       -> bit 6  -> mask value 64
--   Shaman=7   -> bit 7  -> mask value 128
--   Mage=8     -> bit 8  -> mask value 256
--   Warlock=9  -> bit 9  -> mask value 512
--   Druid=11   -> bit 11 -> mask value 2048
--   All classes = 4094 (all bits 1-11 set)

DROP TABLE IF EXISTS `mod_lya_rewards`;
CREATE TABLE `mod_lya_rewards` (
    `id`           INT UNSIGNED NOT NULL AUTO_INCREMENT,
    `target_level` TINYINT UNSIGNED NOT NULL COMMENT '60, 70, or 80',
    `class_mask`   SMALLINT UNSIGNED NOT NULL DEFAULT 4094 COMMENT 'Class bitmask (1<<class)',
    `team_id`      TINYINT UNSIGNED NOT NULL DEFAULT 2     COMMENT '0=Alliance 1=Horde 2=Both',
    `item_id`      INT UNSIGNED NOT NULL,
    `item_count`   TINYINT UNSIGNED NOT NULL DEFAULT 1,
    `sort_order`   SMALLINT UNSIGNED NOT NULL DEFAULT 0    COMMENT 'Lower = delivered first',
    `description`  VARCHAR(255) DEFAULT NULL               COMMENT 'Human-readable note',
    PRIMARY KEY (`id`),
    KEY `idx_lya_rewards_lookup` (`target_level`, `class_mask`, `team_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci COMMENT='Level Your Alt boost reward items';

-- ============================================================
-- SAMPLE REWARDS  (replace item IDs with your preferred gear)
-- ============================================================
-- The entries below use generic purchasable/crafted items that
-- exist in the base WotLK 3.3.5 database as safe placeholders.
-- Replace them with your own curated gear sets.
--
-- Legend:
--   21215 = Netherweave Bag (4-slot bag, all classes/factions)
--   29434 = Elixir of Major Strength  (placeholder consumable)
--   17056 = Amulet of the Dawn        (placeholder neck)
--   17028 = Mark of the Chosen        (placeholder trinket)
--
-- Level 60 placeholder bag reward (all classes, both factions)
INSERT INTO `mod_lya_rewards` (`target_level`, `class_mask`, `team_id`, `item_id`, `item_count`, `sort_order`, `description`) VALUES
(60, 4094, 2, 21215, 4, 0, 'Netherweave Bag x4 - starter bags'),
(60, 4094, 2, 2820,  1, 1, 'Hearthstone (if missing)'),

-- Level 70 placeholder bag reward
(70, 4094, 2, 21215, 4, 0, 'Netherweave Bag x4 - starter bags'),
(70, 4094, 2, 2820,  1, 1, 'Hearthstone (if missing)'),

-- Level 80 placeholder bag reward
(80, 4094, 2, 21215, 4, 0, 'Netherweave Bag x4 - starter bags'),
(80, 4094, 2, 2820,  1, 1, 'Hearthstone (if missing)');
