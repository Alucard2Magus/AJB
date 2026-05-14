#include "Offsets.h"

class OFFSET;

namespace A8CL
{
namespace OFF
{
	// Basic UE Functions

	OFFSET GEngine("GEngine", 0x325F438);
	OFFSET GWorld("GWorld", 0x3261B70);

	OFFSET FMalloc("FMemory::Malloc", 0x5CD7D0);
	OFFSET FRealloc("FMemory::Realloc", 0x5CF9B0);
	OFFSET FFree("FMemory::Free", 0x5C2800);
	OFFSET FNameW("FName::FName wchar_t", 0x6880F0);
	OFFSET FNameA("FName::FName char", 0x688070);
	//OFFSET Logf("FOutputDevice::Logf", 0x64D570);
	OFFSET OutputText("UConsole::OutputText", 0x17AEE00);

	OFFSET ProcessEvent("UObject::ProcessEvent", 0x829D50);
	OFFSET Invoke("UFunction::Invoke", 0x713E10);
	OFFSET AppPreExit("FEngineLoop::AppPreExit", 0x1E3610);

	OFFSET SetClientTravel("UEngine::SetClientTravel", 0x1787D90);
	OFFSET ClientTravelInternal("APlayerController::ClientTravelInternal", 0x160D130);
	OFFSET StartLoadingDestination("FSeamlessTravelHandler::StartLoadingDestination", 0x17D55D0);
	OFFSET PreLogin("AGameModeBase::PreLogin", 0x13DD920);
	OFFSET AJBPreLogin("AJBPreLogin", 0x4A4CB0);
	OFFSET Login("AGameModeBase::Login", 0x13D8C30);
	OFFSET PostLogin("AGameModeBase::PostLogin", 0x13DCC80);
	OFFSET Logout("AGameModeBase::Logout", 0x13D8F70);
	OFFSET BeginPlay("UWorld::BeginPlay", 0x17C10D0);
	OFFSET HandleStartingNewPlayer("AGameModeBase::HandleStartingNewPlayer", 0x13D6730);

	OFFSET InitListen("UIpNetDriver::InitListen", 0x3FC0D0);
	//OFFSET InitConnection("UNetConnection::InitConnection", 0x14FBAF0);
	OFFSET InitLocalConnection("UIpConnection::InitLocalConnection", 0x3FC240);
	OFFSET NotifyControlMessage("UPendingNetGame::NotifyControlMessage", 0x17CCCC0);

	OFFSET Close("UNetConnection::Close", 0x14F92D0);

	OFFSET UConsole("UConsole::ConsoleCommand", 0x179C440);
	OFFSET ConsoleCommand("APlayerController::ConsoleCommand", 0x160D9E0);
	OFFSET Browse("UEngine::Browse", 0x1762740);
	OFFSET RequestLevel("ULevelStreaming::RequestLevel", 0x1496A30);
	OFFSET PrepareMapChange("UEngine::PrepareMapChange", 0x17CFCE0);
	OFFSET IsTimeLimitedExceeded("IsTimeLimitedExceeded", 0x17CADC0);
	OFFSET AddToWorld("UWorld::AddToWorld", 0x17C0430);
	OFFSET RemoveFromWorld("UWorld::RemoveFromWorld", 0x17D10B0);
	OFFSET SpawnActor("UWorld::SpawnActor", 0x149A650);
	OFFSET DestroyActor("UWorld::DestroyActor", 0x11C7EC0);
	OFFSET ProcessMulticastDelegate("ProcessMulticastDelegate", 0x20C2A0);

	OFFSET ClientTeamMessage("APlayerController::ClientTeamMessage", 0x18CC0E0);
	OFFSET ClientTeamMessageImplementation("APlayerController::ClientTeamMessage_Implementation", 0x160CF30);

	OFFSET ActorDestroy("AActor::Destroy", 0x11B27F0);
	OFFSET CopyString("FString::FString", 0x1E1170);

	OFFSET IsNonPakFileNameAllowed("FPakPlatformFile::IsNonPakFilenameAllowed", 0x1925860);
	OFFSET FindFileInPakFiles("FPakPlatformFile::FindFileInPakFiles", 0x1922750);
	OFFSET StaticLoadClass("StaticLoadClass", 0x8513C0);
	OFFSET StaticFindObject("StaticFindObject", 0x8505F0);
	OFFSET StaticLoadObject("StaticLoadObject", 0x851840);
	OFFSET CreateDefaultObject("UClass::CreateDefaultObject", 0x70DC80);
	OFFSET StaticConstructObject("StaticConstructObject_Internal", 0x84F850);
	OFFSET BroadcastDelegate("TMulticastDelegate<void (void),FDefaultDelegateUserPolicy>::Broadcast", 0x1E38B0);

	OFFSET ALevelScriptActorConstructor("ALevelScriptActor::ALevelScriptActor", 0x1484360);
	OFFSET ToFormattedString("FFormatArgumentValue::ToFormattedString", 0x6152A0);

	OFFSET SetInputGameOnly("UWidgetBlueprintLibrary::SetInputMode_GameOnly", 0x10CA3E0);
	OFFSET SetInputMode_GameAndUIEx("UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx", 0x10CA270);

	/*

	UWorld::DestroyActor 0x11C7EC0 (lowest level)
	AActor::Destroy 0x11B27F0
	FString::FString 0x1E1170 (FString copy constructor)
	FMemory::Malloc 0x5CD7D0
	FMemory::QuantizeSize 0x5CF3C0
	FMemory::Realloc 0x5CF9B0
	FMemory::Free 0x5C2800

	*/

	// Native Game Functions

	OFFSET PostEventAtLocation("UAkGameplayStatics::PostEventAtLocation", 0x2931C0);
	OFFSET ChangeState("UFlowStateUtil::ChangeState", 0x21D1A0);
	OFFSET TryGetMatchingMyPairInfo("UAJBGameInstance::TryGetMatchingMyPairInfo", 0x487370);
	OFFSET TryGetMatchingPlayerInfo("UAJBGameInstance::TryGetMatchingPlayerInfoByPlayerIDPureFunction", 0x486E20);
	OFFSET GetUsername("GetUsername", 0x69A870);
	OFFSET GetNationalMatchSchedule("UAJBGameInstance::GetNationalMatchSchedule", 0x47E770);
	OFFSET AJBWindowWidget("UAJBWindowWidget::UAJBWindowWidget", 0x539C90);

	OFFSET IsTenpoHost("AAJBOutGameProxy::IsTenpoHost", 0x50C7D0);
	OFFSET IsAJBOfflineMode("UAJBUtilityFunctionLibrary::IsAJBOfflineMode", 0x49EA70);
	OFFSET IsOfflineMode("UAJBNetworkObserver::IsOfflineMode", 0x4F2830);

}
}
