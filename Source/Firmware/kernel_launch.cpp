/*
  _________.__    .___      __   .__        __       .____                               .__     
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __   |    |   _____   __ __  ____   ____ |  |__  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /   |    |   \__  \ |  |  \/    \_/ ___\|  |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <    |    |___ / __ \|  |  /   |  \  \___|   Y  \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \   |_______ (____  /____/|___|  /\___  >___|  /
        \/         \/    \/     \/       \/     \/           \/    \/           \/     \/     \/ 

 kernel_launch.cpp

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick Launch: example how to implement a .PRG dropper (for C64 and C128)
 Copyright (c) 2019-2022 Carsten Dachsbacher <frenetic@dachsbacher.de>

 Logo created with http://patorjk.com/software/taag/
 
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "kernel_launch.h"

#ifdef WITH_NET
extern CSidekickNet * pSidekickNet;
bool swiftLinkEnabled = false;
static const unsigned swiftLinkLogLengthMax = 64000;
char swiftLinkLog[ swiftLinkLogLengthMax ];
char swiftLinkReceived[ swiftLinkLogLengthMax ];
char swiftLinkByte = 0;
char swiftLinkEcho = 0;
unsigned swiftLinkDataReads = 0;
unsigned swiftLinkDoNMI = 0;
unsigned swiftLinkCounter = 0;
unsigned swiftLinkReceivedCounter = 0;
unsigned swiftLinkBaud = 0;
unsigned swiftLinkBaudOld = 0;
#endif

// we will read this .PRG file
static const char DRIVE[] = "SD:";
#ifndef COMPILE_MENU
static const char FILENAME[] = "SD:C64/test.prg";		// .PRG to start
static const bool c128PRG = false;
#endif
static const char FILENAME_CBM80[] = "SD:C64/launch.cbm80";	// launch code (CBM80 8k cart)
static const char FILENAME_CBM80_NOINIT[] = "SD:C64/launch_noinit.cbm80";	// launch code (CBM80 8k cart)
static const char FILENAME_CBM128[] = "SD:C64/launch128.cbm80";	// launch code (C128 cart)

static const char FILENAME_SPLASH_RGB[] = "SD:SPLASH/sk64_launch.tga";

// cartridge memory window bROML or bROMH
#define ROM_LH		bROML

static u32	configGAMEEXROMSet, configGAMEEXROMClr;
static u32	resetCounter, c64CycleCount;
static u32	disableCart, transferStarted, currentOfs, transferPart;
u32 prgSize;
unsigned char prgData[ 65536 ] AAA;
static u32 startAddr, prgSizeAboveA000, prgSizeBelowA000, endAddr;

// in case the launch code starts with the loading address
#define LAUNCH_BYTES_TO_SKIP	0
static unsigned char launchCode[ 65536 ] AAA;

static u32 resetFromCodeState = 0;
static u32 _playingPSID = 0;

extern volatile u8 forceReadLaunch;

void prepareOnReset( bool refresh = false )
{
	if ( !refresh )
	{
		/*CleanDataCache();
		InvalidateDataCache();
		InvalidateInstructionCache();*/
		SyncDataAndInstructionCache();
	}

	if ( _playingPSID )
	{
		extern unsigned char charset[ 4096 ];
		CACHE_PRELOADL2STRM( &charset[ 2048 ] );
		FORCE_READ_LINEAR32( (void*)&charset[ 2048 ], 1024 );
	}

	// .PRG data
	CACHE_PRELOAD_DATA_CACHE( &prgData[ 0 ], prgSize, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( prgData, prgSize, prgSize * 8 );

	// launch code / CBM80
	CACHE_PRELOAD_DATA_CACHE( &launchCode[ 0 ], 512, CACHE_PRELOADL2KEEP )
	FORCE_READ_LINEAR32a( launchCode, 512, 512 * 16 );

	for ( u32 i = 0; i < prgSizeAboveA000; i++ )
		forceReadLaunch = prgData[ prgSizeBelowA000 + i + 2 ];

	for ( u32 i = 0; i < prgSizeBelowA000 + 2; i++ )
		forceReadLaunch = prgData[ i ];

	// FIQ handler
	CACHE_PRELOAD_INSTRUCTION_CACHE( (void*)&FIQ_HANDLER, 3 * 1024 );
	FORCE_READ_LINEAR32( (void*)&FIQ_HANDLER, 3 * 1024 );
}

