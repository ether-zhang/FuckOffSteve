# Building from source

Requires Windows x64, Visual Studio 2022 C++ Build Tools with the Windows SDK,
Python 3, and a local Mewgenics installation matching Steam build 25143593.
The pinned MewUI SDK is included in `vendor/mew-ui-api`.

Place the checkout at `<Mewgenics>/mods/FuckOffSteven`, then open PowerShell in
that directory. The build reads the game's `Mewgenics.exe` and `resources.gpak`
two directories above the checkout. Those game files are not included here.

```powershell
# Optional: select a specific Python executable.
$env:FOS_PYTHON = 'C:\Path\To\Python\python.exe'

.\build.cmd
.\test.cmd
```

Omit the optional assignment when Python is already available on PATH.
The build verifies native signatures, generates an uncompressed UI resource,
runs the resource checks, and compiles `build/FuckOffSteven.dll`.
The test runner uses synthetic memory and does not connect to the game.

After closing the game, install the built DLL and UI resource with:

```powershell
& .\tools\apply_update.ps1
```

The installer checks that the game is closed and verifies the copied files.
Build and test commands do not install their output or start the game.

The project's original source code is MIT licensed. Preserve the third-party
notices in `licenses/` when redistributing the bundled SDK. The source license
does not apply to the game's own assets.
