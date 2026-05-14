#include "PairMode.h"
#include "AJB.h"
#include "../CmdArgs/CommandLineArgs.h"
#include "../../Dumper-7/CustomSDK/BP_GlobalPatcher_classes.hpp"

using namespace A8CL;
using namespace Global;

// ============================================================
// Helpers 
// Claude Opus
// ============================================================

static void SetFString(UC::FString* Dst, const wchar_t* Src)
{
    UC::FString Temp{Src};
    AJB::CopyString(Dst, &Temp);
}

static void CopyFString(UC::FString* Dst, UC::FString* Src)
{
    AJB::CopyString(Dst, Src);
}

static std::string WtoA(const wchar_t* W)
{
    if (!W) return "NULL";
    std::wstring ws(W);
    return std::string(ws.begin(), ws.end());
}

static std::wstring AtoW(const std::string& A)
{
    return std::wstring(A.begin(), A.end());
}

// Parse a ?Key=Value from a URL options string. Returns empty if not found.
static std::string ParseOption(const std::string& Options, const std::string& Key)
{
    std::string SearchKey = Key + "=";
    size_t pos = Options.find(SearchKey);
    if (pos == std::string::npos) return "";

    size_t valueStart = pos + SearchKey.length();
    size_t valueEnd = Options.find('?', valueStart);
    if (valueEnd == std::string::npos)
        return Options.substr(valueStart);
    return Options.substr(valueStart, valueEnd - valueStart);
}

// Parse ?Name=das-XXXX from Options (may be truncated at 16 chars by UE4 in some places)
static std::string ParseLoginName(const std::string& Options)
{
    return ParseOption(Options, "Name");
}

// ============================================================
// BuildPlayerInfo
// ============================================================

void PairMode::BuildPlayerInfo(
    SDK::FMatchingPlayerInfo& Out,
    uint8 PlayerID,
    const wchar_t* GameServerUserID,
    const wchar_t* PlayerName,
    uint8 TeamID,
    const wchar_t* TeamHostUserID,
    uint8 CharactorID,
    uint8 CharaSkinID,
    uint8 StandSkinID)
{
    memset(&Out, 0, sizeof(SDK::FMatchingPlayerInfo));

    Out.PlayerID = PlayerID;
    Out.TeamID = TeamID;
    Out.CharactorID = CharactorID;
    Out.PlayerIconID = 0;
    Out.PlayerLevel = 1;
    Out.Rate = 0;
    Out.bIsCameraMode = false;
    Out.CustomData.charaSkinId = CharaSkinID;
    Out.CustomData.standSkinId = StandSkinID;

    SetFString(&Out.GameServerUserID, GameServerUserID);
    SetFString(&Out.PlayerName, PlayerName);
    SetFString(&Out.TeamHostUserID, TeamHostUserID);
    SetFString(&Out.PlayerTitle, L"");
}

// ============================================================
// SetupHostPairData
// ============================================================

void PairMode::SetupHostPairData()
{
    if (!AJB::Instance)
    {
        LogA("PairMode", "ERROR: GameInstance is null.");
        return;
    }

    const wchar_t* HostUserID = PairUserID.CStr() && PairUserID.Num() > 0
        ? PairUserID.CStr()
        : CMLA::Username.GetArgumentAsString();

    const wchar_t* HostName = CMLA::Username.GetArgumentAsString();

    HostCharacterID = static_cast<uint8>(AJB::TEMP_CachedCharacterID);
    unsigned char HostSkinID = AJB::GetSelectedSkin();
    unsigned char HostStandSkinID = AJB::GetSelectedStandSkin();

    if (HostSkinID == 0xFF) HostSkinID = 0;
    if (HostStandSkinID == 0xFF) HostStandSkinID = 0;

    LogA("PairMode", std::format("[SetupHost] UserID: {} | CharID: {} | SkinID: {} | StandSkinID: {} | Team: {}",
        WtoA(HostUserID), (int)HostCharacterID, (int)HostSkinID, (int)HostStandSkinID,
        MyTeamName.empty() ? "(none)" : MyTeamName));

    BuildPlayerInfo(
        AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo,
        1, HostUserID, HostName, 1, HostUserID,
        HostCharacterID, HostSkinID, HostStandSkinID
    );

    BuildPlayerInfo(
        AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo,
        1, HostUserID, HostName, 1, HostUserID,
        HostCharacterID, HostSkinID, HostStandSkinID
    );

    AJB::Instance->PlayMode = SDK::EPlayMode::Pair;
    AJB::Instance->bIsLocalSessionMode = true;

    if (AJB::MOD_Global_Synchronizer)
        AJB::MOD_Global_Synchronizer->PlayMode = static_cast<int32>(SDK::EPlayMode::Pair);

    // Clear team lookup and register the host's own team
    ConnectionTeamMap.clear();
    // Host's TMap key isn't known yet (assigned by UE4 networking).
    // We'll add it in FixMatchingPlayersForPair using index 0.

    bIsPairSession = true;
    bMatchingPlayersFixed = false;

    LogA("PairMode", "Host pair data set up. Waiting for players.");
}

// ============================================================
// OnPreLogin — HOST SIDE
// Called from the AJBPreLogin hook. Extracts ?PairTeam=xxx
// from Options and maps it to the ?Name=das-XXXX.
// ============================================================

