# ANGOL C++

Port gry z Pygame na C++ WinAPI/GDI+.

## C++ w CLion albo Visual Studio

1. Otworz ten folder jako projekt CMake.
2. Wybierz target `angol_cpp`.
3. Uruchom projekt.

Gotowy plik po lokalnym buildzie jest tutaj:

```text
build-cpp/Release/angol_cpp.exe
```

Reczny build z terminala, jezeli CMake jest w PATH:

```powershell
cmake -S . -B build-cpp -G "Visual Studio 17 2022" -A x64
cmake --build build-cpp --config Release
```

Sterowanie:

- Strzalki / WASD - ruch
- Gora / W / Spacja - skok
- R - restart poziomu
- ESC - powrot do menu albo wyjscie
