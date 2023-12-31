.. SPDX-License-Identifier: GPL-2.0-or-later

Falcon Mode
===========

Introduction
------------

This document provides an overview of how to add support for Falcon Mode
to a board.

Falcon Mode is introduced to speed up the booting process, allowing
to boot a Linux kernel (or whatever image) without a full blown U-Boot.

Falcon Mode relies on the SPL framework. In fact, to make booting faster,
U-Boot is split into two parts: the SPL (Secondary Program Loader) and U-Boot
image. In most implementations, SPL is used to start U-Boot when booting from
a mass storage, such as NAND or SD-Card. SPL has now support for other media,
and can generally be seen as a way to start an image performing the minimum
required initialization. SPL mainly initializes the RAM controller, and then
copies U-Boot image into the memory.

The Falcon Mode extends this way allowing to start the Linux kernel directly
from SPL. A new command is added to U-Boot to prepare the parameters that SPL
must pass to the kernel, using ATAGS or Device Tree.

In normal mode, these parameters are generated each time before
loading the kernel, passing to Linux the address in memory where
the parameters can be read.
With Falcon Mode, this snapshot can be saved into persistent storage and SPL is
informed to load it before running the kernel.

To boot the kernel, these steps under a Falcon-aware U-Boot are required:

1. Boot the board into U-Boot.
    After loading the desired legacy-format kernel image into memory (and DT as
    well, if used), use the "spl export" command to generate the kernel
    parameters area or the DT.  U-Boot runs as when it boots the kernel, but
    stops before passing the control to the kernel.

2. Save the prepared snapshot into persistent media.
    The address where to save it must be configured into board configuration
    file (CONFIG_CMD_SPL_NAND_OFS for NAND).

3. Boot the board into Falcon Mode. SPL will load the kernel and copy
    the parameters which are saved in the persistent area to the required
    address. If a valid uImage is not found at the defined location, U-Boot
    will be booted instead.

It is required to implement a custom mechanism to select if SPL loads U-Boot
or another image.

The value of a GPIO is a simple way to operate the selection, as well as
reading a character from the SPL console if CONFIG_SPL_CONSOLE is set.

Falcon Mode is generally activated by setting CONFIG_SPL_OS_BOOT. This tells
SPL that U-Boot is not the only available image that SPL is able to start.

Configuration
-------------

CONFIG_CMD_SPL
    Enable the "spl export" command.
    The command "spl export" is then available in U-Boot mode.

CONFIG_SPL_PAYLOAD_ARGS_ADDR
    Address in RAM where the parameters must be copied by SPL.
    In most cases, it is <start_of_ram> + 0x100.

CONFIG_SYS_NAND_SPL_KERNEL_OFFS
    Offset in NAND where the kernel is stored

CONFIG_CMD_SPL_NAND_OFS
    Offset in NAND where the parameters area was saved.

CONFIG_CMD_SPL_NOR_OFS
    Offset in NOR where the parameters area was saved.

CONFIG_CMD_SPL_WRITE_SIZE
    Size of the parameters area to be copied

CONFIG_SPL_OS_BOOT
    Activate Falcon Mode.

Function that a board must implement
------------------------------------

void spl_board_prepare_for_linux(void)
    optional, called from SPL before starting the kernel

spl_start_uboot()
    required, returns "0" if SPL should start the kernel, "1" if U-Boot
    must be started.

Environment variables
---------------------

A board may chose to look at the environment for decisions about falcon
mode.  In this case the following variables may be supported:

boot_os
    Set to yes/Yes/true/True/1 to enable booting to OS,
    any other value to fall back to U-Boot (including unset)

falcon_args_file
    Filename to load as the 'args' portion of falcon mode rather than the
    hard-coded value.

falcon_image_file
    Filename to load as the OS image portion of falcon mode rather than the
    hard-coded value.

Using spl command
-----------------

spl - SPL configuration

Usage::

    spl export <img=atags|fdt> [kernel_addr] [initrd_addr] [fdt_addr ]

img
    "atags" or "fdt"

kernel_addr
    kernel is loaded as part of the boot process, but it is not started.
    This is the address where a kernel image is stored.

initrd_addr
    Address of initial ramdisk
    can be set to "-" if fdt_addr without initrd_addr is used

fdt_addr
    in case of fdt, the address of the device tree.

