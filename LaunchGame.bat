@echo off
cd /d "%~dp0"

set "newdll=..\x64\Proxy\dxgi.dll"
set "olddll=E:\AJB\PC Port Prod v0.5.5\AJB\Binaries\Win64\dxgi.dll"

move "%newdll%" "%olddll%"

cd /d "" "E:\AJB\PC Port Prod v0.5.5\AJB\Binaries\Win64" 

start AJB-Win64-Shipping.exe -log -bDebugInputMode
