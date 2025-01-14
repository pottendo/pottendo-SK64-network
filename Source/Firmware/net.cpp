/*
  _________.__    .___      __   .__        __        _________   ________   _____  
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __    \_   ___ \ /  _____/  /  |  | 
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /    /    \  \//   __  \  /   |  |_
 /        \|  / /_/ \  ___/|    <|  \  \___|    <     \     \___\  |__\  \/    ^   /
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \     \______  /\_____  /\____   | 
        \/         \/    \/     \/       \/     \/            \/       \/      |__| 
 
 net.cpp

 RasPiC64 - A framework for interfacing the C64 and a Raspberry Pi 3B/3B+
          - misc code
 Copyright (c) 2019, 2020 Carsten Dachsbacher <frenetic@dachsbacher.de>

 Network related code in this file contributed by Henning Pingel based on 
 the networking examples within Rene Stanges Circle framework.

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

#include "net.h"
#include "helpers.h"

#ifndef IS264
#include "c64screen.h"
#include "config.h"
#include "PSID/psid64/psid64.h"
#else
#include "264screen.h"
#include "264config.h"
#endif


#include <circle/net/dnsclient.h>
#include <circle/net/ntpclient.h>
#ifdef WITH_TLS
#include <circle-mbedtls/httpclient.h>
#else
#include <circle/net/httpclient.h>
#endif

#ifdef WITH_TLS
#include <circle-mbedtls/tlssimpleclientsocket.h>
#endif
#include <circle/net/in.h>
#include <circle/types.h>
#include <ctype.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef WITH_TLS
#include <mbedtls/error.h>
#endif

// Network configuration
// Time configuration
static const char NTPServer[]    = "pool.ntp.org";
static const int nTimeZone       = 1*60;		// minutes diff to UTC
static const char DRIVE[] = "SD:";
//nDocMaxSize reserved 5 MB as the maximum size of the kernel file
static const unsigned nDocMaxSize = 1027*1024*12-16;

static const char CSDB_HOST[] = "csdb.dk";

#define SK_MODEM_SWIFTLINK	1
#define SK_MODEM_USERPORT_USB 2
#define SK_WIC64_EXP_EMULATION 3

#ifdef WITH_WLAN
#define DRIVEWLAN "SD:"
#define FIRMWARE_PATH	DRIVEWLAN "/wlan/"		// wlan firmware files must be provided here
#define CONFIG_FILE	FIRMWARE_PATH "wpa_supplicant.conf"
#endif

//static 
const u8 WEBUPLOADPRG[] =
{
//Caution: Whenever upstream webUploadMode.prg changes we have to manually call converttool!
//./webcontent/converttool -b webUploadMode.prg > webUploadMode.h
//This has to be put into the workflow

#include "C64Side/webUploadMode.h"
};

//temporary hack
extern u32 prgSizeLaunch;
extern unsigned char prgDataLaunch[ 1027*1024*12 ] AAA;

extern int fileExists( CLogger *logger, const char *DRIVE, const char *FILENAME );

CSidekickNet::CSidekickNet( 
		CInterruptSystem * pInterruptSystem, 
		CTimer * pTimer, 
		CScheduler * pScheduler, 
		CEMMCDevice * pEmmcDevice, 
		CDeviceNameService * pDeviceNameService,
		CKernelMenu * pKernelMenu
):	m_USBHCI (0),
		m_pScheduler(pScheduler),
		m_pInterrupt(pInterruptSystem),
		m_pTimer(pTimer),
		m_EMMC( *pEmmcDevice),
		m_DeviceNameService( pDeviceNameService ),
		m_kMenu(pKernelMenu),
		m_Net (0),
#ifdef WITH_WLAN
		m_WLAN (0),
		m_WPASupplicant (0),
		m_useWLAN (true),
#else
		m_useWLAN (false),
#endif
		m_DNSClient(0),
#ifdef WITH_TLS
		m_TLSSupport(0),
#endif
		m_pBBSSocket(0),
		m_WebServer(0),
		m_pUSBSerial(0),
		m_pUSBMidi(0),
		m_isFSMounted( false ),
		m_isActive( false ),
		m_isPrepared( false ),
		m_isUSBPrepared( false ),
		m_isNetworkInitQueued( false ),
		//m_isKernelUpdateQueued( false ),
		m_isFrameQueued( false ),
		m_isSktpKeypressQueued( false ),
		m_isCSDBDownloadQueued( false ),
		m_isCSDBDownloadSavingQueued( false ),
		m_isDownloadReady( false ),
		m_isDownloadReadyForLaunch( false ),
		m_doLaunchAfterSave(false),
		m_isRebootRequested( false ),
		m_isReturnToMenuRequested( false ),
		m_networkActionStatusMsg( (char * ) ""),
		m_sktpScreenContent( (unsigned char * ) ""),
		m_sktpSessionID( (char * ) ""),
		m_isSktpScreenActive( false ),
		m_wasSktpScreenFreshlyEntered( false ),
		m_isMenuScreenUpdateNeeded( false ),
		m_isC128( false ),
		//m_WiCEmuSendCommand( false ),
		m_WiCEmuIsWriteMode( false ),
		m_WiCEmuIsWriteModeOld( false ),
		m_isBBSSocketConnected(false),
		m_isBBSSocketFirstReceive(true),
		m_BBSSocketDisconnectPlusCount(0),
		m_bSaveCSDBDownload2SD( false ),
		m_PiModel( m_pMachineInfo->Get()->GetMachineModel () ),
		//m_SidekickKernelUpdatePath(0),
		m_queueDelay(0),
		m_timestampOfLastWLANKeepAlive(0),
		m_timeoutCounterStart(0),
		m_skipSktpRefresh(0),
		m_sktpRefreshTimeout(0),
		m_sktpRefreshWaiting(false),
		m_sktpScreenPosition(0),
		m_sktpResponseLength(0),
		m_sktpResponseType(0),
		m_sktpKey(0),
		m_sktpSession(0),
		m_sktpScreenErrorCode(0),
		m_videoFrameCounter(1),
		m_sysMonHeapFree(0),
		m_sysMonCPUTemp(0),
		m_loglevel(2),
		m_CSDBDownloadSavePath((char *) ""), 
		m_currentKernelRunning((char *) "m"),
		m_oldSecondsLeft(0),
		m_modemCommand( (char * ) ""),
		m_modemCommandLength(0),
		m_modemEmuType(0),
		m_modemOutputBufferLength(0),
		m_modemOutputBufferPos(0),
		m_modemInputBufferLength(0),
		m_modemInputBufferPos(0),
		m_socketPort(0),
		m_baudRate(1200),
		m_PRGLaunchTweakValue(4)
{
	assert (m_pTimer != 0);
	assert (& m_pScheduler != 0);

	//timezone is not really related to net stuff, it could go somewhere else
	m_pTimer->SetTimeZone (nTimeZone);
	
	m_modemCommand[0] = '\0';
	m_modemInputBuffer[0] = '\0';
	m_modemOutputBuffer[0] = '\0';
	m_socketHost[0] = '\0';
	
	m_CSDBDownloadPath[0] = '\0';
	m_CSDBDownloadExtension[0] = '\0';
	m_CSDBDownloadFilename[0] = '\0';
}

void CSidekickNet::setErrorMsgC64( char * msg, boolean sticky = true ){ 
	setErrorMsg2( msg, sticky );
	m_isMenuScreenUpdateNeeded = true;
};

/*
void CSidekickNet::setSidekickKernelUpdatePath( unsigned type)
{
	m_SidekickKernelUpdatePath = type;
};*/

boolean CSidekickNet::ConnectOnBoot (){
	//as ConnectOnBoot is called anyway very early we can afford to dump
	//the initialization of the baudrate in here
	if (netModemEmuDefaultBaudrate > 0){
		switch(netModemEmuDefaultBaudrate){
			case 300:
			case 1200:
			case 2400:
			case 4800:
			case 9600:
				m_baudRate = netModemEmuDefaultBaudrate;
		}
	}
	
	//default to cable based network on B models
	if ( netConnectOnBoot && !RaspiHasOnlyWLAN() && usesWLAN() && 
		fileExists( logger, (char*)DRIVE, (char*)(char*)"SD:WLAN/wpa_supplicant.conf" ) <= 0)
	{
		useLANInsteadOfWLAN();
	}
			
	return netConnectOnBoot;
}

boolean CSidekickNet::usesWLAN (){
	return m_useWLAN;
}

boolean CSidekickNet::kernelSupportsWLAN(){
	#ifdef WITH_WLAN
		return true;
	#else
		return false;
	#endif
}


void CSidekickNet::useLANInsteadOfWLAN (){
	m_useWLAN = false;
}

boolean CSidekickNet::Initialize()
{
	const unsigned sleepLimit = 100 * (m_useWLAN ? 10:2);
	unsigned sleepCount = 0;
	
	if (m_isActive)
	{
		if (m_loglevel > 1)
			logger->Write( "CSidekickNet::Initialize", LogNotice, 
				"Strange: Network is already up and running. Skipping another init."
			);
		return true;
	}
	if ( !m_isPrepared )
	{
		if (!Prepare()){
			return false;
		}
		m_isPrepared = true;
	}
	
	while (!m_Net->IsRunning() && sleepCount < sleepLimit)
	{
		usbPnPUpdate();
		m_pScheduler->Yield ();
		m_pScheduler->Yield ();
		m_pScheduler->Yield ();
		m_pScheduler->MsSleep(100);
		sleepCount ++;
	}

	if (!m_Net->IsRunning () && sleepCount >= sleepLimit){
		if ( m_useWLAN )
		{
			if (m_loglevel > 1)
				logger->Write( "CSidekickNet::Initialize", LogNotice, 
					"WLAN connection can't be established - maybe poor reception?"
				);
			//                    "012345678901234567890123456789012345XXXX"
			setErrorMsgC64((char*)"   Wireless network connection failed!  ");
		}
		else{
			if (m_loglevel > 1)
				logger->Write( "CSidekickNet::Initialize", LogNotice, 
					"Network connection is not running - is ethernet cable not attached?"
				);
			//                    "012345678901234567890123456789012345XXXX"
			if ( !netConnectOnBoot)
				setErrorMsgC64((char*)"  Cable plugged in? Check and reconnect ");
		}
		return false;
	}

	//net connection is up and running now
	m_isActive = true;

	m_DNSClient = new CDNSClient (m_Net);

#ifdef WITH_TLS
	m_TLSSupport = new CTLSSimpleSupport (m_Net);
#endif

	bool success = false;
	
	m_CSDB.hostName = CSDB_HOST;
	m_CSDB.port = 443;
	m_CSDB.attempts = 0;
	m_CSDB.valid = false;
	m_CSDB.logPrefix = getLoggerStringForHost( CSDB_HOST, 443);

	m_CSDB_HVSC.hostName = "hvsc.csdb.dk";
	m_CSDB_HVSC.port = 443;
	m_CSDB_HVSC.attempts = 0;
	m_CSDB_HVSC.valid = false;
	m_CSDB_HVSC.logPrefix = getLoggerStringForHost( m_CSDB_HVSC.hostName, 443);

	m_ModarchiveAPI.hostName = "api.modarchive.org";
	m_ModarchiveAPI.port = 443;
	m_ModarchiveAPI.attempts = 0;
	m_ModarchiveAPI.valid = false;
	m_ModarchiveAPI.logPrefix = getLoggerStringForHost( m_ModarchiveAPI.hostName, 443);

	m_NTPServerIP  = getIPForHost( NTPServer, success );

	if (strcmp(netSktpHostName,"") != 0)
	{
		int port = netSktpHostPort != 0 ? netSktpHostPort: HTTP_PORT;
		m_SKTPServer.hostName = netSktpHostName;
		m_SKTPServer.port = port;
		m_SKTPServer.attempts = 0;
		m_SKTPServer.valid = false;
		//m_SKTPServer.ipAddress = getIPForHost( netSktpHostName, success );
		m_SKTPServer.logPrefix = getLoggerStringForHost( netSktpHostName, port);
		
		m_wicStandardHTTPTarget.hostName = netSktpHostName;
		m_wicStandardHTTPTarget.port = port;
		m_wicStandardHTTPTarget.attempts = 0;
		m_wicStandardHTTPTarget.valid = true;
		m_wicStandardHTTPTarget.ipAddress = getIPForHost( netSktpHostName, success );
		m_wicStandardHTTPTarget.logPrefix = getLoggerStringForHost( netSktpHostName, port);
	}
	else
	{
		m_SKTPServer.hostName = "";
		m_SKTPServer.port = 0;
		m_SKTPServer.valid = false;
	}
	
	if ( netEnableWebserver ){
		EnableWebserver();
	}

	m_isMenuScreenUpdateNeeded = true;
	clearErrorMsg(); //on c64screen, kernel menu
	return true;
}

