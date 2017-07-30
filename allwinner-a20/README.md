# How to compile headunit

These instructions assumes you've already got a build environment installed on your Allwinner A20 board.

Install dependencies:

```
sudo apt-get install libsdl2-2.0-0 libsdl2-ttf-2.0-0 libportaudio2 libpng12-0 gstreamer1.0-plugins-base-apps gstreamer1.0-plugins-bad gstreamer1.0-libav gstreamer1.0-alsa
```

We need development headers for openssl, libusb, gstreamer, sdl and gkt

```
sudo apt-get install libssl-dev libusb-1.0-0-dev libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libsdl1.2-dev libgtk-3-dev
```

Note, you may need to edit "allwinner/outputs.cpp" and change the screen size from 1024x600 to whatever your resolution is.

# How to configure Ubuntu for Allwinner A20 with hardware acceleration

You need to be running an Ubuntu 16.04 or later that's designed for your Allwinner A20 board with a working X desktop

Firstly, you need to make sure you're loading the Sunxi modules. Run "lsmod" and see if you see the following

````
sunxi_cedar_mod
mali
````

Verify that Sunxi fbturbo driver is enabled in X. Check /var/log/Xorg.0.log, look for lines like:

````
[    17.193] (II) LoadModule: "fbturbo"
[    17.193] (II) Loading /usr/lib/xorg/modules/drivers/fbturbo_drv.so
[    17.198] (II) Module fbturbo: vendor="X.Org Foundation"
````

Now we should turn on Sunxi optimizations for X. Edit /usr/share/X11/xorg.conf

````
# This is a minimal sample config file, which can be copied to
# /etc/X11/xorg.conf in order to make the Xorg server pick up
# and load xf86-video-fbturbo driver installed in the system.
#
# When troubleshooting, check /var/log/Xorg.0.log for the debugging
# output and error messages.
#
# Run "man fbturbo" to get additional information about the extra
# configuration options for tuning the driver.

Section "Device"
        Identifier      "Allwinner A10/A13 FBDEV"
        Driver          "fbturbo"
        Option          "fbdev" "/dev/fb0"

        Option          "AccelMethod" "G2D"
        Option          "DRI2HWOverlay" "true"
        Option          "DRI2" "true"
        Option          "DRI2_PAGE_FLIP" "true"
        Option          "SwapbuffersWait" "false"
EndSection
````

Restart your X server (or just reboot the machine) and then check /var/log/Xorg.0.log for the following lines

````
[    18.611] (II) FBTURBO(0): enabled G2D acceleration
[    18.611] (==) FBTURBO(0): Backing store disabled
[    18.613] (II) FBTURBO(0): using sunxi disp layers for X video extension
[    18.613] (II) FBTURBO(0): using hardware cursor
````

Install VDPAU packages

````
apt-get install libvdpau-dev libvdpau1 vdpau-va-driver vdpauinfo
````

Compile libcedrus

````
git clone https://github.com/linux-sunxi/libcedrus
cd libcedrus
make
make install
cd ..
````

Compile VDPAU driver for CedarX VPU

````
git clone https://github.com/linux-sunxi/libvdpau-sunxi.git
cd libvdpau-sunxi/
make 
make install
cd ..
````

Verify that VDPA system is working. You may have to run this using sudo. You may also have to play with symlinks to verify VDPAU driver is installed in correct place for your distro.

````
export VDPAU_DRIVER=sunxi
vdpauinfo
````

Should get output similar to this:

````
[VDPAU SUNXI] VE version 0x1623 opened.
API version: 1
Information string: sunxi VDPAU Driver

Video surface:

name   width height types
-------------------------------------------
420     8192  8192  NV12 YV12

Decoder capabilities:

name               level macbs width height
-------------------------------------------
MPEG1                 0 32400  3840  2160
MPEG2_SIMPLE          3 32400  3840  2160
MPEG2_MAIN            3 32400  3840  2160
H264_BASELINE        51 32400  3840  2160
H264_MAIN            51 32400  3840  2160
H264_HIGH            51 32400  3840  2160
MPEG4_PART2_SP       51 32400  3840  2160
MPEG4_PART2_ASP      51 32400  3840  2160
````

Install VAAPI packages

````
apt-get install libva1 libva-x11-1 vainfo libva-dev libdrm-dev libudev-dev mesa-common-dev
````

Verify that VAAPI system is working. You may have to run this using sudo

````
export LIBVA_DRIVER_NAME=vdpau
vainfo
````

Should get output similar to this:

````
libva: VA-API version 0.32.0
libva: User requested driver 'vdpau'
libva: Trying to open /usr/lib/arm-linux-gnueabihf/dri/vdpau_drv_video.so
libva: va_openDriver() returns 0
[VDPAU SUNXI] VE version 0x1623 opened.
vainfo: VA-API version: 0.32 (libva 1.0.15)
vainfo: Driver version: Splitted-Desktop Systems VDPAU backend for VA-API - 0.7.5.pre1
vainfo: Supported profile and entrypoints
      VAProfileMPEG2Simple            : VAEntrypointVLD
      VAProfileMPEG2Main              : VAEntrypointVLD
      VAProfileMPEG4Simple            : VAEntrypointVLD
      VAProfileMPEG4AdvancedSimple    : VAEntrypointVLD
      VAProfileH264Baseline           : VAEntrypointVLD
      VAProfileH264Main               : VAEntrypointVLD
      VAProfileH264High               : VAEntrypointVLD
````

Install gstreamer packages

````
apt-get install gstreamer1.0-x gstreamer1.0-tools gstreamer1.0-plugins-bad gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly
````

Install gstreamer VAAPI

````
apt-get install gstreamer1.0-vaapi
````

Make sure gstreamer VAAPI plugins are correctly installed

````
gst-inspect-1.0 | grep -i vaapi
````

Should get output similar to this:

````
vaapiparse:  vaapiparse_h264: H.264 parser
vaapi:  vaapidecode: VA-API decoder
vaapi:  vaapipostproc: VA-API video postprocessing
vaapi:  vaapisink: VA-API sink
````

To watch a video you should be either root or have sufficient permissions to read/write /dev/cedar_dev. You may find it convenient to change default permissions and make sure you are in the video group

````
sudo chgrp video /dev/cedar_dev
sudo 0660 /dev/cedar_dev
````

Get a H264 video and play it with gstreamer. This pipeline is for video-only. Note, you may have to run this with sudo.

````
gst-launch-1.0 filesrc location=my-video.mp4 ! qtdemux ! vaapidecode ! vaapisink
````

