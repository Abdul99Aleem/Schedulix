# can-mcp2515

This is a driver for the Microchip MCP2515 CAN Controller.

The driver code is split into two parts:
1. lib - The underlying library for communication/control of the MCP2515
2. driver - The actual resmgr that exposes the MCP2515 to clients.

The datasheet for the MCP2515 can be found here:
[MCP2515 Datasheet](https://www.microchip.com/en-us/product/mcp2515)

See below for specifics about the known HW that uses the MCP2515.

**NOTE** At this time, the driver only works on the Raspberry Pi 4 platform.
         It was developed for the Waveshare 2-CH CAN HAT but in theory should
         work for any other HW connected to the RPI4 that uses the MCP2515.

# Building

Building the can-mcp2515 driver uses QNX's standard recursive make process.
You will need SDP8 installed as well as the following additional package:
- com.qnx.qnx800.target.connectivity.can

Because it is broken into two pieces, lib & driver, with a dependency from
the latter to the former, the lib portion needs to be built and installed
first. This will be done automatically by the build process if it is run
in the top-level directory of the project.

To build:
1. Source the SDP development environment into your shell.
   Eg: `source <path_to_sdp>/qnx_env.sh`

2. Go to the top-level directory of the project, where this file is
   located.

3. Export a QCONF_OVERRIDE variable to specify an override file. One is part
   of this project but you may use your own if you want. Be sure the path
   to the file is absolute.
   Eg: "export `pwd`/qconf-override.mk"

4. Execute `make hinstall install`

If you are using the default qconf-override.mk file, this will build the
driver and place it into the `stage/target/aarch64le/bin` directory.

**NOTE**
By default, if not overriden, the installation directory will be within your
SDP folder. You probably don't want this which is why a qcon-override.mk file
that overrides it to be a `stage` directory within the project is provided.
You are, of course, free to use your own qconf-override.mk file if desired.
For details see:
https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.prog/topic/make_convent_qconfig.mk.html


# SPI Configuration

The MCP2515 uses SPI to communicate. The configuration for the QNX SPI driver
will need to be updated to add devices, one for each MCP2515, that is
connected to the SPI bus(es).

The configuration file for the SPI driver is often found at
`/system/etc/config/spi/spi.conf`. However a non-standard location may be
used. Check how the SPI driver was started to be sure.

The SPI configuration for each MCP2515 device should be of the form:
```
[dev]
parent_busno=PARENT_BUS_NUMBER
devno=DEVNO
name=NAME
clock_rate=10000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1
```
Where:
**PARENT_BUS_NUMBER** is the number of the SPI bus the MCP2515 is
connected with as defined by the SPI driver's configuration file.

**DEVNO** The unqiue, on this bus, device number. This usually corresponds to
          how the various CS (ChipSelect) lines should be made active to
          enable the desired device. See the documentation for your platform's
          specific SPI driver for details.

**NAME** A unique name of the device. It will be used to name the node that
is created in the filesystem.

So for example, if `PARENT_BUS_NUMBER` is 0 (spi0) and you want to add
sections for two MCP2515 devices you might use a configuration like:
```
[dev]
parent_busno=0
devno=0
name=dev0
clock_rate=10000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1

[dev]
parent_busno=0
devno=1
name=dev1
clock_rate=10000000
cpha=0
cpol=0
bit_order=msb
word_width=8
idle_insert=1
```

This will cause the QNX SPI driver to create two nodes at:
`/dev/io-spi/spi0/dev0` and `/dev/io-spi/spi0/dev1`.

# SW Interface

This driver exposes the standard QNX CANCTL devctl interface.
For details please see:

[CAN DEVCTLs](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.devctl/topic/can.html)
[canctl](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/c/canctl.html)

## Modes
The MCP2515 has 3 Tx HW buffers and 2 Rx HW buffers. How these buffers are
exposed to and used by clients depends on the mode.

### IO Mode
In IO mode all of the MCP2515's HW buffers buffers are exposed as mailboxes.
Individual MIDs and MFILTERs can be set on these mailboxes as desired.
Filtering is supported by setting an appropriate MFILTER/MID via the CAN DCMD
devctls.

In IO mode all mailboxes have a default MFILTER of 0xFFFFFFFF. This requires
an EXACT match between the MID of the mailbox and the MID in a CAN frame.

### RAW Mode
In RAW mode only a single Tx/Rx mailbox pair is exposed. In this case no
MFILTERs can be set and thus no filtering on reception is possible.
Anything received by the MCP2515 will be placed into the single Rx mailbox.

An MID can still be set on the Tx mailbox, in order to support direct
writes (ie: not devctls) to the mailbox. Trying to set a MID on the Rx
mailbox will generate an error.

In RAW mode all mailboxes have a default MFILTER of 0. This means a frame
with any MID is accepted by the mailbox.

## MIDs
The CAN protocol has two type of data frames:
- Standard Data Frames (SDF) that have an 11bit ID, called a SID.
- Extended Data Frames (EDF) that have a 29bit ID, called an EID.

SIDs/EIDs are collectively known as a MID (Message ID). In a MID:
- An SID occupies bits 18-28
- An EID occupies bits  0-28
However there is no explicit way to look at a MID and know if it is a SID or
an EID. This causes ambiguity when setting MIDs and MFILTERs through the
`CAN_DEVCTL_SET_MID` and `CAN_DEVCTL_SET_MFILTER` devctls. Or when writing a
frame through write() calls.

By default, the driver will assume SIDs for these two devctls. This works
well if your CAN bus only deals with SDF frames. You can also explicitly
set this by specifying '--mid=sid' on the command line.

If your CAN bus only deals with EDF frames, then use the '--mid=eid' command
line switch to have the driver assume EIDs for these two devctls.

If your CAN bus needs to deal with a mix of SDF and EDF frames then you
can use the '--mid=ext' command line option to enable an extension to
the MID format. Bit 30 of the MID indicates whether it is an EID or an SID.
- If the bit is set (high) the MID is an EID.
- If the bit is unset (low) the MID is an SID.
It is the client's responsibility to set the bit appropriately. In addition,
when setting an MFILTER, the currently set MID is checked to see which bits
of the MFILTER are valid. Thus always set the MID first when using this mode.

**NOTE** DEVCTLs that explicitly provide a CAN_MSG struct for transmission
         explicitly indicate whether the MID is extended or not. Thus the
         --mid setting does not impact those devctls.

### Default MIDs

By default a mailbox has a MID equal to its index. The MID will be an EID
when operating in EID mode, otherwise it will be a SID.

So for example:
In SID or EXT mode:
- rx0 will have a MID of 0
- rx1 will have a MID of 0x40000
- tx2 will have a MID of 0x80000
- tx3 will have a MID of 0xC0000
- tx4 will have a MID of 0x100000

In EID mode:
- rx0 will have a MID of 0
- rx1 will have a MID of 1
- tx2 will have a MID of 2
- tx3 will have a MID of 3
- tx4 will have a MID of 4

# Command line

The command line is used to configure the driver. Important options are called
out here but please see the "use" information for full details.

## Required arguments
The following three arguments MUST be provided.

-s/--spi
Path to the SPI device node for the MCP2515 the driver will control.
The path will depend on the SPI configuration. Using the same terms as in the
[SPI Configuration](#SPI-Configuration) section above it would be:
`/dev/io-spi/spi<PARENT_BUS_NUMBER>/<NAME>`

-c/--clock
The clock frequency being used to drive the MCP2515. It must be in the range
of 1000000-40000000 (1MHz-40MHz).

-g/--gpio
The GPIO number used for the MCP2515's INT pin. The driver will automatically
configure the pin.

### Optional but common arguments
The following arguments are all optional but are often used. They all have
default values if not specified.

-B/--bps
Sets the bitrate of the CAN bus. Supported values are defined by the CAN
specification. Typically is one of 250000, 500000, or 1000000 for a bitrate of
250Kbps, 500Kbps, or 1Mbps respectively.

Defaults to 500000.

-m/--mode
Sets the operating mode of the driver. The driver can operate in one of two
modes: io or raw. See the [Modes](#Modes) section above for details.

Defaults to io.

--mid
Sets how to interpret MIDs. The driver supports three interpretations of a MID:
sid, eid, or ext. See the [MIDs](#MIDs) section above for details.

Defaults to sid.

-u/--unit
The CAN unit this driver exposes. The driver creates its nodes in the fileystem
at `/dev/can<unit>'. Each driver MUST manage its own unique unit.

Defaults to 0.

# Hardware

## Waveshare 2-CH CAN HAT

The [Waveshare 2-CH CAN HAT](https://www.waveshare.com/2-ch-can-hat.htm) is
a board designed for the RPI series of devices. It makes use of the RPI's 40
pin GPIO header to connect to the RPI's CPU.

**NOTE** At this time, the driver only works on the RaspberryPi-4 platform
         and its variants.

The [Wiki](https://www.waveshare.com/wiki/2-CH_CAN_HAT) is a good resource
for information about the HW. It is Linux specific but information about the
HW applies to QNX as well. The SW information can generally be disregarded.

Two MCP2515 chips are used on the Waveshare 2-CH CAN HAT to manage two
independent CAN buses. Both MCP2515 chips are connected to the same physical
SPI bus on the RPI: SPI0.

Once the SPI bus is configured to add one (or two) devices for the MCP2515
chips on the Waveshare, this can-mcp2515 driver can be started to expose
the CANCTL interface. One or two instances of the can-mcp2515 driver can be
started to expose one or two CAN buses from the Waveshare. Just make sure to
start the instances with the appropriate GPIO and Unit#.

The Waveshare uses a 16MHz clock to drive its MCP2515 chips. So the `--clock`
command line argument should be set to 16000000.

The Waveshare is configured to use a specific, unique, GPIO for each MCP2515.
This configuration can be changed via a HW modification. See the Wiki for
details.

CAN0/Dev0 uses GPIO23 by default or GPIO22 with a HW mod.
CAN1/Dev1 uses GPIO25 by default or GPIO24 with a hW mod.

### Examples
All these examples assume that the two SPI nodes are exposed as dev0 and dev1
for the waveshare's can0 and can1 buses respectively.

Starting a driver for the CAN0 device on the waveshare using defaults for all
optional values:
```
can-mcp2515 -s /dev/io-spi/spi0/dev0 -c 16000000 -g 23
```

Starting a driver for the CAN1 device on the waveshare using defaults for all
optional values:
```
can-mcp2515 -s /dev/io-spi/spi0/dev1 -c 16000000 -g 25
```

Starting drivers for both of CAN0 and CAN1 using defaults where possible
**NOTE** The unit needs to be specified otherwise both drivers will try to
         create the sw interface at /dev/can0 and the second instance will
         fail due to that conflict.
```
can-mcp2515 -s /dev/io-spi/spi0/dev0 -c 16000000 -g 23
can-mcp2515 -s /dev/io-spi/spi0/dev1 -c 16000000 -g 25 -u 1
```

# Example communication

[canctl](https://www.qnx.com/developers/docs/8.0/com.qnx.doc.neutrino.utilities/topic/c/canctl.html)
can be used to access the driver from the command line once it is running.
It can be used to set the MID and MFILTER for an Rx mailbox.  It can also be
used to send/receive frames from an appropriate mailbox.

Please see the documentation linked above, or the output of `canctl -h` for
more details on how it can be used.

For example, say you have two MCP2515 devices, like on the Waveshare 2-CH CAN
HAT that are wired together in a loopback configuration. Two instances of the
can-mcp2515 driver have been started in IO mode to create can0 and can1
mailboxes.

You can then send a message from can0 to can1 with:
```
$ canctl -u 0,tx2 -w 0x1,1,hello
```
which produces the output
```
mid = 0x1
dat = hello
dat len = 6
mid type = extended
WRITE_CANMSG_EXT: OK
```

**NOTE** The length of the frame is 6 instead of 5 because canctl will include
         the NULL terminator of data string in the packet.

The message will be received at can1 with rx1 due to the default MID/MFILTER
setup.  You can retrieve it with:
```
canctl -u 1,rx1 -r
```
which produces the output
```
READ_CANMSG_EXT 1:
   mid = 0x1
   timestamp = 0x4C538C8
   dat len = 6
   dat =  68 65 6c 6c 6f 00
          hello.
```