void CSidekickNet::usbPnPUpdate()
{
	boolean bUpdated = m_USBHCI->UpdatePlugAndPlay();
	if ( bUpdated )
	{
		if ( m_pUSBSerial == 0 )
		{
			m_pUSBSerial = (CUSBSerialFT231XDevice *) m_DeviceNameService->GetDevice ("utty1", FALSE);
			if (m_pUSBSerial == 0)
				m_pUSBSerial = (CUSBSerialPL2303Device *) m_DeviceNameService->GetDevice ("utty1", FALSE);
				if (m_pUSBSerial == 0)
					m_pUSBSerial = (CUSBSerialCH341Device *) m_DeviceNameService->GetDevice ("utty1", FALSE);
			if (m_pUSBSerial != 0)
			{
				logger->Write( "CSidekickNet::Initialize", LogNotice, 
					"USB TTY device detected."
				);
				if (m_modemEmuType == 0){
					m_modemEmuType = SK_MODEM_USERPORT_USB;
					setModemEmuBaudrate(m_baudRate);
				}
			}
		}
		if ( m_pUSBMidi == 0 )
		{
			m_pUSBMidi = (CUSBMIDIDevice *) m_DeviceNameService->GetDevice ("umidi1", FALSE);
			if (m_pUSBMidi != 0)
			{
				logger->Write( "CSidekickNet::Initialize", LogNotice, 
					"USB Midi device detected."
				);
			}
		}
	}
}

void CSidekickNet::EnableWebserver(){
	if (m_loglevel > 1)
		logger->Write ("CSidekickNet::Initialize", LogNotice, "Starting webserver.");
	m_WebServer = new CWebServer (m_Net, 80, KERNEL_MAX_SIZE + 2000, 0, this);
}

boolean CSidekickNet::isWebserverRunning(){
	return (netEnableWebserver && m_WebServer != 0);
}

CString CSidekickNet::getBaudrate(){
	CString Number;
	Number.Format ("%02d", m_baudRate);
	return Number;
}

CString CSidekickNet::getPRGLaunchTweakValueAsString(){
	CString Number;
	Number.Format ("%02d", m_PRGLaunchTweakValue);
	return Number;
}

u8 CSidekickNet::getPRGLaunchTweakValue(){
	return m_PRGLaunchTweakValue;
}

void CSidekickNet::increasePRGLaunchTweakValue(){
	if ( ++m_PRGLaunchTweakValue > 7 )
	  m_PRGLaunchTweakValue = 0;
}

CString CSidekickNet::getLoggerStringForHost( CString hostname, int port){
	CString s = "http";
	s.Append( (port == 443) ? "s://" : "://" );
	s.Append(	hostname );
	if ( port != 443 && port != 80 )
	{
		CString Number;
		Number.Format ("%02d", port);
		s.Append(":");
		s.Append(Number);
	}
	s.Append("\%s");
	if (m_loglevel > 2)
		logger->Write ("getLoggerStringForHost", LogNotice, s, "/dummyPath");
	
	return s;	
}

boolean CSidekickNet::mountSDDrive()
{
	if ( m_isFSMounted )
	{
		if (m_loglevel > 1)
			logger->Write ("CSidekickNet::mountSDDrive", LogError,
					"Drive %s is already mounted. Skip remount.", DRIVE);
		return true;
	}
	if (f_mount (&m_FileSystem, DRIVE, 1) != FR_OK)
	{
		if (m_loglevel > 1)
			logger->Write ("CSidekickNet::Initialize", LogError,
					"Cannot mount drive: %s", DRIVE);
		m_isFSMounted = false;
	}
	else
		m_isFSMounted = true;
	return m_isFSMounted;
}

boolean CSidekickNet::unmountSDDrive()
{
	if (f_mount ( 0, DRIVE, 0) != FR_OK)
	{
		if (m_loglevel > 1)
			logger->Write ("CSidekickNet::unmountSDDrive", LogError,
					"Cannot unmount drive: %s", DRIVE);
		return false;
	}
	m_isFSMounted = false;
	return true;
}		

boolean CSidekickNet::RaspiHasOnlyWLAN()
{
	return (m_PiModel == MachineModel3APlus || m_PiModel == MachineModelZero2W);
}

void CSidekickNet::checkForSupportedPiModel()
{
	if ( m_PiModel != MachineModel3APlus && 
		m_PiModel != MachineModel3BPlus && 
		m_PiModel != MachineModelZero2W &&
		m_PiModel != MachineModel4B
	)
	{
		if (m_loglevel > 1)
			logger->Write( "CSidekickNet::Initialize", LogWarning, 
				"Warning: The model of Raspberry Pi you are using is not a model supported by Sidekick64/264!"
			);
	}
	if ( RaspiHasOnlyWLAN() && !m_useWLAN )
	{
		if (m_loglevel > 1)
			logger->Write( "CSidekickNet::Initialize", LogNotice, 
				"Your Raspberry Pi model (3A+ or Zero 2 W) doesn't have an ethernet socket. This kernel is built for cable based network. Use WLAN kernel instead."
			);
	}
}

boolean CSidekickNet::isReturnToMenuRequired(){
	boolean result =  m_isRebootRequested || m_isReturnToMenuRequested;
	m_isReturnToMenuRequested = false;
	return result;
}

boolean CSidekickNet::isRebootRequested(){
	if (!m_isRebootRequested)
		return false;
	unsigned waitDuration = 3;
	signed secondsLeft = waitDuration - (m_pTimer->GetUptime() - m_timeoutCounterStart);
	if ( secondsLeft < 0 ) secondsLeft = 0;
	if ( secondsLeft <= 0){
		//unmountSDDrive(); //TODO: check if this needs FIQ to be off!
		return true;
	}
	else
	{
		if ( m_oldSecondsLeft != secondsLeft ){
			CString msg = "  Please wait, rebooting Sidekick in ";
			CString Number;
			Number.Format ("%01d", secondsLeft-2 > 0 ? secondsLeft-2 : 0 );
			msg.Append( Number );
			msg.Append("  ");
			const char * tmp = msg;
			setErrorMsgC64( (char*) tmp );
			m_isMenuScreenUpdateNeeded = true;// to update the waiting message instantly
			m_oldSecondsLeft = secondsLeft;
		}
		return false;
	}
}

void CSidekickNet::requestReboot(){
	m_timeoutCounterStart = m_pTimer->GetUptime();
	m_isRebootRequested = true;
	//setErrorMsgC64( (char*)"   Please wait, rebooting Sidekick...   " );
}


boolean CSidekickNet::disableActiveNetwork(){
	//FIXME THIS DOES NOT WORK
	m_isActive = false;
	m_isUSBPrepared = false;
	m_isPrepared = false;
	/*
	logger->Write( "CSidekickNet", LogNotice, 
		"disableActiveNetwork: Now deleting instance of CTLSSimpleSupport."
	);
	logger->Write ("CSidekickNet", LogNotice, getSysMonInfo(1));
	delete m_TLSSupport;
	m_TLSSupport = 0;
	
	/*
	logger->Write( "CSidekickNet", LogNotice, 
		"disableActiveNetwork: Now deleting instance of CDNSClient."
	);
	logger->Write ("CSidekickNet", LogNotice, getSysMonInfo(1));
	
	delete m_DNSClient;
	m_DNSClient = 0;

	logger->Write( "CSidekickNet", LogNotice, 
		"disableActiveNetwork: Now deleting instance of CNetSubSystem."
	);
	logger->Write ("CSidekickNet", LogNotice, getSysMonInfo(1));
	delete m_Net;
	m_Net = 0;
	
	logger->Write( "CSidekickNet", LogNotice, 
		"disableActiveNetwork: Now deleting instance of CUSBHCIDevice."
	);
	logger->Write ("CSidekickNet", LogNotice, getSysMonInfo(1));
	delete m_USBHCI;
	//m_USBHCI = 0;
*/
	m_isActive = false;
	m_isUSBPrepared = false;
	m_isPrepared = false;
	/*
	logger->Write( "CSidekickNet", LogNotice, 
		"disableActiveNetwork: Setting to null."
	);
	
	m_TLSSupport = 0;
	m_DNSClient = 0;
	m_Net = 0;
	m_USBHCI = 0;
	*/
	logger->Write( "CSidekickNet", LogNotice, 
		"disableActiveNetwork: EOM"
	);
	return false;
}

CUSBHCIDevice * CSidekickNet::getInitializedUSBHCIDevice()
{
		if ( !m_isUSBPrepared)
		{
			if ( !initializeUSBHCIDevice() )
				return 0;
		}
		return m_USBHCI;
}

boolean CSidekickNet::initializeUSBHCIDevice()
{
	if ( !m_isUSBPrepared)
	{
		m_USBHCI = new CUSBHCIDevice (m_pInterrupt, m_pTimer, TRUE);
		if (!m_USBHCI->Initialize ())
		{
			logger->Write( "CSidekickNet", LogNotice, 
				"Couldn't initialize instance of CUSBHCIDevice."
			);
			setErrorMsgC64((char*)"Error on USB init. Sorry.");
			return false;
		}
		m_isUSBPrepared = true;
	}
	return true;
}

boolean CSidekickNet::Prepare()
{
	if ( RaspiHasOnlyWLAN() && !m_useWLAN )
	{
		logger->Write( "CSidekickNet::Initialize", LogNotice, 
			"Your Raspberry Pi model (3A+/Zero2W) doesn't have an ethernet socket. Skipping init of CNetSubSystem."
		);
		//                 "012345678901234567890123456789012345XXXX"
		setErrorMsgC64((char*)" No WLAN support in this kernel. Sorry. ");
		return false;
	}

	if ( !initializeUSBHCIDevice())
	{
		setErrorMsgC64((char*)"Error on USB init. Sorry.");
		return false;
	}
	
	if ( !m_isFSMounted && !mountSDDrive())
	{
		setErrorMsgC64((char*)"Can't mount SD card. Sorry.");
		return false;
	}	
	#ifndef WITHOUT_STDLIB
	//CGlueStdioInit (m_FileSystem);
	#endif
	
	if (m_useWLAN)
	{
	#ifdef WITH_WLAN
		m_WLAN = new CBcm4343Device (FIRMWARE_PATH);
		if (!m_WLAN->Initialize ())
		{
			logger->Write( "CSidekickNet::Initialize", LogNotice, 
				"Couldn't initialize instance of WLAN."
			);
			return false;
		}
	#endif
	}
	if ( strcmp( netSidekickHostname, "") == 0 )
		strncpy( netSidekickHostname, "sidekick64", 255 );
	m_Net = new CNetSubSystem (0, 0, 0, 0, netSidekickHostname, m_useWLAN ? NetDeviceTypeWLAN : NetDeviceTypeEthernet );
	if (!m_Net->Initialize (false))
	{
		logger->Write( "CSidekickNet::Initialize", LogNotice, 
			"Couldn't initialize instance of CNetSubSystem."
		);
		setErrorMsgC64((char*)"Can't initialize CNetSubSystem.");
		return false;
	}
	if (m_useWLAN)
	{
	#ifdef WITH_WLAN
		m_WPASupplicant = new CWPASupplicant (CONFIG_FILE);
		if (!m_WPASupplicant->Initialize ())
		{
			logger->Write( "CSidekickNet::Initialize", LogNotice, 
				"Couldn't initialize instance of CWPASupplicant."
			);
			setErrorMsgC64((char*)"Can't initialize WLAN/WPA. Sorry.");
			return false;
		}
		#endif
	}
	return true;
}

boolean CSidekickNet::IsRunning()
{
	 return m_isActive; 
}

boolean CSidekickNet::IsStillRunning()
{
	if (m_isActive && !m_Net->IsRunning())
	{
		logger->Write( "CSidekickNet::IsRunning", LogNotice, 
		 "Error: Network became inactive!"
		);
	} 
	return m_isActive; 
 }


boolean CSidekickNet::IsConnecting ()
{
	 return m_isNetworkInitQueued; 
}

void CSidekickNet::queueNetworkInit()
{ 
	m_isNetworkInitQueued = true;
	//                                 "012345678901234567890123456789012345XXXX"
	m_networkActionStatusMsg = (char*) "    Trying to connect. Please wait.     ";
	m_queueDelay = 1;
}

/*
void CSidekickNet::queueKernelUpdate()
{ 
	m_isKernelUpdateQueued = true; 
	//                                 "012345678901234567890123456789012345XXXX"
	m_networkActionStatusMsg = (char*) "  Trying to update kernel. Please wait. ";
	m_queueDelay = 1;
}*/

//this is for kernel render
void CSidekickNet::queueFrameRequest()
{
 	m_isFrameQueued = true;
	m_queueDelay = 0;
}

void CSidekickNet::queueSktpKeypress( int key )
{
	if ( !isAnyNetworkActionQueued())
		m_queueDelay = 0; //this could lead to disturbing an already running queuedelay
	m_isSktpKeypressQueued = true;
	m_sktpKey = key;
	//logger->Write ("CSidekickNet::queueSktpKeypress", LogNotice, "Queuing keypress");
}

bool CSidekickNet::isSKTPRefreshWaiting()
{
	return m_sktpRefreshWaiting;
}

void CSidekickNet::cancelSKTPRefresh()
{
	m_sktpRefreshWaiting = false;
}


