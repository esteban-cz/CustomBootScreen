# Switch CFW Bootlogo Tutorial

This creates an Atmosphere logo patch folder for the Switch CFW bootlogo. CustomBootScreen uses that generated `logo/` folder as a preset.

## Requirements

- A `308x350` PNG
- `96x96` DPI
- `24-bit` color depth
- A patch generator script named `gen_patches.py`

## Create The Patch Folder

1. Create your image as:

```text
logo.png
```

2. Put `logo.png` in the same folder as `gen_patches.py`.
3. Open a command prompt in that folder.
4. Run:

```text
py gen_patches.py logo logo.png
```

This should generate a new folder named:

```text
logo/
```

The generated `logo/` folder should contain Atmosphere `.ips` patch files.

## Use With CustomBootScreen

Create a Switch CFW preset folder on the SD card:

```text
/switch/CustomBootScreen/switch-bootlogo/MyBootlogo/
  logo.png
  logo/
    *.ips
    *.isp
```

Then open CustomBootScreen, switch to Switch CFW mode, select the preset, and apply it.

CustomBootScreen copies the selected preset's `logo/` folder to:

```text
/atmosphere/exefs_patches/logo/
```

## Manual Install

To install without CustomBootScreen:

1. Delete the old folder:

```text
/atmosphere/exefs_patches/logo/
```

2. Copy the newly generated `logo/` folder to:

```text
/atmosphere/exefs_patches/
```

3. Reboot the Switch.
