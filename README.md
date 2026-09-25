# Fuck Off Steve

Adds a **FUCK OFF STEVE** button to the quit warning in Mewgenics, matching the game's original UI. Choose it to save and quit without adding a new save-scumming penalty for that exit.

The normal Yes/No buttons keep their original behavior. Existing penalty counts and Deja Vu conditions are unchanged.

**Version:** 0.1.2. **Requirements:** Windows x64, Mewgenics Steam build 25143593, and [Mewjector 3](https://www.nexusmods.com/mewgenics/mods/218).

## Installation

1. Close the game and install Mewjector if needed.
2. Extract `FuckOffSteve-0.1.2.zip` into the game folder, next to `Mewgenics.exe`. The DLL should be at `mods/FuckOffSteve/FuckOffSteve.dll`.
3. In the game's `chainloader.ini`, keep `ScanPath=mods` and add the DLL under `[LoadOrder]`. If this is your only DLL mod:

   ```ini
   [LoadOrder]
   Mod1=FuckOffSteve\FuckOffSteve.dll
   ```

   If other entries exist, keep them and use the next consecutive number (`Mod2`, `Mod3`, etc.), without gaps. Add the entry only once; keep your existing loader files and settings.

4. Launch the game. After taking an action in combat, open the quit warning and select **FUCK OFF STEVE**.

Credits: MewUI API and Mewjector. Their license notices are included in `licenses/`.