void PairMode::OnPreLogin(const std::string& Options)
{
    if (!bIsPairSession) return;
    if (!AJB::IsServer()) return;

    std::string loginName = ParseLoginName(Options);
    std::string teamName = ParseOption(Options, "PairTeam");

    if (!loginName.empty() && !teamName.empty())
    {
        // UE4 may truncate the Name in some log lines but the full key
        // is used internally. Store what we got — FixMatchingPlayersForPair
        // will match by prefix if needed.
        ConnectionTeamMap[loginName] = teamName;
        LogA("PairMode", std::format("[OnPreLogin] Registered: {} -> team '{}'", loginName, teamName));
    }
    else if (!loginName.empty())
    {
        LogA("PairMode", std::format("[OnPreLogin] Player {} has no PairTeam set (solo).", loginName));
    }
}

// ============================================================
// OnInitLocalConnection — CLIENT SIDE
// Appends ?PairTeam=xxx to the URL options before connecting.
// ============================================================

void PairMode::OnInitLocalConnection(SDK::FURL& InURL)
{
    if (MyTeamName.empty()) return;
    if (AJB::IsServer()) return; // Host doesn't need this

    // Check if PairTeam is already in the options
    for (int i = 0; i < InURL.Op.Num(); ++i)
    {
        if (InURL.Op[i].ToString().find("PairTeam") != std::string::npos)
            return; // Already appended
    }

    // Append PairTeam option
    std::wstring optionStr = L"PairTeam=" + AtoW(MyTeamName);
    static SDK::FString PairTeamOption{};
    SetFString(&PairTeamOption, optionStr.c_str());
    if (AJB::MOD_GlobalPatcher)
    {
        AJB::MOD_GlobalPatcher->AppendToFStringArray(InURL.Op, PairTeamOption);
        bIsPairSession = true; // Joining with a team name = pair session
        LogA("PairMode", std::format("[OnInitLocalConnection] Appended ?PairTeam={}", MyTeamName));
    }
}

// ============================================================
// FixMatchingPlayersForPair — RUNS ON ALL INSTANCES
//
// Multi-team version:
// 1. Iterates all TMap entries
// 2. On the HOST: uses ConnectionTeamMap to look up each entry's
//    team name. Pairs entries with matching team names.
// 3. On CLIENTs: uses MyTeamName to find the partner entry.
// 4. Assigns TeamIDs, CharactorIDs, PlayerIDs to all entries.
// 5. Sets up PairRoomPlayerInfo for THIS instance's pair.
// ============================================================

