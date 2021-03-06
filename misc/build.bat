@echo off

set INCLUDES=-I W:\lib\SDL2\include\ -I W:\lib\SDL2_ttf\include\ -I W:\lib\SDL_FontCache\ -I W:\lib\SDL2_mixer\include\ -I W:\lib\stb\ -I W:\lib\SDL2_image\include\ -I W:\lib\half\include\
set LIBRARIES=shell32.lib W:\lib\SDL2\lib\x64\SDL2main.lib W:\lib\SDL2\lib\x64\SDL2.lib W:\lib\SDL2_ttf\lib\x64\SDL2_ttf.lib W:\lib\SDL_FontCache\SDL_FontCache.lib W:\lib\SDL2_mixer\lib\x64\SDL2_mixer.lib W:\lib\SDL2_image\lib\x64\SDL2_image.lib
set WARNINGS=-W4 -wd4100 -wd4201 -wd4127 -wd4505
REM Set wd4505 off when release, checks for unused procedures with internal linkage.

IF NOT EXIST W:\lib\SDL_FontCache\SDL_FontCache.lib (
	pushd W:\lib\SDL_FontCache\
	cl  /c /EHsc -MTd -O2 -I W:\lib\SDL2\include\ -I W:\lib\SDL2_ttf\include\ W:\lib\SDL_FontCache\SDL_FontCache.c
	lib SDL_FontCache.obj
	popd
)

IF NOT EXIST W:\build\ (
	mkdir W:\build\
)

pushd W:\build\
REM COMMENT THIS OUT TO RELEASE! BATCH IS HORRIBLE.
REM set DEBUG=1
if DEFINED DEBUG (
	echo "Debug build"
	del *.pdb > NUL 2> NUL
	echo "LOCK" > LOCK.tmp
	cl /nologo /DDATA_DIR="\"W:/data/\"" /DEXE_DIR="\"W:/build/\"" /DDEBUG=1 /O2 /Z7 /std:c++17 /MTd /GR- /EHsc /EHa- %WARNINGS% %INCLUDES% /LD         W:\src\Room.cpp     /link /DEBUG:FULL /opt:ref /incremental:no /subsystem:windows %LIBRARIES% /PDB:Room_%RANDOM%.pdb /EXPORT:initialize /EXPORT:boot_down /EXPORT:boot_up /EXPORT:update /EXPORT:render
	cl /nologo /DDATA_DIR="\"W:/data/\"" /DEXE_DIR="\"W:/build/\"" /DDEBUG=1 /O2 /Z7 /std:c++17 /MTd /GR- /EHsc /EHa- %WARNINGS% %INCLUDES% /FeRoom.exe W:\src\platform.cpp /link /DEBUG:FULL /opt:ref /incremental:no /subsystem:windows %LIBRARIES%
	sleep 0.1
	del LOCK.tmp
) else (
	echo "Release build"
	cl /nologo /DDATA_DIR="\"./data/\"" /DEXE_DIR="\"./\"" /O2 /std:c++17 /MTd /GR- /EHsc /EHa- /W4 /wd4201 %INCLUDES% /LD         W:\src\Room.cpp     /link /incremental:no /subsystem:windows %LIBRARIES% /EXPORT:initialize /EXPORT:boot_down /EXPORT:boot_up /EXPORT:update /EXPORT:render
	cl /nologo /DDATA_DIR="\"./data/\"" /DEXE_DIR="\"./\"" /O2 /std:c++17 /MTd /GR- /EHsc /EHa- /W4 /wd4201 %INCLUDES% /FeRoom.exe W:\src\platform.cpp /link -incremental:no -subsystem:windows %LIBRARIES%
)
popd