void CSidekickNet::setSktpRefreshTimeout( unsigned timeout )
{
	//logger->Write ("CSidekickNet::setSktpRefreshTimeout", LogNotice, "%i", timeout);
	m_sktpRefreshWaiting = true;
	m_skipSktpRefresh = 0;
	m_sktpRefreshTimeout = timeout;
}

void CSidekickNet::queuedSktpRefreshAllowed()
{
	//refesh when user didn't press a key
	//this has to be quick for multiplayer games (value 4)
	//and can be slow for csdb browsing (value 16)
	if ( m_sktpRefreshWaiting && ++m_skipSktpRefresh >= m_sktpRefreshTimeout )
//	if ( m_sktpRefreshWaiting && ++m_skipSktpRefresh >= m_sktpRefreshTimeout && !isAnyNetworkActionQueued())
	{
//		logger->Write ("CSidekickNet::queuedSktpRefreshAllowed", LogNotice, " - is allowed!");
		m_sktpRefreshWaiting = false;
		m_skipSktpRefresh = 0;
		m_sktpRefreshTimeout = 0;
		m_sktpKey = 0;
		updateSktpScreenContent();
		m_isMenuScreenUpdateNeeded = true;
	}
}

char * CSidekickNet::getCSDBDownloadFilename(){
	return (char *) m_CSDBDownloadFilename;
}

boolean CSidekickNet::isLibOpenMPTFileType(char * x){
	return ( 
		strcmp(x,"mod") == 0 ||
		strcmp(x,"xm") == 0 ||
		strcmp(x,"it") == 0 ||
		strcmp(x,"pp") == 0 ||
		strcmp(x,"ym") == 0 ||
		strcmp(x,"med") == 0 ||
		strcmp(x,"sfx") == 0 ||
		strcmp(x,"s3m") == 0 ||
		strcmp(x,"hvl") == 0 ||
		strcmp(x,"dbm") == 0 ||
		strcmp(x,"ptm") == 0 ||
		strcmp(x,"mptm") == 0 ||
		strcmp(x,"wav") == 0
	);
}

u8 CSidekickNet::getCSDBDownloadLaunchType(){
	u8 type = 0;
	if ( isLibOpenMPTFileType(m_CSDBDownloadExtension) )	
	{
		type = 42;
#ifndef IS264		
		if (netModPlayOutputHDMI) type ++;
#endif
	}
	else if ( strcmp( m_CSDBDownloadExtension, "crt") == 0 )
	{
		type = 99;
		if (m_loglevel > 2)
			logger->Write ("CSidekickNet::getCSDBDownloadLaunchType", LogNotice, "CRT detected: >%s<",m_CSDBDownloadExtension);
	}
	else if ( strcmp( m_CSDBDownloadExtension, "d64" ) == 0)
	{
#ifndef IS264		
		//with the new d2ef approach available within Sidekick we try to launch the D64
		//as an dynamically created EF crt!
		type = 99;
		extern int createD2EF( unsigned char *diskimage, int imageSize, unsigned char *cart, int build, int mode, int autostart );
		unsigned char *cart = new unsigned char[ 1024 * 1027 ];
		u32 crtSize = createD2EF( prgDataLaunch, prgSizeLaunch, cart, 2, 0, true );
		memcpy( &prgDataLaunch[0], cart, crtSize );
		prgSizeLaunch = crtSize;
		//this is only an irrelevant dummy name ending wih crt that will be used to try to load a tga file
		memcpy(m_CSDBDownloadFilename, "SD:C64/temp.crt", 15); 
		m_CSDBDownloadFilename[15] = '\0';
#else
		type = 0; //unused, we only save the file (on Sidekick 264)
#endif
		if (m_loglevel > 2)
			logger->Write ("CSidekickNet::getCSDBDownloadLaunchType", LogNotice, "D64 detected: >%s<",m_CSDBDownloadExtension);
	}
	else if ( strcmp( m_CSDBDownloadExtension, "sid" ) == 0)
	{
#ifndef IS264
		//FIXME: This was just copied from c64screen.cpp. Put this into a method.
		Psid64 *psid64 = new Psid64();

		psid64->setVerbose(false);
		psid64->setUseGlobalComment(false);
		psid64->setBlankScreen(false);
		psid64->setNoDriver(false);

		if ( !psid64->load( prgDataLaunch, prgSizeLaunch ) )
		{
			//return false;
		}
		//logger->Write( "exec", LogNotice, "psid loaded" );

		// convert the PSID file
		if ( !psid64->convert() ) 
		{
			//return false;
		}
		//logger->Write( "exec", LogNotice, "psid converted, prg size %d", psid64->m_programSize );

		memcpy( &prgDataLaunch[0], psid64->m_programData, psid64->m_programSize );
		prgSizeLaunch = psid64->m_programSize;

		delete psid64;
		//FIXME: End of copy
		type = 41;
#else
		type = 0; //unused, we only save the file (on Sidekick 264)
#endif

		if (m_loglevel > 2)
			logger->Write ("CSidekickNet::getCSDBDownloadLaunchType", LogNotice, "SID detected: >%s<",m_CSDBDownloadExtension);
	}
	else if ( strcmp( m_CSDBDownloadExtension, "bin" ) == 0 && m_isC128)
	{
		type = 95; //C128 U36 ROM in memory
		if (m_loglevel > 2)
			logger->Write ("CSidekickNet::getCSDBDownloadLaunchType", LogNotice, "BIN detected: >%s<",m_CSDBDownloadExtension);
	}
	else //prg
	{
		type = 40;
		if (m_loglevel > 2)
			logger->Write ("CSidekickNet::getCSDBDownloadLaunchType", LogNotice, "PRG detected: >%s<",m_CSDBDownloadExtension);
	}
	return type;
}

boolean CSidekickNet::isUsbUserportModemConnected(){
		return m_pUSBSerial != 0;
}

void CSidekickNet::handleQueuedNetworkAction()
{
	//boolean isRunning = IsStillRunning();
	boolean isRunning = IsRunning();
	//logger->Write( "handleQueuedNetworkAction", LogNotice, "Yield");
//	if ( isRunning )
//		m_pScheduler->Yield (); // this is needed for webserver and wlan keep-alive
	
	if ( isRunning && (!isAnyNetworkActionQueued() || !usesWLAN()) )
	{
		if ( m_WebServer == 0 && netEnableWebserver ){
			EnableWebserver();
		}

		handleModemEmulation( false );
	
		unsigned repeats = 3; //lan, test time update in system information
		if ( usesWLAN()){
			repeats = 100;
			if ( m_isBBSSocketConnected) repeats = 25;
		} 
//	else if ( m_isBBSSocketConnected) repeats = 50;

		for (unsigned z=0; z < repeats; z++)
		{
			//in case there is something incoming from the webserver while we are in the loop -> break!
			if (m_isDownloadReadyForLaunch)
			{
				//Caution: Having this log entry here seems to be very important for timing!!!! :)
				//also try to sleep here m_pScheduler->MsSleep(100);

				logger->Write ("CSidekickNet", LogNotice, "Early-exit multi-yield...");
				break;
			}
			m_pScheduler->Yield ();
		}
		
		//only do cache stuff when in menu kernel
		//doing this in launcher kernel ruins the running prg
		//maybe we can check here ich sktp browser is active too?
		if ( usesWLAN() && 
				!RaspiHasOnlyWLAN() && 
				!m_isC128 && 
				!isSKTPScreenActive() && 
				strcmp( m_currentKernelRunning, "m" ) == 0 && 
				( strcmp( m_CSDBDownloadExtension, "d64" ) != 0)
		)
			requireCacheWellnessTreatment();
	}
	
	if (m_queueDelay > 0 )
	{
		m_queueDelay--;
		//logger->Write( "handleQueuedNetworkAction", LogNotice, "m_queueDelay: %i", m_queueDelay);		
		return;
	}

	if ( m_isNetworkInitQueued && !isRunning )
	{
		assert (!m_isActive);
		if (Initialize())
		{
			unsigned tries = 0;
			while (!UpdateTime() && tries < 3){ tries++;};
		}
		m_isNetworkInitQueued = false;
		return;
	}
	else if (isRunning)
	{

		if (m_isCSDBDownloadQueued)
		{
			if (m_loglevel > 2){
				logger->Write( "handleQueuedNetworkAction", LogNotice, "m_CSDBDownloadPath: %s", m_CSDBDownloadPath);
			}
			m_isCSDBDownloadQueued = false;
			getCSDBBinaryContent();
			//m_isSktpKeypressQueued = false;
		}
		/*
	
		else if (m_isCSDBDownloadSavingQueued)
		{
			if (m_loglevel > 2)
				logger->Write( "handleQueuedNetworkAction", LogNotice, "sCSDBDownloadSavingQueued");		
			m_isCSDBDownloadSavingQueued = false;
			saveDownload2SD();
			m_isSktpKeypressQueued = false;
		}
*/		
		//handle keypress anyway even if we have downloaded or saved something
		else if (m_isSktpKeypressQueued)
		{
			updateSktpScreenContent();
			m_isSktpKeypressQueued = false;
		}
		else if (m_sktpRefreshWaiting)
		{
			if( m_sktpKey == 0)
				queuedSktpRefreshAllowed();
			else{
				updateSktpScreenContent();
				m_isSktpKeypressQueued = false;
			}
		}
	}
}

boolean CSidekickNet::checkForSaveableDownload(){
	if (m_isCSDBDownloadSavingQueued)
	{
		{
			if (m_loglevel > 0)
				logger->Write( "checkForSaveableDownload", LogNotice, "sCSDBDownloadSavingQueued");		
			m_isCSDBDownloadSavingQueued = false;
			saveDownload2SD();
			m_isSktpKeypressQueued = false;
			return true;
		}
	}
	return false;
}

boolean CSidekickNet::isAnyNetworkActionQueued()
{
	return
			m_isNetworkInitQueued || 
			m_isFrameQueued || 
			m_isSktpKeypressQueued || 
			m_isCSDBDownloadQueued || 
			m_isCSDBDownloadSavingQueued;
}

void CSidekickNet::saveDownload2SD()
{
	m_isCSDBDownloadSavingQueued = false;
	if (m_loglevel > 2)
	{
		CString downloadLogMsg = "Writing download to SD card, bytes to write: ";
		CString Number;
		Number.Format ("%02d", prgSizeLaunch);
		downloadLogMsg.Append( Number );
		downloadLogMsg.Append( " Bytes, path: '" );
		downloadLogMsg.Append( m_CSDBDownloadSavePath );
		downloadLogMsg.Append( "'" );
		logger->Write( "saveDownload2SD", LogNotice, downloadLogMsg);
	}
	requireCacheWellnessTreatment();
	writeFile( logger, DRIVE, m_CSDBDownloadSavePath, (u8*) prgDataLaunch, prgSizeLaunch );
	requireCacheWellnessTreatment();
	resetF7BrowserState();
	requireCacheWellnessTreatment();
	logger->Write( "saveDownload2SD", LogNotice, "Finished writing.");
	if (m_doLaunchAfterSave){
		m_isDownloadReadyForLaunch = true;
		m_doLaunchAfterSave = false;
	}
	else if (strcmp( m_currentKernelRunning, "m" ) == 0 ){
		redrawSktpScreen();
		setSktpRefreshTimeout(5);
		cleanupDownloadData();
		//logger->Write( "saveDownload2SD", LogNotice, "clear Error + update needed + redraw");
	}
}

void CSidekickNet::prepareLaunchOfUpload( char * ext, char * filename, u8 mode, char * msgtemp2 ){
	//mode 0 = launch, 1 = save, 2= launch & save
	
	char msgtemp[255] = "Now ";
	
	if (mode == 1){
		m_isCSDBDownloadSavingQueued = true;
		strcat( msgtemp, "saving ");
	}
	else if (mode == 0){
		m_isDownloadReadyForLaunch = true;
		m_isCSDBDownloadSavingQueued = false;
		strcat( msgtemp, "launching ");
	}
	else if (mode == 2){
		m_isCSDBDownloadSavingQueued = true;
		m_doLaunchAfterSave = true;
		strcat( msgtemp, "launching and saving ");
	}
	else{
		logger->Write( "prepareLaunchOfUpload", LogError, "Illegal value for mode (%u).", mode);
	}

	strcat( msgtemp, filename);
	
	memcpy(m_CSDBDownloadExtension,ext,3);
	m_CSDBDownloadExtension[3]='\0';
	memcpy(m_CSDBDownloadFilename, filename, strlen(filename));
	m_CSDBDownloadFilename[strlen(filename)]='\0';
	if ( m_isCSDBDownloadSavingQueued ){
		setSavePath("!upload");
		strcat( msgtemp, " to ");
		strcat( msgtemp, m_CSDBDownloadSavePath);
	}
	strcat( msgtemp, ".\0");
	logger->Write( "prepareLaunchOfUpload", LogNotice, "Message: '%s'.", msgtemp);
	sprintf(msgtemp2, msgtemp);

	//if already in launcher kernel (l), leave it
	if ( strcmp( m_currentKernelRunning, "l" ) == 0 && (m_isDownloadReadyForLaunch || m_doLaunchAfterSave)){
		m_isReturnToMenuRequested = true;
		//logger->Write( "prepareLaunchOfUpload", LogNotice, "ReturnToMenuRequested.");
	}
	
}

