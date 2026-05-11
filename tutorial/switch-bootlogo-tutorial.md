# Switch CFW Bootlogo Tutorial

This creates an Atmosphere logo patch folder for the Switch CFW bootlogo. CustomBootScreen uses that generated `logo/` folder as a preset.

## Requirements

- Latest Python installed on your PC
- `pip` available from the command line
- The dependencies from `requirements.txt`
- A `308x350` PNG
- `96x96` DPI
- `24-bit` color depth
- A patch generator script named `gen_patches.py`

The tutorial folder should contain:

```text
tutorial/
  gen_patches.py
  requirements.txt
```

## Install Python Requirements

1. Install the latest Python from:

```text
https://www.python.org/downloads/
```

2. During installation, enable the option to add Python to `PATH`.
3. Open a command prompt in the `tutorial` folder.
4. Install the required Python packages:

```text
py -m pip install -r requirements.txt
```

## Create The Patch Folder

1. Create your image as:

```text
tutorial/logo.png
```

2. Put `logo.png` in the same folder as `gen_patches.py`.
3. Open a command prompt in that folder.
4. If you have not done it already, install the requirements:

```text
py -m pip install -r requirements.txt
```

5. Run:

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
