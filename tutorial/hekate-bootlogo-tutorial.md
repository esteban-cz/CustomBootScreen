# Hekate Bootlogo Tutorial

This creates a Hekate `bootlogo.bmp` that can be used directly by Hekate or as a CustomBootScreen Hekate preset.

Official Hekate reference: [hekate README_BOOTLOGO.md](https://github.com/CTCaer/hekate/blob/master/README_BOOTLOGO.md)

## Image Requirements

- Maximum size: `720x1280`
- Format: `32-bit BMP`
- Hekate expects `ARGB` BMP. Some converters label this as `32-bit RGBA`; if one converter does not work, try another that exports 32-bit BMP with alpha.
- Classic `24-bit RGB` BMP is not supported by Hekate.

Hekate centers smaller images automatically. The background color is taken from the first pixel of the image.

The official Hekate docs describe the bootlogo as a landscape image rotated `90 degrees counterclockwise`. In practice, if you design directly at `720x1280`, test it on console and rotate the source if the boot screen appears sideways.

## Create The Bootlogo

1. Create a `720x1280` PNG in an editor such as Canva.
2. Convert the PNG to a `32-bit BMP`.
   One online option is: <https://online-converting.com/image/convert2bmp/#>
3. Rename the converted file to:

```text
bootlogo.bmp
```

## Use With CustomBootScreen

Create a Hekate preset folder on the SD card:

```text
/switch/CustomBootScreen/hekate-bootlogo/MyBootlogo/
  bootlogo.bmp
```

Then open CustomBootScreen, switch to Hekate mode, select the preset, and apply it.

CustomBootScreen copies the selected file to:

```text
/bootloader/bootlogo.bmp
```

## Manual Install

To install without CustomBootScreen:

1. Copy `bootlogo.bmp` to:

```text
/bootloader/bootlogo.bmp
```

2. Replace the old file if one already exists.
3. Reboot the Switch.

If `/bootloader/bootlogo.bmp` is missing, Hekate uses its default built-in logo unless a boot entry has its own `logopath=` configured.