void CSidekickNet::cleanupDownloadData()
{
  clearErrorMsg(); //on c64screen, kernel menu
  redrawSktpScreen();
	m_CSDBDownloadPath[0] = '\0';
	m_CSDBDownloadExtension[0] = '\0';
	// this is used from kernel_menu to display name on screen or to load a tga image
	m_CSDBDownloadFilename[0] = '\0';
	m_CSDBDownloadSavePath = (char *)"";
	m_bSaveCSDBDownload2SD = false;
	m_isDownloadReadyForLaunch = false;
	m_doLaunchAfterSave = false;
	requireCacheWellnessTreatment();
}

boolean CSidekickNet::isDownloadReadyForLaunch()
{
	return m_isDownloadReadyForLaunch;
}

boolean CSidekickNet::checkForFinishedDownload()
{
	boolean bTemp = m_isDownloadReady;
	if ( m_isDownloadReady){
		m_isDownloadReady = false;
		if ( strcmp( m_CSDBDownloadExtension, "tga" ) == 0)
		{
			//display image
			//m_isDownloadReadyForLaunch = true;
			//m_isCSDBDownloadSavingQueued = true;
			if ( screenType == 1 && strcmp( m_CSDBDownloadExtension, "tga" ) == 0)
				drawTGAImageOnTFT();
			
		}
		else if ( m_bSaveCSDBDownload2SD )
		{
			m_isDownloadReadyForLaunch = false;
			if (m_loglevel > 2)
				logger->Write( "isDownloadReady", LogNotice, "Download is ready and we want to save it.");
			m_isCSDBDownloadSavingQueued = true;
			//m_queueDelay = 5;
			if ( strcmp( m_CSDBDownloadExtension, "d64" ) == 0)
			{
				//                    "012345678901234567890123456789012345XXXX"
				setErrorMsgC64((char*)"             Saving D64 file            ", false);
//				setErrorMsgC64((char*)"     Saving and launching D64 file      ");
			}
			else if ( strcmp( m_CSDBDownloadExtension, "prg" ) == 0)
			{
				//                    "012345678901234567890123456789012345XXXX"
				setErrorMsgC64((char*)"             Saving PRG file            ", false);
//				setErrorMsgC64((char*)"     Saving and launching PRG file      ");
			}
			else if ( strcmp( m_CSDBDownloadExtension, "bin" ) == 0)
			{
				//                    "012345678901234567890123456789012345XXXX"
				setErrorMsgC64((char*)"             Saving BIN file            ", false);
//				setErrorMsgC64((char*)"     Saving and launching BIN file      ");
			}
			else if ( strcmp( m_CSDBDownloadExtension, "sid" ) == 0)
			{
				//                    "012345678901234567890123456789012345XXXX"
				setErrorMsgC64((char*)"             Saving SID file            ", false);
//				setErrorMsgC64((char*)"     Saving and launching SID file      ");
			}
			else if ( strcmp( m_CSDBDownloadExtension, "ym" ) == 0)
			{
				//                    "012345678901234567890123456789012345XXXX"
				setErrorMsgC64((char*)"             Saving YM file            ", false);
			}
			else if ( strcmp( m_CSDBDownloadExtension, "wav" ) == 0)
			{
				//                    "012345678901234567890123456789012345XXXX"
				setErrorMsgC64((char*)"             Saving WAV file            ", false);
			}
			else if ( isLibOpenMPTFileType(m_CSDBDownloadExtension) )
			{
				//                    "012345678901234567890123456789012345XXXX"
				setErrorMsgC64((char*)"             Saving MOD file            ", false);
			}
			else if ( strcmp( m_CSDBDownloadExtension, "crt" ) == 0)
			{
				//                    "012345678901234567890123456789012345XXXX"
				setErrorMsgC64((char*)"             Saving CRT file            ", false);
//				setErrorMsgC64((char*)"     Saving and launching CRT file      ");
				//m_queueDelay = 15;
			}
			requireCacheWellnessTreatment();
		}
		else{
			m_isDownloadReadyForLaunch = true;
			//                    "012345678901234567890123456789012345XXXX"
			setErrorMsgC64((char*)"       Launching without saving         ");
			requireCacheWellnessTreatment();
		}
	}
	return bTemp;
}

char * CSidekickNet::getNetworkActionStatusMessage()
{
	return m_networkActionStatusMsg;
}

CString CSidekickNet::getTimeString()
{
	//the most complicated and ugly way to get the data into the right form...
	CString *pTimeString = m_pTimer->GetTimeString();
	CString Buffer;
	if (pTimeString != 0)
	{
		Buffer.Append (*pTimeString);
	}
	delete pTimeString;
	return Buffer;
}

CString CSidekickNet::getUptime()
{
	unsigned uptime = m_pTimer->GetUptime();
	unsigned hours = 0, minutes = 0, seconds = 0;
	hours = uptime / 3600;
	minutes = (uptime % 3600) / 60;
	seconds = (uptime % 3600) % 60;
	CString strUptime = " ";
	CString Number;
	Number.Format ("%02d", hours);
	strUptime.Append( Number );
	strUptime.Append( "h ");
	Number.Format ("%02d", minutes);
	strUptime.Append( Number );
	strUptime.Append( "m ");
	Number.Format ("%02d", seconds);
	strUptime.Append( Number );
	strUptime.Append( "s");
	return strUptime;
}


CString CSidekickNet::getRaspiModelName()
{
	return m_pMachineInfo->Get()->GetMachineName();
}

CNetConfig * CSidekickNet::GetNetConfig(){
	assert (m_isActive);
	return m_Net->GetConfig();
}

CIPAddress CSidekickNet::getIPForHost( const char * host, bool & success )
{
	assert (m_isActive);
	success = false;
	unsigned attempts = 0;
	CIPAddress ip;
	while ( attempts < 3)
	{
		attempts++;
		if (!m_DNSClient->Resolve (host, &ip))
		{
			if (m_loglevel > 2)
				logger->Write ("getIPForHost", LogWarning, "Cannot resolve: %s",host);
		}
		else
		{
			CString IPString;
			ip.Format (&IPString);
			success = true;
			if (m_loglevel > 2)
				logger->Write ("getIPForHost", LogNotice, "Resolved %s as %s",host, (const char* ) IPString);
			break;
		}
	}
	return ip;
}

boolean CSidekickNet::UpdateTime(void)
{
	assert (m_isActive);
	CNTPClient NTPClient (m_Net);
	unsigned nTime = NTPClient.GetTime (m_NTPServerIP);
	if (nTime == 0)
	{
		if (m_loglevel > 2)
			logger->Write ("CSidekickNet::UpdateTime", LogWarning, "Cannot get time from %s",
						(const char *) NTPServer);

		return false;
	}

	if (CTimer::Get ()->SetTime (nTime, FALSE))
	{
		if (m_loglevel > 2)
			logger->Write ("CSidekickNet::UpdateTime", LogNotice, "System time updated");
		return true;
	}
	else
	{
		if (m_loglevel > 1)
			logger->Write ("CSidekickNet::UpdateTime", LogWarning, "Cannot update system time");
	}
	return false;
}

void CSidekickNet::drawTGAImageOnTFT(){
	//logger->Write( "drawTGAImageOnTFT", LogNotice, "now render tga image on display");		
	//requireCacheWellnessTreatment();
	m_isCSDBDownloadSavingQueued = false;
	m_isDownloadReady = false;
	extern unsigned char tempTGA[ 256 * 256 * 4 ];
	int w = 0, h = 0;
	tftParseTGA( tempTGA, prgDataLaunch, &w, &h, false, prgSizeLaunch );
	tftLoadBackgroundTGAMemory( tempTGA, 240, 240, false);
	tftCopyBackground2Framebuffer();
	tftInitImm();
	tftSendFramebuffer16BitImm( tftFrameBuffer );
	//buggy: tftSplashScreenMemory( (u8*) prgDataLaunch, prgSizeLaunch );
	cleanupDownloadData();
	//requireCacheWellnessTreatment();
}

void CSidekickNet::getCSDBBinaryContent( ){
	assert (m_isActive);
	u32 iFileLength = 0;
	if ( HTTPGet ( m_CSDBDownloadHost, (char *) m_CSDBDownloadPath, (char *) prgDataLaunch, iFileLength)){
		prgSizeLaunch = iFileLength;
		m_isDownloadReady = true;
		if (m_loglevel > 3)
			logger->Write( "getCSDBBinaryContent", LogNotice, "memcpy finished.");
		requireCacheWellnessTreatment();
	}
	else if (m_CSDBDownloadHost.port == 443)
		setErrorMsgC64((char*)"          HTTPS request failed          ", false);
		//                    "012345678901234567890123456789012345XXXX"
		
	else
		setErrorMsgC64((char*)"           HTTP request failed          ", false);
		//                    "012345678901234567890123456789012345XXXX"
	if (m_loglevel > 2)
		logger->Write( "getCSDBBinaryContent", LogNotice, "HTTPS Document length: %i", iFileLength);
}

void CSidekickNet::resetSktpSession(){
	m_sktpSession	= 0;
}

void CSidekickNet::redrawSktpScreen(){
	if (m_sktpSession == 1)
		m_sktpSession	= 2;
}

boolean CSidekickNet::isSktpRedrawNeeded(){
	return m_sktpSession == 2;
}


boolean CSidekickNet::isSktpSessionActive(){
	return m_sktpSession > 0;
}

boolean CSidekickNet::launchSktpSession(){
	char * pResponseBuffer = new char[33];	// +1 for 0-termination
	CString urlSuffix = "/sktp.php?session=new";
	if (strcmp(netSktpHostUser,"") != 0)
	{
		urlSuffix.Append("&username=");
		urlSuffix.Append(netSktpHostUser);
		if (strcmp(netSktpHostPassword,"") != 0)
		{
			urlSuffix.Append("&password=");
			urlSuffix.Append(netSktpHostPassword);
		}
	}
	urlSuffix.Append("&sktpv=5&type=");
	#ifndef IS264
	if (m_isC128)
		urlSuffix.Append("128");
	else
		urlSuffix.Append("64");
	#else
		urlSuffix.Append("264");
	#endif

	if (HTTPGet ( m_SKTPServer, urlSuffix, pResponseBuffer, m_sktpResponseLength))
	{
		if ( m_sktpResponseLength == 1 )
		{
			m_sktpScreenErrorCode = 6;
		}
		else if ( m_sktpResponseLength > 25 && m_sktpResponseLength < 34){
			m_sktpSessionID = pResponseBuffer;
			m_sktpSessionID[m_sktpResponseLength] = '\0';
			if (m_loglevel > 2)
				logger->Write( "launchSktpSession", LogNotice, "Got session id: %s", m_sktpSessionID);
		}
	}
	else{
		m_sktpScreenErrorCode = 5;
		if (m_loglevel > 1)
			logger->Write( "launchSktpSession", LogError, "Could not get session id.");
		m_sktpSessionID = (char*) "";
		return false;
	}
	return true;
}

CString CSidekickNet::getSktpPath( unsigned key )
{
	CString path = "/sktp.php?";
	CString Number; 
	Number.Format ("%02X", key);
	path.Append( "&key=" );
	path.Append( Number );

	path.Append( "&sessionid=" );
	path.Append( m_sktpSessionID );
	
	if ( m_sktpSession == 2) //redraw
	{
		m_sktpSession = 1;
		path.Append( "&redraw=1" );
		//logger->Write( "getSktpPath", LogNotice, "Enforce sktp page redraw.");
	}
	return path;
}

void CSidekickNet::enteringSktpScreen(){
	m_isSktpScreenActive = true;
	m_wasSktpScreenFreshlyEntered = true;
}

void CSidekickNet::leavingSktpScreen(){
	m_isSktpScreenActive = false;
	m_wasSktpScreenFreshlyEntered = false;
}

boolean CSidekickNet::isFirstSKTPScreen(){
	boolean tmp = m_wasSktpScreenFreshlyEntered;
	m_wasSktpScreenFreshlyEntered = false;
	return tmp;
}

boolean CSidekickNet::isSKTPScreenActive(){
	return m_isSktpScreenActive;
}

boolean CSidekickNet::isMenuScreenUpdateNeeded(){
	boolean tmp = m_isMenuScreenUpdateNeeded;
	m_isMenuScreenUpdateNeeded = false;
	return tmp;
}

unsigned CSidekickNet::getSKTPErrorCode(){
	return m_sktpScreenErrorCode;
}


