@echo off
REM Launches CLIP STUDIO PAINT, waits a few seconds for it to start, then
REM launches ArtCompanion. ArtCompanion will auto-close itself once it
REM detects CSP has closed (see the watchdog timer in main.cpp), so you
REM don't need a matching "close both" script.
REM
REM IMPORTANT: edit the path below to match your actual CSP install location.
REM To find it: right-click your normal CLIP STUDIO PAINT shortcut ->
REM Properties -> the "Target" field is the exact path to use here.

start "" "C:\Program Files\CELSYS\CLIP STUDIO 1.5\CLIP STUDIO PAINT\CLIPStudioPaint.exe"

timeout /t 3 /nobreak >nul

REM %~dp0 = the folder this .bat file is sitting in, so keep this .bat
REM next to ArtCompanion.exe (don't move just this file elsewhere).
start "" "%~dp0ArtCompanion.exe"
