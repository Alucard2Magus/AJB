#include "Aeyth8/A8CL/NTSurfer/NTSurfer.hpp"
#include "Aeyth8/Proxy8/Entry/ProxyEntry.hpp"
#include "Aeyth8/Logger.hpp"

#undef IMAGE_ORDINAL_FLAG

using namespace NTS;

extern void InjectSteamInput()
{
	static qword Ldr = NTSurfer::FindExportAddress(NTSurfer::FindDLLAddress(Proxy8::PEB, L"ntdll.dll"), "LdrLoadDll");
	NTSurfer::UNICODE_STRING Path{L"GameOverlayRenderer64.dll"};

	void* Handle{0};
	NTSTATUS Result = Call<long(__stdcall*)(PCWSTR, PULONG, NTSurfer::UNICODE_STRING*, PVOID*)>(Ldr)(0, 0, &Path, &Handle);

	Logger::DebugLog("SteamOverlay", "Injected");

	Call<void(__stdcall*)()>(reinterpret_cast<qword>(Handle) + 0xAB6A0);

	Logger::DebugLog("SteamOverlay", "Called some random dxgi hook bull");
}