boolean CSidekickNet::parseURL( remoteHTTPTarget & target, char * urlBuffer, u16 urlLength){

	char hostName[256];
	char protocolChecker[9] = "https://";
	u16 protocolType = 0, i = 0, j = 0;
	u8 limit = 0;
	
	if (urlLength < 9)
	{
		logger->Write( "parseURL", LogNotice, "Error: UrlLength is way too small");
		return false;
	}
	
	if (urlBuffer[4] == 's')
	{
		limit = 5; //for s in https
		protocolType = 443;
	}
	else if (urlBuffer[4] == ':')
	{
		limit = 4; //for p in http
		protocolType = 80; //could be another port in the string later on, but let's default to 80!
	}
	else
	{
		logger->Write( "parseURL", LogNotice, "Error: protocol detection step 1 failed (not http, not https)");
		return false;
	}
	
	if (limit > 0)
	{
		for (i = 0; i < 8; i ++)
		{
//			logger->Write( "parseSKTPDownloadCommand", LogNotice, "Comparison %i %i %c %c",urlBuffer[i], protocolChecker[i], urlBuffer[i], protocolChecker[i]  );
			#ifndef WITHOUT_STDLIB
			if (protocolChecker[i] != tolower(urlBuffer[j]))
			#else
			if (protocolChecker[i] != urlBuffer[j])
			#endif
			{
//				logger->Write( "parseURL", LogNotice, "Comparison stoped at i=%i j=%i ",i,j);
				limit = 0;
				protocolType = 0;
				break;
			}
			if (i == 3 && limit == 4) i++; //skip the s-check for http
			j++;
		}
	}
	if ( protocolType == 0)
	{
		logger->Write( "parseURL", LogNotice, "Error: wrong protocol (not http, not https)");
		return false;
	}
	
	char * pathBuffer = &	urlBuffer[limit+3];
	//logger->Write( "parseURL", LogNotice, "Pathbuffer is: %s, urlbuffer is: %s protocol is: %u", pathBuffer, urlBuffer, protocolType);

	for ( i = 0; i < urlLength; i++ ){
		//logger->Write( "parseURL", LogNotice, "hostnamebuild: char %c", pResponseBuffer[ i ] );
		if ( pathBuffer[ i ] == '/' || pathBuffer[ i ] == ':')
			break;
		hostName[ i ] = pathBuffer[ i ];
	}
	hostName[i] = '\0';
	//logger->Write( "parseURL", LogNotice, "Detected hostname: >%s<", hostName);

	if ( pathBuffer[ i ] == ':' ){
		while( pathBuffer[ i ] != '/')
		{
			//logger->Write( "parseURL", LogNotice, "skip port: char %c", pResponseBuffer[ i ] );
			i++;
		}
	}

	if (strcmp(hostName, m_CSDB.hostName) == 0){
		target = m_CSDB;
	}
	else if ( strcmp(hostName, m_CSDB_HVSC.hostName) == 0){
		target = m_CSDB_HVSC;
	}
	else if ( strcmp(hostName, m_SKTPServer.hostName) == 0){
		target = m_SKTPServer;
	}
	else if ( strcmp(hostName, m_ModarchiveAPI.hostName) == 0){
		//logger->Write( "parseURL", LogNotice, "MODARCHIVE: >%s<", hostName);
		target = m_ModarchiveAPI;
	}
	else{
		logger->Write( "parseURL", LogNotice, "Resolving unknown host: >%s<", hostName);
		target.hostName = hostName;
		target.port = protocolType;
		target.attempts = 0;
		target.valid = false;
		target.logPrefix = getLoggerStringForHost( hostName, protocolType);
	}
	//logger->Write( "parseURL", LogNotice, "hostname is ok, now memcopy");
	u16 pathLength = urlLength - i - limit -3;
	memcpy(target.urlPath , &pathBuffer[ i ], pathLength);
	target.urlPath[pathLength] = '\0';
	//logger->Write( "parseURL", LogNotice, "download path: >%s<", m_CSDBDownloadPath);
	target.port = protocolType;
	return true;

}

void CSidekickNet::parseSKTPDownloadCommand( char * pResponseBuffer, unsigned offset){
	u8 tmpUrlLength = pResponseBuffer[offset];
	u8 tmpFilenameLength = pResponseBuffer[offset+1];
	m_bSaveCSDBDownload2SD = ((int)pResponseBuffer[offset+2] == 1);

	char * urlBuffer = &pResponseBuffer[offset +3];
	boolean success = parseURL( m_CSDBDownloadHost, urlBuffer, tmpUrlLength);
	
	u8 pl =strlen(m_CSDBDownloadHost.urlPath);
	memcpy(m_CSDBDownloadPath, m_CSDBDownloadHost.urlPath, pl);
	m_CSDBDownloadPath[pl] = '\0';

	memcpy( m_CSDBDownloadFilename, &pResponseBuffer[ 3 + tmpUrlLength + offset ], tmpFilenameLength );
	m_CSDBDownloadFilename[tmpFilenameLength] = '\0';
	//logger->Write( "parseSKTPDownloadCommand", LogNotice, "filename: >%s<", m_CSDBDownloadFilename);
	//FIXME the extension grabbing only works if the download stuff is at the end of the response
	//this is currently the case, but never change this...
	u8 o = 0;
	if (m_CSDBDownloadFilename[ tmpFilenameLength -3 ] == '.') o = 1;
	memcpy( m_CSDBDownloadExtension, &m_CSDBDownloadFilename[ tmpFilenameLength -3 +o], 3-o);
	m_CSDBDownloadExtension[3-o] = '\0';
	#ifndef WITHOUT_STDLIB
	//workaround: tolower is only available with stdlib!
	//enforce lowercase for extension because we compare it a lot
	for(int i = 0; i < 3; i++){
		m_CSDBDownloadExtension[i] = tolower(m_CSDBDownloadExtension[i]);
	}
	#endif
	logger->Write( "parseSKTPDownloadCommand", LogNotice, "extension: >%s<", m_CSDBDownloadExtension);
}


void CSidekickNet::updateSktpScreenContent(){
	m_sktpScreenErrorCode = 0;
	
	if ( m_sktpSession < 1){
		if (!m_isActive || !m_isSktpScreenActive)
		{
			m_sktpScreenErrorCode = 3;
			return;
		}
		else if (strlen(m_SKTPServer.hostName) == 0 )
		{
			m_sktpScreenErrorCode = 1;
			return;
		}
		else if (m_SKTPServer.port == 0)
		{
			m_sktpScreenErrorCode = 4;
			return;
		}
		
		if (!launchSktpSession())
			return;
		m_sktpSession = 1;
	}

	//we need more than 233KB so that we can put in a tga image if we need to...
	char pResponseBuffer[1024 * 500 + 1]; //maybe turn this into member var when creating new sktp class?
	if (HTTPGet ( m_SKTPServer, getSktpPath( m_sktpKey ), pResponseBuffer, m_sktpResponseLength))
	{
		if ( m_sktpResponseLength > 0 )
		{
			//logger->Write( "updateSktpScreenContent", LogNotice, "HTTP Document m_sktpResponseLength: %i", m_sktpResponseLength);
			m_sktpResponseType = pResponseBuffer[0];
			//logger->Write( "updateSktpScreenContent", LogNotice, "response type : %i", m_sktpResponseType);
			if ( m_sktpResponseType == 2) // url for binary download, e. g. csdb
			{
				parseSKTPDownloadCommand(pResponseBuffer,1);
				m_sktpResponseLength = 1;
				m_sktpScreenContent = (unsigned char * ) pResponseBuffer;
				m_sktpScreenPosition = 1;
				m_isCSDBDownloadQueued = true;
				m_queueDelay = 0;
				if (m_bSaveCSDBDownload2SD)
					setSavePath("!downloads");
				else
					m_CSDBDownloadSavePath = (char *)"";
				
				//m_sktpResponseType = 1; //just to clear the screen
			}
			if ( m_sktpResponseType == 3) // no session or session expired
			{
				m_sktpScreenErrorCode = 7; //will not do anything
				logger->Write( "updateSktpScreenContent", LogNotice, "Notice: Session has expired, starting new SKTP session");
				m_sktpSession = 0;
				m_sktpKey = 0; //keypress is out of context with a new session
				updateSktpScreenContent();
				return;
			}
			else
			{
				m_sktpScreenContent = (unsigned char * ) pResponseBuffer;
				m_sktpScreenPosition = 1;
				//m_isMenuScreenUpdateNeeded = true;
			}
		}
		//logger->Write( "updateSktpScreenContent", LogNotice, "HTTP Document content: '%s'", m_sktpScreenContent);
		
	}
	else
	{
		m_sktpScreenErrorCode = 2;
	}

	#ifndef IS264
	//temporary workaround to reset the tft gfx when a menu is left
	//until this is implemented in sktp
	if ( m_sktpKey == 135 || m_sktpKey == 95)
		m_kMenu->SplashScreenTFT();
	#endif
	
	m_sktpKey = 0;
}


void CSidekickNet::setSavePath(char * subfolder)
{
	CString savePath;
	savePath = "SD:";
	if ( strcmp(m_CSDBDownloadExtension,"prg") == 0)
#ifndef IS264
		savePath.Append( (const char *) "PRG/" );
#else
		savePath.Append( (const char *) "PRG264/" );
#endif
	else if ( strcmp(m_CSDBDownloadExtension,"crt") == 0)
#ifndef IS264
		savePath.Append( (const char *) "CRT/" );
#else
		savePath.Append( (const char *) "CART264/" );
#endif
	else if ( strcmp(m_CSDBDownloadExtension,"d64") == 0)
#ifndef IS264
		savePath.Append( (const char *) "D64/" );
#else
		savePath.Append( (const char *) "D264/" );
#endif
	else if ( strcmp(m_CSDBDownloadExtension,"bin") == 0)
		savePath.Append( (const char *) "KERNAL/" );
	else if ( strcmp(m_CSDBDownloadExtension,"sid") == 0)
		savePath.Append( (const char *) "SID/" );
	else if ( isLibOpenMPTFileType(m_CSDBDownloadExtension))
		savePath.Append( (const char *) "MUSIC/" );
	savePath.Append(subfolder);
	
	f_mkdir(savePath);
	
	savePath.Append("/");
	savePath.Append(m_CSDBDownloadFilename);

	m_CSDBDownloadSavePath = savePath;
	//logger->Write( "setSavePath", LogNotice, "savePath: '%s'", savePath);
	
}

boolean CSidekickNet::IsSktpScreenContentEndReached()
{
	return m_sktpScreenPosition >= m_sktpResponseLength;
}

boolean CSidekickNet::IsSktpScreenToBeCleared()
{
	return m_sktpResponseType == 0;
}

boolean CSidekickNet::IsSktpScreenUnchanged()
{
	return m_sktpResponseType == 2; //2 is the http download
}

void CSidekickNet::ResetSktpScreenContentChunks()
{
	m_sktpScreenPosition = 1;
}


u8 CSidekickNet::GetSktpScreenContentChunkType()
{
	if ( m_sktpScreenPosition >= m_sktpResponseLength )
		return 255; //end reached
	return m_sktpScreenContent[ m_sktpScreenPosition ];
}

boolean CSidekickNet::getSKTPBorderBGColorCharset( u8 &borderColor, u8 &bgColor)
{
		borderColor = m_sktpScreenContent[ ++m_sktpScreenPosition ] -1;
		bgColor = m_sktpScreenContent[ ++m_sktpScreenPosition ] -1;
		boolean charLower = (m_sktpScreenContent[ ++m_sktpScreenPosition ] == 1);
		m_sktpScreenPosition++;
		return charLower;
}

void CSidekickNet::enableSktpRefreshTimeout(){
	//logger->Write( "enableSktpRefreshTimeout", LogNotice, "enabled");
	m_sktpScreenContent[m_sktpScreenPosition] = 88; //destroy refresh chunk in case it is parsed a second time
	u8 timeout = m_sktpScreenContent[ ++m_sktpScreenPosition ];
	setSktpRefreshTimeout( timeout < 1 ? 1 : timeout); //minimum value
	m_sktpScreenPosition++;
}

void CSidekickNet::prepareDownloadOfTGAImage(){
	if ( screenType == 1 ){
		parseSKTPDownloadCommand((char*)m_sktpScreenContent, m_sktpScreenPosition+1);
		m_isCSDBDownloadQueued = true;
		m_queueDelay = 4;
	}
	//m_sktpResponseType = 2; //suggest that the content is unchanged
	m_sktpScreenContent[m_sktpScreenPosition] = 88; //destroy tga chunk in case it is parsed a second time
	m_sktpScreenPosition = m_sktpResponseLength;
}

void CSidekickNet::updateTGAImageFromSKTPChunk()
{
	m_sktpScreenContent[m_sktpScreenPosition] = 88; //destroy tga chunk in case it is parsed a second time
	prgSizeLaunch = 
		m_sktpScreenContent[ ++m_sktpScreenPosition ] * 65536 +
		m_sktpScreenContent[ ++m_sktpScreenPosition ] * 256 +
		m_sktpScreenContent[ ++m_sktpScreenPosition ];
	
	if ( prgSizeLaunch > 1024 * 350)
	{
		logger->Write( "updateTGAImageFromSKTPChunk", LogNotice, "prgSizeLaunch too big: %u", prgSizeLaunch);
		return;
	}

	memcpy( prgDataLaunch, &m_sktpScreenContent[ m_sktpScreenPosition +1], prgSizeLaunch);
	prgDataLaunch[prgSizeLaunch+1] = '\0';
	extern unsigned char tempTGA[ 256 * 256 * 4 ];
	int w = 0, h = 0;
	tftParseTGA( tempTGA, prgDataLaunch, &w, &h, false, prgSizeLaunch );
	tftLoadBackgroundTGAMemory( tempTGA, 240, 240, false);
	tftCopyBackground2Framebuffer();
	tftInitImm();
	tftSendFramebuffer16BitImm( tftFrameBuffer );

//	m_sktpScreenPosition += prgSizeLaunch;
	m_sktpScreenPosition = m_sktpResponseLength;

}

