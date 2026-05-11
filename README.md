# CustomBootScreen

Nintendo Switch homebrew app for switching Atmosphere and Hekate boot logos.

Install the built `CustomBootScreen.nro` to:

```text
/switch/CustomBootScreen.nro
```

The app creates this directory and its two preset subdirectories automatically on first start if they do not exist:

```text
/switch/CustomBootScreen/
  switch-bootlogo/
  hekate-bootlogo/
```

## Switch CFW Bootlogo

Each Atmosphere/Switch CFW boot screen preset is one subdirectory under `switch-bootlogo`:

```text
/switch/CustomBootScreen/switch-bootlogo/
  DarkLogo/
    logo.png
    logo/
      *.ips
      *.isp
  logo-red/
    preview.png
    logo/
      *.ips
      *.isp
```

The preset folder name is shown in the app. For example, `/switch/CustomBootScreen/switch-bootlogo/logo-red/` appears as `logo-red`.

The preview image is optional. The app accepts `preview.png`, `preview.jpg`, `preview.jpeg`, `preview.bmp`, `logo.png`, `logo.jpg`, `logo.jpeg`, or `logo.bmp` in the preset folder.

When a Switch CFW preset is applied, the app copies the selected preset's `logo/` directory contents into:

```text
/atmosphere/exefs_patches/logo/
```

The previous logo directory is backed up to:

```text
/switch/CustomBootScreen/_switch_current_backup/
```

## Hekate Bootlogo

Each Hekate boot screen preset is one subdirectory under `hekate-bootlogo`:

```text
/switch/CustomBootScreen/hekate-bootlogo/
  RedScreen/
    bootlogo.bmp
  BlueScreen/
    bootlogo.bmp
```

The `bootlogo.bmp` file is used as the preview. The app rotates it 90 degrees in the preview to match how it appears during boot.

When a Hekate preset is applied, the app copies the selected preset's `bootlogo.bmp` into:

```text
/bootloader/bootlogo.bmp
```

The previous Hekate bootlogo is backed up to:

```text
/switch/CustomBootScreen/_hekate_bootlogo_backup.bmp
```