static u32 nBytesRead, stage;

static u8 showSlideShow = 0;
static u8 curSlideShowImage = 0;
static u16 curPixelRow = 0;
static u16 curCopyRow = 0;
static u32 pauseSlideShow = 0;
static u8 timeSlideShow[ 32 ];


#ifdef COMPILE_MENU
void KernelLaunchRun( CGPIOPinFIQ m_InputPin, CKernelMenu *kernelMenu, const char *FILENAME, bool hasData = false, u8 *prgDataExt = NULL, u32 prgSizeExt = 0, u32 c128PRG = 0, u32 playingPSID = 0, u8 noInitStartup = 0 )
#else
void CKernelLaunch::Run( void )
#endif
{
	// initialize latch and software I2C buffer
	initLatch();

	latchSetClearImm( 0, LATCH_RESET | LATCH_LED_ALL | LATCH_ENABLE_KERNAL );

	if ( c128PRG )
	{
		configGAMEEXROMSet = bGAME | bEXROM | bNMI | bDMA;
		configGAMEEXROMClr = 0; 
	} else
	{
		// set GAME and EXROM as defined above (+ set NMI, DMA and latch outputs)
		configGAMEEXROMSet = bGAME | bNMI | bDMA;
		configGAMEEXROMClr = bEXROM; 
	}
	SETCLR_GPIO( configGAMEEXROMSet, configGAMEEXROMClr );

	#ifndef COMPILE_MENU
	m_EMMC.Initialize();
	#endif

	#ifdef COMPILE_MENU
	_playingPSID = playingPSID;

	showSlideShow = 0;
	tftSlideShowNImages = 0;

	if ( screenType == 0 )
	{
		splashScreen( sidekick_launch_oled );
	} else
	if ( screenType == 1 )
	{
		char fn[ 1024 ];//, fnslide[ 1024*2 ];
		// attention: this assumes that the filename ending is always ".crt"!
		memset( fn, 0, 1024 );

		strncpy( fn, FILENAME, strlen( FILENAME ) - 4 );
		strcat( fn, ".tga" );

		logger->Write( "RaspiFlash", LogNotice, "trying to load: '%s'", fn );

		if ( tftLoadBackgroundTGA( (char*)DRIVE, fn ) )
		{
			tftCopyBackground2Framebuffer();
		} else
		{
			tftLoadBackgroundTGA( DRIVE, FILENAME_SPLASH_RGB, 8 );

			int w, h; 
			extern char FILENAME_LOGO_RGBA[128];
			extern unsigned char tempTGA[ 256 * 256 * 4 ];

			if ( tftLoadTGA( DRIVE, FILENAME_LOGO_RGBA, tempTGA, &w, &h, true ) )
			{
				tftBlendRGBA( tempTGA, tftBackground, 0 );
			}

			tftCopyBackground2Framebuffer();
		}

		memset( fn, 0, 1024 );
		strncpy( fn, FILENAME, strlen( FILENAME ) - 4 );
		strcat( fn, "-slideshow.tga" );

		if ( tftLoadSlideShowTGA( DRIVE, fn ) && tftSlideShowNImages > 0 )
		{
			showSlideShow = 1;
			curSlideShowImage = tftSlideShowNImages - 1;
			curCopyRow = curPixelRow = 0;
			pauseSlideShow = 0;

			fn[ strlen( fn ) - 3 ] = 0;
			strcat( fn, "time" );
			u32 size = 0;
			for ( u32 i = 0; i < 32; i++ )
				timeSlideShow[ i ] = 10;
			readFile( logger, DRIVE, fn, timeSlideShow, &size, 32 );
		}

		tftInitImm();
		tftSendFramebuffer16BitImm( tftFrameBuffer );
	}
	#endif

	// read launch code and .PRG
	u32 size;
	if ( c128PRG )
		readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM128, launchCode, &size ); else
	{
		if ( noInitStartup )
			readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80_NOINIT, launchCode, &size ); else
			readFile( logger, (char*)DRIVE, (char*)FILENAME_CBM80, launchCode, &size );
	}

	#ifdef COMPILE_MENU
	if ( !hasData )
	{
		readFile( logger, (char*)DRIVE, (const char*)FILENAME, prgData, &prgSize );
	} else
	{
		prgSize = prgSizeExt;
		memcpy( prgData, prgDataExt, prgSize );
	}
	#else
	readFile( logger, (char*)DRIVE, (const char*)FILENAME, prgData, &prgSize );
	#endif

	startAddr = prgData[ 0 ] + prgData[ 1 ] * 256;
	endAddr = startAddr + prgSize - 2;
	prgSizeBelowA000 = 0xa000 - startAddr;
	if ( prgSizeBelowA000 > prgSize - 2 )
	{
		prgSizeBelowA000 = prgSize - 2;
		prgSizeAboveA000 = 0;
	} else
		prgSizeAboveA000 = prgSize - prgSizeBelowA000;

	resetFromCodeState = 0;

	#ifdef WITH_NET
	swiftLinkEnabled = false; //too early to enable here
	swiftLinkByte = 0;
	swiftLinkEcho = 0;
	swiftLinkDoNMI = 0;
	swiftLinkCounter = 0;
	swiftLinkLog[0] = '\0';
	swiftLinkReceivedCounter = 0;
	swiftLinkReceived[0] = '\0';
	swiftLinkDataReads = 0;
	unsigned keepNMILow = 0;

	pSidekickNet->setCurrentKernel( (char*)"l" );
	unsigned netDelay = _playingPSID ? 900000000: 30000000; //TODO: improve this
	#endif

	// setup FIQ
	prepareOnReset();
	DisableIRQs();
	m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
	m_InputPin.EnableInterrupt ( GPIOInterruptOnRisingEdge );

