#include "Config.h"
#include "CharacterDatabase.h"
#include "Chat.h"
#include "WorldDatabase.h"
#include "GossipDef.h"
#include "Item.h"
#include "Log.h"
#include "Mail.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedGossip.h"
#include "ScriptMgr.h"
#include "WorldSession.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

namespace LevelYourAlt
{
    // Level milestone tiers
    constexpr uint32 TIER_60 = 60;
    constexpr uint32 TIER_70 = 70;
    constexpr uint32 TIER_80 = 80;

    // Gossip menu/text IDs — pick IDs that do not clash with other modules
    constexpr uint32 GOSSIP_MENU_ID   = 62100;
    constexpr uint32 GOSSIP_TEXT_ID   = 90010;

    // Gossip action constants
    enum GossipActions
    {
        ACTION_SHOW_MENU    = GOSSIP_ACTION_INFO_DEF + 1,
        ACTION_BOOST_60     = GOSSIP_ACTION_INFO_DEF + 2,
        ACTION_BOOST_70     = GOSSIP_ACTION_INFO_DEF + 3,
        ACTION_BOOST_80     = GOSSIP_ACTION_INFO_DEF + 4,
        ACTION_CONFIRM_60   = GOSSIP_ACTION_INFO_DEF + 5,
        ACTION_CONFIRM_70   = GOSSIP_ACTION_INFO_DEF + 6,
        ACTION_CONFIRM_80   = GOSSIP_ACTION_INFO_DEF + 7,
    };

    // Mail sender name for overflow items
    static const std::string MAIL_SENDER = "Level Your Alt";
    static const std::string MAIL_SUBJECT_60 = "Your Level 60 Boost Gear";
    static const std::string MAIL_SUBJECT_70 = "Your Level 70 Boost Gear";
    static const std::string MAIL_SUBJECT_80 = "Your Level 80 Boost Gear";
    static const std::string MAIL_BODY = "Here are items from your level boost that did not fit in your bags. Equip them when you have space!";
} // namespace LevelYourAlt

// ---------------------------------------------------------------------------
// Config helpers
// ---------------------------------------------------------------------------

static bool LYAEnabled()
{
    return sConfigMgr->GetOption<bool>("LevelYourAlt.Enable", true);
}

static bool LYATierAllowed(uint32 tier)
{
    switch (tier)
    {
        case LevelYourAlt::TIER_60: return sConfigMgr->GetOption<bool>("LevelYourAlt.Allow60", true);
        case LevelYourAlt::TIER_70: return sConfigMgr->GetOption<bool>("LevelYourAlt.Allow70", true);
        case LevelYourAlt::TIER_80: return sConfigMgr->GetOption<bool>("LevelYourAlt.Allow80", true);
        default: return false;
    }
}

static uint32 LYANpcEntry()
{
    return static_cast<uint32>(sConfigMgr->GetOption<int32>("LevelYourAlt.NpcEntry", 900001));
}

// Returns gold cost in copper (gold * 10000)
static uint64 LYACost(uint32 tier)
{
    int32 gold = 0;
    switch (tier)
    {
        case LevelYourAlt::TIER_60: gold = sConfigMgr->GetOption<int32>("LevelYourAlt.Cost60", 0);   break;
        case LevelYourAlt::TIER_70: gold = sConfigMgr->GetOption<int32>("LevelYourAlt.Cost70", 0);   break;
        case LevelYourAlt::TIER_80: gold = sConfigMgr->GetOption<int32>("LevelYourAlt.Cost80", 0);   break;
        default: break;
    }
    if (gold < 0)
        gold = 0;
    return static_cast<uint64>(gold) * 10000u;  // 1 gold = 10000 copper in core
}

static bool LYAMailOverflow()
{
    return sConfigMgr->GetOption<bool>("LevelYourAlt.MailOverflowItems", true);
}

