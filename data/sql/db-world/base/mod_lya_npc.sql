-- Level Your Alt: NPC template and world spawn
-- NPC entry 900001 is used by default. Change LevelYourAlt.NpcEntry in the conf if you use a different ID.

DELETE FROM `creature_template` WHERE `entry` = 900001;
INSERT INTO `creature_template`
    (`entry`, `difficulty_entry_1`, `difficulty_entry_2`, `difficulty_entry_3`,
     `KillCredit1`, `KillCredit2`, `modelid1`, `modelid2`, `modelid3`, `modelid4`,
     `name`, `subname`,
     `IconName`, `gossip_menu_id`, `minlevel`, `maxlevel`, `exp`,
     `faction`, `npcflag`, `speed_walk`, `speed_run`,
     `scale`, `rank`, `dmgschool`,
     `BaseAttackTime`, `RangeAttackTime`,
     `BaseVariance`, `RangeVariance`,
     `unit_class`, `unit_flags`, `unit_flags2`,
     `dynamicflags`, `family`, `trainer_type`, `trainer_spell`, `trainer_class`, `trainer_race`,
     `type`, `type_flags`, `lootid`, `pickpocketloot`, `skinloot`,
     `PetSpellDataId`, `VehicleId`,
     `mingold`, `maxgold`,
     `AIName`, `MovementType`, `HoverHeight`,
     `HealthModifier`, `ManaModifier`, `ArmorModifier`, `DamageModifier`, `ExperienceModifier`,
     `RacialLeader`, `movementId`, `RegenHealth`,
     `mechanic_immune_mask`, `spell_school_immune_mask`,
     `flags_extra`, `ScriptName`, `VerifiedBuild`)
VALUES
    (900001, 0, 0, 0,
     0, 0, 16176, 0, 0, 0,
     'Level Your Alt', 'Account Boost NPC',
     'Speak', 0, 80, 80, 0,
     35,       -- neutral faction
     1,        -- UNIT_NPC_FLAG_GOSSIP
     1.0, 1.14286,
     1.0, 0, 0,
     2000, 2000,
     1.0, 1.0,
     1,        -- unit_class: UNIT_CLASS_WARRIOR
     33554432, -- unit_flags: UNIT_FLAG_IMMUNE_TO_PC
     2048,     -- unit_flags2: UNIT_FLAG2_REGENERATE_POWER
     0, 0, 0, 0, 0, 0,
     7, 0, 0, 0, 0,     -- type = CREATURE_TYPE_HUMANOID
     0, 0,
     0, 0,
     'SmartAI', 0, 1.0,
     10.0, 1.0, 1.0, 1.0, 1.0,
     0, 0, 1,
     0, 0,
     2, 'npc_level_your_alt', 0);

-- Gossip menu text for the NPC
DELETE FROM `npc_text` WHERE `ID` = 90010;
INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `lang0`, `prob0`, `em0_0`, `em0_1`, `em0_2`, `em0_3`, `em0_4`, `em0_5`) VALUES
(90010,
 'Greetings, $N!\n\nI can boost one of your characters to a level that another character on your account has already reached.\n\nWhich tier would you like to unlock?',
 '',
 0, 1.0, 0, 0, 0, 0, 0, 0);

-- Gossip menu entry so the NPC opens the gossip window
DELETE FROM `gossip_menu` WHERE `MenuID` = 62100;
INSERT INTO `gossip_menu` (`MenuID`, `TextID`) VALUES (62100, 90010);

-- World spawn: Stormwind Keep (Alliance) — adjust as needed
-- Use a negative GUID offset to avoid conflicts; here we use guid 900001.
DELETE FROM `creature` WHERE `id1` = 900001;
INSERT INTO `creature`
    (`guid`, `id1`, `map`, `zoneId`, `areaId`,
     `spawnMask`, `phaseMask`,
     `modelid`, `equipment_id`,
     `position_x`, `position_y`, `position_z`, `orientation`,
     `spawntimesecs`, `wander_distance`, `currentwaypoint`,
     `curhealth`, `curmana`,
     `MovementType`, `npcflag`, `unit_flags`, `dynamicflags`,
     `ScriptName`, `VerifiedBuild`)
VALUES
    -- Stormwind: Trade District near the fountain
    (900001, 900001, 0, 0, 0,
     1, 1,
     0, 0,
     -8872.58, 668.84, 97.90, 5.31,
     300, 0, 0,
     100000, 0,
     0, 0, 0, 0,
     '', 0),
    -- Orgrimmar: Valley of Strength near entrance
    (900002, 900001, 1, 0, 0,
     1, 1,
     0, 0,
     1562.71, -4421.83, 16.06, 0.07,
     300, 0, 0,
     100000, 0,
     0, 0, 0, 0,
     '', 0);