// this code is in here twice?
	c64CycleCount = resetCounter = 0;
	disableCart = transferStarted = currentOfs = 0;
	transferPart = 1;

	// warm caches
	prepareOnReset( true );
	prepareOnReset( false );

	// ready to go
	latchSetClear( LATCH_RESET, 0 );

	c64CycleCount = resetCounter = 0;
	disableCart = transferStarted = currentOfs = 0;
	transferPart = 1;
	CACHE_PRELOADL2KEEP( &prgData[ prgSizeBelowA000 + 2 ] );
	CACHE_PRELOADL2KEEP( &prgData[ 0 ] );

	nBytesRead = 0; stage = 1;
	u32 cycleCountC64_Stage1 = 0;
	#ifdef WITH_NET
	pSidekickNet->setCurrentKernel( (char*)"l" );
	unsigned netDelay = _playingPSID ? 900000000: 300; //TODO: improve this
	#endif

	// wait forever
	while ( true )
	{
		if ( !disableCart && !c128PRG )
		{
			if ( ( ( stage == 1 ) && nBytesRead > 0 ) ||
 				 ( ( stage == 2 ) && nBytesRead < 32 && c64CycleCount - cycleCountC64_Stage1 > 500000 ) )
			{
				if ( stage == 1 ) 
				{ 
					stage = 2; cycleCountC64_Stage1 = c64CycleCount; 
				} else 
				{
					stage = 0;
					latchSetClear( 0, LATCH_RESET );
					DELAY(1<<11);
					latchSetClear( LATCH_RESET, 0 );
					nBytesRead = 0; stage = 1;
					c64CycleCount = 0;
				}
			}
		}

		#ifdef WITH_NET
		if ( keepNMILow > 0 ){
				keepNMILow --;
				if ( keepNMILow == 0 )
					SET_GPIO( bNMI );
		};
		#endif
		
		#ifdef COMPILE_MENU
		TEST_FOR_JUMP_TO_MAINMENU( c64CycleCount, resetCounter )

		if ( resetFromCodeState == 2 )
		{
			EnableIRQs();
			m_InputPin.DisableInterrupt();
			m_InputPin.DisconnectInterrupt();
			#ifdef WITH_NET
			if ( swiftLinkCounter > 0)
			{
					logger->Write( "sk", LogNotice, "swiftLinkLog: '%s', (%i)",swiftLinkLog, swiftLinkCounter );
					swiftLinkCounter = 0;
					swiftLinkLog[0] = '\0';
			}
			logger->Write( "sk", LogNotice, "leaving launcher kernel");
			#endif
			return;		
		}
		#endif


		#ifdef WITH_NET
		if ( pSidekickNet->IsRunning() )
		{
			if ( disableCart && transferStarted)
				netDelay--;
			if (netDelay <= 0 )
			{
				boolean justEnabled = !swiftLinkEnabled;
				swiftLinkEnabled = true;
				netDelay = _playingPSID ? 3000: 300000000;
				m_InputPin.DisableInterrupt();
				m_InputPin.DisconnectInterrupt();
				EnableIRQs();
				if(justEnabled){
					logger->Write( "sk", LogNotice, "swiftLinkEnabled unlocked");
				}
				else
					logger->Write( "sk", LogNotice, "netdelay is zero, sw data reads = %i", swiftLinkDataReads);

				if ( swiftLinkEnabled )
				{
					if ( swiftLinkCounter > 0 && swiftLinkCounter <200)
					{
						logger->Write( "sk", LogNotice, "swiftLinkLog: '%s', (%i)",swiftLinkLog, swiftLinkCounter );
	 					swiftLinkCounter = 0;
						swiftLinkLog[0] = '\0';
					}
					else if ( swiftLinkCounter > 0 && swiftLinkCounter >=200)
					{
						logger->Write( "sk", LogNotice, "swiftLinkLog: bla (%i)", swiftLinkCounter );
						//logger->Write( "sk", LogNotice, "swiftLinkLog: '%s', (%i)",swiftLinkLog, swiftLinkCounter );
						swiftLinkCounter = 0;
						swiftLinkLog[0] = '\0';
					}
					if ( swiftLinkByte > 0){
					  //logger->Write( "sk", LogNotice, "swiftLinkByte: '%i', '%s'",swiftLinkByte, swiftLinkByte );
						
						logger->Write( "sk", LogNotice, "swiftLinkByte: '%i', received: '%s'",swiftLinkByte , swiftLinkReceived);
						swiftLinkByte = 0;
					}

					if  ( swiftLinkBaud != swiftLinkBaudOld){
						swiftLinkBaudOld = swiftLinkBaud;
						unsigned baud = 0;
						switch ( swiftLinkBaud )
						{
							case  0: baud = 99999;	break; //enable enhanced speed!
							case  5: baud = 300;	break;
							case  6: baud = 600;	break;
							case  7: baud = 1200;	break;
							case  8: baud = 2400;	break;
							case  9: baud = 3300;	break;
							case 10: baud = 4800;	break;
							case 11: baud = 7200;	break;
							case 12: baud = 9600;	break;
							case 13: baud = 1440;	break;
							case 14: baud = 19200;	break;
							case 15: baud = 38400;	break;
							default: baud = 77777; break;
						}					
						logger->Write( "sk", LogNotice, "swiftLinkBaud change to: %i", baud);	
					}
				}

				//if ( keepNMILow >0 )
				//	logger->Write( "sk", LogNotice, "NMI coming up now.");	
				
				if ( pSidekickNet->isReturnToMenuRequired())
					return;
				
				kernelMenu->updateSystemMonitor();
				pSidekickNet->handleQueuedNetworkAction();
				//pSidekickNet->requireCacheWellnessTreatment();

				// warm caches
				prepareOnReset( true );
				DELAY(1<<18);
				prepareOnReset( true );
				DELAY(1<<18);
				prepareOnReset( true );
				
				DisableIRQs();
				m_InputPin.ConnectInterrupt( FIQ_HANDLER, FIQ_PARENT );
				m_InputPin.EnableInterrupt( GPIOInterruptOnRisingEdge );
				
			}
			if ( swiftLinkDoNMI > 0 ){
				if ( --swiftLinkDoNMI == 0)
				{
					keepNMILow = 20;
					CLR_GPIO( bNMI );
				}
			}
			
		}
		#endif

		asm volatile ("wfi");
		
	}

	// and we'll never reach this...
	m_InputPin.DisableInterrupt();
}

