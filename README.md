# CM03/04 — Attribute analysis (native player radar)

An experimental, **in-game**, eight-axis Football Manager-style attribute polygon for Championship Manager 03/04. This is a PE32 native framebuffer modification, **not an overlay**.

The package contains the native renderer source (C + x86 assembly), build script, self-contained version-checked installer, and the required Ter skin player-profile layout.

## Compatibility

- **Executable:** `cm0304.exe` with a compatible 32-bit PE layout and the expected instructions at the hook locations.
- Designed and tested by it's developer under **Linux / Proton Experimental**, with the Ter skin.
- **Not a general patch for every CM03/04 edition, patch level, or resolution.** The radar's framebuffer placement is resolution/layout dependent.
- This repo **does not contain or distribute the copyrighted game executable**.

## Install

**Close CM03/04 first.** Back up your installation, especially if you have customized the Ter player-profile XML.

From the extracted repository folder, run:

```bash
python3 install.py "/full/path/to/Championship Manager 03-04/cm0304.exe"
```

The installer:
1. Checks the **PE layout and specific machine-code instructions** needed for the hooks, then patches compatible executables. No whole-file hash is checked. An already-patched payload is recognized. Incompatible code layouts are refused.
2. Keeps a backup of the original game executable as `cm0304.exe.radar-tailjmp-backup`.
3. Installs `skin/data/skins/ter/panels/player profile.xml` into the game at `data/skins/ter/panels/player profile.xml`.
4. Backs up an existing different profile XML as `player profile.xml.attribute-analysis-backup` (if that backup doesn't already exist).

**Important for existing customized Ter skins:** The included profile XML may replace some of your custom panel adjustments. To preserve those adjustments, merge the following into your own player profile XML instead of replacing it wholesale:

```xml
<widget kind="tbox" title="Attribute analysis">
    <layout kind="stks" algn="all" laye="true" inse="0" />
    <widget kind="gbox" id="radr">
        <layout kind="stks" algn="all" laye="true" inse="0" />
    </widget>
</widget>
```

Keep the panel in the intended left-side profile layout. You can install the executable patch without overwriting skin XML using:

```bash
python3 install.py "/full/path/to/cm0304.exe" --no-skin
```

To install only the XML layout:

```bash
python3 install.py "/full/path/to/cm0304.exe" --skin-only
```

## Restore original game

With the game closed:

```bash
python3 install.py "/full/path/to/cm0304.exe" --restore
```

This restores the **original executable saved by this installer** and, if the XML backup exists, the previous player-profile XML. It intentionally refuses to restore over an unrecognized patched binary.

## Build from source

On Linux with 32-bit GCC/binutils support:

```bash
cd src
./build.sh
```

The compiled `src/radar.bin` must not exceed the allocated 4096-byte `.tjmp` section. The installer uses that binary payload. The bundled `radar.bin` is the known payload.

## What is implemented

- Live native polygon from player data captured inside the game.
- Separate outfield and goalkeeper eight-axis charts.
- Four rings (5/10/15/20) and readable headings.
- Goalkeeper layout shifted right and resized for long labels.
- A conservative check for incomplete attribute records; if insufficient plausible values are present, it suppresses the polygon.

## Known limitations

- **Scouting is not fully supported.** The incomplete-record safeguard is *not* the game's own visibility/knowledge flag. Some unknown attributes can still be available in memory and may leak into the radar.
- The game-position classification currently uses a heuristic, not decoded player-position metadata.
- Radar coordinates are hard-coded around the tested screen/profile layout and may not work at other resolutions or with different skins.
- The eight category averages are approximations based on the attributes accessible in CM03/04; not every later Football Manager attribute exists.
- The profile visibility mechanism is based on a limited presentation-update counter and may have edge cases.
- **No other versions are supported** unless their executable code locations are independently validated.

## Repository contents

- `src/radar.c` — native framebuffer drawing, label positioning, grid, and category calculations.
- `src/hook.s` — x86 hook stubs for rendering, player-profile activity, and attribute capture.
- `src/link.ld`, `src/build.sh` — compile/link configuration.
- `src/radar.bin` — compiled known payload.
- `install.py` — install/reinstall/restore from a verified original or existing exact executable.
- `skin/data/skins/ter/panels/player profile.xml` — required profile panel, titled **Attribute analysis**.

### Credits

Concept, project direction, game testing, screenshots and feedback: Project creator.

Reverse-engineering assistance, code generation, debugging and patch-building: OpenAI GPT models through Codex. These tools contributed the majority of the coding work.

### License

The original source code of this project is released under the MIT License. This license does not apply to Championship Manager 03/04, third-party skins or other assets owned by their respective rights holders.

### Disclaimer

This is an unofficial, non-commercial fan project and is not affiliated with or endorsed by Sports Interactive, Eidos, Sega or OpenAI.

The project does not distribute the original game executable. Users must provide their own compatible copy of Championship Manager 03/04.

All trademarks and third-party assets remain the property of their respective owners.
