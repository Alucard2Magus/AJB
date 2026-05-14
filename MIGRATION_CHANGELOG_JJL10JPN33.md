# AJB PC Port Migration: v0.5.5 → JJL10JPN-33

## Changes Applied

### Step 1: Offsets.cpp — REPLACED
All 65+ function offsets updated to JJL10JPN-33 values (verified via IDA diff).

### Step 2: Offsets.h — Byte patches UPDATED
All 9 constexpr byte patch addresses updated:
- HideCursorCaller: 0x04A04A0 → 0x04A66B0
- AJBGetMaxTickRate: 0x13CCB43 → 0x13D2D53
- AJBGetMaxTickRateCap: 0x13CCBD8 → 0x13D2DE8
- ResetPP: 0x04840B0 → 0x0484420
- StartConsumePP: 0x0522CE0 → 0x05282A0
- LogVerbosity: 0x300D3C8 → 0x3017348
- NetDriverGetNetMode: 0x14F90F0 → 0x14FF300
- WorldInternalGetNetMode: 0x17C4820 → 0x17CAA30
- ActorInternalGetNetMode: 0x11BA5C0 → 0x11C07D0

### Step 3: SDK — REPLACED
- Dumper-7/SDK/ — Entire folder replaced (3502 files)
- SDK.hpp, UnrealContainers.hpp, PropertyFixup.hpp, NameCollisions.inl, UtfN.hpp — Replaced
- Assertions.inl — NEW FILE added (required by new Basic.hpp)
- Basic.hpp offsets verified: GObjects=0x02DEE120, AppendString=0x00689A10, GNames=0x0315D630, GWorld=0x03261B70, ProcessEvent=0x00829D50, ProcessEventIdx=0x41

### Step 4: Removed Class — HANDLED
- BP_SimpleStartLocationSelectGameMode_classes.hpp no longer exists in new SDK
- #include commented out in AJB.cpp:38 and UFunctions.cpp:206
- Map string comparison at UFunctions.cpp:1261 KEPT (map may still exist, only the GameMode class was removed)
- Commented-out servertravel string at AJB.cpp:1076 unchanged (already dead code)

### Step 5: Struct Layout Changes — HANDLED BY SDK REPLACEMENT
- Engine classes (UWorld, AActor, APlayerController, AGameModeBase) — sizes UNCHANGED
- ABP_AJBInGamePlayerController_C grew 0x08E8 → 0x0900 (+0x18) — new SDK has correct layout
- 52 changed SDK class files all replaced wholesale

### Step 6: CustomSDK — VERIFIED COMPATIBLE
- All CustomSDK files inherit from unchanged base classes (UObject, UUserWidget, etc.)
- No changes needed

### Files Included
- nbamsavdat_stub.cpp — Stub DLL source to prevent arcade save data crash

## Build Notes
- Target: x64 DLL
- Test with JJL10JPN-33 AJB-Win64-Shipping.exe
- Place compiled nbamsavdat.dll stub alongside the exe