#ifdef COMPILE_MENU
void KernelLaunchFIQHandler( void *pParam )
#else
void CKernelLaunch::FIQHandler (void *pParam)
#endif
{
	register u32 D;

	// after this call we have some time (until signals are valid, multiplexers have switched, the RPi can/should read again)
	START_AND_READ_ADDR0to7_RW_RESET_CS

	// update some counters
	UPDATE_COUNTERS_MIN( c64CycleCount, resetCounter )

	// read the rest of the signals
	WAIT_AND_READ_ADDR8to12_ROMLH_IO12_BA

	if ( resetCounter > 3 && resetFromCodeState != 2 )
	{
		disableCart = transferStarted = 0;
		nBytesRead = 0; stage = 1;
		SETCLR_GPIO( configGAMEEXROMSet | bNMI, configGAMEEXROMClr );
		FINISH_BUS_HANDLING
		return;
	}

	if ( disableCart )
	{
		//strcat( swiftLinkLog, "x" );

		if ( _playingPSID )
		{
			if ( IO2_ACCESS && CPU_READS_FROM_BUS && GET_IO12_ADDRESS == 0x55 )
			{
				static u32 oc = 0;
				extern unsigned char charset[ 4096 ];
				u32 D = charset[ 2048 + oc ];
				oc ++; oc &= 1023;
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2STRM( &charset[ 2048 + oc ] );
			}
			if ( resetFromCodeState == 0 && IO2_ACCESS && CPU_WRITES_TO_BUS && GET_IO12_ADDRESS == 0x11 )
			{
				READ_D0to7_FROM_BUS( D )
				if ( D == 0x22 )
					resetFromCodeState = 1;
			}
			if ( resetFromCodeState == 1 && IO2_ACCESS && CPU_WRITES_TO_BUS && GET_IO12_ADDRESS == 0x33 )
			{
				READ_D0to7_FROM_BUS( D )
				if ( D == 0x44 )
				{
					resetFromCodeState = 2;
					latchSetClear( 0, LATCH_RESET );
				}
			}
			OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
			return;
		} else
		{
			if ( showSlideShow )
			{
				if ( pauseSlideShow )
				{
					pauseSlideShow --;
				} else
				if ( bufferEmptyI2C() )
				{
					extern void setMultiplePixels( u32 x, u32 y, u32 nx, u32 ny, u16 *c );
					setMultiplePixels( curCopyRow, 0, 0, 239, (u16 *)&tftSlideShow[ (curSlideShowImage * 240 + curCopyRow ) * 240 * 2 ] );

					do {
						curPixelRow ++;
						if ( curPixelRow > 255 )
						{
							curPixelRow = 0;
							pauseSlideShow = (u32)timeSlideShow[ tftSlideShowNImages - 1 - curSlideShowImage ] * 500000;
							curSlideShowImage = ( curSlideShowImage + tftSlideShowNImages - 1 ) % tftSlideShowNImages;
						} 
						curCopyRow = flipByte( curPixelRow );
					} while ( curCopyRow >= 240 );
				}
			}

			prepareOutputLatch4Bit();
			outputLatch();
			OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
			return;
		}
		#ifdef WITH_NET
		else if ( swiftLinkEnabled )
		{
			//strcat( swiftLinkLog, "-" );
			if ( IO1_ACCESS && CPU_READS_FROM_BUS ) // && (GET_IO12_ADDRESS >= 0x00 && GET_IO12_ADDRESS <= 0x03))
			{
				u32 D = 0;
				if ( GET_IO12_ADDRESS == 0x01){ //status register
					if ( swiftLinkEcho == 0)
						D = 16 + 32; //set transmit flag ( +DSR), show that we are ready to get next byte from C64
					else
						D = 8; //8; //set data register flag, show c64 that it may now pick up a byte
				}
				else if ( GET_IO12_ADDRESS == 0x00){ //} && swiftLinkEcho > 0){ //data register
					 D = swiftLinkEcho;
					 swiftLinkEcho = 0;
					 if (D == 0) D = 72;//fake echo
					 swiftLinkDataReads++;
				}
				unsigned hh = D/16, hl = D % 16;
				
				hh = hh < 10 ? hh + 48 : hh+55;
				hl = hl < 10 ? hl + 48 : hl+55;
				
				if ( swiftLinkCounter + 20 < swiftLinkLogLengthMax)
				{
					swiftLinkLog[swiftLinkCounter++] = GET_IO12_ADDRESS + 48;
					swiftLinkLog[swiftLinkCounter++] = 'r';
					swiftLinkLog[swiftLinkCounter++] = hh;
					swiftLinkLog[swiftLinkCounter++] = hl;
					swiftLinkLog[swiftLinkCounter++] = '/';
					swiftLinkLog[swiftLinkCounter] = '\0';
				}
				
				WRITE_D0to7_TO_BUS( D )
				FINISH_BUS_HANDLING
			}
			else if ( IO1_ACCESS && CPU_WRITES_TO_BUS  )
			{
				READ_D0to7_FROM_BUS( D )
				
				unsigned hh = D/16, hl = D % 16;
				
				unsigned hh2 = hh < 10 ? hh + 48 : hh+55;
				unsigned hl2 = hl < 10 ? hl + 48 : hl+55;
				
				boolean doLog = swiftLinkCounter + 20 < swiftLinkLogLengthMax;
				if (doLog){
					swiftLinkLog[swiftLinkCounter++] = GET_IO12_ADDRESS +48;
					swiftLinkLog[swiftLinkCounter++] = 'w';
					swiftLinkLog[swiftLinkCounter++] = hh2;
					swiftLinkLog[swiftLinkCounter++] = hl2;
				}
				if ( GET_IO12_ADDRESS == 0x03){ //control register
					swiftLinkBaud = hl;
				}
				else if ( GET_IO12_ADDRESS == 0x02){ //command register
//					if ( D & (1 << 4))
//					swiftLinkLog[swiftLinkCounter++] = 'e';
				}
				else if ( GET_IO12_ADDRESS == 0x00){ //data register
					swiftLinkByte = D;
					swiftLinkEcho = D;
					swiftLinkDoNMI = 5;
					
					swiftLinkReceived[ swiftLinkReceivedCounter++ ] = D;
					swiftLinkReceived[ swiftLinkReceivedCounter ] = '\0';
				}

				if (doLog){
					swiftLinkLog[swiftLinkCounter++] = '/';
					swiftLinkLog[swiftLinkCounter] = '\0';
				}
				//FINISH_BUS_HANDLING
			}
			
			else if ( IO2_ACCESS && CPU_READS_FROM_BUS ) // && (GET_IO12_ADDRESS >= 0x00 && GET_IO12_ADDRESS <= 0x03))
			{
				u32 D = 0;
				WRITE_D0to7_TO_BUS( D )
				//FINISH_BUS_HANDLING
			}
			else if ( IO2_ACCESS && CPU_WRITES_TO_BUS ) //&& (GET_IO12_ADDRESS >= 0x00 && GET_IO12_ADDRESS <= 0x03))
			{
				u32 D = 0;
				READ_D0to7_FROM_BUS( D )
				//FINISH_BUS_HANDLING
			}
		}
		#endif
	}

	if ( swiftLinkEnabled )
	{
		OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
		return;
	}
	
	// access to CBM80 ROM (launch code)
	if ( CPU_READS_FROM_BUS && ACCESS( ROM_LH ) )
	{
		WRITE_D0to7_TO_BUS( launchCode[ GET_ADDRESS + LAUNCH_BYTES_TO_SKIP ] );
		nBytesRead++;
	}

	if ( !disableCart && IO1_ACCESS ) 
	{
		if ( CPU_WRITES_TO_BUS ) 
		{
			transferStarted = 1;

			// any write to IO1 will (re)start the PRG transfer
			if ( GET_IO12_ADDRESS == 2 )
			{
				currentOfs = prgSizeBelowA000 + 2;
				transferPart = 1; 
				CACHE_PRELOADL2KEEP( &prgData[ prgSizeBelowA000 + 2 ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ prgSizeBelowA000 + 2 ];
			} else
			{
				currentOfs = 0;
				transferPart = 0;
				CACHE_PRELOADL2KEEP( &prgData[ 0 ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ 0 ];
			}
			return;
		} else
		// if ( CPU_READS_FROM_BUS ) 
		{
			if ( GET_IO12_ADDRESS == 1 )	
			{
				// $DE01 -> get number of 256-byte pages
				if ( transferPart == 1 ) // PRG part above $a000
					D = ( prgSizeAboveA000 + 255 ) >> 8;  else
					D = ( prgSizeBelowA000 + 255 ) >> 8; 
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ currentOfs ];
			} else
			if ( GET_IO12_ADDRESS == 4 ) // full 256-byte pages 
			{
				D = ( prgSize - 2 ) >> 8;
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ currentOfs ];
			} else
			if ( GET_IO12_ADDRESS == 5 ) // bytes on last non-full 256-byte page
			{
				D = ( prgSize - 2 ) & 255;
				WRITE_D0to7_TO_BUS( D )
				CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
				FINISH_BUS_HANDLING
				forceReadLaunch = prgData[ currentOfs ];
			} else
			if ( GET_IO12_ADDRESS == 2 )	
			{
				// $DE02 -> get BASIC end address
				WRITE_D0to7_TO_BUS( (u8)( endAddr & 255 ) )
				FINISH_BUS_HANDLING
			} else
			if ( GET_IO12_ADDRESS == 3 )	
			{
				// $DE02 -> get BASIC end address
				WRITE_D0to7_TO_BUS( (u8)( (endAddr>>8) & 255 ) )
				FINISH_BUS_HANDLING
			} else
			{
				// $DE00 -> get next byte
/*				if ( transferPart == 1 ) // PRG part above $a000
				{
					//D = prgData[ prgSizeBelowA000 + 2 + currentOfs++ ]; 
					D = forceReadLaunch;	currentOfs ++;
					//D = (currentOfs ++) & 255;
					WRITE_D0to7_TO_BUS( D )
					CACHE_PRELOADL2KEEP( &prgData[ prgSizeBelowA000 + 2 + currentOfs ] );
					FINISH_BUS_HANDLING
					forceReadLaunch = prgData[ prgSizeBelowA000 + 2 + currentOfs ];
				} else*/
				{
					D = forceReadLaunch;	currentOfs ++;
					//D = prgData[ currentOfs++ ];
					WRITE_D0to7_TO_BUS( D )
					CACHE_PRELOADL2KEEP( &prgData[ currentOfs ] );
					FINISH_BUS_HANDLING
					forceReadLaunch = prgData[ currentOfs ];
				}
			}
				
			return;
		}
	}

	if ( !disableCart && CPU_WRITES_TO_BUS && IO2_ACCESS ) // writing #123 to $df00 (IO2) will disable the cartridge
	{
		READ_D0to7_FROM_BUS( D )

		if ( GET_IO12_ADDRESS == 0 && D == 123 )
		{
			disableCart = 1;
			SET_GPIO( bGAME | bEXROM | bNMI );
			FINISH_BUS_HANDLING
			return;
		}
	}

	OUTPUT_LATCH_AND_FINISH_BUS_HANDLING
}