static bool LYALearnWeaponSkills()
{
    return sConfigMgr->GetOption<bool>("LevelYourAlt.LearnWeaponSkills", true);
}

// ---------------------------------------------------------------------------
// Account-unlock detection
// ---------------------------------------------------------------------------

// Returns the highest level character on the same account (excluding self if desired,
// but here we include self so that the triggering character may also benefit if it
// somehow never reached the tier before but another character did).
static uint32 GetAccountHighestLevel(uint32 accountId)
{
    QueryResult result = CharacterDatabase.Query(
        "SELECT MAX(`level`) FROM `characters` WHERE `account` = {}",
        accountId);

    if (!result)
        return 0;

    return result->Fetch()[0].Get<uint32>();
}

// A tier is unlocked if ANY character on the account has reached at least that level.
static bool IsBoostTierUnlocked(Player* player, uint32 tier)
{
    if (!player)
        return false;

    uint32 accountId = player->GetSession()->GetAccountId();
    uint32 highestLevel = GetAccountHighestLevel(accountId);
    return highestLevel >= tier;
}

// ---------------------------------------------------------------------------
// Safety guard
// ---------------------------------------------------------------------------

static bool CanReceiveBoost(Player* player, uint32 targetTier, Creature* npc)
{
    if (!player || !player->GetSession())
        return false;

    ChatHandler ch(player->GetSession());

    if (player->GetLevel() >= targetTier)
    {
        ch.SendNotification("You are already level %u or higher!", targetTier);
        CloseGossipMenuFor(player);
        return false;
    }

    if (player->IsInCombat())
    {
        ch.SendNotification("You cannot use this while in combat.");
        CloseGossipMenuFor(player);
        return false;
    }

    if (player->IsInFlight())
    {
        ch.SendNotification("You cannot use this while on a flight path.");
        CloseGossipMenuFor(player);
        return false;
    }

    if (player->InBattleground() || player->InArena())
    {
        ch.SendNotification("You cannot use this in a Battleground or Arena.");
        CloseGossipMenuFor(player);
        return false;
    }

    if (!IsBoostTierUnlocked(player, targetTier))
    {
        ch.SendNotification("You have not unlocked level %u boosts on this account yet.", targetTier);
        CloseGossipMenuFor(player);
        return false;
    }

    if (!LYATierAllowed(targetTier))
    {
        ch.SendNotification("Level %u boosts are currently disabled.", targetTier);
        CloseGossipMenuFor(player);
        return false;
    }

    uint64 cost = LYACost(targetTier);
    if (cost > 0 && player->GetMoney() < cost)
    {
        uint32 goldNeeded = static_cast<uint32>(cost / 10000u);
        ch.SendNotification("You need at least %u gold to purchase this boost.", goldNeeded);
        CloseGossipMenuFor(player);
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Gear grant
// ---------------------------------------------------------------------------

static std::string GetMailSubject(uint32 tier)
{
    switch (tier)
    {
        case LevelYourAlt::TIER_60: return LevelYourAlt::MAIL_SUBJECT_60;
        case LevelYourAlt::TIER_70: return LevelYourAlt::MAIL_SUBJECT_70;
        case LevelYourAlt::TIER_80: return LevelYourAlt::MAIL_SUBJECT_80;
        default:                    return "Level Boost Gear";
    }
}

// Deliver reward items for the given tier to the player.
// Items that cannot be stored in bags are mailed if LYAMailOverflow() is true.
static void GrantBoostRewards(Player* player, uint32 tier)
{
    if (!player)
        return;

    uint32 cls      = player->getClass();
    uint32 team     = player->GetTeamId(); // TEAM_ALLIANCE = 0, TEAM_HORDE = 1

    // Query the reward table: filter by target_level, class, and team (or team_id = 2 meaning both)
    QueryResult result = WorldDatabase.Query(
        "SELECT `item_id`, `item_count` FROM `mod_lya_rewards` "
        "WHERE `target_level` = {} AND `class_mask` & (1 << {}) AND (`team_id` = {} OR `team_id` = 2) "
        "ORDER BY `sort_order` ASC",
        tier, cls, team);

    if (!result)
    {
        LOG_WARN("module", "mod-level-your-alt: No reward rows found for tier={} class={} team={}", tier, cls, team);
        return;
    }

    MailDraft draft(GetMailSubject(tier), LevelYourAlt::MAIL_BODY);
    bool mailHasItems = false;

    do
    {
        Field* fields     = result->Fetch();
        uint32 itemId     = fields[0].Get<uint32>();
        uint32 itemCount  = fields[1].Get<uint32>();

        if (itemCount == 0)
            itemCount = 1;

        ItemTemplate const* proto = sObjectMgr->GetItemTemplate(itemId);
        if (!proto)
        {
            LOG_ERROR("module", "mod-level-your-alt: reward item {} does not exist in item_template", itemId);
            continue;
        }

        // Try to store into bags first
        uint32 remaining = itemCount;
        while (remaining > 0)
        {
            uint32 batch = std::min(remaining, static_cast<uint32>(proto->GetMaxStackSize()));
            remaining -= batch;

            ItemPosCountVec dest;
            InventoryResult canStore = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, itemId, batch);
            if (canStore == EQUIP_ERR_OK)
            {
                Item* item = player->StoreNewItem(dest, itemId, true);
                if (item)
                    player->SendNewItem(item, batch, true, false);
            }
            else
            {
                // Bags full — mail if enabled
                if (LYAMailOverflow())
                {
                    Item* mailItem = Item::CreateItem(itemId, batch, player);
                    if (mailItem)
                    {
                        mailItem->SaveToDB(CharacterDatabase);
                        draft.AddItem(mailItem);
                        mailHasItems = true;
                    }
                }
                else
                {
                    ChatHandler(player->GetSession()).SendNotification("Your bags are full; some items could not be delivered.");
                }
            }
        }

    } while (result->NextRow());

    if (mailHasItems)
    {
        SQLTransaction trans = CharacterDatabase.BeginTransaction();
        draft.SendMailTo(trans, MailReceiver(player), MailSender(MAIL_NORMAL, 0, MAIL_STATIONERY_GM));
        CharacterDatabase.CommitTransaction(trans);
    }
}

// ---------------------------------------------------------------------------
// Weapon / ride skill grants
// ---------------------------------------------------------------------------

static void LearnWeaponSkills(Player* player)
{
    // Cast the "Learn All Weapon Skills" GM-equivalent via direct skill set.
    // We iterate over weapon skill lines that exist in SkillLineEntry and set them.
    static const std::vector<uint32> weaponSkills = {
        43,   // Swords
        44,   // Axes
        45,   // Bows
        46,   // Guns
        54,   // Staves
        136,  // Crossbows
        160,  // Maces
        162,  // Daggers
        172,  // Defense
        173,  // Fist weapons
        229,  // Polearms
        473,  // Wands
        136,  // Crossbows (duplicate guard, harmless)
        5011, // Thrown
        760,  // Thrown (alt)
    };

    uint8 level = player->GetLevel();
    for (uint32 skillId : weaponSkills)
    {
        if (player->HasSkill(skillId))
            player->SetSkill(skillId, player->GetSkillStep(skillId), std::min(player->GetMaxSkillValueForLevel(), static_cast<uint32>(level * 5)), player->GetMaxSkillValueForLevel());
    }
}

// ---------------------------------------------------------------------------
// Core boost function
// ---------------------------------------------------------------------------

static void ApplyLevelBoost(Player* player, uint32 tier)
{
    if (!player)
        return;

    uint64 cost = LYACost(tier);
    if (cost > 0)
        player->ModifyMoney(-static_cast<int64>(cost));

    // Set the level
    player->GiveLevel(tier);
    player->InitTalentForLevel();
    player->SetUInt32Value(PLAYER_XP, 0);

    // Update stats / HP / Mana
    player->UpdateAllStats();
    player->SetFullHealth();
    player->SetPower(player->getPowerType(), player->GetMaxPower(player->getPowerType()));

    if (LYALearnWeaponSkills())
        LearnWeaponSkills(player);

    // Grant gear
    GrantBoostRewards(player, tier);

    LOG_INFO("module", "mod-level-your-alt: Account {} boosted character {} ({}) to level {}",
        player->GetSession()->GetAccountId(), player->GetGUID().GetCounter(), player->GetName(), tier);

    ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00You have been boosted to level %u! Check your bags and mail for gear.|r", tier);
}

// ---------------------------------------------------------------------------
// Gossip helpers
// ---------------------------------------------------------------------------

static std::string FormatBoostOption(uint32 tier, bool unlocked, bool configEnabled)
{
    if (!configEnabled)
        return "";

    uint64 cost = LYACost(tier);
    std::string costStr;
    if (cost == 0)
        costStr = "Free";
    else
        costStr = std::to_string(static_cast<uint32>(cost / 10000u)) + " gold";

    if (!unlocked)
        return "|cffaaaaaa[Locked] Boost to Level " + std::to_string(tier) + " (" + costStr + ")|r";

    return "Boost to Level " + std::to_string(tier) + " (" + costStr + ")";
}

static void ShowMainMenu(Player* player, ObjectGuid npcGuid)
{
    if (!player)
        return;

    ClearGossipMenuFor(player);

    uint32 accountId = player->GetSession()->GetAccountId();
    uint32 highest   = GetAccountHighestLevel(accountId);

    bool unlocked60 = highest >= LevelYourAlt::TIER_60 && LYATierAllowed(LevelYourAlt::TIER_60);
    bool unlocked70 = highest >= LevelYourAlt::TIER_70 && LYATierAllowed(LevelYourAlt::TIER_70);
    bool unlocked80 = highest >= LevelYourAlt::TIER_80 && LYATierAllowed(LevelYourAlt::TIER_80);

    bool show60 = LYATierAllowed(LevelYourAlt::TIER_60);
    bool show70 = LYATierAllowed(LevelYourAlt::TIER_70);
    bool show80 = LYATierAllowed(LevelYourAlt::TIER_80);

    if (show60)
    {
        std::string label = FormatBoostOption(LevelYourAlt::TIER_60, unlocked60, true);
        AddGossipItemFor(player, LevelYourAlt::GOSSIP_MENU_ID, label, GOSSIP_SENDER_MAIN, LevelYourAlt::ACTION_BOOST_60);
    }

    if (show70)
    {
        std::string label = FormatBoostOption(LevelYourAlt::TIER_70, unlocked70, true);
        AddGossipItemFor(player, LevelYourAlt::GOSSIP_MENU_ID, label, GOSSIP_SENDER_MAIN, LevelYourAlt::ACTION_BOOST_70);
    }

    if (show80)
    {
        std::string label = FormatBoostOption(LevelYourAlt::TIER_80, unlocked80, true);
        AddGossipItemFor(player, LevelYourAlt::GOSSIP_MENU_ID, label, GOSSIP_SENDER_MAIN, LevelYourAlt::ACTION_BOOST_80);
    }

    SendGossipMenuFor(player, LevelYourAlt::GOSSIP_TEXT_ID, npcGuid);
}

static void ShowConfirmation(Player* player, ObjectGuid npcGuid, uint32 tier)
{
    if (!player)
        return;

    ClearGossipMenuFor(player);

    uint64 cost = LYACost(tier);
    std::string costStr = (cost == 0) ? "free" : std::to_string(static_cast<uint32>(cost / 10000u)) + " gold";

    std::string confirmLabel = "Yes — boost me to level " + std::to_string(tier) + " (" + costStr + ")";
    std::string cancelLabel  = "No, cancel.";

    uint32 confirmAction = LevelYourAlt::ACTION_CONFIRM_60;
    if (tier == LevelYourAlt::TIER_70)
        confirmAction = LevelYourAlt::ACTION_CONFIRM_70;
    else if (tier == LevelYourAlt::TIER_80)
        confirmAction = LevelYourAlt::ACTION_CONFIRM_80;

    AddGossipItemFor(player, LevelYourAlt::GOSSIP_MENU_ID, confirmLabel, GOSSIP_SENDER_MAIN, confirmAction);
    AddGossipItemFor(player, LevelYourAlt::GOSSIP_MENU_ID, cancelLabel,  GOSSIP_SENDER_MAIN, LevelYourAlt::ACTION_SHOW_MENU);

    SendGossipMenuFor(player, LevelYourAlt::GOSSIP_TEXT_ID, npcGuid);
}

// ---------------------------------------------------------------------------
// NPC script
// ---------------------------------------------------------------------------

class npc_level_your_alt : public CreatureScript
{
public:
    npc_level_your_alt() : CreatureScript("npc_level_your_alt") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!LYAEnabled())
            return false;

        if (!player || !creature)
            return false;

        ShowMainMenu(player, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!LYAEnabled() || !player || !creature)
            return false;

        switch (action)
        {
            case LevelYourAlt::ACTION_SHOW_MENU:
                ShowMainMenu(player, creature->GetGUID());
                break;

            case LevelYourAlt::ACTION_BOOST_60:
                if (!IsBoostTierUnlocked(player, LevelYourAlt::TIER_60))
                {
                    ChatHandler(player->GetSession()).SendNotification("You have not unlocked Level 60 boosts. Reach level 60 on another character first!");
                    CloseGossipMenuFor(player);
                }
                else
                    ShowConfirmation(player, creature->GetGUID(), LevelYourAlt::TIER_60);
                break;

            case LevelYourAlt::ACTION_BOOST_70:
                if (!IsBoostTierUnlocked(player, LevelYourAlt::TIER_70))
                {
                    ChatHandler(player->GetSession()).SendNotification("You have not unlocked Level 70 boosts. Reach level 70 on another character first!");
                    CloseGossipMenuFor(player);
                }
                else
                    ShowConfirmation(player, creature->GetGUID(), LevelYourAlt::TIER_70);
                break;

            case LevelYourAlt::ACTION_BOOST_80:
                if (!IsBoostTierUnlocked(player, LevelYourAlt::TIER_80))
                {
                    ChatHandler(player->GetSession()).SendNotification("You have not unlocked Level 80 boosts. Reach level 80 on another character first!");
                    CloseGossipMenuFor(player);
                }
                else
                    ShowConfirmation(player, creature->GetGUID(), LevelYourAlt::TIER_80);
                break;

            case LevelYourAlt::ACTION_CONFIRM_60:
                if (CanReceiveBoost(player, LevelYourAlt::TIER_60, creature))
                {
                    CloseGossipMenuFor(player);
                    ApplyLevelBoost(player, LevelYourAlt::TIER_60);
                }
                break;

            case LevelYourAlt::ACTION_CONFIRM_70:
                if (CanReceiveBoost(player, LevelYourAlt::TIER_70, creature))
                {
                    CloseGossipMenuFor(player);
                    ApplyLevelBoost(player, LevelYourAlt::TIER_70);
                }
                break;

            case LevelYourAlt::ACTION_CONFIRM_80:
                if (CanReceiveBoost(player, LevelYourAlt::TIER_80, creature))
                {
                    CloseGossipMenuFor(player);
                    ApplyLevelBoost(player, LevelYourAlt::TIER_80);
                }
                break;

            default:
                CloseGossipMenuFor(player);
                break;
        }

        return true;
    }
};

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void AddLevelYourAltScripts()
{
    new npc_level_your_alt();
}
