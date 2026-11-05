# Frame Finger Correction

Steam Frame controller finger deadzone correction. See [Releases](https://github.com/wassan0330/frame-finger-correction/releases) for the preview and installation instructions.

## Build the v0.1.0 preview on Steam Frame

This source builds the ARM64 UI, correction driver and dashboard registration helper. On the Frame, use its existing C++17 compiler, Cairo development files and Valve OpenVR headers/runtime:

```sh
FRAME_FINGER_RELEASE=1 FRAME_FINGER_RUNTIME=november-beta sh scripts/build-frame-finger.sh
```

The driver targets the exact verified November SteamVR beta binary pair. See Releases for installation and removal. The app is unofficial. The bundled Valve header has its own license in third_party/openvr-driver.
