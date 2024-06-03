# Mini40
Interfacing with ATI Mini40 NETBA

## Requirements
1. Install (if needed) https://sourceforge.net/projects/mingw/
2. Ensure msys2 is installed - https://www.msys2.org/
3. In msys2-UCRT64 terminal, run ``pacman -S mingw-w64-ucrt-x86_64-gcc`` and ``pacman -S mingw-w64-x86_64-winpthreads-git`` for git version of winpthreads
4. Add C:\msys64\ucrt64\bin to path by: Start --> Edit the system enviornment variables --> Environment Variables --> Select Path Variable --> Edit... --> New --> C:\msys64\ucrt64\bin -- OK.
5. Clone this repository
6. Compile using ``gcc netft_WIN_v9.c -o netft_WIN_v9 -lws2_32`` in cmd

Reference - https://mingw-w64-public.narkive.com/5N7Clzzj/mingw-w64-clock-gettime  
