# p3udl

USB downloader for pioneer3 and maybe others.

You will need the following:
  - The usb updater binary IPL: https://github.com/DongshanPI/SigmaStar-USBDownloadTool/blob/master/ReleaseSampleTool/usb_updater.bin
  - A u-boot.img file that is designed to be loaded via usb_updater.bin. Github actions should be generating such binaries here.

## Building

## Usage

- Reset the board with the USB boot strap.
- Remove the strap (otherwise your SPI flash will be inaccessible...)
- Run `p3udl` like this:

```
p3udl --ipl=<path to usb_updater.bin> --uboot=<path to the u-boot.img>
```

- Once u-boot is running you can use u-boot as if booted from local storage
- You probably want to use the 'dfu' support in u-boot along with 'dfu-util' to upload images but ymodem etc works too.