unsigned char * CSidekickNet::GetSktpScreenContentChunk( u16 & startPos, u8 &color, boolean &inverse, u8 &repeat )
{
	repeat = 0;
	startPos = 0;
	color = 0;
	inverse = false;
	if ( m_sktpScreenPosition >= m_sktpResponseLength ){
		//logger->Write( "GetSktpScreenContentChunk", LogNotice, "End reached.");
		m_sktpScreenPosition = 1;
		return (unsigned char *) '\0';
	}
	u8 type = m_sktpScreenContent[ m_sktpScreenPosition ];
	u16 scrLength = 0;
	
	if (type < 3 || type >= 5)
	{
		scrLength = m_sktpScreenContent[ m_sktpScreenPosition + 1]; // this is only the lsb
		u16 byteLength= 0;
		u8 startPosL = m_sktpScreenContent[ m_sktpScreenPosition + 2 ];//screen pos x/y
		u8 startPosM = m_sktpScreenContent[ m_sktpScreenPosition + 3 ]&3;//screen pos x/y
		if (type < 3 || type == 6)
		{
			scrLength += ((m_sktpScreenContent[ m_sktpScreenPosition + 3 ]&16)+ 
									 (m_sktpScreenContent[ m_sktpScreenPosition + 3 ]&32)) *16; //msb bits
		}
		color        = m_sktpScreenContent[ m_sktpScreenPosition + 4 ];// this is gap in case 6
		if ( type != 6)
			color = color&127;//0-15 for c64, 0-127 for c264, this is gap in case 6
		inverse    = m_sktpScreenContent[ m_sktpScreenPosition + 4 ]>>7;//test bit 8
		startPos = startPosM * 256 + startPosL;//screen pos x/y
		
		//some plausibilty checks of the values
		if (scrLength > 1000) scrLength = 1000;
		if (startPos > 999) startPos = 999;
		if (startPos + scrLength > 1001) scrLength = 1001 - startPos;

		byteLength = scrLength;
		
		if ( type == 0 || type == 2)
		{
			memcpy( m_sktpScreenContentChunk, &m_sktpScreenContent[ m_sktpScreenPosition + 5], byteLength);
		}
		else if ( type == 1) //repeat one single character for scrLength times
		{
			byteLength = 1;
			char fillChar = m_sktpScreenContent[ m_sktpScreenPosition + 5 ];
			for (unsigned i = 0; i < scrLength; i++)
				m_sktpScreenContentChunk[i] = fillChar;
		}
		else if (type >= 5) //paintbrush
		{
			repeat = m_sktpScreenContent[ m_sktpScreenPosition + 5 ];
			memcpy( m_sktpScreenContentChunk, &m_sktpScreenContent[ m_sktpScreenPosition + 6], byteLength);
			byteLength = byteLength + 1;
		}	

		//logger->Write( "GetSktpScreenContentChunk", LogNotice, "Chunk parsed: length=%u, startPos=%u, color=%u ",scrLength, startPos, color);
		m_sktpScreenPosition += 5+byteLength;//begin of next chunk
	}
	m_sktpScreenContentChunk[scrLength] = '\0';
  return m_sktpScreenContentChunk;
}

boolean CSidekickNet::HTTPGet (remoteHTTPTarget & target, const char * path, char *pBuffer, unsigned & nLengthRead )
{
	assert (pBuffer != 0);
	unsigned nLength = nDocMaxSize;
	if (m_loglevel > 3)
		logger->Write( "HTTPGet", LogNotice, target.logPrefix, path );
	//check if we need to resolve the target
	if ( !target.valid)
	{
		if ( target.attempts <= 3)
		{
			bool success = false;
			target.ipAddress = getIPForHost( target.hostName, success );
			target.valid = success;
			target.attempts++;
			if (!success)
			{
				logger->Write( "HTTPGet", LogError, "Resolve of hostname '%s' failed - attempt %u", target.hostName, target.attempts);
				return false;
			}
		}
		else{
			logger->Write( "HTTPGet", LogError, "Resolve of hostname '%s' failed too many times", target.hostName);
			return false;
		}
	}
	
#ifdef WITH_TLS	
	CHTTPClient client( m_TLSSupport, target.ipAddress, target.port, target.hostName, target.port == 443 );
	CircleMbedTLS::THTTPStatus Status = client.Get (path, (u8 *) pBuffer, &nLength);
	if (Status != CircleMbedTLS::HTTPOK)
#else
	CHTTPClient client( m_Net, target.ipAddress, target.port, target.hostName );
	::THTTPStatus Status = client.Get (path, (u8 *) pBuffer, &nLength);
	if (Status != ::HTTPOK)
	
#endif	

	{
		if (m_loglevel > 0)
			logger->Write( "HTTPGet", LogError, "Failed with status %u, >%s<", Status, path);
		return false;
	}
	assert (nLength <= nDocMaxSize);
	pBuffer[nLength] = '\0';
	nLengthRead = nLength;
	return true;
}

void CSidekickNet::updateSystemMonitor( size_t freeSpace, unsigned CpuTemp)
{
	m_sysMonHeapFree = freeSpace;
	m_sysMonCPUTemp = CpuTemp;
}

CString CSidekickNet::getSysMonInfo( unsigned details )
{
	CString m_sysMonInfo = "";
	m_sysMonInfo.Append("CPU ");
	CString Number; 
	Number.Format ("%02d", m_sysMonCPUTemp);
	m_sysMonInfo.Append( Number );
	m_sysMonInfo.Append("'C, Uptime:");
	m_sysMonInfo.Append( getUptime() );
	
	if ( details > 0 )
	{
		m_sysMonInfo.Append(", ");
		CString Number2; 
		Number2.Format ("%02d", m_sysMonHeapFree/1024);
		m_sysMonInfo.Append( Number2 );
		m_sysMonInfo.Append(" kb free");
	}
	return m_sysMonInfo;
}

void CSidekickNet::requireCacheWellnessTreatment(){
	m_kMenu->doCacheWellnessTreatment();
}

/*
void CSidekickNet::getNetRAM( u8 * content, u32 * size){
	CString path = "/getNetRam.php";
	if (!HTTPGet ( m_SKTPServer, path, (char*) content, *size)){
		logger->Write( "getNetRAM", LogError, "Failed with path >%s<", path);
	}
}*/

void CSidekickNet::setCurrentKernel( char * r){
	m_currentKernelRunning = r;
}

void CSidekickNet::setC128Mode()
{
	m_isC128 = true;
}

void CSidekickNet::enterWebUploadMode(){
	
	m_modemEmuType = 0; //stop any active modem emu

	//if not in menu kernel (l), leave it
	
	if ( strcmp( m_currentKernelRunning, "m" ) == 0){
		prgSizeLaunch = sizeof WEBUPLOADPRG;
		memcpy( prgDataLaunch, WEBUPLOADPRG, prgSizeLaunch);
		memcpy(m_CSDBDownloadExtension,"prg",3);
		m_CSDBDownloadExtension[3]='\0';
		m_isDownloadReadyForLaunch = true;
	}

}

void CSidekickNet::iterateModemEmuBaudrate()
{
		switch(m_baudRate){
			case 300:
				m_baudRate = 1200;
				break;
			case 1200:
				m_baudRate = 2400;
				break;
			case 2400:
				m_baudRate = 4800;
				break;
			case 4800:
				m_baudRate = 9600;
				break;
			case 9600:
			default:
				m_baudRate = 300;
				break;
		if ( m_modemEmuType == SK_MODEM_USERPORT_USB )
		{
			m_pUSBSerial->SetBaudRate(m_baudRate);
		}
	}
}


void CSidekickNet::setModemEmuBaudrate( unsigned rate )
{
	if ( m_modemEmuType == SK_MODEM_USERPORT_USB )
	{
		m_pUSBSerial->SetBaudRate(rate);
		m_baudRate = rate;
	}
	else if ( m_modemEmuType == SK_MODEM_SWIFTLINK )
	{
		//...
	}
}

void CSidekickNet::cleanUpModemEmuSocket()
{
	logger->Write ("CSidekickNet", LogNotice, "cleanup modem socket connection");
	if ( m_isBBSSocketConnected )
	{
		m_isBBSSocketConnected = false;
		delete(m_pBBSSocket);
		m_pBBSSocket = 0;
		m_isBBSSocketFirstReceive = false;
		m_BBSSocketDisconnectPlusCount = 0;
	}
	m_modemCommandLength = 0;
	m_modemCommand[0] = '\0';
	m_socketHost[0] = '\0';

	m_modemInputBufferPos = 0;
	m_modemInputBufferLength = 0;
	m_modemInputBuffer[0] = '\0';

	m_modemOutputBufferPos = 0;
	m_modemOutputBufferLength = 0;
	m_modemOutputBuffer[0] = '\0';

}

int CSidekickNet::readCharFromFrontend( unsigned char * buffer)
{
		if ( m_modemEmuType == SK_MODEM_USERPORT_USB )
		{
			return m_pUSBSerial->Read(buffer, 1);
		}
		else if ( m_modemEmuType == SK_MODEM_SWIFTLINK || m_modemEmuType == SK_WIC64_EXP_EMULATION)
		{
			if (m_modemOutputBufferLength > 0)
			{
				if ( m_modemOutputBufferPos == m_modemOutputBufferLength)
				{
					m_modemOutputBufferLength = 0;
					m_modemOutputBufferPos = 0;
					//m_modemOutputBuffer = (char * ) "";
					//m_modemOutputBuffer[0] = '\0';
					logger->Write ("CSidekickNet", LogNotice, "readCharFromFrontend - resetting m_modemOutputBuffer to empty string");
				}
				else{
					buffer[0] = m_modemOutputBuffer[m_modemOutputBufferPos++];
					
//					logger->Write ("CSidekickNet", LogNotice, "readCharFromFrontend - got '%u' (%c)from m_modemOutputBuffer",buffer[0], (buffer[0] > 31 && buffer[0] <126) ? buffer[0]:126 );
					return 1;
				}
			}
		}
		buffer[0] = '0';
		return 0;
}

int CSidekickNet::writeCharsToFrontend( unsigned char * buffer, unsigned length)
{
	if ( m_modemEmuType == SK_MODEM_USERPORT_USB )
	{
		return m_pUSBSerial->Write(buffer, length);
	}
	else if ( m_modemEmuType == SK_MODEM_SWIFTLINK || m_modemEmuType == SK_WIC64_EXP_EMULATION )
	{
			if ( m_modemInputBufferLength > 0 && m_modemInputBufferPos == m_modemInputBufferLength )
			{
				m_modemInputBufferPos = 0;
				m_modemInputBufferLength = 0;
				m_modemInputBuffer[0] = '\0';
				logger->Write ("CSidekickNet", LogNotice, "writeCharsToFrontend - resetting m_modemInputBuffer to empty string");
			}
			//char * tmp;
			//memcpy( tmp, &buffer[0], length );
		  //strcat( m_modemInputBuffer, tmp );
			logger->Write ("CSidekickNet", LogNotice, "writeCharsToFrontend - adding %u to %u chars", length, m_modemInputBufferLength);
			
			//FIXME: add sanity length check
			for ( unsigned c = 0; c < length; c++)
			{
				m_modemInputBuffer[m_modemInputBufferLength] = buffer[c];
				m_modemInputBufferLength++;
			}
			m_modemInputBuffer[m_modemInputBufferLength]  = '\0';
			//strcat( m_modemInputBuffer, buffer );
			//m_modemInputBufferLength += length;
			return length;
	}
	buffer[0] = '0';
	return 0;
}

unsigned char CSidekickNet::getCharFromInputBuffer()
{
	if (m_modemInputBufferLength == 0 || m_modemInputBufferPos >= m_modemInputBufferLength ){
			return 0;
	}
	unsigned char payload = m_modemInputBuffer[m_modemInputBufferPos++];
	return payload;
}

bool CSidekickNet::areCharsInInputBuffer()
{
	return ( m_modemInputBufferLength > 0 && m_modemInputBufferPos < m_modemInputBufferLength );
}

bool CSidekickNet::areCharsInOutputBuffer()
{
	return ( m_modemOutputBufferLength > 0 && m_modemOutputBufferPos < m_modemOutputBufferLength );
}

