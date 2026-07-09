#include "soh/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include <libultraship/libultraship.h>
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
    //payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["addToQueue"] = true;
    payload["modId"] = modId;
    payload["getItemId"] = getItemId;
    payload["ownTeamId"] = "";

    if (modId == MOD_RANDOMIZER && getItemId == RG_ICE_TRAP && roomState.iceTrapMode != 0) {
        payload["ownTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");

        if (roomState.iceTrapMode == 1) {
            Notification::Emit({
                .message = "You sent the trap to your opponents!",
            });
        }
        if (roomState.iceTrapMode == 2) {
            Notification::Emit({
                .message = "You sent the trap to everyone!",
            });
        }
    }

    if (!(modId == MOD_RANDOMIZER && getItemId == RG_ICE_TRAP && roomState.iceTrapMode != 0)) {
        payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    }

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
    std::string ownTeamId = payload["ownTeamId"].get<std::string>();

    GetItemEntry getItemEntry;
    if (modId == MOD_NONE) {
        getItemEntry = ItemTableManager::Instance->RetrieveItemEntry(MOD_NONE, getItemId);
    } else {
        getItemEntry = Rando::StaticData::RetrieveItem(static_cast<RandomizerGet>(getItemId)).GetGIEntry_Copy();
    }

    // For ice traps that only go to opposing teams
    if (modId == MOD_RANDOMIZER && getItemId == RG_ICE_TRAP && roomState.iceTrapMode == 1) {
        if (ownTeamId == CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default")) {

            prefix = client.name;
            message = "found an";
            suffix = Rando::StaticData::RetrieveItem((RandomizerGet)getItemEntry.getItemId).GetName().english;
            preposition = "and sent it to opponents!";

            Notification::Emit({
                .itemIcon = icon,
                .prefix = prefix,
                .message = message,
                .suffix = suffix,
                .preposition = preposition,
                .info = info,
                .mute = mute,
            });
            return;
        }
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
        message = "found";

        if (getItemEntry.modIndex == MOD_NONE) {
            icon = GetTextureForItemId(getItemEntry.itemId);
            suffix = SohUtils::GetItemName(getItemEntry.itemId);
        } else if (getItemEntry.modIndex == MOD_RANDOMIZER) {
            suffix = Rando::StaticData::RetrieveItem((RandomizerGet)getItemEntry.getItemId).GetName().english;
        }

        if (suffix == "Zelda's Lullaby" || suffix == "Epona's Song" || suffix == "Saria's Song" ||
            suffix == "Sun's Song" || suffix == "Song of Time" || suffix == "Song of Storms" ||
            suffix == "Minuet of Forest" || suffix == "Bolero of Fire" || suffix == "Serenade of Water" ||
            suffix == "Nocturne of Shadow" || suffix == "Requiem of Spirit" || suffix == "Prelude of Light") {
            message = "learned";
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
            if (suffix != "Bombchu (5)" && suffix != "Bottle with Blue Fire") {
                Audio_PlayFanfare_Rando(getItemEntry);
                mute = true;
            }
        }

        // For ice traps that are only received by opposing teams
        if (modId == MOD_RANDOMIZER && getItemId == RG_ICE_TRAP && roomState.iceTrapMode != 0) {
            if (ownTeamId != CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default")) {

                prefix = client.name;
                message = "sent your team an";
                suffix = Rando::StaticData::RetrieveItem((RandomizerGet)getItemEntry.getItemId).GetName().english;
                preposition = "!";
                info = "";
            }
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