void PairMode::FixMatchingPlayersForPair()
{
    if (!AJB::Instance) return;
    if (!bIsPairSession) return;
    if (bMatchingPlayersFixed) return;

    fixRunCount++;
    const bool verbose = (fixRunCount <= 3); // full logging for first 3 runs only

    // Lazy init: grab the computer name for client self-identification.
    // TMap keys are COMPUTERNAME-HEXHASH (e.g. "DESKTOP-MPQA52L-6F4C").
    // We prefix-match against the computer name to find our own entry.
    if (MyTMapKeyPrefix.empty())
    {
        char compName[MAX_COMPUTERNAME_LENGTH + 1]{};
        DWORD size = sizeof(compName);
        if (GetComputerNameA(compName, &size))
        {
            MyTMapKeyPrefix = compName;
            LogA("PairMode", std::format("[Fix] Computer name for TMap matching: '{}'", MyTMapKeyPrefix));
        }
    }

    const bool isServer = AJB::IsServer();
    const int MapCount = AJB::Instance->MatchingPlayers.Num();
    if (MapCount < 2)
    {
        if (verbose)
            LogA("PairMode", std::format("[Fix] Not enough entries yet ({}), will retry.", MapCount));
        return;
    }

    LogA("PairMode", std::format("[Fix] Run #{}: Fixing {} entries (IsServer={})...", fixRunCount, MapCount, isServer));

    // Dump BEFORE state
    if (verbose)
    {
        for (int i = 0; i < MapCount; ++i)
        {
            auto& Entry = AJB::Instance->MatchingPlayers[i];
            LogA("PairMode", std::format("[BEFORE][{}] Key: {} | {}", i, Entry.First.ToString(), AJB::PlayerInfoParser(Entry.Second)));
        }
    }

    // ==================================================================
    // HOST PATH — full authority over TeamIDs
    // ==================================================================
    if (isServer)
    {
        // Register host's own team under its TMap key (index 0)
        std::string hostKey = AJB::Instance->MatchingPlayers[0].First.ToString();
        if (ConnectionTeamMap.find(hostKey) == ConnectionTeamMap.end())
        {
            ConnectionTeamMap[hostKey] = MyTeamName;
            LogA("PairMode", std::format("[Fix] Host registered: {} -> team '{}'", hostKey, MyTeamName));
        }

        // Resolve each entry's team name via ConnectionTeamMap
        std::vector<std::string> teamForEntry(MapCount);
        for (int i = 0; i < MapCount; ++i)
        {
            std::string entryKey = AJB::Instance->MatchingPlayers[i].First.ToString();

            // If already locked, use the locked TeamID directly
            if (LockedTeamIDs.count(entryKey))
            {
                teamForEntry[i] = "_locked";
                continue;
            }

            auto it = ConnectionTeamMap.find(entryKey);
            if (it != ConnectionTeamMap.end())
            {
                teamForEntry[i] = it->second;
            }
            else
            {
                // Prefix match (UE4 may truncate keys)
                for (auto& [connKey, connTeam] : ConnectionTeamMap)
                {
                    if (entryKey.length() >= 8 && (connKey.find(entryKey) == 0 || entryKey.find(connKey) == 0))
                    {
                        teamForEntry[i] = connTeam;
                        break;
                    }
                }
            }
            if (verbose)
                LogA("PairMode", std::format("[Fix] Entry[{}] {} -> team '{}'", i, entryKey,
                    teamForEntry[i].empty() ? "(solo)" : teamForEntry[i]));
        }

        // First: try to lock any unlocked entries that have matching team names
        // (in case TryLockPairs missed them due to timing)
        {
            std::map<std::string, std::vector<int>> unlockedByTeam;
            for (int i = 0; i < MapCount; ++i)
            {
                std::string entryKey = AJB::Instance->MatchingPlayers[i].First.ToString();
                if (LockedTeamIDs.count(entryKey)) continue;
                if (teamForEntry[i].empty() || teamForEntry[i] == "_locked") continue;
                unlockedByTeam[teamForEntry[i]].push_back(i);
            }
            for (auto& [teamName, members] : unlockedByTeam)
            {
                if (members.size() >= 2)
                {
                    int idx1 = members[0];
                    int idx2 = members[1];
                    std::string key1 = AJB::Instance->MatchingPlayers[idx1].First.ToString();
                    std::string key2 = AJB::Instance->MatchingPlayers[idx2].First.ToString();

                    int32 teamID = NextTeamID++;
                    LockedTeamIDs[key1] = teamID;
                    LockedTeamIDs[key2] = teamID;
                    LockedTeamHostKeys[teamID] = key1;

                    AJB::Instance->MatchingPlayers[idx1].Second.TeamID = teamID;
                    AJB::Instance->MatchingPlayers[idx2].Second.TeamID = teamID;
                    CopyFString(&AJB::Instance->MatchingPlayers[idx1].Second.TeamHostUserID,
                        &AJB::Instance->MatchingPlayers[idx1].First);
                    CopyFString(&AJB::Instance->MatchingPlayers[idx2].Second.TeamHostUserID,
                        &AJB::Instance->MatchingPlayers[idx1].First);

                    LogA("PairMode", std::format("[PAIR LOCKED] '{}': {} + {} = TeamID {}",
                        teamName, key1, key2, teamID));
                }
            }
        }

        // Then: auto-pair ALL remaining unlocked players (unmatched team names + solos)
        {
            std::vector<int> unpairedIndices;
            for (int i = 0; i < MapCount; ++i)
            {
                std::string entryKey = AJB::Instance->MatchingPlayers[i].First.ToString();
                if (LockedTeamIDs.count(entryKey)) continue; // already locked
                unpairedIndices.push_back(i);
            }
            int autoPairNum = 1;
            for (size_t s = 0; s + 1 < unpairedIndices.size(); s += 2)
            {
                int idx1 = unpairedIndices[s];
                int idx2 = unpairedIndices[s + 1];
                std::string key1 = AJB::Instance->MatchingPlayers[idx1].First.ToString();
                std::string key2 = AJB::Instance->MatchingPlayers[idx2].First.ToString();

                int32 teamID = NextTeamID++;
                LockedTeamIDs[key1] = teamID;
                LockedTeamIDs[key2] = teamID;
                LockedTeamHostKeys[teamID] = key1;
                AJB::Instance->MatchingPlayers[idx1].Second.TeamID = teamID;
                AJB::Instance->MatchingPlayers[idx2].Second.TeamID = teamID;
                CopyFString(&AJB::Instance->MatchingPlayers[idx1].Second.TeamHostUserID,
                    &AJB::Instance->MatchingPlayers[idx1].First);
                CopyFString(&AJB::Instance->MatchingPlayers[idx2].Second.TeamHostUserID,
                    &AJB::Instance->MatchingPlayers[idx1].First);

                LogA("PairMode", std::format("[PAIR LOCKED] Auto-paired: {} + {} = TeamID {}",
                    key1, key2, teamID));
            }
            if (unpairedIndices.size() % 2 == 1)
            {
                LogA("PairMode", std::format("[Fix] Odd player left unpaired: idx {}", unpairedIndices.back()));
            }
        }

        // Write cosmetic fields for ALL entries (locked or not)
        for (int i = 0; i < MapCount; ++i)
        {
            auto& Info = AJB::Instance->MatchingPlayers[i].Second;
            auto& Key  = AJB::Instance->MatchingPlayers[i].First;
            std::string entryKey = Key.ToString();

            Info.PlayerID = static_cast<uint8>(i + 1);
            CopyFString(&Info.GameServerUserID, &Key);
            Info.PlayerIconID = 0;
            Info.PlayerLevel = 1;
            Info.Rate = 0;
            Info.bIsCameraMode = false;
            SetFString(&Info.PlayerTitle, L"");

            // TeamID is already set by LockedTeamIDs — just ensure it's written
            if (LockedTeamIDs.count(entryKey))
            {
                int32 tid = LockedTeamIDs[entryKey];
                Info.TeamID = tid;

                // Re-apply TeamHostUserID from stored host key
                if (LockedTeamHostKeys.count(tid))
                {
                    // Find the TMap entry matching the stored host key (prefix match)
                    const std::string& hostKey = LockedTeamHostKeys[tid];
                    for (int j = 0; j < MapCount; ++j)
                    {
                        std::string jKey = AJB::Instance->MatchingPlayers[j].First.ToString();
                        if (hostKey.find(jKey) == 0 || jKey.find(hostKey) == 0)
                        {
                            CopyFString(&Info.TeamHostUserID, &AJB::Instance->MatchingPlayers[j].First);
                            break;
                        }
                    }
                }
            }
            else
            {
                Info.TeamID = 0; // still unlocked/solo
            }

            if (i == 0)
            {
                SetFString(&Info.PlayerName, CMLA::Username.GetArgumentAsString());
                Info.CharactorID = HostCharacterID > 0 ? HostCharacterID : static_cast<uint8>(AJB::TEMP_CachedCharacterID);
                if (Info.CharactorID == 0) Info.CharactorID = 1;
            }
            else
            {
                if (Info.CharactorID == 0) Info.CharactorID = 1;
                if (!Info.PlayerName.CStr() || Info.PlayerName.Num() <= 1)
                    SetFString(&Info.PlayerName, L"Player");
            }
        }

        // Find host's partner from LockedTeamIDs
        int myIndex = 0;
        int partnerIndex = -1;
        std::string myKey = AJB::Instance->MatchingPlayers[0].First.ToString();
        if (LockedTeamIDs.count(myKey))
        {
            int32 myTeamID = LockedTeamIDs[myKey];
            for (int i = 1; i < MapCount; ++i)
            {
                std::string otherKey = AJB::Instance->MatchingPlayers[i].First.ToString();
                if (LockedTeamIDs.count(otherKey) && LockedTeamIDs[otherKey] == myTeamID)
                {
                    partnerIndex = i;
                    break;
                }
            }
        }

        // Fill PairRoomPlayerInfo
        if (partnerIndex >= 0)
        {
            auto& MyInfo = AJB::Instance->MatchingPlayers[myIndex].Second;
            auto& MyKey  = AJB::Instance->MatchingPlayers[myIndex].First;
            auto& PartInfo = AJB::Instance->MatchingPlayers[partnerIndex].Second;
            auto& PartKey  = AJB::Instance->MatchingPlayers[partnerIndex].First;

            auto& HostSlot = AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo;
            HostSlot = MyInfo;
            CopyFString(&HostSlot.GameServerUserID, &MyKey);

            auto& GuestSlot = AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo;
            GuestSlot = PartInfo;
            CopyFString(&GuestSlot.GameServerUserID, &PartKey);

            CopyFString(&AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.GameServerUserID, &MyKey);
            AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.PlayerID = MyInfo.PlayerID;
            AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.TeamID = MyInfo.TeamID;
            AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.CharactorID = MyInfo.CharactorID;

            LogA("PairMode", std::format("[Fix] HOST pair: idx {}+{} TeamID={}", myIndex, partnerIndex, (int)MyInfo.TeamID));
        }
        else
        {
            LogA("PairMode", "[Fix] HOST: partner not found yet.");
        }
    }
    // ==================================================================
    // CLIENT PATH — preserve host-set TeamIDs, only fix cosmetic fields
    // ==================================================================
    else
    {
        // Light touch: fix PlayerID sequencing and cosmetic defaults,
        // but DO NOT overwrite TeamID, CharactorID, or TeamHostUserID.
        // The host set those and they arrived via TMap replication.
        for (int i = 0; i < MapCount; ++i)
        {
            auto& Info = AJB::Instance->MatchingPlayers[i].Second;
            auto& Key  = AJB::Instance->MatchingPlayers[i].First;

            Info.PlayerID = static_cast<uint8>(i + 1);
            CopyFString(&Info.GameServerUserID, &Key);
            if (Info.PlayerIconID < 0) Info.PlayerIconID = 0;
            if (Info.PlayerLevel <= 0) Info.PlayerLevel = 1;
            SetFString(&Info.PlayerTitle, L"");
            // TeamID, CharactorID, TeamHostUserID — UNTOUCHED (host authority)
        }

        // Find MY entry by matching computer name prefix against TMap keys.
        // TMap keys are COMPUTERNAME-HEXHASH (e.g. "DESKTOP-MPQA52L-6F4C").
        // The host already set TeamIDs on all entries, so once we find ourselves
        // we just read our TeamID and find the partner with the same TeamID.

        int myIndex = -1;
        int partnerIndex = -1;

        // Separate real entries (contain '-') from placeholder entries (Player1, etc.)
        struct RealEntry { int index; int32 teamID; uint8 charID; std::string key; };
        std::vector<RealEntry> realEntries;
        for (int i = 0; i < MapCount; ++i)
        {
            std::string key = AJB::Instance->MatchingPlayers[i].First.ToString();
            if (key.find('-') != std::string::npos)
            {
                realEntries.push_back({
                    i,
                    AJB::Instance->MatchingPlayers[i].Second.TeamID,
                    AJB::Instance->MatchingPlayers[i].Second.CharactorID,
                    key
                });
            }
        }

        LogA("PairMode", std::format("[Fix] CLIENT: {} real entries, MyTMapKeyPrefix='{}'", realEntries.size(), MyTMapKeyPrefix));
        for (auto& re : realEntries)
            LogA("PairMode", std::format("[Fix]   idx={} key={} TeamID={} CharID={}", re.index, re.key, re.teamID, (int)re.charID));

        // Strategy 1: find self by computer name prefix match
        if (!MyTMapKeyPrefix.empty())
        {
            for (auto& re : realEntries)
            {
                if (re.key.find(MyTMapKeyPrefix) == 0)
                {
                    myIndex = re.index;
                    LogA("PairMode", std::format("[Fix] CLIENT: found self by prefix '{}' -> idx={} TeamID={}", MyTMapKeyPrefix, myIndex, re.teamID));
                    break;
                }
            }
        }

        // If we found ourselves, find partner by same TeamID
        if (myIndex >= 0)
        {
            int32 myTeamID = AJB::Instance->MatchingPlayers[myIndex].Second.TeamID;
            if (myTeamID > 0)
            {
                for (auto& re : realEntries)
                {
                    if (re.index != myIndex && re.teamID == myTeamID)
                    {
                        partnerIndex = re.index;
                        LogA("PairMode", std::format("[Fix] CLIENT: found partner by TeamID={} -> idx={}", myTeamID, partnerIndex));
                        break;
                    }
                }
            }

            // TeamID was 0 (host hasn't set it yet or replication failed)
            if (partnerIndex < 0 && realEntries.size() >= 2)
            {
                for (auto& re : realEntries)
                {
                    if (re.index != myIndex)
                    {
                        partnerIndex = re.index;
                        AJB::Instance->MatchingPlayers[myIndex].Second.TeamID = 1;
                        AJB::Instance->MatchingPlayers[partnerIndex].Second.TeamID = 1;
                        LogA("PairMode", std::format("[Fix] CLIENT: TeamID=0 fallback, forced pair idx {} + {}", myIndex, partnerIndex));
                        break;
                    }
                }
            }
        }

        // Strategy 2 (fallback): computer name match failed — use CharactorID
        if (myIndex < 0)
        {
            uint8 myCharID = static_cast<uint8>(AJB::TEMP_CachedCharacterID > 0 ? AJB::TEMP_CachedCharacterID : 1);
            LogA("PairMode", std::format("[Fix] CLIENT: prefix match failed, falling back to charID={}", (int)myCharID));

            std::map<int32, std::vector<int>> teamGroups;
            for (auto& re : realEntries)
            {
                if (re.teamID > 0)
                    teamGroups[re.teamID].push_back(re.index);
            }

            for (auto& [tid, members] : teamGroups)
            {
                if (members.size() != 2) continue;
                for (int mi = 0; mi < 2; ++mi)
                {
                    uint8 entryChar = AJB::Instance->MatchingPlayers[members[mi]].Second.CharactorID;
                    if (entryChar == myCharID)
                    {
                        myIndex = members[mi];
                        partnerIndex = members[1 - mi];
                        LogA("PairMode", std::format("[Fix] CLIENT: charID fallback matched in TeamID={}", tid));
                        break;
                    }
                }
                if (myIndex >= 0) break;
            }
        }

        // Strategy 3: last resort — pick last two real entries
        if (myIndex < 0 && realEntries.size() >= 2)
        {
            LogA("PairMode", "[Fix] CLIENT: last resort — picking last two real entries.");
            myIndex = realEntries.back().index;
            partnerIndex = realEntries[realEntries.size() - 2].index;
            AJB::Instance->MatchingPlayers[myIndex].Second.TeamID = 1;
            AJB::Instance->MatchingPlayers[partnerIndex].Second.TeamID = 1;
        }

        // Fill PairRoomPlayerInfo (client is always the "guest")
        if (myIndex >= 0 && partnerIndex >= 0)
        {
            auto& MyInfo = AJB::Instance->MatchingPlayers[myIndex].Second;
            auto& MyKey  = AJB::Instance->MatchingPlayers[myIndex].First;
            auto& PartInfo = AJB::Instance->MatchingPlayers[partnerIndex].Second;
            auto& PartKey  = AJB::Instance->MatchingPlayers[partnerIndex].First;

            // Set TeamHostUserID on both entries (may not replicate from host)
            // Use the lower index as the team host (matches host's convention)
            int teamHostIdx = (myIndex < partnerIndex) ? myIndex : partnerIndex;
            CopyFString(&MyInfo.TeamHostUserID, &AJB::Instance->MatchingPlayers[teamHostIdx].First);
            CopyFString(&PartInfo.TeamHostUserID, &AJB::Instance->MatchingPlayers[teamHostIdx].First);

            auto& HostSlot = AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo;
            HostSlot = PartInfo;
            CopyFString(&HostSlot.GameServerUserID, &PartKey);

            auto& GuestSlot = AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo;
            GuestSlot = MyInfo;
            CopyFString(&GuestSlot.GameServerUserID, &MyKey);

            CopyFString(&AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.GameServerUserID, &MyKey);
            AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.PlayerID = MyInfo.PlayerID;
            AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.TeamID = MyInfo.TeamID;
            AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.CharactorID = MyInfo.CharactorID;
            CopyFString(&AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.TeamHostUserID, 
                &AJB::Instance->MatchingPlayers[teamHostIdx].First);

            LogA("PairMode", std::format("[Fix] CLIENT pair: me=idx{} (char={}) partner=idx{} (char={}) TeamID={}",
                myIndex, (int)MyInfo.CharactorID, partnerIndex, (int)PartInfo.CharactorID, (int)MyInfo.TeamID));
        }
        else
        {
            LogA("PairMode", std::format("[Fix] CLIENT: pair not found. myIdx={} partnerIdx={}", myIndex, partnerIndex));
        }
    }

    // Common finalization
    AJB::Instance->bIsLocalSessionMode = true;
    bMatchingPlayersFixed = true;

    // Dump AFTER state
    if (verbose)
    {
        for (int i = 0; i < MapCount; ++i)
        {
            auto& Entry = AJB::Instance->MatchingPlayers[i];
            LogA("PairMode", std::format("[AFTER][{}] Key: {} | {}", i, Entry.First.ToString(), AJB::PlayerInfoParser(Entry.Second)));
        }
    }

    LogA("PairMode", std::format("[Fix] PairRoom Host: {} CharID={}",
        AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo.GameServerUserID.ToString(),
        (int)AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo.CharactorID));
    LogA("PairMode", std::format("[Fix] PairRoom Guest: {} CharID={}",
        AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo.GameServerUserID.ToString(),
        (int)AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo.CharactorID));
    LogA("PairMode", std::format("[Fix] MyInfo: {} PlayerID={}",
        AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.GameServerUserID.ToString(),
        (int)AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.PlayerID));
    LogA("PairMode", std::format("[Fix] Run #{} Done!", fixRunCount));
}