void CSidekickNet::launchWiCCommand( u8 cmd, u8 cmdState ){
	int bsize = 4096;
	unsigned char inputChar[bsize];
	char * wicURLPath;
	logger->Write ("launchWiCCommand", LogNotice, "entering");
	int x = 0, i = 0;
	while ( readCharFromFrontend( inputChar ) == 1)
	{
		x++;
		inputChar[0] = (inputChar[0] > 31 && inputChar[0] <126) ? inputChar[0]:126;
		m_modemCommand[ m_modemCommandLength++ ] = inputChar[0];
		m_modemCommand[ m_modemCommandLength ] = '\0';
	}
	if (x==0)
		logger->Write ("launchWiCCommand", LogNotice, "error received command with ZERO chars from C64, command is %i, / state %i", cmd, cmdState);
	else if (x>0){
		logger->Write ("launchWiCCommand", LogNotice, "received command with %i chars from C64:'%s', command is %i, / state %i", x, m_modemCommand, cmd, cmdState);
		
		if (cmd == 1)
		{
			remoteHTTPTarget wicHTTPTarget;
			if (m_modemCommand[0] == '!') //shorthand command
			{
//				if ( !m_wicStandardHTTPTarget.valid)
//					m_wicStandardHTTPTarget = m_SKTPServer;
					
//				{
					wicHTTPTarget = m_wicStandardHTTPTarget;
					wicURLPath = wicHTTPTarget.urlPath;
					char * suffix = &m_modemCommand[1];
					//logger->Write ("launchWiCCommand", LogNotice, wicHTTPTarget.hostName, "hostName");
					logger->Write ("launchWiCCommand", LogNotice, "suffix: '%s' , path: %s", suffix, wicURLPath);
					strcat( wicURLPath, suffix);
					logger->Write ("launchWiCCommand", LogNotice, "processing wic command 1 with shorthand (!), urlPath is: '%s'", wicURLPath);
//				}
//				else
//					logger->Write ("launchWiCCommand", LogNotice, "ERROR skipped processing wic command 1 with shorthand (!) as there is no Default Server set");
			}
			else{
				logger->Write ("launchWiCCommand", LogNotice, "processing wic command 1 without shorthand (full url)");
				if (!parseURL( wicHTTPTarget, m_modemCommand, m_modemCommandLength))
				{
					logger->Write ("launchWiCCommand", LogNotice, "Error parsing URL, discarding command");
					cleanUpModemEmuSocket();
					return;
				}
			}
			wicURLPath = wicHTTPTarget.urlPath;
			char pResponseBuffer[1024 * 500 + 1]; //maybe turn this into member var when creating new sktp class?
			
/*			
			 "01234567890123456789012345"; //26
			m_sktpResponseLength = 26;
*/			
			if (HTTPGet ( wicHTTPTarget, wicURLPath, pResponseBuffer, m_sktpResponseLength))
			{
				if ( m_sktpResponseLength > 0 )
				{
					m_modemInputBufferPos = 0;
					m_modemInputBufferLength = 0;
					m_modemInputBuffer[0] = '\0';

					u8 lsb = (m_sktpResponseLength) % 256;
					u8 msb = (m_sktpResponseLength) / 256;
					logger->Write ("launchWiCCommand", LogNotice, "payload length %u msb: %u lsb: %u payload '%s'",m_sktpResponseLength,msb,lsb, pResponseBuffer  );
					if (m_sktpResponseLength == 1)
						logger->Write ("launchWiCCommand", LogNotice, "Disclosing payload: '%s' %i", pResponseBuffer[0],pResponseBuffer[0] );
					i= writeCharsToFrontend( (unsigned char *)65, 1); //dummy byte
					i= writeCharsToFrontend( (unsigned char *)msb, 1);
					i= writeCharsToFrontend( (unsigned char *)lsb, 1);
					i= writeCharsToFrontend( (unsigned char *)pResponseBuffer, m_sktpResponseLength);
					//i= writeCharsToFrontend( (unsigned char *)66, 1); //dummy byte
				}
			}
		}
		else if (cmd == 6) // get ip address
		{
			logger->Write ("launchWiCCommand", LogNotice, "processing wic command 6: get ip address");
			CString strHelper;
			unsigned char tmp[50];
			GetNetConfig()->GetIPAddress ()->Format (&strHelper);
			unsigned l = sprintf( (char* )tmp, strHelper );
			//logger->Write ("CSidekickNet", LogNotice, "get ip: sh:'%s', tmp:'%s'", strHelper, tmp );
			i= writeCharsToFrontend( (unsigned char *)65, 1); //dummy byte
			i= writeCharsToFrontend( (unsigned char *)0, 1);
			i= writeCharsToFrontend( (unsigned char *)l, 1);
			i = writeCharsToFrontend(tmp, l);
		}
		else if (cmd == 8) // set default server
		{
			//logger->Write ("launchWiCCommand", LogNotice, "before processing wic command 8: setting default url %s ", m_wicStandardHTTPTarget.urlPath);
			//logger->Write ("launchWiCCommand", LogNotice, m_wicStandardHTTPTarget.hostName, "hostName before");
			boolean success = parseURL( m_wicStandardHTTPTarget, m_modemCommand, m_modemCommandLength);
			logger->Write ("launchWiCCommand", LogNotice, "processing wic command 8: setting default url %s ", m_wicStandardHTTPTarget.urlPath);
			//logger->Write ("launchWiCCommand", LogNotice, m_wicStandardHTTPTarget.hostName, "hostName after");

		}
		else if (cmd == 9) // rem command
		{
			logger->Write ("launchWiCCommand", LogNotice, "processing wic command 9: rem command: '%s'",m_modemCommand);
		}
		else if (cmd == 10) // get connected wlan name
		{
			logger->Write ("launchWiCCommand", LogNotice, "processing wic command 10: get connected wlan name: '%s'",m_modemCommand);
			i= writeCharsToFrontend( (unsigned char *)65, 1); //dummy byte
			i= writeCharsToFrontend( (unsigned char *)0, 1);
			i= writeCharsToFrontend( (unsigned char *)12, 1);
			i= writeCharsToFrontend((unsigned char *)"sidekickwlan", 12);
		}
		else{
			logger->Write ("launchWiCCommand", LogNotice, "Ignoring unhandled wic command : %u", cmd);
		}
		m_modemCommandLength = 0;
		m_modemCommand[0] = '\0';
	}

}

void CSidekickNet::setWiCEmuWriteMode( bool mode  = false){
	m_WiCEmuIsWriteMode = mode;
}

void CSidekickNet::handleWiC64ExpEmulation( bool silent = false)
{
}

void CSidekickNet::handleModemEmulation( bool silent = false)
{
	//if ( !silent && m_modemEmuType == 0)
	//	usbPnPUpdate(); //still try to detect usb modem hot plugged

	if ( m_modemEmuType == SK_WIC64_EXP_EMULATION ){
		if (strcmp( m_currentKernelRunning, "m" ) == 0 && (m_modemInputBufferLength + m_modemOutputBufferLength + m_modemCommandLength) >0 )
			cleanUpModemEmuSocket();
		handleWiC64ExpEmulation(silent);
		return;
	}


	if (  m_modemEmuType == 0 ) return;

	if ( (strcmp( m_currentKernelRunning, "m" ) == 0) && m_isBBSSocketConnected){
		cleanUpModemEmuSocket();
		m_wicStandardHTTPTarget.urlPath[0] = '\0';

	}

	if ( m_modemEmuType == SK_MODEM_SWIFTLINK )
	{
		if ( silent && m_isBBSSocketConnected)
			return; // come back in none-silent case
		else if ( !silent && (m_socketPort > 0) && !m_isBBSSocketConnected)
		{
			SocketConnect( m_socketHost, m_socketPort, false);
			m_socketPort = 0; //prevent retry
			return;
		}
	}

	//buffers should be at least of size FRAME_BUFFER_SIZE (1600)
	int bsize = 4096;
	unsigned char buffer[bsize];
	bool success = false;
	
	if (!m_isBBSSocketConnected)
		handleModemEmulationCommandMode( silent );
	else{

		//buffers should be at least of size FRAME_BUFFER_SIZE (1600)
		unsigned char inputChar[bsize];
		bool noCarrier = false;

		int x = 0;
		int fromFrontend = readCharFromFrontend( inputChar );
		if ( fromFrontend > 0 )
		{
			
			if ( fromFrontend == 1 && inputChar[0] == '+')
				m_BBSSocketDisconnectPlusCount++;
			else
				m_BBSSocketDisconnectPlusCount=0;
			if ( m_BBSSocketDisconnectPlusCount >= 3)
			{
				logger->Write ("CSidekickNet", LogNotice, "+++ hanging up");
				int a = writeCharsToFrontend((unsigned char *)"hanging up\r", 11);
				cleanUpModemEmuSocket();
				return;
			}
			else
			{
				//if ( !silent)
					logger->Write ("CSidekickNet", LogNotice, "Terminal: sent %i chars to modem", fromFrontend);
//				m_pScheduler->Yield ();
				x = m_pBBSSocket->Send (inputChar, fromFrontend, 0); //MSG_DONTWAIT);
				if (x < 0 )
				{
					logger->Write ("CSidekickNet", LogNotice, "ERROR - error on socket send");
					noCarrier = true;
				}
				if ( m_modemEmuType == SK_MODEM_SWIFTLINK)
					m_isBBSSocketFirstReceive = true;
			}
		}

//m_isBBSSocketFirstReceive = false;
		
		//if ( m_modemEmuType == SK_MODEM_SWIFTLINK)
		//m_isBBSSocketFirstReceive = true;
				
		//logger->Write ("CSidekickNet", LogNotice, "Terminal: now checking if we can receive something");
		
		unsigned harvest = 0, attempts = 0; //dummy start value
		bool again = !noCarrier;
		m_pScheduler->Yield ();
		m_pScheduler->Yield ();
		m_pScheduler->Yield ();
		while (again)
		{
			attempts++;
			x = m_pBBSSocket->Receive ( buffer, bsize -2, m_isBBSSocketFirstReceive ? 0 : MSG_DONTWAIT);
			m_pScheduler->Yield ();
			m_pScheduler->Yield ();
			m_pScheduler->Yield ();
			if (x > 0)
			{
				int a = writeCharsToFrontend(buffer, x);
				logger->Write ("CSidekickNet", LogNotice, "Terminal: wrote %u chars to frontend", x);
				harvest += x;
			}
			else if (x < 0 )
			{
				logger->Write ("CSidekickNet", LogNotice, "ERROR - error on socket receive");
				noCarrier = true;
				break;
			}

			if ( attempts <= 5)
			{
				again = true;
			}
			
/*
			if ( m_modemEmuType == SK_MODEM_SWIFTLINK && attempts <= 10 ) // && !m_isBBSSocketFirstReceive)
			{
				again = true;
				//m_pScheduler->MsSleep(1);
				//m_pScheduler->Yield();
			}
			else if ( m_modemEmuType == SK_MODEM_USERPORT_USB && attempts <= 5)
			{
				again = true;
			}
			*/			
			else
			{
				again = false;
			}
			m_isBBSSocketFirstReceive = false;
			
		}
		if (harvest > 0) 
			logger->Write ("CSidekickNet", LogNotice, "Terminal: %u attempts. harvest %u", attempts, harvest);

		if ( noCarrier )
		{
			int a = writeCharsToFrontend((unsigned char *)"no carrier\r", 11);
			cleanUpModemEmuSocket();
		}
	}
}

