.. SPDX-License-Identifier: GPL-2.0-or-later

Poplar board
############

Board Information
=================

Developed by HiSilicon, the board features the Hi3798C V200 with an
integrated quad-core 64-bit ARM Cortex A53 processor and high
performance Mali T720 GPU, making it capable of running any commercial
set-top solution based on Linux or Android. Its high performance
specification also supports a premium user experience with up to H.265
HEVC decoding of 4K video at 60 frames per second.

* SOC  Hisilicon Hi3798CV200
* CPU  Quad-core ARM Cortex-A53 64 bit
* DRAM DDR3/3L/4 SDRAM interface, maximum 32-bit data width 2 GB
* USB  Two USB 2.0 ports One USB 3.0 ports
* CONSOLE  USB-micro port for console support
* ETHERNET  1 GBe Ethernet
* PCIE  One PCIe 2.0 interfaces
* JTAG  8-Pin JTAG
* EXPANSION INTERFACE  Linaro 96Boards Low Speed Expansion slot
* DIMENSION Standard 160×120 mm 96Boards Enterprice Edition form factor
* WIFI  802.11AC 2*2 with Bluetooth
* CONNECTORS  One connector for Smart Card One connector for TSI

Build instructions
==================

.. note::

  U-Boot has a **strong** dependency with the l-loader and the ARM trusted
  firmware repositories.

The boot sequence is::

    l-loader --> arm_trusted_firmware --> U-Boot

U-Boot needs to be aware of the BL31 runtime location and size to avoid writing
over it. Currently, BL31 is being placed below the kernel text offset (check
poplar.c) but this could change in the future.

The current version of U-Boot has been tested with

- https://github.com/Linaro/poplar-l-loader.git::

    commit f0988698dcc5c08bd0a8f50aa0457e138a5f438c
    Author: Alex Elder <elder@linaro.org>
    Date:   Fri Jun 16 08:57:59 2017 -0500

    l-loader: use external memory region definitions

    The ARM Trusted Firmware code now has a header file that collects
    all the definitions for the memory regions used for its boot stages.
    Include that file where needed, and use the definitions found therein

    Signed-off-by: Alex Elder <elder@linaro.org>

- https://github.com/Linaro/poplar-arm-trusted-firmware.git::

    commit 6ac42dd3be13c99aa8ce29a15073e2f19d935f68
    Author: Alex Elder <elder@linaro.org>
    Date:   Fri Jun 16 09:24:50 2017 -0500

    poplar: define memory regions in a separate file

    Separate the definitions for memory regions used for the BL stage
    images and FIP into a new file.  The "l-loader" image uses knowledge
    of the sizes and locations of these memory regions, and it can now
    include this (external) header to get these definitions, rather than
    having to make coordinated changes to both code bases.

    The new file has a complete set of definitions (more than may be
    required by one or the other user).  It also includes a summary of
    how the boot process works, and how it uses these regions.

    It should now be relatively easy to adjust the sizes and locations
    of these memory regions, or to add to them (e.g. for TEE).

    Signed-off-by: Alex Elder <elder@linaro.org>


Compile from source
-------------------

Get all the sources

.. code-block:: bash

  mkdir -p ~/poplar/src ~/poplar/bin
  cd ~/poplar/src
  git clone https://github.com/Linaro/poplar-l-loader.git l-loader
  git clone https://github.com/Linaro/poplar-arm-trusted-firmware.git atf
  git clone https://github.com/Linaro/poplar-U-Boot.git U-Boot

Make sure you are using the correct branch on each one of these repositories.
The definition of "correct" might change over time (at this moment in time this
would be the "latest" branch).

Compile U-Boot
~~~~~~~~~~~~~~

Prerequisite:

.. code-block:: bash

  sudo apt-get install device-tree-compiler

.. code-block:: bash

  cd ~/poplar/src/U-Boot
  make CROSS_COMPILE=aarch64-linux-gnu- poplar_defconfig
  make CROSS_COMPILE=aarch64-linux-gnu-
  cp U-Boot.bin ~/poplar/bin

Compile ARM Trusted Firmware (ATF)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: bash

  cd ~/poplar/src/atf
  make CROSS_COMPILE=aarch64-linux-gnu- all fip \
       SPD=none BL33=~/poplar/bin/U-Boot.bin DEBUG=1 PLAT=poplar

Copy resulting binaries

.. code-block:: bash

  cp build/hi3798cv200/debug/bl1.bin ~/poplar/src/l-loader/atf/
  cp build/hi3798cv200/debug/fip.bin ~/poplar/src/l-loader/atf/

Compile l-loader
~~~~~~~~~~~~~~~~

.. code-block:: bash

  cd ~/poplar/src/l-loader
  make clean
  make CROSS_COMPILE=arm-linux-gnueabi-

Due to BootROM requiremets, rename l-loader.bin to fastboot.bin:

.. code-block:: bash

  cp l-loader.bin ~/poplar/bin/fastboot.bin

Flash instructions
==================

Two methods:

Using USB debrick support
    Copy fastboot.bin to a FAT partition on the USB drive and reboot the
    poplar board while pressing S3(usb_boot).

    The system will execute the new U-Boot and boot into a shell which you
    can then use to write to eMMC.

Using U-BOOT from shell
    1) using AXIS usb ethernet dongle and tftp
    2) using FAT formated USB drive

Flash using TFTP (USB ethernet dongle)
--------------------------------------

