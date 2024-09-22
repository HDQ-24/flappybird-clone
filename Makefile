# Basic target-depency command 
flappybird.exe: main.cpp
	g++ main.cpp resources.o -IC:\SDL2-2.28.1\x86_64-w64-mingw32\include\SDL2 -LC:\SDL2-2.28.1\x86_64-w64-mingw32\lib -lmingw32 -lSDL2main -lSDL2 -lSDL2_image -lSDL2_mixer -mwindows -o flappybird.exe

   