void CSidekickNet::handleModemEmulationCommandMode( bool silent ){

	//buffers should be at least of size FRAME_BUFFER_SIZE (1600)
	int bsize = 4096;
	unsigned char inputChar[bsize];

	int a = readCharFromFrontend(inputChar);
	/*
	if ( a > 1 )
	{
		//this happens for example with the Quantum Link software
		logger->Write ("CSidekickNet", LogNotice, "Read from USB serial more than one: %u - %s", a, inputChar);
		m_pBBSSocket->Send (buffer, a, MSG_DONTWAIT);
	}*/
	if ( a == 1 )
	{
		if (inputChar[0] == 20)
		{
			a = writeCharsToFrontend(inputChar, 1);//echo
			//logger->Write ("CSidekickNet", LogNotice, "USB serial read char delete");
			if (m_modemCommandLength > 0)
				m_modemCommand[ --m_modemCommandLength ] = '\0';
		}
		else if (
			(
				inputChar[0] < 32 || 
				(inputChar[0] > 127 && inputChar[0] < 193) ||
				inputChar[0] > 218
			) && inputChar[0] != 13)
		{
			//ignore key
			if ( !silent)
				logger->Write ("CSidekickNet", LogNotice, "USB serial read ignored char %u", inputChar[0]);
		}
		else if (inputChar[0] != 13)
		{
			a = writeCharsToFrontend(inputChar, 1);//echo
			#ifdef WITHOUT_STDLIB
			m_modemCommand[ m_modemCommandLength++ ] = inputChar[0];
			#else
			m_modemCommand[ m_modemCommandLength++ ] = tolower(inputChar[0]);
			#endif
			m_modemCommand[ m_modemCommandLength ] = '\0';
			//logger->Write ("CSidekickNet", LogNotice, "USB serial read %u chars from C64 - %u - %u - %s", a, inputChar[0], m_modemCommandLength, m_modemCommand);
		}
		else
		{
			//RETURN KEY was pressed
			a = writeCharsToFrontend(inputChar, 1);//echo
			m_modemCommand[ m_modemCommandLength ] = '\0';
			if ( !silent)
				logger->Write ("CSidekickNet", LogNotice, "USB serial read ENTER:  '%s', length %i", m_modemCommand, m_modemCommandLength);
			
			if(m_modemCommandLength < 2)
			{
				if(m_modemCommandLength > 0)
					SendErrorResponse();
				m_modemCommandLength = 0;
				m_modemCommand[0] = '\0';
				return;
			}
			//trim command, remove spaces TODO
			
			if ( m_modemCommand[0] != 'a' && m_modemCommand[0] != 't')
			{
				if ( !silent)
					logger->Write ("CSidekickNet", LogNotice, "ERROR - cmd has to start with at");
				SendErrorResponse();
				return;
			}
			
			//trim spaces
			unsigned start = 2, stop = m_modemCommandLength;
			while (m_modemCommand[start] == 32){start++;};
			while (m_modemCommand[stop] == 32){stop--;};
			
			if ( start == stop){
				if ( !silent)
					logger->Write ("CSidekickNet", LogNotice, "ERROR - no chars after at");
				SendErrorResponse();
				return;
			}
			assert( stop > start);
			if ( m_modemCommand[start] == 'd')
			{
				start++;
				while (m_modemCommand[start] == 32){start++;};
				
				//begin and end have to be quotes
				if (  stop -1 > start && m_modemCommand[start] == '"' && m_modemCommand[stop-1] == '"')
				{
					if ( !silent)
						logger->Write ("CSidekickNet", LogNotice, "surrounding quotes detected, nice");
					start++;stop--;
				}
				else if (  stop -1 > start && m_modemCommand[start] == '"')
				{
					if ( !silent)
						logger->Write ("CSidekickNet", LogNotice, "opening quotes detected, still acceptable");
					start++;
				}
				else if ( m_modemCommand[start] == 't')
				{
					if ( !silent)
						logger->Write ("CSidekickNet", LogNotice, "no quotes but atdt detected, still acceptable");
					start++;
					while (m_modemCommand[start] == 32){start++;};
				}
				else
				{
					char keyword[256];
					memcpy( keyword, &m_modemCommand[start], stop - start );
					keyword[ stop - start ] = '\0';
					if (!checkShortcut( keyword, silent ))
					{
						if ( !silent)
							logger->Write ("CSidekickNet", LogNotice, "ERROR - no quotes detected");
						SendErrorResponse();
					}
					m_modemCommandLength = 0;
					m_modemCommand[0] = '\0';
					return;
				}
				
				if ( start == stop){
					SendErrorResponse();
					if ( !silent)
						logger->Write ("CSidekickNet", LogNotice, "ERROR - no chars between quotes");
					return;
				}
				assert( stop > start);
				
				unsigned separator = 0, c;
				for ( c = start; c <= stop; c++)
				{
					if ( m_modemCommand[c] == ':' )
					{
						if (separator > 0)
						{
							if ( !silent)
								logger->Write ("CSidekickNet", LogNotice, "ERROR - more than one separator");
							SendErrorResponse();
							return;
							
						}
						separator = c;
					}
					else if ( m_modemCommand[c] == 32 )
					{
						if ( !silent)
							logger->Write ("CSidekickNet", LogNotice, "ERROR - no blanks allowed in here");
						SendErrorResponse();
						return;
					}	

				}
				if ( separator == 0 || separator == stop || separator == start)
				{
					if ( !silent)
						logger->Write ("CSidekickNet", LogNotice, "ERROR - no separator found between hostname and port");
					SendErrorResponse();
					return;
				}

				char hostStr[256], portStr[256];
				memcpy( hostStr, &m_modemCommand[start], separator - start );
				memcpy( portStr, &m_modemCommand[separator+1], stop - separator );
				hostStr[ separator - start ] = '\0';
				portStr[ stop - separator ] = '\0';
				unsigned portNo = atoi(portStr);
				//logger->Write ("CSidekickNet", LogNotice, "hostStr: '%s' portNo: '%s' %i", hostStr, portStr, portNo);
				SocketConnect( hostStr, portNo, silent);
			} //end of d command (atd)
			else if ( m_modemCommand[start] == 'b')
			{
				if (strcmp(m_modemCommand, "atb300") == 0)
				{
					a = writeCharsToFrontend((unsigned char *)"OK\r", 3);
					m_pScheduler->MsSleep(100);
					setModemEmuBaudrate(300);
				}
				else if (strcmp(m_modemCommand, "atb1200") == 0)
				{
					a = writeCharsToFrontend((unsigned char *)"OK\r", 3);
					m_pScheduler->MsSleep(100);
					setModemEmuBaudrate(1200);
				}
				else if (strcmp(m_modemCommand, "atb2400") == 0)
				{
					a = writeCharsToFrontend((unsigned char *)"OK\r", 3);
					m_pScheduler->MsSleep(100);
					setModemEmuBaudrate(2400);
				}
				else if (strcmp(m_modemCommand, "atb4800") == 0)
				{
					a = writeCharsToFrontend((unsigned char *)"OK\r", 3);
					m_pScheduler->MsSleep(100);
					setModemEmuBaudrate(4800);
				}
				else if (strcmp(m_modemCommand, "atb9600") == 0)
				{
					a = writeCharsToFrontend((unsigned char *)"OK\r", 3);
					m_pScheduler->MsSleep(100);
					setModemEmuBaudrate(9600);
				}
			}
			else if ( m_modemCommand[start] == 'i')
			{
				if ( start + 2 == stop)
				{
					CString strHelper;
					unsigned char tmp[50];
					
					if( m_modemCommand[start+1] == '7')
					{
						//ati7 time and date
						strHelper = getTimeString();
						strHelper.Append("\r");
						unsigned l = sprintf( (char *) tmp, strHelper );
						//logger->Write ("CSidekickNet", LogNotice, "get time: sh:'%s',tmp:'%s'", strHelper, tmp );
						a = writeCharsToFrontend(tmp, l);
					}
					else if( m_modemCommand[start+1] == '2')
					{
						//ati2 ip address
						GetNetConfig()->GetIPAddress ()->Format (&strHelper);
						strHelper.Append("\r");
						unsigned l = sprintf( (char* )tmp, strHelper );
						//logger->Write ("CSidekickNet", LogNotice, "get ip: sh:'%s', tmp:'%s'", strHelper, tmp );
						a = writeCharsToFrontend(tmp, l);
					}
				}
				else if (start +1 == stop){
					//ati
					if ( m_modemEmuType == SK_MODEM_USERPORT_USB )
						a = writeCharsToFrontend((unsigned char *)"sidekick64 userport modem emulation\rhave fun!\r", 46);
					else
						a = writeCharsToFrontend((unsigned char *)"sidekick64 swiftlink modem emulation\rhave fun!\r", 47);
					a = writeCharsToFrontend((unsigned char *)"current baudrate: ", 18);

					CString strHelper;
					unsigned char tmp[50];
					strHelper = getBaudrate();
					strHelper.Append("\r");
					unsigned l = sprintf( (char *) tmp, strHelper );
					a = writeCharsToFrontend(tmp, l);
						
						//a = writeCharsToFrontend((unsigned char *)"\x40\x60 \x9f \x7dc\x05 1w\x1c 2w\x1e 3g\x1f 4b\x95 5b\x96 6r\x97 7g\x98 8g\x99 9g\x9a ab\x9c bp\x9e cy\x05-" ,13*4+4 );
				}
				else{
					SendErrorResponse();
				}
					
					/*
							 colors
							 
							 white 	5     \x05
							 red 		28		\x1c
							 grn 		30		\x1e
							 blue 	31		\x1f
							 blk 		144   
							 brown 	149   \x95
							 lt red 150		\x96
							 grey1 	151		\x97
							 grey2 	152		\x98
							 lt green 153	\x99
							 lt blue 154  \x9a
							 pur		156		\x9c
							 yel 		158   \x9e
							 cyan 	159   \x9f
					
					*/
					
			}
			else if ( m_modemCommand[start] == 'v')
			{
				a = writeCharsToFrontend((unsigned char *)"OK\r", 3);
				if ( !silent)
					logger->Write ("CSidekickNet", LogNotice, "Command tolerated but not implemented :)");
			}
			else if (m_modemCommandLength > 0){
				if ( !silent)
					logger->Write ("CSidekickNet", LogNotice, "ERROR - unknown command");
				SendErrorResponse();
				return;
			}
			
			m_modemCommandLength = 0;
			m_modemCommand[0] = '\0';
		} //end of return key
	} //end of read one char from serial
}

void CSidekickNet::SendErrorResponse()
{
	writeCharsToFrontend((unsigned char *) "ERROR\r", 6);
	m_modemCommandLength = 0;
	m_modemCommand[0] = '\0';
}

void CSidekickNet::SocketConnect( char * hostname, unsigned port, bool silent )
{
	bool success = true;
	int a;
	
	if ( silent )
	{
		unsigned c = 0;
		//for (unsigned c = 0; c < strlen(hostname); c++)
		while ( hostname[c] != '\0')
		{
			m_socketHost[c] = hostname[c];
			c++;
		}
		m_socketHost[c] = '\0';
		//m_socketHost[strlen(hostname)] = '\0';
		//strcpy(m_socketHost, hostname);
		//m_socketHost[strlen(hostname)] = '\0';
		m_socketPort = port;
		return;
	}
	
	CIPAddress bbsIP = getIPForHost( hostname, success);
	if ( !success || bbsIP.IsNull() )
	{
		a = writeCharsToFrontend((unsigned char *) "FAILED DNS\r\n", 12);
		logger->Write ("CSidekickNet", LogNotice, "Couldn't resolve IP address for %s", hostname);
	}
	else
		SocketConnectIP( bbsIP, port);
}

void CSidekickNet::SocketConnectIP( CIPAddress bbsIP, unsigned port )
{
	m_pBBSSocket = new CSocket (m_Net, IPPROTO_TCP);
	if ( m_pBBSSocket->Connect ( bbsIP, port) == 0)
	{
		writeCharsToFrontend((unsigned char *) "CONNECT\r\n", 9);
		m_isBBSSocketConnected = true;
		m_isBBSSocketFirstReceive = true;
	}
	else
	{
		writeCharsToFrontend((unsigned char *) "FAILED PORT\r\n", 13);
		logger->Write ("CSidekickNet", LogNotice, "Socket connect failed at port %u", port);
	}
}

boolean CSidekickNet::checkShortcut( char * keyword, bool silent )
{
	boolean found = true;
	logger->Write ("CSidekickNet", LogNotice, "keyword: '%s'", keyword);
	//unsigned key = atoi(keyword);
	//shortcuts for btx, qlink, habitat
	//Quantum Link / QLink
	if (strcmp(keyword, "t 5551212") == 0 ||
					strcmp(keyword, "t5551212") == 0 )
	{
		setModemEmuBaudrate(1200);
		SocketConnect("q-link.net", 5190, silent);
	}
	//BTX
	else if (
		strcmp(keyword, "t01910") == 0 || 
		strcmp(keyword, "190") == 0 || 
		strcmp(keyword, "btx") == 0
	){
		if (strcmp(keyword, "t01910") == 0 )
			setModemEmuBaudrate(2400); // Plus/4 online
		else
			setModemEmuBaudrate(1200);
		
		//btx.hanse.de or 195.201.94.166, could be two different instances
		//static const u8 btx[] = {195, 201, 94, 166}; //lazy, avoiding resolve
		//CIPAddress BTXIPAddress;
		//BTXIPAddress.Set(btx);
		//SocketConnectIP(BTXIPAddress, 20000);
		SocketConnect("static.166.94.201.195.clients.your-server.de", 20000, silent);
	}
	else if (strcmp(keyword, "@habitat") == 0)
	{
		setModemEmuBaudrate(1200);
		SocketConnect("neohabitat.demo.spi.ne", 1986, silent);
	}
	else if (strcmp(keyword, "@qw") == 0)
		SocketConnect("ryzentux", 64128, silent);
	else if (strcmp(keyword, "@rf") == 0)
		SocketConnect("rapidfire.hopto.org", 64128, silent);
	else if (strcmp(keyword, "@ro") == 0)
		SocketConnect("raveolution.hopto.org", 64128, silent);
	else if (strcmp(keyword, "@rc") == 0)
		SocketConnect("bbs.retrocampus.com", 6510, silent);
	else if (strcmp(keyword, "@cm") == 0)
		SocketConnect("coffeemud.net", 2323, silent);
	else if (strcmp(keyword, "@dnsfail") == 0)
		SocketConnect("doesnotexist246789.hopto.org.bla", 64128, silent); //test dns resolve fail
	else
		found = false;
	return found;
}

void CSidekickNet::addToModemOutputBuffer( unsigned char mchar)
{
	m_modemOutputBuffer[m_modemOutputBufferLength++] = mchar;
	m_modemOutputBuffer[m_modemOutputBufferLength] = '\0';
}

unsigned CSidekickNet::getModemEmuType(){
	return m_modemEmuType;
}

void CSidekickNet::setModemEmuType( unsigned type ){
	m_modemEmuType = type;
}

bool CSidekickNet::isModemSocketConnected(){
	return m_isBBSSocketConnected;
}
