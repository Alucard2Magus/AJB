#pragma once

// PairMode.h — Pair Mode support for AJB PC Port (Multi-Team v4)

#include "../../Dumper-7/SDK/AJB_classes.hpp"
#include "../../Dumper-7/SDK/BP_AJBGameInstance_classes.hpp"
#include "../../Dumper-7/CustomSDK/BP_Synchronizer_classes.hpp"
#include "../Global.hpp"

#include <string>
#include <sstream>
#include <format>
#include <map>
#include <vector>

namespace PairMode
{
    using namespace A8CL;

    // --- Session State ---
    inline bool bIsPairSession = false;
    inline bool bMatchingPlayersFixed = false;
    inline UC::FString PairUserID{};
    inline int32 SelectedMapArea = 3; // Default: Angelo Rock (Area=3)

    // --- My Team ---
    // Each instance stores its own team name (persists across matches).
    // Set by "team <name>". Empty string = solo/no team.
    inline std::string MyTeamName{};

    // --- Client Self-Identification ---
    // The client's own TMap key prefix (computer name), used to find
    // itself in the MatchingPlayers TMap. TMap keys are COMPUTERNAME-HEXHASH.
    inline std::string MyTMapKeyPrefix{};

    // --- Host-side Team Lookup ---
    // Maps das-XXXX (connection name from login) -> team name.
    // Built from PreLogin Options parsing (?PairTeam=xxx).
    // The host's own entry is added during SetupHostPairData.
    inline std::map<std::string, std::string> ConnectionTeamMap{};

    // --- Legacy (kept for backward compat with pairsetguest) ---
    inline uint8 HostCharacterID = 0;
    inline uint8 GuestCharacterID = 0;

    // --- Fix run counter (for log verbosity) ---
    inline int fixRunCount = 0;

    // --- Locked Pairs (host-side) ---
    // Once two players share a team name, they get a TeamID and are locked.
    // Maps TMap key (das-XXXX truncated) -> assigned TeamID.
    // Locked entries are never reassigned on subsequent fix runs.
    inline std::map<std::string, int32> LockedTeamIDs{};
    // Maps TeamID -> the TMap key of the first member (team host) for that pair.
    // Used to re-apply TeamHostUserID after server travel.
    inline std::map<int32, std::string> LockedTeamHostKeys{};
    inline int32 NextTeamID = 1;

    // Try to lock new pairs as players arrive (host-side, called from OnPostLogin).
    // Only locks pairs — doesn't do the full TMap write.
    void TryLockPairs();

    void BuildPlayerInfo(
        SDK::FMatchingPlayerInfo& Out,
        uint8 PlayerID,
        const wchar_t* GameServerUserID,
        const wchar_t* PlayerName,
        uint8 TeamID,
        const wchar_t* TeamHostUserID,
        uint8 CharactorID,
        uint8 CharaSkinID = 0,
        uint8 StandSkinID = 0
    );

    void SetupHostPairData();

    // Fix MatchingPlayers TMap entries in-place with correct pair data.
    // Multi-team aware: assigns TeamIDs based on ConnectionTeamMap (host)
    // or MyTeamName (client).
    void FixMatchingPlayersForPair();

    // Called from AJBPreLogin hook on the host.
    // Parses ?PairTeam=xxx from the Options string and maps it to the
    // connecting player's ?Name=das-XXXX.
    void OnPreLogin(const std::string& Options);

    // Called from PostLogin hook on the host.
    void OnPostLogin(SDK::AGameModeBase* GameMode, SDK::APlayerController* NewPlayer);

    // Called from InitLocalConnection hook on the client.
    // Appends ?PairTeam=xxx to the URL options if a team is set.
    void OnInitLocalConnection(SDK::FURL& InURL);

    bool HandleCommand(const std::string& StrCommand);

    void Reset();
}