// ============================================================
// OnPostLogin — HOST SIDE
// ============================================================

void PairMode::OnPostLogin(SDK::AGameModeBase* GameMode, SDK::APlayerController* NewPlayer)
{
    if (!bIsPairSession) return;
    if (!AJB::IsServer()) return;
    if (!AJB::Instance) return;

    int32 PlayerCount = 0;
    if (GameMode && GameMode->GameState)
        PlayerCount = GameMode->GameState->PlayerArray.Num();

    LogA("PairMode", std::format("[OnPostLogin] PlayerCount: {} | Teams registered: {} | Locked pairs: {}",
        PlayerCount, ConnectionTeamMap.size(), LockedTeamIDs.size() / 2));

    // Try to lock pairs once we have enough entries
    if (AJB::Instance->MatchingPlayers.Num() >= 2)
        TryLockPairs();
}

// ============================================================
// TryLockPairs — lock pairs as players arrive (host-side)
// ============================================================

void PairMode::TryLockPairs()
{
    if (!AJB::Instance) return;
    if (!AJB::IsServer()) return;

    const int MapCount = AJB::Instance->MatchingPlayers.Num();
    if (MapCount < 2) return;

    try
    {
    // Register host's own team if not done yet
    std::string hostKey = AJB::Instance->MatchingPlayers[0].First.ToString();
    if (ConnectionTeamMap.find(hostKey) == ConnectionTeamMap.end())
    {
        ConnectionTeamMap[hostKey] = MyTeamName;
        LogA("PairMode", std::format("[Lock] Host registered: {} -> team '{}'", hostKey, MyTeamName));
    }

    // Build team name for each entry
    std::vector<std::string> teamForEntry(MapCount);
    for (int i = 0; i < MapCount; ++i)
    {
        std::string entryKey = AJB::Instance->MatchingPlayers[i].First.ToString();

        // Skip already locked entries
        if (LockedTeamIDs.count(entryKey)) continue;

        auto it = ConnectionTeamMap.find(entryKey);
        if (it != ConnectionTeamMap.end())
        {
            teamForEntry[i] = it->second;
        }
        else
        {
            // Prefix match
            for (auto& [connKey, connTeam] : ConnectionTeamMap)
            {
                if (entryKey.length() >= 8 && (connKey.find(entryKey) == 0 || entryKey.find(connKey) == 0))
                {
                    teamForEntry[i] = connTeam;
                    break;
                }
            }
        }
    }

    // Find unlocked entries that share a team name — lock them as pairs
    // Group unlocked entries by team name
    std::map<std::string, std::vector<int>> unlockedByTeam;
    for (int i = 0; i < MapCount; ++i)
    {
        std::string entryKey = AJB::Instance->MatchingPlayers[i].First.ToString();
        if (LockedTeamIDs.count(entryKey)) continue; // already locked
        if (teamForEntry[i].empty()) continue; // solo/no team
        unlockedByTeam[teamForEntry[i]].push_back(i);
    }

    for (auto& [teamName, members] : unlockedByTeam)
    {
        if (members.size() >= 2)
        {
            // Lock the first two as a pair
            int idx1 = members[0];
            int idx2 = members[1];
            std::string key1 = AJB::Instance->MatchingPlayers[idx1].First.ToString();
            std::string key2 = AJB::Instance->MatchingPlayers[idx2].First.ToString();

            int32 teamID = NextTeamID++;
            LockedTeamIDs[key1] = teamID;
            LockedTeamIDs[key2] = teamID;
            LockedTeamHostKeys[teamID] = key1; // first member is team host

            // Write TeamID immediately so TMap reflects it
            AJB::Instance->MatchingPlayers[idx1].Second.TeamID = teamID;
            AJB::Instance->MatchingPlayers[idx2].Second.TeamID = teamID;

            // Set TeamHostUserID to first member
            CopyFString(&AJB::Instance->MatchingPlayers[idx1].Second.TeamHostUserID,
                &AJB::Instance->MatchingPlayers[idx1].First);
            CopyFString(&AJB::Instance->MatchingPlayers[idx2].Second.TeamHostUserID,
                &AJB::Instance->MatchingPlayers[idx1].First);

            LogA("PairMode", std::format("[PAIR LOCKED] '{}': {} + {} = TeamID {}",
                teamName, key1, key2, teamID));

            // If one of them is the host, update PairRoomPlayerInfo immediately
            if (idx1 == 0 || idx2 == 0)
            {
                int myIdx = 0;
                int partIdx = (idx1 == 0) ? idx2 : idx1;

                auto& MyInfo = AJB::Instance->MatchingPlayers[myIdx].Second;
                auto& MyKey  = AJB::Instance->MatchingPlayers[myIdx].First;
                auto& PartInfo = AJB::Instance->MatchingPlayers[partIdx].Second;
                auto& PartKey  = AJB::Instance->MatchingPlayers[partIdx].First;

                auto& HostSlot = AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo;
                HostSlot = MyInfo;
                CopyFString(&HostSlot.GameServerUserID, &MyKey);

                auto& GuestSlot = AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo;
                GuestSlot = PartInfo;
                CopyFString(&GuestSlot.GameServerUserID, &PartKey);

                CopyFString(&AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.GameServerUserID, &MyKey);
                AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.PlayerID = MyInfo.PlayerID;
                AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.TeamID = teamID;
                AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.CharactorID = MyInfo.CharactorID;

                LogA("PairMode", std::format("[PAIR LOCKED] Host paired with {} (TeamID={})", 
                    PartKey.ToString(), teamID));
            }
        }
    }
    } catch (...) {
        LogA("PairMode", "[TryLockPairs] TMap entries not ready yet, will retry.");
    }
}

