# Mini40
Interfacing with ATI Mini40 NETBA

## Requirements
1. Install (if needed) https://sourceforge.net/projects/mingw/
2. Ensure msys2 is installed - https://www.msys2.org/
3. In msys2-UCRT64 terminal, run pacman -S mingw-w64-ucrt-x86_64-gcc
4. Install git version of winpthreads - https://packages.msys2.org/package/mingw-w64-x86_64-winpthreads-git
5. Compile using gcc netft_WIN_v9.c -o netft_WIN_v9 -lws2_32
6. Add C:\msys64\ucrt64\bin to path

Reference - https://mingw-w64-public.narkive.com/5N7Clzzj/mingw-w64-clock-gettime  
