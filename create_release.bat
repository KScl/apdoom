REM Compiling crispy-doom
MSBUILD "build\Crispy Doom.sln" /t:crispy-doom /p:Configuration="Release" /p:Platform="Win32"
COPY build\bin\Release\crispy-doom.exe Release\crispy-apdoom.exe

REM Compiling crispy-heretic
MSBUILD "build\Crispy Doom.sln" /t:crispy-heretic /p:Configuration="Release" /p:Platform="Win32"
COPY build\bin\Release\crispy-heretic.exe Release\crispy-apheretic.exe

REM Compiling crispy-setup
MSBUILD "build\Crispy Doom.sln" /t:crispy-setup /p:Configuration="Release" /p:Platform="Win32"
COPY build\bin\Release\crispy-setup.exe Release\crispy-setup.exe

REM Copy over APcpp DLL
COPY build\bin\Release\APCpp.dll Release\APCpp.dll

REM REM Compiling launcher
MSBUILD Launcher\APDoomLauncher\APDoomLauncher.sln /t:APDoomLauncher /p:Configuration="Release"
COPY Launcher\APDoomLauncher\bin\Release\APDoomLauncher.exe Release\apdoom-launcher.exe

REM REM Archiving apworlds
DEL /F /Q ..\Archipelago\worlds\doom_1993\__pycache__
winrar a -afzip -ep1 -r Release\doom_1993.apworld ..\Archipelago\worlds\doom_1993

DEL /F /Q ..\Archipelago\worlds\doom_ii\__pycache__
winrar a -afzip -ep1 -r Release\doom_ii.apworld ..\Archipelago\worlds\doom_ii

DEL /F /Q ..\Archipelago\worlds\heretic\__pycache__
winrar a -afzip -ep1 -r Release\heretic.apworld ..\Archipelago\worlds\heretic

REM Generating default yamls
python3 ..\Archipelago\Launcher.py "Generate Template Settings"
COPY "..\Archipelago\Players\Templates\DOOM 1993.yaml" "Release\DOOM 1993.yaml"
COPY "..\Archipelago\Players\Templates\DOOM II.yaml" "Release\DOOM II.yaml"
COPY "..\Archipelago\Players\Templates\Heretic.yaml" "Release\Heretic.yaml"

REM Credits
COPY credits-doom-1993.txt Release\credits-doom-1993.txt
COPY credits-doom-ii.txt Release\credits-doom-ii.txt
COPY credits-heretic.txt Release\credits-heretic.txt
COPY credits-chex.txt Release\credits-chex.txt

REM Copy WADs
COPY APDOOM.WAD Release\APDOOM.WAD
COPY APHERETIC.WAD Release\APHERETIC.WAD

REM Archiving release
winrar a -afzip -ep1 -r Release\APDOOM_x_x_x.zip @release.lst
