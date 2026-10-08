---
name: QNX Docs
description: Look up the official QNX documentation on qnx.com. Use for any question about QNX OS behavior, C or POSIX APIs (threads, signals, message passing, memory), resource managers, io_msg and ioctl, CAN and other drivers, BSP/IFS/QNX7/QSTI/CTI target images, or qcc and SDP build flags.
---

# QNX Documentation

Root index: <https://www.qnx.com/developers/docs/qnxeverywhere/index.html>

Target for this repo: QNX SDP 8.0, `qcc` 12.2.0, `aarch64le`, QNX OS 8.0.0,
Raspberry Pi 4.

## 1. Find the page offline, then fetch only that page

`qnx.com` is a heavy Oxygen WebHelp app and is slow and intermittently
unreachable. Page *lookup* does not need the web UI: the publication ships a
static index of every page, so you can find the URL offline and then fetch only
that one page.

```bash
python .opencode/skills/qnx-docs/scripts/qnx_doc.py find CANDEV
python .opencode/skills/qnx-docs/scripts/qnx_doc.py find "target image"
python .opencode/skills/qnx-docs/scripts/qnx_doc.py list --guide com.qnx.doc.ddk
```

The script downloads `htmlFileInfoList.js` once, caches it in the system temp
directory, and greps the cache locally. Repeat lookups cost no network at all.

To search page *contents* rather than titles, the same directory holds a
full-text inverted index: `index-1.js`, `index-2.js`, `index-3.js`
(word to page id), plus `keywords.js` (keyword to page id) and
`htmlFileInfoList.js` (page id to url and title). Fetch those only when a title
search misses:

```bash
python .opencode/skills/qnx-docs/scripts/qnx_doc.py refresh
```

## 2. Guide map

All paths are relative to
`https://www.qnx.com/developers/docs/qnxeverywhere/`.

| Guide | Relative path |
| --- | --- |
| Introduction | `introduction.html` |
| Self-Hosted Developer Desktop | `com.qnx.doc.qdd/topic/about.html` |
| Quick Start Target Image, Raspberry Pi 4/5 | `com.qnx.doc.target_images/topic/qsti/intro.html` |
| Quick Start Target Image, QEMU | `com.qnx.doc.target_images/topic/qsti_qemu/about.html` |
| Custom Target Image, Pi 4/5 and QEMU x86_64 | `com.qnx.doc.target_images/topic/cti/about.html` |
| Hardware Interfacing | `com.qnx.doc.interfacing/topic/interfacing-hardware.html` |
| QNX Porting Guide, Linux to QNX | `com.qnx.doc.qpg/topic/about.html` |
| Driver Development Kit | `com.qnx.doc.ddk/topic/about.html` |

Indexed page counts: DDK 152, Porting 87, Target Images 26, QDD 19,
Interfacing 7, root 3.

**CAN work in this repo:** the DDK has a dedicated CAN section at
`com.qnx.doc.ddk/topic/can/`. It includes a sample MCP2515 SPI CAN driver, which
is the usual controller on a Raspberry Pi:

| Topic | Relative path |
| --- | --- |
| CAN DDK overview | `com.qnx.doc.ddk/topic/can/about.html` |
| Quick start | `com.qnx.doc.ddk/topic/can/sample_quickstart.html` |
| Sample MCP2515 driver | `com.qnx.doc.ddk/topic/can/sample.html` |
| libcan integration | `com.qnx.doc.ddk/topic/can/sample_source.html` |
| CAN library reference | `com.qnx.doc.ddk/topic/can/lib_ref.html` |
| `canmsg_t`, `canmsg_list_t` | `com.qnx.doc.ddk/topic/can/canmsg_t.html`, `.../canmsg_list_t.html` |
| Resource manager calls | `com.qnx.doc.ddk/topic/can/can_resmgr_init.html`, `..._start.html`, `..._fini.html`, `..._create_device.html`, `..._attach_intr.html`, `..._detach_intr.html` |
| Client calls | `com.qnx.doc.ddk/topic/can/transmit.html`, `.../event_handler.html`, `.../devctl.html`, `.../canmsg_queue_element.html`, `.../canmsg_dequeue_element.html` |
| Device ioctl codes | `com.qnx.doc.ddk/topic/can/CANDEV.html`, `CANDEV_INIT`, `CANDEV_MODE`, `CANDEV_TYPE`, `CLIENTWAIT`, `CLIENTWAITQ` |

**Raspberry Pi hardware:** the Hardware Interfacing guide covers the Pi at
`com.qnx.doc.interfacing/topic/rpi/`, including `rpi_GPIO-apis.html`,
`rpi_SMBus-apis.html`, `rpi_GPIO_UART.html`, and `rpi_Hardware.html`.

## 3. Coverage gap: the C library is not in this doc set

The QNX Everywhere publication is 294 pages of tutorials and guides. It does
**not** contain the Neutrino Programmer's Guide or the C System Library
reference. Searching its index for `pthread_attr_setstacksize`, `sigevent`, or
`IonWeb` returns nothing, and the legacy `com.qnx.doc.neutrino.tware` paths on
qnx.com now return 404.

So for C and POSIX API questions:

1. Say plainly that the answer is not in this doc set. Do not answer from memory
   and imply it was verified.
2. Try the official product documentation search:
   <https://qnx.software/en/search#t=ProductDocumentation>. It is a JavaScript
   app, so use the `websearch` tool rather than a plain fetch.
3. Otherwise scope a `websearch` query to the official domain.

## 4. Network etiquette

`qnx.com` intermittently refuses connections; one URL can return 200 and then
time out on the next attempt. Retry three to five times before declaring a page
missing. Prefer one specific `topic/*.html` fetch over loading `index.html`.
Expect several seconds per page, and longer on a cold first fetch.