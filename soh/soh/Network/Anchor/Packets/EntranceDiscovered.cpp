#include "soh/Network/Anchor/Anchor.h"
#include <nlohmann/json.hpp>
#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/randomizer/randomizer_entrance.h"
#include "soh/OTRGlobals.h"
#include "soh/Notification/Notification.h"

/**
 * ENTRANCE_DISCOVERED
 */

void Anchor::SendPacket_EntranceDiscovered(u16 entranceIndex) {
    if (!IsSaveLoaded() || isProcessingIncomingPacket || !roomState.syncItemsAndFlags) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = ENTRANCE_DISCOVERED;
    payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    payload["entranceIndex"] = entranceIndex;
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_EntranceDiscovered(nlohmann::json payload) {
    if (!IsSaveLoaded() || !roomState.syncItemsAndFlags) {
        return;
    }

    u16 entranceIndex = payload.at("entranceIndex").get<u16>();
    Entrance_SetEntranceDiscovered(entranceIndex, 1);
}

void Anchor::SendPacket_EnterScene(s16 sceneNum) {
    if (!IsSaveLoaded()) {
//        return;
    }

    nlohmann::json payload;
    payload["type"] = ENTER_SCENE;
    payload["sceneNum"] = sceneNum;
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_EnterScene(nlohmann::json payload) {
    if (!IsSaveLoaded()) {
        return;
    }

    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    AnchorClient& client = clients[clientId];

    s16 sceneNum = payload.at("sceneNum").get<s16>();
    std::string prefix = client.name;
    std::string message = "";

    switch (sceneNum) { 
    case SCENE_INSIDE_GANONS_CASTLE:
        message = "entered Ganon's Castle!";
        break;
    case SCENE_GANONS_TOWER:
        message = "entered Ganon's Tower!";
        break;
    case SCENE_GANONDORF_BOSS:
        message = "entered Ganondorf's Lair!";
        break;
    case SCENE_GANON_BOSS:
        message = "is facing Ganon!";
        break;
    default:
        break;
    }

    if (message != "") {

        Notification::Emit({
            .prefix = prefix,
            .message = message,
        });
    }
}
