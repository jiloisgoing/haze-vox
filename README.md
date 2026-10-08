# Haze Vox: free late-night R&B vocal plugin

A VST3 / AU / Standalone vocal effect for the slowed, dreamy, late-night R&B sound:
pitched-down "slowed record" vocals, warm and dark tone, stacked doubles, and long hazy space
that blooms between your lines.

**Haze Vox is free and open source** (AGPLv3). Download it from the [Releases page](../../releases).

## What's new in v2.1
- **New look**: a calm cream-and-sage design with minimal knobs and a soft "haze field" that drifts behind the main controls. It thickens as you turn up Haze and breathes with your vocal. Switch between **Light** and **Dark** at the top right.
- **Guide bar**: hover any control and the bar at the top explains what it does. Double-click any knob to reset it.
- **Glide**: Pitch and Formant now slide smoothly instead of jumping. Glide sets how slow the slide is (30 ms to 2 s), so you can automate dramatic voice drops.
- **Smooth Tape link**: turning Tape link on or off glides the voice instead of snapping.
- **No zipper noise**: Tone, Warmth, Haze, Drive, Smooth, delay Feedback and reverb Decay all glide when you move or automate them.

## What's new in v2
- **Haze**: one master knob for the whole vibe. 50% = your knobs exactly as set; up = more space, longer and darker; down = drier and brighter.
- **Simple view / Advanced view**: Simple shows Pitch, Haze, Tape link, Mix, Output. Advanced shows everything.
- **New hall reverb** with **Decay** (seconds) and **Pre-delay**, so words stay clear inside the haze.
- **Duck**: reverb and delay dip while you sing and bloom back in the gaps (about 0.2 s).
- **De-ess** and **Smooth** (a gentle leveler) clean the vocal before it hits the reverb.
- **Width** is now real doubles: one copy 7 cents up on the left, one 7 cents down on the right.
- **Delay sync** to your DAW tempo (1/16 to 1/2, including dotted).
- **Mix fixed**: it now blends the effects only. The pitch shift always stays on.
- **Meters** for input, output, and how hard Comp, S's (de-ess) and Duck are working.
- **Save your own presets**, plus **A/B** to compare two settings.
- **Pitch shows speed**: "-3.0 st 84%" means it sounds like the song at 84% speed.

## Signal flow
Pitch/Formant -> Smooth -> Drive -> Low cut -> Warmth -> Tone -> De-ess -> Doubles (Width)
-> sends: Ping-pong delay + Pre-delay -> Hall reverb (both ducked by your vocal) -> Mix -> Output

## Presets
Slowed & Low, 3AM Haze, Late Night Drive, Deep Pitch Clear, Chopped (Extreme), Sped Up, Pitch Only (No FX).
Your own presets are saved as `.hazevox` files; use "Open presets folder" in the preset menu to find them.

## Getting the plugin

### Option A: download it (easiest)
Go to the [Releases page](../../releases) and download **HazeVox-Windows.zip** or **HazeVox-Mac.zip**
(the Mac version runs on both Apple Silicon and Intel). Unzip it, then follow "Installing" below.

### Option B: build it yourself
You need CMake 3.24+, Git, and a C++ compiler.
- **Windows:** Visual Studio 2022 Community with "Desktop development with C++", plus CMake and Git.
- **Mac:** run `xcode-select --install`, then `brew install cmake`.

From inside this folder:
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
The first build takes several minutes (it downloads JUCE automatically).
Finished plugins are in `build/HazeVox_artefacts/Release/`. On Mac they are also copied into your plug-in folders.

### Publishing a new version (for the maintainer)
Every push builds Windows and Mac versions on the repo's Actions tab. To publish a release, push a version tag:
```
git tag v2.1.0
git push origin v2.1.0
```
GitHub builds both versions and attaches the zips to a new release automatically.

## Installing
- **Windows:** copy `Haze Vox.vst3` into `C:\Program Files\Common Files\VST3\`.
- **Mac:** copy `Haze Vox.vst3` into `~/Library/Audio/Plug-Ins/VST3/` and `Haze Vox.component` into `~/Library/Audio/Plug-Ins/Components/`
  (Option B does this for you). If you downloaded it, macOS will block it the first time because it isn't notarized by Apple. Fix that by running this in Terminal:
  ```
  xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Haze Vox.vst3" ~/Library/Audio/Plug-Ins/Components/"Haze Vox.component"
  ```

**Ableton Live:** Settings > Plug-ins > turn on "Use VST3 Plug-in System Folders" (and Audio Units on Mac) > Rescan.
Haze Vox appears under Plug-ins. Drag it onto your vocal track.

Updating from v1: v2 replaces v1 in your plugin folder. Old sessions open with the new controls at their defaults.

## Tips
- Put Haze Vox **after** pitch correction (Auto-Tune, Melodyne).
- It adds about 120 ms of latency for clean pitch shifting. Ableton compensates on playback; for tracking, record dry and add it after.
- For the full slowed-song version (beat too), set the clips' Warp Mode to **Re-Pitch** and lower the project tempo by about 15%; then use the "Pitch Only" preset or no pitch on the vocal so it isn't lowered twice.

## License
Haze Vox is free software: you can use, share, and change it under the terms of the
**GNU Affero General Public License v3** (see `LICENSE`). If you share a changed version,
you must share its source code under the same license.

Built with [JUCE](https://juce.com) (used under AGPLv3) and
[Signalsmith Stretch](https://github.com/Signalsmith-Audio/signalsmith-stretch) (MIT).

Copyright (C) 2026 HazeAudio.
