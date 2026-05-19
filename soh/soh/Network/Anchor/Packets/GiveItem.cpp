#include "soh/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Notification/Notification.h"
#include "soh/Enhancements/randomizer/randomizer.h"
#include "soh/SohGui/ImGuiUtils.h"
#include "soh/Enhancements/item-tables/ItemTableManager.h"
#include "soh/OTRGlobals.h"

extern "C" {
#include "functions.h"
extern PlayState* gPlayState;
}

/**
 * GIVE_ITEM
 */

uint8_t incomingIceTrapsFromAnchor = 0;

void Anchor::SendPacket_GiveItem(u16 modId, s16 getItemId) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags) {
        return;
    }

    if (modId == MOD_RANDOMIZER && getItemId == RG_ICE_TRAP && incomingIceTrapsFromAnchor > 0) {
        incomingIceTrapsFromAnchor = MAX(incomingIceTrapsFromAnchor - 1, 0);
        return;
    }

    // Ignore sending master sword in final Ganon fight
    if (modId == MOD_RANDOMIZER && getItemId == RG_MASTER_SWORD && gPlayState->sceneNum == SCENE_GANON_BOSS) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = GIVE_ITEM;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["modId"] = modId;
    payload["getItemId"] = getItemId;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_GiveItem(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    const char* icon = nullptr;
    std::string prefix = "";
    std::string message = "";
    std::string suffix = "";
    std::string preposition = "";
    std::string info = "";
    bool mute = false;
    

    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    AnchorClient& client = clients[clientId];
    u16 modId = payload.at("modId").get<u16>();
    u16 getItemId = payload.at("getItemId").get<u16>();

    GetItemEntry getItemEntry;
    if (modId == MOD_NONE) {
        getItemEntry = ItemTableManager::Instance->RetrieveItemEntry(MOD_NONE, getItemId);
    } else {
        getItemEntry = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(getItemId)).GetGIEntry_Copy();
    }

    if (getItemEntry.modIndex == MOD_NONE) {
        if (getItemEntry.getItemId == GI_SWORD_BGS) {
            gSaveContext.bgsFlag = true;
        }
        Item_Give(gPlayState, static_cast<u8>(getItemEntry.itemId));
    } else if (getItemEntry.modIndex == MOD_RANDOMIZER) {
        if (getItemEntry.getItemId == RG_ICE_TRAP) {
            gSaveContext.ship.pendingIceTrapCount++;
            incomingIceTrapsFromAnchor++;
        } else {
            Randomizer_Item_Give(gPlayState, getItemEntry);
        }
    }

    // Full heal if getting a heart container or piece
    if (getItemEntry.gid == GID_HEART_CONTAINER || getItemEntry.gid == GID_HEART_PIECE) {
        gSaveContext.healthAccumulator = 0x140;
    }

    // Handle if the player gets a 4th heart piece (usually handled in z_message)
    s32 heartPieces = (s32)(gSaveContext.inventory.questItems & 0xF0000000) >> (QUEST_HEART_PIECE + 4);
    if (heartPieces >= 4) {
        gSaveContext.inventory.questItems &= ~0xF0000000;
        gSaveContext.inventory.questItems += (heartPieces % 4) << (QUEST_HEART_PIECE + 4);
        gSaveContext.healthCapacity += 0x10 * (heartPieces / 4);
        gSaveContext.health += 0x10 * (heartPieces / 4);
    }

    if (getItemEntry.getItemCategory != ITEM_CATEGORY_JUNK) {

        prefix = client.name;
        if ((getItemEntry.itemId >= ITEM_SONG_LULLABY && getItemEntry.itemId <= ITEM_SONG_PRELUDE) ||
            (getItemEntry.getItemId >= 0xBB && getItemEntry.getItemId <= 0xC6) || 
            (getItemEntry.objectId == OBJECT_GI_MELODY)){
            message = "learned";
        } else {
            message = "found";
        }
        if (getItemEntry.modIndex == MOD_NONE) {
            icon = GetTextureForItemId(getItemEntry.itemId);
            suffix = SohUtils::GetItemName(getItemEntry.itemId);
        } else if (getItemEntry.modIndex == MOD_RANDOMIZER) {
            suffix = Rando::StaticData::RetrieveItem((RandomizerGet)getItemEntry.getItemId).GetName().english;
        }

        if (locationMessage != "")
        {
            preposition = "from";
            info = locationMessage;
            locationMessage = "";
        }

        if (suffix == "Master Sword" && info == "Market ToT Master Sword") {
            locationMessage = "";
            return;
        }

        if (!CVarGetInteger(CVAR_SETTING("NotificationLocationInfo"), 0))
        {
            preposition = "";
            info = "";
        }

        if (getItemEntry.getItemCategory == ITEM_CATEGORY_MAJOR || getItemEntry.getItemCategory == ITEM_CATEGORY_BOSS_KEY) {
            Audio_PlayFanfare_Rando(getItemEntry);
            mute = true;
        }

        Notification::Emit({
            .itemIcon = icon,
            .prefix = prefix,
            .message = message,
            .suffix = suffix,
            .preposition = preposition,
            .info = info,
            .mute = mute,
        });
    }
    locationMessage = "";
}
