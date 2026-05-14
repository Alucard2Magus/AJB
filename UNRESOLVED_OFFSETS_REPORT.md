# UNRESOLVED HARDCODED OFFSETS — AJB Migration to JJL10JPN-33

The `Offsets.cpp` and `Offsets.h` byte patches were updated correctly. However, there are **29 hardcoded offsets in active code** that were NOT covered by the Offsets.cpp replacement because they're declared inline in `AJB.cpp` and `UFunctions.cpp`.

These all still contain **old v0.5.5 values** and need to be updated to JJL10JPN-33 values via IDA.

---

## AJB.cpp — OFFSET Declarations (15 functions, ACTIVE CODE)

| Line | Symbol | Old Offset | Function Name |
|------|--------|------------|---------------|
| 177 | NetID | 0x4ECC80 | UAJBNetworkObserver::GetNetID |
| 178 | OpenCommand | 0x506DD0 | AAJBOutGameProxy::GetOpenCommand |
| 179 | PostEventByName | 0x291650 | UAkComponent::PostAkEventByName |
| 180 | PostEvent | 0x291390 | UAkComponent::PostAkEvent |
| 181 | LoadBankByName | 0x286850 | UAkGameplayStatics::LoadBankByName |
| 284 | ExecCharacterNo | 0x549AA0 | UAJBGameInstance::execSetSelectedCharacterNo |
| 293 | CharacterNo | 0x485F70 | UAJBGameInstance::SetSelectedCharacterNo |
| 312 | ObjBlueprint | 0x49F080 | UAJBUtilityFunctionLibrary::NewObjectFromBlueprint |
| 319 | GetBaseMaterial | 0x109B860 | UMaterialInterface::GetBaseMaterial |
| 327 | GetMaterialInterface | 0x14D2C50 | FMaterialResource::GetMaterialInterface |
| 343 | oGetDefaultMaterial | 0x14ABA70 | UMaterial::GetDefaultMaterial |
| 364 | FTextConstructor | 0x5DCD20 | FText::FText (constructor) |
| 373 | oAddActionMapping | 0x1793C60 | UPlayerInput::AddActionMapping |
| 384 | oWinGetUsername | 0x699AA0 | FWindowsPlatformProcess::UserName |
| 400 | GSetString | 0x626330 | FConfigCacheIni::SetString |

## AJB.cpp — Inline PB() Calls (12 addresses, ACTIVE CODE)

| Line | Old Offset | Context / Function Name |
|------|------------|------------------------|
| 379 | 0x17A80B0 | UPlayerInput key press dispatch |
| 425 | 0x1908090 | Gets UClass* (config interface related) |
| 428 | 0x8066C0 | Calls something on UObject with UClass (config interface) |
| 433 | 0x620550 | FConfigCacheIni::Flush |
| 485 | 0x2233A0 | NBAM Save Data retrieval (byte-patched to disable) |
| 487 | 0x20E680 | AMActivator_Destroy |
| 496 | 0x20E8D0 | AMActivator_RequestOneTimeKey |
| 515 | 0x20EA10 | GetSignatureGeneration |
| 534 | 0x223610 | FDrive folder creation (byte-patched to disable) |
| 535 | 0x20FB00 | AMActivator_Update (byte-patched to disable) |
| 651 | 0x6331F0 | FConfigCacheIni::SetString (may be different overload from line 400) |
| 651 | 0x3051380 | GEngineIni data pointer (FString*) |

## UFunctions.cpp — Inline PB() Calls (2 addresses, ACTIVE CODE)

| Line | Old Offset | Function Name |
|------|------------|---------------|
| 265 | 0x10C1D10 | UImage::SetBrushFromMaterial |
| 275 | 0x10C1C10 | UBorder::SetBrushFromMaterial |

## UFunctions.cpp — VTable Index (VERIFY)

| Line | Value | Note |
|------|-------|------|
| 764 | 0x51 | UEngine::GetMaxFPS vtable index — likely unchanged but should verify |

## Commented-Out Code (Low Priority)

| File | Line | Old Offset | Note |
|------|------|------------|------|
| AJB.cpp | 615 | 0x47C510 | ClearMatchingID (commented out) |
| AJB.cpp | 638 | 0x6246F0, 0x3051380 | GetSectionPrivate (commented out) |
| AJB.cpp | 643 | 0x6213C0 | GenerateExportedPropertyLine (commented out) |
| AJB.cpp | 646 | 0x3051380 | GEngineIni (commented out) |
| UFunctions.cpp | 285 | 0x18D70A0 | execPrimitiveSetMaterial (commented out) |

## CustomSDK — SAFE (No Changes Needed)

The CustomSDK files contain struct member offsets (not function addresses). These are for custom port-specific widgets inheriting from `UUserWidget` (size 0x208, UNCHANGED). The static_asserts will catch any issues at compile time.

---

## How to Resolve

All 29 active-code offsets need their JJL10JPN-33 equivalents found via IDA Pro on the new binary. Methods:
1. **String search** — Many functions reference unique strings; search for those strings in IDA
2. **Signature matching** — Use byte signatures from the old binary to find relocated functions
3. **Symbol matching** — For Wwise functions (PostEvent, LoadBank), check the Wwise integration vtables
4. **Decompile diff** — Compare decompiled pseudocode structure between old and new

The Sega arcade system functions (AMActivator_*, NBAM, FDrive) at lines 485-535 are particularly critical since those are byte-patched at startup to prevent arcade hardware calls — wrong addresses = crash.
