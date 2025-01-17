
# Network features for Sidekick64

<!-- TOC -->

- [Network features for Sidekick64](#network-features-for-sidekick64)
	- [Introduction](#introduction)
	- [Summary: Network features in a nutshell](#summary-network-features-in-a-nutshell)
	- [Quickstart](#quickstart)
	- [Joining a network](#joining-a-network)
		- [Basics](#basics)
		- [Network on demand](#network-on-demand)
		- [Network on boot](#network-on-boot)
	- [Web interface and web server](#web-interface-and-web-server)
	- [Stand-alone Sidekick mode (via web interface)](#stand-alone-sidekick-mode-via-web-interface)
	- [Modem emulation](#modem-emulation)
		- [Minimal Hayes compatible AT-command set](#minimal-hayes-compatible-at-command-set)
		- [Userport modem](#userport-modem)
		- [Swiftlink/Turbo232 modem (highly experimental)](#swiftlinkturbo232-modem-highly-experimental)
	- [SKTP browser](#sktp-browser)
		- [CSDb Launcher](#csdb-launcher)
		- [HVSC Browser](#hvsc-browser)
		- [Forum64 RSS Viewer](#forum64-rss-viewer)
		- [Simple text chat](#simple-text-chat)
	- [Network configuration via SD card](#network-configuration-via-sd-card)
		- [Booting the right Sidekick64 kernel image](#booting-the-right-sidekick64-kernel-image)
		- [WLAN, SSID and passphrase](#wlan-ssid-and-passphrase)
		- [Configuration parameters](#configuration-parameters)
	- [Differences to vanilla Sidekick64](#differences-to-vanilla-sidekick64)
		- [USB stack, power consumption and CPU temperature](#usb-stack-power-consumption-and-cpu-temperature)
		- [Changes to the Sidekick64 menu](#changes-to-the-sidekick64-menu)
			- [New submenu "Network"](#new-submenu-network)
			- [Enforced screen refreshes](#enforced-screen-refreshes)
		- [Framework related differences](#framework-related-differences)
			- [Bigger kernel image size](#bigger-kernel-image-size)
			- [Bare metal framework Circle and circle-stdlib](#bare-metal-framework-circle-and-circle-stdlib)
			- [Network kernel variants](#network-kernel-variants)
		- [SD card: Mount state, potentially more write operations](#sd-card-mount-state-potentially-more-write-operations)
	- [Known problems](#known-problems)
		- [Raspberry Pi 3B+ needs a cooling airflow](#raspberry-pi-3b-needs-a-cooling-airflow)
		- [Recommended setting for jumper "A13-BTN"](#recommended-setting-for-jumper-a13-btn)
		- [Network reliability](#network-reliability)
		- [CRT files in root folder](#crt-files-in-root-folder)
		- [Git branch will regularly be rebased and history will be rewritten](#git-branch-will-regularly-be-rebased-and-history-will-be-rewritten)
	- [Credits](#credits)

<!-- /TOC -->

## Introduction
The main goal of this fork of Sidekick64 is to find out if it is possible to use the network capabilities of a Raspberry Pi in parallel with the emulation modes of Sidekick64 without meddling too much with any timings needed for reliably emulating C64 carts. Further goals were for me to learn how to use the Circle bare metal framework and to learn how Sidekick64 does its magic by trying to understand its source code.

Once network features were squeezed into the "clockwork" of Sidekick64 without disturbing it too much, I started to add some network demos and tried to implement modem emulation to make use of the newly gained network capabilities. Without the excellent network examples of the Circle bare metal framework this would have been much harder to achieve.
## Summary: Network features in a nutshell
Currently the following network related features are offered by the experimental Sidekick64 network kernel:
* Join your network, obtain IP address (DHCP mandatory)
* Cable based LAN (RPI 3B+ only) and WLAN are both possible. There is one kernel image for both WLAN and cable based ethernet.
* For WLAN, store WLAN SSID and passphrase in configuration file on SD card
* System date and time will be set via NTP (UTC)
* A network connection can be established by the user via keypress at the Sidekick menu when needed or can be configured to be automatically done during each Sidekick64 boot.
* Web interface
 	- Becomes available once the built-in web server is activated.
	- Offers a file upload form allowing a Sidekick64 kernel update without removing the SD card.
	- Also allows to upload, save and launch PRGs, SIDs, CRTs, D64, BIN, MOD, YM, WAV files.
 	- Can be reached via the IP address or by hostname (default is "sidekick64" if no custom hostname was configured)
* Modem emulation
	- Only compatible with terminal software in PRG format (CCGMS, etc.)
	- Userport modem emulation is possible via a little hardware extension
	- (highly experimental) Swiftlink emulation
* SKTP browser
	- Load screen content to C64 via HTTP or HTTPS via a simple binary protocol from a web application called "SKTP server".
	- Trigger download and launch of remote binary files like PRG, D64, SID, CRT, BIN, etc.
	- Example apps:
		- Browse and download stuff from [C-64 Scene Database](https://csdb.dk/) (CSDb) via webservice provided by CSDb.
		- Browse and download stuff from [High Voltage SID Collection](https://www.hvsc.c64.org/) (HVSC) (via instance hosted at hvsc.csdb.dk)
		- Access to RSS-Feeds.
		- Join a simple text chat to chat with other SKTP users.
		- PETSCII Experiments (including advent calendar)
* Changes to the Sidekick64 menu
	- Added network menu page entry to main menu (open with key @, also allow access to with subpages "SKTP browser" and System information
	- Added SKTP browser: .... Control screen content and user key presses remotely via "SKTP server" (HTTP server with web application). Ability to download, launch and save files fetched via HTTP(S): PRG, SID, CRT, D64
	- Added system information page: Allows to see the current RPi CPU temperature, basic network connection information and also meta info like date and time and information about the Sidekick64 kernel running
* Outlook: Sidekick264 network support is also implemented in a highly experimental state. Due to the many variants that have to be tested Sidekick264 network has not been tested a lot.
## Quickstart

1. Download the following archives and files to your PC/MAC/Desktop:
    * Basis: [Sidekick64 release v0.51 by Frenetic](https://github.com/frntc/Sidekick64/releases/download/v0.51/Sidekick64-0.51a.zip)
    * Download the network kernel image (supports both WLAN and cable based network): [`kernel_sk64_net.img`](https://github.com/hpingel/Sidekick64/releases/download/sk64-v0.51%2Bnet-alpha5/kernel_sk64_net.img) (link points to release alpha5)
    * For *WLAN* based network with Raspberry Pi Zero 2 W, 3A+ or 3B+: [`wlan.zip`](https://github.com/hpingel/Sidekick64/releases/download/sk64-v0.48%2Bnet-alpha1/wlan.zip)
2. Extract all downloaded zip archives.
3. Copy the files of Sidekick64 release v0.51 (by Frenetic) to an SD card to create a working vanilla Sidekick64 SD card. Make sure your Sidekick64 is already booting and working fine with the SD card in this state.
4. Copy the network kernel file to the root folder of your SD card. So next to the existing kernel file `kernel_sk64.img` you will see `kernel_sk64_net.img`.
5. If you wish to use WLAN you additionally need to copy the folder `wlan` (from archive `wlan.zip`) to the SD card's root folder. The file `wpa_supplicant.conf` needs to be edited to add the correct SSID and passphrase of your personal WLAN. See [WLAN, SSID and passphrase](#wlan-ssid-and-passphrase) for details.
6. Important: You need to edit the file `sidekick64_rpi0_c128.txt` in the root folder (this is by default the active profile included in config.txt). Change the name of the kernel image that should be booted to `kernel=kernel_sk64_net.img`. You can always revert to booting the vanilla kernel by changing this line to `kernel=kernel_sk64.img`.
7. Boot Sidekick64 and you should see a menu item "Network" on the main menu. If it doesn't boot, connect a HDMI screen to the Raspberry and have a look if you can see any error messages during boot.
8. To use the SKTP browser it needs to know the SKTP server's hostname to connect to. Add the following line to the file `C64/sidekick64.cfg` on the SD card: `NET_SKTPHOST_NAME "sktpdemo.cafeobskur.de"`

## Joining a network
### Basics
Sidekick64 can connect to a local area network as long as a DHCP server is present on the network that provides Sidekick64 with an IP address and DNS server. In case of WLAN the SSID and passphrase are stored in an unencrypted text file on the SD card. The default hostname of a Sidekick64 is "sidekick64" - this can be changed - see section [Configuration parameters](#configuration-parameters) for details. On a successful connect, the date and time will be set automatically via NTP (at the moment this information is not readable for C64 applications but it is accessible in the Sidekick64 menu). The time zone will most likely still be incorrect so that the time is displayed a couple of hours off your local time. (A configurable offset needs to be implemented at some point and daylight saving times are to be considered then too.)

### Network on demand
The Sidekick64 kernel with network features doesn't force the user to always activate the network. Establishing a network connection on Sidekick64 means that the user has to wait for a couple of seconds until all the steps necessary - including turning on the USB stack - are finished. Besides the waiting time, an active network will currently lead to a higher CPU temperature and energy consumption of the Raspberry Pi (discussed in detail in the [hardware section](#usb-stack-power-consumption-and-cpu-temperature) below).

Additionally, a user might not need network connectivity with Sidekick64 on a daily basis. Because of this it seemed to make sense to make the network connection optional and offer a way to activate the network connection on demand via the Sidekick64 menu. This means that network only has to be turned on when it is really needed and wanted.

### Network on boot
On the other hand it also seemed to make sense to offer a configuration option to always directly establish a network connection during boot time of Sidekick64 if desired. Therefore, a setting can be added to enforce network on boot - see section [Configuration parameters](#configuration-parameters) for details. Enabling this will mean that the Sidekick64 will need several seconds longer to boot until it shows its normal menu screen on the C64.

## Web interface and web server
One way to transfer files from a PC, tablet or smartphone to Sidekick64 is to use the web interface that is provided by the built-in webserver. The web interface allows to upload Sidekick64 kernel images (overwrites the current Sidekick kernel on SD card and reboots), PRG files, SID files, CRT files, D64 files or BIN files (C128 custom roms) and also MOD, YM and WAV files (Warning: Only try to upload very small WAV files!). The file types targeted at the C64/C128 platform can be launched directly via Sidekick64 after the upload is finished and/or saved to the SD card.

You can upload a PRG or do a kernel update via the command line by calling curl like this:

`curl -s --show-error -F "kernelimg=@/home/ich/Downloads/wolflingteaser.prg" -F "radio_saveorlaunch=l" http://sidekick64/upload.html > /dev/null`

The web server can be launched manually from the Sidekick64 menu's network page by pressing "w" on the keyboard or it may also become active straight after a network connection is established by adding a configuration parameter - see section [Configuration parameters](#configuration-parameters) for details. In combination with network on boot this might be helpful for developers who want to test their own cross-developed C64 software sending it from a PC over to Sidekick64 to be executed on the real machine.

The web interface is currently only available unencrypted via HTTP (on port 80) and doesn't come with authentication or password protection.

The web interface UI is currently based on Bootstrap loaded via CDN.

## Stand-alone Sidekick mode (via web interface)
In theory it is possible to have the Sidekick64 cartridge lying on a table without being plugged-in to the expansion port of a Commodore computer and still being able to use the web interface to upload data to the SD card of Sidekick64. (This feature has not been tested regularly.)

## Modem emulation
Two types of modems may be emulated as long as Sidekick is not busy with emulating something very demanding like an EasyFlash or a Freezer cartridge. As long as Sidekick is in launcher mode or in the BASIC prompt mode, modem emulation is possible with PRGs loaded via Sidekick or from a floppy disk. Terminal software used has to be in PRG format which is not a problem as CCGMS2021 and other tools all are available as PRGs.

### Minimal Hayes compatible AT-command set
Besides implementing two modem types a basic command line interface also had to be implemented to set the baud rate and connect to a BBS via hostname and port. At the moment, the following commands are implemented (while trying to stay compatible with the [Zimodem firmware](https://github.com/bozimmerman/Zimodem) by Bo Zimmerman):

* `ATI` (currently shows welcome message and modem emulation type and baudrate )
* `ATI2` (print current local IP address of Sidekick64 )
* `ATI7` (print current date and time)
* `ATB` (change baud rate, values possible are `ATB300`, `ATB1200`, `ATB2400`, `ATB4800`, `ATB9600` - this doesn't affect the Swiftlink emulation so it is only relevant for Userport modem emulation)
* `ATD"host:port"` or `ATDThost:port` or `ATDT host:port` (connect to a remote server host with port)
* `ATD@RC` (temporary development shortcut to quickly connect to the [Retrocampus BBS](https://retrocampus.com/bbs/) of Francesco Sblendorio)

The default baud rate can also be changed by setting the configuration parameter `NET_MODEM_DEFAULT_BAUDRATE` to a desired value. Check section [Configuration parameters](#configuration-parameters) for further details.


When it comes to the type of modem being emulated, there is a choice:
### Userport modem
A userport based UP9600 modem may be emulated with relatively simple cabling or with a helper PCB that plugs into the userport and is connected to Sidekick 64 via USB via a cheap FTDI-USB-adapter (FT231x). The cabling needed is the same as described in this [blog post](https://1200baud.wordpress.com/2012/10/14/build-your-own-c64-2400-baud-usb-device-for-less-than-15/) by alwyz. He also links to a [helpful Wiki page](http://www.hardwarebook.info/C64_RS232_User_Port) describing the assignment needed for software emulated RS232 at the Userport in a table. This table doesn't include the additional connections for needed for the UP9600 hack which are described in alwyz's blog post. If you already own a PCB for userport modem emulation based on ESP32 or ESP8266 chances are very high that you can reuse this PCB if you can remove the ESP from it (if it has a socket).

We connect RX (crossed), TX (crossed) and GND between the Userport and the FTDI adapter (set to 5V).

The emulation currently works with a baud rate up to 4800 without Flow Control. For a reliable baud rate of 9600 flow control was hacked into Circle by me. For this, we need another two cables for connecting CTS (and RTS, but in reality it is not really needed). Also, the CTS signal from an FT231x needs to be logically inverted to work or the terminal progam needs to be hacked to work with the un-inverted logic!

If you have cabled up your userport to the FTDI-Adapter and plugged the FTDI into a USB port of the Raspberry Pi, enable Userport modem emulation like this:
* In the network menu screen of Sidekick64 hit the key "m" until the modem emulation type is shown to be "Userport".
* In CCGMS, set modem type to `UP9600` or to `Userport modem` depending on your cabling.

As an alternative to FT231x, a USB2RS232 adaper with PL2303 chipset is also supported.

### Swiftlink/Turbo232 modem (highly experimental)
A Swiftlink modem may be emulated (which normally would be connected to the expansion port). This is still highly experimental at the moment and will crash CCGMS after a couple of screens.

If you want to test this, use ethernet based network kernel as Swiftlink emulation works better there than with WLAN.
* In the network menu screen of Sidekick64 hit the key "m" until the modem emulation type is shown to be "Swiftlink".
* In CCGMS, set modem type to `Swift/Turbo DE`. The Baud rate setting can be left unchanged.

## SKTP browser
SKTP jokingly stands for "Sidekick64 Transfer Protocol" and is a very simple and experimental binary protocol on top of HTTP that is so unspectacular that doesn't really deserve a name. Its main purpose is to allow the C64 to send keypresses or other events to a web application and in response get screen updates from a web application. Basically, the C64 works like a terminal and presents screen content that was arranged by a web application.
This enables us to do the following:

* Applications / features available through Sidekick64 don't have to run on Sidekick64 but can be running in a hosted or cloud environment. This means for example that changes and updates required to such an application will not force the Sidekick64 kernel to be updated.
* In theory it allows interactive multi user applications like simple games or chat apps (see Arena).
* It allows interaction with most services on the World Wide Web
* In addition to screen updates SKTP allows to request Sidekick64 to download a payload from a web adress (via HTTP or HTTPS), store it on the SD card and/or execute it on the C64. This means, files like PRG, SID, CRT, etc. may be downloaded from the internet. This is tightly coupled to the Sidekick64 menu code as the possibility to launch a binary is essential here.

Currently four example applications exist that make use of SKTP:
### CSDb Launcher
Allows to browse latest releases and a couple of selected top lists to easily access attractive releases from the world of the C64 demo scene. A search function allows access to all releases. Release screenshots will be displayed on the mini TFT display on the Sidekick64 module.
### HVSC Browser
Allows to browse and play the complete music in the High Voltage SID Collection.
### Forum64 RSS Viewer
Allows to launch a dynamically generated PRG (available in different flavours for C64, C128@80columns, C16/Plus/4) that displays the latest posts from the RSS feed of Forum64.
### Arena with mini game and simple text chat
To test and demonstrate multiuser capabilities a mini game and simple text chat is available.

If you want to try the SKTP browser without Sidekick64 there is a [Javascript based SKTP client](https://sktpdemo.cafeobskur.de/) available running in any web browser.

## Network configuration via SD card
You may change some network default settings by editing configuration files on the SD card via an SD card reader plugged into your Desktop/Notebook/PC/MAC/Pi/etc.

### Booting the right Sidekick64 kernel image

It should in theory be possible to switch between the normal Sidekick64 kernel (vanilla) and the network kernel (WLAN or ethernet cable based) while everything is stored on one SD card.

There might be problems with the Raspberry Pi firmware files which exist in older and newer revisions. The network kernels tend to use the latest Raspberry Pi firmware files while the vanilla kernel uses older ones.

The vanilla kernel is called `kernel_sk64.img` and is always in the root folder of the SD card. The network kernel is called `kernel_sk64_net.img` and should also be stored in the root folder.

Within the file `sidekick64.txt` in the root folder of the SD card you can clarify which kernel image should be booted when Sidekick64 is powered up. The vanilla kernel is booted by the line `kernel=kernel_sk64.img`. You can add lines below this line like `kernel=kernel_sk64_net.img` to boot the cable based network kernel instead. You may also disable lines by prefixing it with `#`. But there should always be one line that is set to active. Otherwise Sidekick64 will fail to boot.

### WLAN, SSID and passphrase

If you want to use WLAN, you have to check if you have a folder `wlan` on your SD card containing the Raspberry Pi firmware files for WLAN networking.

You also have to edit the file `wlan/wpa_supplicant.conf` and add SSID and passphrase in cleartext. Edit the two values for `ssid` and `psk`:

	network={
		ssid="my-ssid"
		psk="my-password"
		proto=WPA2
		key_mgmt=WPA-PSK
	}

For security and privacy reasons, you should not share this file with anybody and it is recommended that you use a "playground WLAN" (a separate WLAN access point to your normal one which may have restricted rights or may have a disposable/easily changeable passphrase).

### Configuration parameters

The following options may be added to the file `C64/sidekick64.cfg` but are not mandatory. A vanilla Sidekick64 kernel (without network features) will ignore these options if present.

* **NET_CONNECT_ON_BOOT**: Add the line `NET_CONNECT_ON_BOOT "1"` to the configuration if you always want to establish a network connection during boot when you power up your Sidekick64. The boot time will be much longer if enabled (at least 10 seconds longer). The C64 may be switched on even before Sidekick64 boot is finished. You can also revert to manual network activation via keypress in the network menu like this: `NET_CONNECT_ON_BOOT "0"`. Default value: "0"

* **NET_ENABLE_WEBSERVER**: If you want to automatically activate the web server/web interface each time a network connection was established, add the line `NET_ENABLE_WEBSERVER "1"` to the configuration. The web server listens on port 80 while the Sidekick64 menu is active or the PRG launcher is active (display says "Launcher"). You can also revert to manual web server start via keypress in the network menu like this: `NET_ENABLE_WEBSERVER "0"`. Default value: "0"

* **NET_SIDEKICK_HOSTNAME**: If you want to customize the hostname of your Sidekick64 you may do so by providing the hostname in a line like this: `NET_SIDEKICK_HOSTNAME "mySidekickNumberOne"`. This hostname may be used to access the web interface without the need to type in the IP address (example: http://mySidekickNumberOne). Default value: "sidekick64"

* **NET_MODEM_DEFAULT_BAUDRATE**: The modem emulation baud rate may be changed in a terminal program like CCGMS by using the command `ATB`. But you can also configure your favourite baud rate by adding a line with this parameter like this: `NET_MODEM_DEFAULT_BAUDRATE "4800"`. Possible values are 300, 1200, 2400, 4800, 9600. Default value: "1200"

* **NET_SKTPHOST_NAME**: To use the SKTP browser it needs to know the SKTP server's hostname to connect to. Provide an SKTP server address, for example the following demo instance: `NET_SKTPHOST_NAME "sktpdemo.cafeobskur.de"`. Default value: none

* **NET_SKTPHOST_PORT**: If you have specified an SKTP host (see above) you may also provide a port number (only needed if this should differ from 80). For example, the line `NET_SKTPHOST_PORT "443"` will enforce HTTPS access. Default: "80"


## Differences to vanilla Sidekick64
I would like to point out some of the differences between the normal Sidekick64 release kernel and the network kernel so that people can inform themselves before testing the network kernel.
### USB stack, power consumption and CPU temperature
To be able to use the network devices of the Raspberry Pi we have to tell Circle to turn on the USB stack of the Raspberry Pi. Handling the USB stack during Sidekick64's runtime will mean that the single CPU core used by Sidekick64 is a little bit more busy than before. Because the CPU core is forced to run at a fixed maximum speed this will result in an increased CPU temperature and power consumption.

The power consumption change depends on the Raspberry Pi model used.  While a model 3A+ or Zero 2 W will probably have a very similar power consumption even with USB stack and WLAN turned on, a model 3B+ will double it's power consumption from around 350mA to around 700mA when ethernet is enabled. This has to be taken into account when considering if it is feasible to power Sidekick64 from the expansion port of the C64 and therefore from the C64 PSU.

Now for the CPU temperature: In an everyday use case the Raspberry Pi would throttle CPU cores once it detects that the CPU  is getting "hot" but in the case of Sidekick64 the reliability of the communication with the expansion port of the C64 is a priority and therefore throttling the Raspberry's CPU is not an option as it would destabilize the emulation which would lead to crashes or flickering menu screens. This may sound dangerous, but in fact there is not much harm done except for an emulation that was stable before suddenly becoming unstable.

While the CPU temperature of a running Sidekick64 would normally be below 55° Celsius, turning on network features and enabling the USB stack with it may eventually make the temperature hit 60° Celsius. This is not a problem as such but has consequences that need to be known and understood to circumvent problems.

With default settings for `temp_soft_limit`, a Raspberry Pi 3A+ or 3B+ will automatically start to throttle its CPU once the 60° C are reached to prevent overheating (although the CPU would be capable of running with more than 80°C without damage). If you observe a CPU temperature of 59°C on the system info screen of the network menu then your Sidekick64 emulation may appear to become unstable very soon. Nothing can break, nothing can be damaged, as the Raspberry is still running fine and just behaving as it should behave. Only the emulation appears unstable. (This doesn't seem to apply for the Zero 2 W where there is no parameter `temp_soft_limit`.)

If you detect that the Raspberry's CPU temperature is likely to reach the 60° C there are two directions in which you can go to solve the problem: Either improve the air circulation around the CPU to stop it from reaching 60°C or increase the default throttling temperature limit to something higher - like for example 62°C - and hope that the Raspberry's CPU will not reach the new limit.

How quickly your Sidekick's Raspberry CPU is heating up may depend on different factors:
*    Inside a case: Does the case allow for some airflow?
*    Raspberry Pi Model: The Raspberry Pi 3B+ is getting hotter than an 3A+. A model 3A+ will probably stay at around 57-58°C and not reach the 60° unless it is inside of a case.
*    PCB form factor: A Sidekick64 PCB v0.3 (outdated) offers more airflow to the Pi so that a 3B+ will stay beneath 60°C.

The most simple solution is to improve the airflow by using a little tiny active fan for 1€ that gets powered by the 3.3V pin on the Sidekick PCB. This is a custom hack that can even be done if the Sidekick is inside a case. Just let the fan blow air into the gap between Sidekick PCB and the Raspberry Pi from the side. (TODO: Add photos to illustrate)

### Changes to the Sidekick64 menu
#### New submenu "Network"
The network menu allows to check if a connection is established, to start a connection and afterwards to launch the web server or select the modem emulation type. It also provides another submenu called "System information" containing details like the IP address, CPU temperature and system time.
#### Enforced screen refreshes
Sidekick64 ships with a C64 assembler program that is responsible for fetching the menu screen content of Sidekick64 and displaying it via VIC2 on the C64 video output. This program called rpimenu.prg had to be modified for the network kernel to perform a C64 screen content redraw not only when a user presses a key on the keyboard but also when something important happens on the Raspberry Pi's side when the Sidekick64 cartridge will trigger a non-maskable interrupt (NMI) to tell the C64 program that new screen content needs to be fetched and displayed. This enforced refresh is necessary for screens that need to be redrawn regularly like the system info screen displaying the current date and time and CPU temperature of the Pi.

To avoid the hassle of constantly replacing the rpimenu.prg on the SD card when switching between vanilla kernel and network kernel, the modified rpimenu.prg is currently baked directly into the network kernel image so that there is no possibility to have the wrong version on the SD card and no need to replace any PRG files.
### Framework related differences
#### Bigger kernel image size
Although it doesn't present any problems given todays sizes of SD cards, the Sidekick64 network kernel images are generally bigger in file size than the original kernel (TODO: Add example numbers). Although the rpimenu.prg file is currently baked into the network kernel image to avoid confusion when switching between vanilla kernel and network kernel, these are only a couple of additional bytes. The majority of the gain in size is only explainable by highlighting that [circle-stdlib](https://github.com/smuehlst/circle-stdlib) is normally being used during compilation for the network kernel mainly for the reason to offer the option to use HTTPS (with the help of mbedtls and newlib).
#### Bare metal framework Circle and circle-stdlib
The Sidekick kernel software is based on the bare metal Raspberry Pi C++ framework [Circle](https://github.com/rsta2/circle) created by Rene Stange. While the official Sidekick software v0.48 (at the time of writing the latest release) uses Circle Step 43.1, the network enabled Sidekick fork is based on the latest available version - Circle Step 44.3. The network fork is also using the GCC 10 based toolchain version and Raspberry Pi firmware versions that are recommended to be used together with Circle Step 44.3.

Because it seemed to make a lot of sense to be able to make use of HTTPS requests  with a network enabled Sidekick, the network fork is commonly compiled against the project [circle-stdlib](https://github.com/smuehlst/circle-stdlib) that is maintained by Stephan Mühlstrasser. It combines Circle together with newlib as C++ standard library and already includes mbedtls. This means, the network fork is being compiled directly against circle-stdlib (at the time of writing circle-stdlib v15.10 including Circle Step 44.3).

There are multiple variants of the Makefile available within the network branch as sometimes we want to test something just with Circle and sometimes with circle-stdlib.

### SD card: Mount state, potentially more write operations

During the runtime of Sidekick64 the SD card is normally only mounted for a short while it is being accessed. Right after an access it will be unmounted. With the network kernel enabled the SD card will stay mounted during the whole runtime of Sidekick64 as requests to the SD card may occur more frequently (for example in case of HTTPS requests where certificates may be searched for on the SD card). Write operations to the SD card may happen more often as network features may allow to download and store files.

## Known problems
### Raspberry Pi 3B+ needs a cooling airflow
This is explained in detail in the section [USB stack, power consumption and CPU temperature](#usb-stack-power-consumption-and-cpu-temperature). A Raspberry Pi 3A+ is less affected unless it is inside of an enclosure without any airflow.

### Recommended setting for jumper "A13-BTN"
Until the following is resolved by a software change it is recommended to leave the jumper A13-BTN  set to BTN.

The Sidekick64 PCB can be configured to listen to the signal A13 of the C64/C128 by closing the "A13-BTN" jumper in position A13. The main purpose of this setting is to emulate the presence of a C128 function ROM in software (while the real socket U36 stays empty). If you are not running Sidekick64 with a C128 or while you are not trying to emulate a function ROM at a C128, the jumper setting A13 is not mandatory.

Background: During tests of the network kernel we have observed that the menu screen refresh might be affected if the signal jumper is set to A13. This is most obvious in the network setting subscreen of the Sidekick menu where the CPU temperature is displayed and the screen will automatically refresh itself regularly. The Sidekick64 network enabled kernel uses an NMI based screen refresh while the menu is active. It seems that the A13 signal influences the triggering of NMIs.

### Network reliability
Depending on the current emulation type Sidekick64 may be too busy to handle network traffic in parallel without instabilities in the emulation of the current payload. This typically applies to cartridge emulation like EasyFlash, NeoRAM or Freezer cartridges or the audio player for modules. This means, network connectivity will hibernate while Sidekick64 emulates a cartridge. Pressing the Sidekick64 reset button for a more than one second will stop the emulation, bring the Sidekick menu back up and also continue network connectivity.

Cable-based ethernet with the Raspberry Pi 3B+ works reliable after hibernation but WLAN needs to reconnect to the Access point and may freeze the Sidekick64 if it fails to do so. In a lucky situation the reconnect only takes two or three seconds.

Emulating PRGs via the Launcher mode is more easy-going as an average PRG (game, music, demo) may not be interested at all to communicate to a expansion port cartridge. In those cases Sidekick64 doesn't  need to pay attention to what is going on the address and data lines of the expansion port.

This also means that during Launcher mode the WLAN connection will not be lost and WLAN can be enjoyed reliably over a longer period of time.

Uploading a "large" file via the web interface may take longer via WLAN as compared to cable based network.

To wrap it up, WLAN is less convenient to use than ethernet via cable. In exchange a Raspberry Pi 3B+ with active network has the inconvenience of needing airflow to keep the CPU temperature steady.
### CRT files in root folder
While there is no problem with storing CRT files (cartridge images) in their destined locations on the SD card it is not recommended to put any CRT files into the root folder of the SD card as  mbedtls will likely look for certificate files in the root folder when HTTPS is being used.
### Git branch will regularly be rebased and history will be rewritten
This git repository is rebased frequently on top of Frenetic's latest Sidekick64 releases to make it as easy as possible to be merged into Frenetic's main git repository should it become stable enough for Frenetic to decide to integrate it with the mainline code. Regular rebases are necessary in this fork and will rewrite git history of this repository so it might be irritating to try to pull from this branch from time to time as local and remote history will not match.
## Credits
I would like to thank
* Frenetic for releasing exciting open source retro projects like Sidekick64 and for his help on pointing me to the areas in his source code where I could safely try to add some network stuff.
* Rene Stange for creating, sharing and documenting Circle with attention to detail and Stephan Mühlstrasser for creating and continuously updating circle-stdlib with the latest upstream components.
* My network testers call286 (also for proof-reading), ILAH, znarF, SkulleateR, JohnFante, Emwee, TurboMicha, felixw, -trb- at forum64.de for their valuable feedback and ideas!
* I would also like to thank the CSDb admins for offering the CSDb web service and also hosting an instance of the HVSC. Last but not least thanks to the very active C64 demo scene for still releasing exciting demos, music and other stuff.

Written by: hpingel aka emulaThor