// ============================================================
// Console Commands
// ============================================================

bool PairMode::HandleCommand(const std::string& StrCommand)
{
    // --- team <name> ---
    if (StrCommand.find("team ") == 0)
    {
        std::string name = StrCommand.substr(5);
        size_t start = name.find_first_not_of(" \t");
        if (start == std::string::npos)
        {
            MyTeamName.clear();
            LogA("PairMode", "Team cleared. You will play solo.");
        }
        else
        {
            name = name.substr(start);
            size_t end = name.find_last_not_of(" \t");
            if (end != std::string::npos)
                name = name.substr(0, end + 1);
            MyTeamName = name;
            LogA("PairMode", std::format("Team set to: '{}' (cached for future sessions)", MyTeamName));
        }
        return true;
    }

    if (StrCommand == "team")
    {
        LogA("PairMode", std::format("Current team: '{}'", MyTeamName.empty() ? "(none/solo)" : MyTeamName));
        return true;
    }

    // --- hostpair [area] ---
    if (StrCommand.find("hostpair") == 0)
    {
        if (!AJB::Instance)
        {
            LogA("PairMode", "ERROR: GameInstance not available.");
            return true;
        }

        // Clean state from any previous pair session before hosting a new one
        Reset();

        // hostpair [area] [npccount] — e.g. hostpair 8 20
        int npcCount = 0;
        if (StrCommand.length() > 9)
        {
            try
            {
                std::string args = StrCommand.substr(9);
                size_t start = args.find_first_not_of(" \t");
                if (start != std::string::npos)
                {
                    args = args.substr(start);
                    // First arg = area
                    size_t space = args.find_first_of(" \t");
                    if (space != std::string::npos)
                    {
                        SelectedMapArea = std::stoi(args.substr(0, space));
                        // Second arg = npc count
                        std::string npcStr = args.substr(space);
                        size_t npcStart = npcStr.find_first_not_of(" \t");
                        if (npcStart != std::string::npos)
                            npcCount = std::stoi(npcStr.substr(npcStart));
                    }
                    else
                    {
                        SelectedMapArea = std::stoi(args);
                    }
                    LogA("PairMode", std::format("Map area set to: {} | NPCs: {}", SelectedMapArea, npcCount));
                }
            }
            catch (...) {}
        }

        SetupHostPairData();

        // Set battle settings
        SDK::FAJBBattleSettings TheSettings{};
        TheSettings.DamageAreaType = SelectedMapArea;
        TheSettings.AILevel = 0;
        AJB::Instance->SetBattleSettings(TheSettings);
        AJB::Instance->AreaTypeID = SelectedMapArea;
        AJB::Instance->NPCNum = npcCount;
        AJB::Instance->NPCNumMax = npcCount;

        AJB::Instance->CreateSession();
        LogA("PairMode", std::format("Hosting pair session. Area={} | NPCs={} | Team='{}'",
            SelectedMapArea, npcCount, MyTeamName.empty() ? "(none)" : MyTeamName));
        return true;
    }

    // --- joinpair ---
    if (StrCommand == "joinpair")
    {
        if (!AJB::Instance)
        {
            LogA("PairMode", "ERROR: GameInstance not available.");
            return true;
        }

        AJB::Instance->PlayMode = SDK::EPlayMode::Pair;
        AJB::Instance->bIsLocalSessionMode = true;
        bIsPairSession = true;
        bMatchingPlayersFixed = false;

        const wchar_t* MyUserID = PairUserID.CStr() && PairUserID.Num() > 0
            ? PairUserID.CStr()
            : CMLA::Username.GetArgumentAsString();
        const wchar_t* MyName = CMLA::Username.GetArgumentAsString();

        uint8 myCharID = static_cast<uint8>(AJB::TEMP_CachedCharacterID > 0 ? AJB::TEMP_CachedCharacterID : 1);

        // Set up PlayerLoginInfo
        SetFString(&AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.GameServerUserID, MyUserID);
        SetFString(&AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.PlayerName, MyName);
        AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.PlayerID = 2;
        AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.TeamID = 1;
        AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.CharactorID = myCharID;

        // Placeholder PairRoomPlayerInfo (will be overwritten by FixMatchingPlayersForPair)
        BuildPlayerInfo(
            AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo,
            2, MyUserID, MyName, 1, L"Host", myCharID, 0, 0
        );
        BuildPlayerInfo(
            AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo,
            1, L"Host", L"Host", 1, L"Host", 1, 0, 0
        );

        if (AJB::MOD_Global_Synchronizer)
            AJB::MOD_Global_Synchronizer->PlayMode = static_cast<int32>(SDK::EPlayMode::Pair);

        AJB::Instance->JoinSession();

        LogA("PairMode", std::format("Joining pair session as: {} | Team: '{}'",
            WtoA(MyUserID), MyTeamName.empty() ? "(none/solo)" : MyTeamName));
        return true;
    }

    // --- setpairid <id> ---
    if (StrCommand.find("setpairid ") == 0)
    {
        std::string idStr = StrCommand.substr(10);
        size_t start = idStr.find_first_not_of(" \t");
        if (start == std::string::npos)
        {
            LogA("PairMode", "ERROR: Empty pair ID.");
            return true;
        }
        idStr = idStr.substr(start);
        size_t end = idStr.find_last_not_of(" \t");
        if (end != std::string::npos)
            idStr = idStr.substr(0, end + 1);

        std::wstring NewID(idStr.begin(), idStr.end());
        PairUserID = UC::FString(NewID.c_str());
        LogA("PairMode", std::format("Pair ID set to: '{}'", idStr));
        return true;
    }

    // --- info ---
    if (StrCommand == "info")
    {
        if (!AJB::Instance)
        {
            LogA("PairMode", "GameInstance is null.");
            return true;
        }

        LogA("PairMode", std::format("[State] bIsPairSession: {} | bMatchingPlayersFixed: {} | PlayMode: {} | bIsLocalSessionMode: {} | IsServer: {} | MyTeam: '{}'",
            bIsPairSession,
            bMatchingPlayersFixed,
            (int)AJB::Instance->PlayMode,
            (bool)AJB::Instance->bIsLocalSessionMode,
            AJB::IsServer(),
            MyTeamName.empty() ? "(none)" : MyTeamName));

        LogA("PairMode", std::format("[MyInfo] UserID: {} | PlayerID: {} | TeamID: {} | CharID: {}",
            AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.GameServerUserID.ToString(),
            (int)AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.PlayerID,
            (int)AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.TeamID,
            (int)AJB::Instance->PlayerLoginInfo.MatchingPlayerInfo.CharactorID));

        LogA("PairMode", std::format("[PairRoom Host] UserID: {} | PlayerID: {} | CharID: {} | TeamID: {}",
            AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo.GameServerUserID.ToString(),
            (int)AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo.PlayerID,
            (int)AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo.CharactorID,
            (int)AJB::Instance->PairRoomPlayerInfo.HostPlayerInfo.TeamID));

        LogA("PairMode", std::format("[PairRoom Guest] UserID: {} | PlayerID: {} | CharID: {} | TeamID: {}",
            AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo.GameServerUserID.ToString(),
            (int)AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo.PlayerID,
            (int)AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo.CharactorID,
            (int)AJB::Instance->PairRoomPlayerInfo.GuestPlayerInfo.TeamID));

        LogA("PairMode", std::format("[ConnectionTeamMap] {} entries:", ConnectionTeamMap.size()));
        for (auto& [key, team] : ConnectionTeamMap)
            LogA("PairMode", std::format("  {} -> '{}'", key, team));

        LogA("PairMode", std::format("[LockedPairs] {} entries (NextTeamID={}):", LockedTeamIDs.size(), NextTeamID));
        for (auto& [key, tid] : LockedTeamIDs)
            LogA("PairMode", std::format("  {} -> TeamID {}", key, tid));

        LogA("PairMode", std::format("[MatchingPlayers] Count: {}", AJB::Instance->MatchingPlayers.Num()));
        for (int i = 0; i < AJB::Instance->MatchingPlayers.Num(); ++i)
        {
            auto& Entry = AJB::Instance->MatchingPlayers[i];
            LogA("PairMode", std::format("[MatchingPlayers][{}] Key: {} | {}",
                i, Entry.First.ToString(), AJB::PlayerInfoParser(Entry.Second)));
        }

        return true;
    }

    // --- setguest <charID> ---
    if (StrCommand.find("setguest ") == 0)
    {
        try
        {
            GuestCharacterID = static_cast<uint8>(std::stoi(StrCommand.substr(9)));
            LogA("PairMode", std::format("Guest character set to: {}", (int)GuestCharacterID));
        }
        catch (...) { LogA("PairMode", "Invalid character ID."); }
        return true;
    }

    // --- char <charID> ---
    if (StrCommand.find("char ") == 0)
    {
        try
        {
            int charID = std::stoi(StrCommand.substr(5));
            AJB::TEMP_CachedCharacterID = charID;
            AJB::SetSelectedCharacter((AJB::ESelectedCharacter)charID);
            LogA("PairMode", std::format("Character set to: {}", charID));
        }
        catch (...) { LogA("PairMode", "Invalid character ID."); }
        return true;
    }

    // --- setmap <area> ---
    if (StrCommand.find("setmap ") == 0)
    {
        try
        {
            SelectedMapArea = std::stoi(StrCommand.substr(7));
            LogA("PairMode", std::format("Map area set to: {}", SelectedMapArea));
        }
        catch (...)
        {
            LogA("PairMode", "Areas: 2=TrainStation, 3=AngeloRock, 4=Rural, 5=Owson, 6=Kameyu, 7=Trattoria, 8=Cairo, 9=Farm, 10=Colosseum, 11=Venezia");
        }
        return true;
    }

    // --- fix ---
    if (StrCommand == "fix")
    {
        bMatchingPlayersFixed = false;
        FixMatchingPlayersForPair();
        return true;
    }

    return false;
}

// ============================================================
// Reset
// ============================================================

void PairMode::Reset()
{
    bIsPairSession = false;
    bMatchingPlayersFixed = false;
    fixRunCount = 0;
    HostCharacterID = 0;
    GuestCharacterID = 0;
    ConnectionTeamMap.clear();
    LockedTeamIDs.clear();
    LockedTeamHostKeys.clear();
    NextTeamID = 1;
    // Note: MyTeamName is NOT cleared — it persists across matches

    if (AJB::Instance)
        memset(&AJB::Instance->PairRoomPlayerInfo, 0, sizeof(SDK::FRoomPlayerInfo));

    LogA("PairMode", "Pair mode state reset. (Team name preserved)");
}