The *spl export* command does not write to a storage media. The user is
responsible to transfer the gathered information (assembled ATAGS list
or prepared FDT) from temporary storage in RAM into persistent storage
after each run of *spl export*. Unfortunately the position of temporary
storage can not be predicted nor provided at command line, it depends
highly on your system setup and your provided data (ATAGS or FDT).
However at the end of an successful *spl export* run it will print the
RAM address of temporary storage. The RAM address of FDT will also be
set in the environment variable *fdtargsaddr*, the new length of the
prepared FDT will be set in the environment variable *fdtargslen*.
These environment variables can be used in scripts for writing updated
FDT to persistent storage.

Now the user have to save the generated BLOB from that printed address
to the pre-defined address in persistent storage
(CONFIG_CMD_SPL_NAND_OFS in case of NAND).
The following example shows how to prepare the data for Falcon Mode on
twister board with ATAGS BLOB.

The *spl export* command is prepared to work with ATAGS and FDT. However,
using FDT is at the moment untested. The ppc port (see a3m071 example
later) prepares the fdt blob with the fdt command instead.


Usage on the twister board
--------------------------

Using mtd names with the following (default) configuration
for mtdparts::

    device nand0 <omap2-nand.0>, # parts = 9
     #: name        size        offset      mask_flags
     0: MLO                 0x00080000      0x00000000      0
     1: u-boot              0x00100000      0x00080000      0
     2: env1                0x00040000      0x00180000      0
     3: env2                0x00040000      0x001c0000      0
     4: kernel              0x00600000      0x00200000      0
     5: bootparms           0x00040000      0x00800000      0
     6: splashimg           0x00200000      0x00840000      0
     7: mini                0x02800000      0x00a40000      0
     8: rootfs              0x1cdc0000      0x03240000      0

::

    twister => nand read 82000000 kernel

    NAND read: device 0 offset 0x200000, size 0x600000
    6291456 bytes read: OK

Now the kernel is in RAM at address 0x82000000::

    twister => spl export atags 0x82000000
    ## Booting kernel from Legacy Image at 82000000 ...
       Image Name:   Linux-3.5.0-rc4-14089-gda0b7f4
       Image Type:   ARM Linux Kernel Image (uncompressed)
       Data Size:    3654808 Bytes = 3.5 MiB
       Load Address: 80008000
       Entry Point:  80008000
       Verifying Checksum ... OK
       Loading Kernel Image ... OK
    OK
    cmdline subcommand not supported
    bdt subcommand not supported
    Argument image is now in RAM at: 0x80000100

The result can be checked at address 0x80000100::

    twister => md 0x80000100
    80000100: 00000005 54410001 00000000 00000000    ......AT........
    80000110: 00000000 00000067 54410009 746f6f72    ....g.....ATroot
    80000120: 65642f3d 666e2f76 77722073 73666e20    =/dev/nfs rw nfs

The parameters generated with this step can be saved into NAND at the offset
0x800000 (value for twister for CONFIG_CMD_SPL_NAND_OFS)::

    nand erase.part bootparms
    nand write 0x80000100 bootparms 0x4000

Now the parameters are stored into the NAND flash at the address
CONFIG_CMD_SPL_NAND_OFS (=0x800000).

Next time, the board can be started into Falcon Mode moving the
setting the GPIO (on twister GPIO 55 is used) to kernel mode.

The kernel is loaded directly by the SPL without passing through U-Boot.

Example with FDT: a3m071 board
------------------------------

To boot the Linux kernel from the SPL, the DT blob (fdt) needs to get
prepared/patched first. U-Boot usually inserts some dynamic values into
the DT binary (blob), e.g. autodetected memory size, MAC addresses,
clocks speeds etc. To generate this patched DT blob, you can use
the following command:

1. Load fdt blob to SDRAM::

        => tftp 1800000 a3m071/a3m071.dtb

2. Set bootargs as desired for Linux booting (e.g. flash_mtd)::

        => run mtdargs addip2 addtty

3. Use "fdt" commands to patch the DT blob::

        => fdt addr 1800000
        => fdt boardsetup
        => fdt chosen

4. Display patched DT blob (optional)::

        => fdt print

5. Save fdt to NOR flash::

        => erase fc060000 fc07ffff
        => cp.b 1800000 fc060000 10000
        ...


Falcon Mode was presented at the RMLL 2012. Slides are available at:

http://schedule2012.rmll.info/IMG/pdf/LSM2012_UbootFalconMode_Babic.pdf
