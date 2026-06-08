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
    nlohmann::json payload;
    payload["type"] = ENTER_SCENE;
    payload["sceneNum"] = sceneNum;
    payload["quiet"] = true;
    payload["linkAge"] = gSaveContext.linkAge;

    if (sceneNum != SCENE_INSIDE_GANONS_CASTLE && sceneNum != SCENE_GANONS_TOWER && sceneNum != SCENE_GANONDORF_BOSS &&
        sceneNum != SCENE_GANON_BOSS) {
        payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    }

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_EnterScene(nlohmann::json payload) {
    if (!IsSaveLoaded()) {
        return;
    }

    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    AnchorClient& client = clients[clientId];

    if (payload.value("linkAge", (s32)LINK_AGE_ADULT)== 0) {
        client.displayAge = 0;
    }
    if (payload.value("linkAge", (s32)LINK_AGE_ADULT) == 1) {
        client.displayAge = 1;
    }

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

void Anchor::SendPacket_PlayerDeath() {
    if (!IsSaveLoaded()) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = PLAYER_DEATH;
    payload["quiet"] = true;

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_PlayerDeath(nlohmann::json payload) {
    if (!IsSaveLoaded()) {
        return;
    }

    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    AnchorClient& client = clients[clientId];

    std::string prefix = client.name;
    std::string message = "died!";

    if (message != "") {

        Notification::Emit({
            .prefix = prefix,
            .message = message,
        });
    }
}

void Anchor::SendPacket_BossDefeat(void* refActor) {
    if (!IsSaveLoaded()) {
        return;
    }

    Actor* bossActor = (Actor*)refActor;

    if (bossActor->id == ACTOR_BOSS_GANON2) {
        return;
    }

    nlohmann::json payload;
    payload["type"] = BOSS_DEFEAT;
    payload["bossID"] = bossActor->id;

    if (roomState.bossDefNotifMode == 0) {
        payload["targetTeamId"] = CVarGetString(CVAR_REMOTE_ANCHOR("TeamId"), "default");
    }

    SendJsonToRemote(payload);
}

void Anchor::HandlePacket_BossDefeat(nlohmann::json payload) {
    if (!IsSaveLoaded()) {
        return;
    }

    uint32_t clientId = payload.at("clientId").get<uint32_t>();
    uint16_t bossId = payload.at("bossID").get<uint16_t>();
    AnchorClient& client = clients[clientId];

    std::string prefix = client.name;
    std::string message = "defeated";
    std::string info = "";

    switch (bossId) { 
        case ACTOR_BOSS_GOMA:
            info = "Gohma";
            break;
        case ACTOR_BOSS_DODONGO:
            info = "King Dodongo";
            break;
        case ACTOR_BOSS_VA:
            info = "Barinade";
            break;
        case ACTOR_BOSS_GANONDROF:
            info = "Phantom Ganon";
            break;
        case ACTOR_BOSS_FD2:
            info = "Volvagia";
            break;
        case ACTOR_BOSS_MO:
            info = "Morpha";
            break;
        case ACTOR_BOSS_SST:
            info = "Bongo Bongo";
            break;
        case ACTOR_BOSS_TW:
            info = "Twinrova";
            break;
        case ACTOR_BOSS_GANON:
            info = "Ganondorf";
            break;
        default:
            break;
    }

    if (message != "") {
        Notification::Emit({
            .prefix = prefix,
            .message = message,
            .info = info,
        });
    }
}
