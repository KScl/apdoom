REM Compiling ap_gen_tool
MSBUILD "build\Crispy Doom.sln" /t:ap_gen_tool /p:Configuration="Release" /p:Platform="Win32"
COPY build\bin\Release\ap_gen_tool.exe Release\ap_gen_tool\ap_gen_tool.exe

REM Archiving ap-gen-tool
winrar a -afzip -ep1 -r Release\ap_gen_tool\ap_gen_tool_.zip Release\ap_gen_tool\ap_gen_tool.exe Release\ap_gen_tool\imgui.ini ap_gen_tool\assets ap_gen_tool\data ap_gen_tool\games Release\zlib1.dll