Plug a USB AXIS ethernet dongle on any of the USB2 ports on the Poplar board.
Copy fastboot.bin to your tftp server.
In U-Boot make sure your network is properly setup.

Then::

  => tftp 0x30000000 fastboot.bin
  starting USB...
  USB0:   USB EHCI 1.00
  scanning bus 0 for devices... 1 USB Device(s) found
  USB1:   USB EHCI 1.00
  scanning bus 1 for devices... 3 USB Device(s) found
         scanning usb for storage devices... 0 Storage Device(s) found
         scanning usb for ethernet devices... 1 Ethernet Device(s) found
  Waiting for Ethernet connection... done.
  Using asx0 device
  TFTP from server 192.168.1.4; our IP address is 192.168.1.10
  Filename 'poplar/fastboot.bin'.
  Load address: 0x30000000
  Loading: #################################################################
       #################################################################
       ###############################################################
       2 MiB/s
  done
  Bytes transferred = 983040 (f0000 hex)

  => mmc write 0x30000000 0 0x780

  MMC write: dev # 0, block # 0, count 1920 ... 1920 blocks written: OK
  => reset

Flash using USB FAT drive
-------------------------

Copy fastboot.bin to any partition on a FAT32 formated usb flash drive.
Enter the uboot prompt::

  => fatls usb 0:2
     983040   fastboot.bin

  1 file(s), 0 dir(s)

  => fatload usb 0:2 0x30000000 fastboot.bin
  reading fastboot.bin
  983040 bytes read in 44 ms (21.3 MiB/s)

  => mmc write 0x30000000 0 0x780

  MMC write: dev # 0, block # 0, count 1920 ... 1920 blocks written: OK

Boot trace
==========

::

  Bootrom start
  Boot Media: eMMC
  Decrypt auxiliary code ...OK

  lsadc voltage min: 000000FE, max: 000000FF, aver: 000000FE, index: 00000000

  Entry boot auxiliary code

  Auxiliary code - v1.00
  DDR code - V1.1.2 20160205
  Build: Mar 24 2016 - 17:09:44
  Reg Version:  v134
  Reg Time:     2016/03/18 09:44:55
  Reg Name:     hi3798cv2dmb_hi3798cv200_ddr3_2gbyte_8bitx4_4layers.reg

  Boot auxiliary code success
  Bootrom success

  LOADER:  Switched to aarch64 mode
  LOADER:  Entering ARM TRUSTED FIRMWARE
  LOADER:  CPU0 executes at 0x000ce000

  INFO:    BL1: 0xe1000 - 0xe7000 [size = 24576]
  NOTICE:  Booting Trusted Firmware
  NOTICE:  BL1: v1.3(debug):v1.3-372-g1ba9c60
  NOTICE:  BL1: Built : 17:51:33, Apr 30 2017
  INFO:    BL1: RAM 0xe1000 - 0xe7000
  INFO:    BL1: Loading BL2
  INFO:    Loading image id=1 at address 0xe9000
  INFO:    Image id=1 loaded at address 0xe9000, size = 0x5008
  NOTICE:  BL1: Booting BL2
  INFO:    Entry point address = 0xe9000
  INFO:    SPSR = 0x3c5
  NOTICE:  BL2: v1.3(debug):v1.3-372-g1ba9c60
  NOTICE:  BL2: Built : 17:51:33, Apr 30 2017
  INFO:    BL2: Loading BL31
  INFO:    Loading image id=3 at address 0x129000
  INFO:    Image id=3 loaded at address 0x129000, size = 0x8038
  INFO:    BL2: Loading BL33
  INFO:    Loading image id=5 at address 0x37000000
  INFO:    Image id=5 loaded at address 0x37000000, size = 0x58f17
  NOTICE:  BL1: Booting BL31
  INFO:    Entry point address = 0x129000
  INFO:    SPSR = 0x3cd
  INFO:    Boot bl33 from 0x37000000 for 364311 Bytes
  NOTICE:  BL31: v1.3(debug):v1.3-372-g1ba9c60
  NOTICE:  BL31: Built : 17:51:33, Apr 30 2017
  INFO:    BL31: Initializing runtime services
  INFO:    BL31: Preparing for EL3 exit to normal world
  INFO:    Entry point address = 0x37000000
  INFO:    SPSR = 0x3c9

  U-Boot 2017.05-rc2-00130-gd2255b0 (Apr 30 2017 - 17:51:28 +0200)poplar

  Model: HiSilicon Poplar Development Board
  BOARD: Hisilicon HI3798cv200 Poplar
  DRAM:  1 GiB
  MMC:   Hisilicon DWMMC: 0
  In:    serial@f8b00000
  Out:   serial@f8b00000
  Err:   serial@f8b00000
  Net:   Net Initialization Skipped
  No ethernet found.

  Hit any key to stop autoboot:  0
  starting USB...
  USB0:   USB EHCI 1.00
  scanning bus 0 for devices... 1 USB Device(s) found
  USB1:   USB EHCI 1.00
  scanning bus 1 for devices... 4 USB Device(s) found
         scanning usb for storage devices... 1 Storage Device(s) found
         scanning usb for ethernet devices... 1 Ethernet Device(s) found

  USB device 0:
      Device 0: Vendor: SanDisk Rev: 1.00 Prod: Cruzer Blade
          Type: Removable Hard Disk
          Capacity: 7632.0 MB = 7.4 GB (15630336 x 512)
  ... is now current device
  Scanning usb 0:1...
  =>
