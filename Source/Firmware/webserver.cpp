//
// webserver.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2015  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "webserver.h"
#include <circle/chainboot.h>
#include <circle/logger.h>
#include <circle/machineinfo.h>
#include <circle/string.h>
#include <circle/util.h>
#include <circle/version.h>
#include <assert.h>
#include "helpers.h"
#include "config.h"
#include "lowlevel_arm64.h"

#define MAX_CONTENT_SIZE	40000

static const char FILENAME_CONFIG[] = "SD:C64/sidekick64.cfg";		
static const char DRIVE[] = "SD:";

extern u32 prgSizeLaunch;
extern unsigned char prgDataLaunch[ 1027*1024*12 ] AAA;

// our content
static const char s_Index[] =
#include "webcontent/index.h"
;

static const char s_Upload[] =
#include "webcontent/upload.h"
;

/*
static const char s_Tuning[] =
#include "webcontent/tuning.h"
;
*/

static const u8 s_Style[] =
#include "webcontent/style.h"
;

static const u8 s_Favicon[] =
{
#include "webcontent/favicon.h"
};

static const u8 s_SK64Logo[] =
{
#include "webcontent/sidekick64_logo.h"
};


static const char FromWebServer[] = "webserver";

CWebServer::CWebServer (CNetSubSystem *pNetSubSystem, u16 nPort,
				  unsigned nMaxMultipartSize, CSocket *pSocket, CSidekickNet * pSidekickNet)
:	CHTTPDaemon (pNetSubSystem, pSocket, MAX_CONTENT_SIZE, nPort, nMaxMultipartSize),
	m_nPort (nPort),
	m_nMaxMultipartSize (nMaxMultipartSize),
	m_SidekickNet( pSidekickNet )
{
}

CWebServer::~CWebServer (void)
{
}

CHTTPDaemon *CWebServer::CreateWorker (CNetSubSystem *pNetSubSystem, CSocket *pSocket)
{
	return new CWebServer (pNetSubSystem, m_nPort, m_nMaxMultipartSize, pSocket, m_SidekickNet);
}

::THTTPStatus CWebServer::GetContent (const char  *pPath,
					 const char  *pParams,
					 const char  *pFormData,
					 u8	     *pBuffer,
					 unsigned    *pLength,
					 const char **ppContentType)
{
	assert (pPath != 0);
	assert (ppContentType != 0);

	CString String;
	const u8 *pContent = 0;
	unsigned nLength = 0;

	if ( strcmp (pPath, "/") == 0
	    || strcmp (pPath, "/index.html") == 0
			|| strcmp (pPath, "/upload.html") == 0)
	{
/*		
		const char *pMsg = 0;		
		pMsg = "Welcome!";
		String.Format (s_Index, pMsg, CIRCLE_VERSION_STRING,
						 CMachineInfo::Get ()->GetMachineName ());

		pContent = (const u8 *) (const char *) String;
		nLength = String.GetLength ();
		*ppContentType = "text/html; charset=UTF-8";		
	}
	else if ( strcmp (pPath, "/upload.html") == 0)
	{*/
		//const 
		char * pMsg;
		pMsg[0] = '\0';
		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;
		
		char filename[255];
		char extension[10];
		u8 startChar = 0;
		filename[0] = '\0';
		extension[0] = '\0';
		
		bool noFormParts = true;
		bool textFileTransfer = false;
		
		u32 cfgBytes;
		char cfg[ 16384 ];
		memset( cfg, 0, 16384 );
		
		if (GetMultipartFormPart (&pPartHeader, &pPartData, &nPartLength))
		{
			noFormParts = false;
			assert (pPartHeader != 0);
			char * startpos = strstr (pPartHeader, "filename=\"");
			if ( startpos != 0)
			{
				startpos += 10;
				u16 fnl = 0;
				u16 exl = 0;
				u16 extensionStart = 0;
				while ( (startpos[fnl] != '\"') && (fnl < 254) )
				{
					filename[fnl] = startpos[fnl];
					if ( filename[fnl] == '.' )
						extensionStart = fnl+1;
					fnl++;
				}
				filename[fnl] = '\0';
				while ( extensionStart > 0 && extensionStart < fnl && exl < 9)
				{
					u8 tmp = filename[extensionStart + exl];
					if (tmp >=65 && tmp <=90) tmp+=32; //strtolower
					extension[exl] = tmp;
					exl++;
				}
				extension[exl] = '\0';
				//logger->Write( FromWebServer, LogNotice, "found filename: '%s' %i", filename, strlen(filename));
				//logger->Write( FromWebServer, LogNotice, "found extension: '%s' %i", extension, strlen(extension));
			}
			else{
					char * matchTA = "name=\"textarea_config\"";
					char * startposTextArea = strstr (pPartHeader, matchTA);
					if( startposTextArea != 0){
						u8 c = 0;
						while (c < strlen(matchTA)+4){
							c++;
							startposTextArea++;
						}

						char * endposTextArea = strstr (startposTextArea, "---------");
						char * current = startposTextArea;
						u16 charCount = 0;
						while (charCount < 16384 && current++ != endposTextArea )
							charCount ++;
						if ( charCount < 16384 && charCount > 2)
						{
							memcpy( cfg, startposTextArea, --charCount);
							cfg[--charCount] = '\0';
							logger->Write( FromWebServer, LogNotice, "config string lentgh: %i",charCount );
							writeFile( logger, DRIVE, FILENAME_CONFIG, (u8*) cfg, charCount );
							m_SidekickNet->requireCacheWellnessTreatment();
							textFileTransfer = true;
						}
						else
						{
							pMsg = "Config file content fetched but illegal length!";
							logger->Write( FromWebServer, LogNotice, "illegal config string lentgh: %i",charCount );
						}
					}
			}
		}

		const char *pPartHeader_radio;
		const u8 *pPartData_radio;
		unsigned nPartLength_radio;
			
		if (GetMultipartFormPart (&pPartHeader_radio, &pPartData_radio, &nPartLength_radio))
		{
			noFormParts = false;
			assert (pPartHeader != 0);
			char * match;
			char * startpos2;
			if( textFileTransfer)
			{
				match = "name=\"radio_configsavereboot\"";
				startpos2 = strstr (pPartHeader_radio, match);
				if ( startpos2 != 0)
				{
					startChar = startpos2[strlen(match)+4]; //r/s
					//logger->Write( FromWebServer, LogNotice, "found radiobutton: '%u' ", startChar);
					if ( startChar == 114)
					{
						pMsg = "Config file changes were saved to SD card. Now rebooting.";
						m_SidekickNet->requestReboot();
						
					}
					else
						pMsg = "Config file changes were saved to SD card.";
				}
				else
					logger->Write( FromWebServer, LogNotice, "did not find radiobutton");

				
			}
			else{
				match = "name=\"radio_saveorlaunch\"";
				startpos2 = strstr (pPartHeader_radio, match);
				if ( startpos2 != 0)
				{
					startChar = startpos2[strlen(match)+4];
					//logger->Write( FromWebServer, LogNotice, "found radiobutton: '%u' ", startChar);
				}
				else
					logger->Write( FromWebServer, LogNotice, "did not find radiobutton");
			}
		}

		if(!noFormParts){

		if (textFileTransfer){
			//nothing to do here
		}
		else if (strstr (pPartHeader, "name=\"kernelimg\"") != 0
		    && ( strstr (filename, "kernel") != 0 || strstr (filename, "rpi4_kernel") != 0 )
		    && strcmp (extension, "img") == 0
		    && nPartLength > 0)
		{
			assert (pPartData != 0);
			
			m_SidekickNet->requireCacheWellnessTreatment();

#ifndef IS264
			#if RASPPI >= 4
				const char * filenamek = "SD:rpi4_kernel_sk64_net.img";
			#else
				const char * filenamek = !m_SidekickNet->kernelSupportsWLAN() ? "SD:kernel_sk64_ethernet.img" : "SD:kernel_sk64_net.img";
			#endif
#else
			#if RASPPI >= 4
				const char * filenamek = m_SidekickNet->usesWLAN() ? "SD:rpi4_kernel_sk264_wlan.img" : "SD:rpi4_kernel_sk264_net.img";
			#else
				const char * filenamek = m_SidekickNet->usesWLAN() ? "SD:kernel_sk264_wlan.img" : "SD:kernel_sk264_net.img";
			#endif
#endif			
			logger->Write( FromWebServer, LogNotice, "Saving kernel image to SD card, length: %u", nPartLength);
			writeFile( logger, "SD:", filenamek, (u8*) pPartData, nPartLength );
			m_SidekickNet->requireCacheWellnessTreatment();
			m_SidekickNet->requestReboot();
			
			pMsg = "Now rebooting into new kernel...";
		}
		else if (nPartLength > 0)
		{

			if (( strcmp (extension, "prg") == 0 ||
					strcmp (extension, "d64") == 0 ||
					strcmp (extension, "crt") == 0 ||
					strcmp (extension, "sid") == 0 ||
					m_SidekickNet->isLibOpenMPTFileType(extension) ||
					strcmp (extension, "bin") == 0) &&
					strlen(filename)>0
			){
				prgSizeLaunch = nPartLength;
				memcpy( prgDataLaunch, pPartData, nPartLength);
				u8 mode = 0; //launch only 108
				if ( startChar == 115) mode = 1; //save only
				else if ( startChar == 98) mode = 2; //save and launch
				
				m_SidekickNet->prepareLaunchOfUpload( extension, filename, mode, pMsg);
			}
			else
			{
				pMsg = "Invalid request (1)";
			}
		}
		else
			pMsg = "Invalid request (2)";
	}
	else
	{
		pMsg = "Send a PRG, SID, CRT, D64 or BIN file to Sidekick64 for instant launch. Or upload a Sidekick kernel image update (includes reboot).";
		m_SidekickNet->enterWebUploadMode();
	}

		assert (pMsg != 0);
		
		if (!textFileTransfer){
			if ( !readFile( logger, DRIVE, FILENAME_CONFIG, (u8*)cfg, &cfgBytes ) )
				logger->Write( FromWebServer, LogNotice, "Could not read config file");
		}
		
		String.Format (s_Upload, pMsg, cfg, CIRCLE_VERSION_STRING,
			       CMachineInfo::Get ()->GetMachineName ());

		pContent = (const u8 *) (const char *) String;
		nLength = String.GetLength ();
		*ppContentType = "text/html; charset=UTF-8";
	}
	/*
	else if (strcmp (pPath, "/tuning.html") == 0)
	{
		const char *pMsg = 0;

		const char *pPartHeader;
		const u8 *pPartData;
		unsigned nPartLength;
		if (strcmp (pFormData, "") != 0)
		{
			unsigned l = strlen(pFormData);
			pMsg = pFormData;
			unsigned mode = 0;
			unsigned key = 0;
			unsigned vl = 0;
			char *value;
			for (unsigned c = 0; c <= l; c++)
			{
				if (mode == 0 )
				{
					if (c+1 < l && pFormData[c+1] == '=')
					{
						key = atoi( & pFormData[c]);
						//logger->Write( FromWebServer, LogNotice, "Parsing form data: Key detected: %i", key);
						value = "";
						vl = 0;
						mode = 1;
					}
					else
					{
						logger->Write( FromWebServer, LogNotice, "Parsing form data: Error on finding key. %i / %i", c,l);
					}
				}
				if (mode == 1 && pFormData[c] == '=')
				{
					if ( c+1 < l)
					{
						//logger->Write( FromWebServer, LogNotice, "Parsing form data: Found = where we wanted it.");
						mode = 2;
					}
					else
					{
						logger->Write( FromWebServer, LogNotice, "Parsing form data: Error looking for =.");
						mode = 0;
						break; //end of string, no value left after key =
					}
				}
				else if (mode == 2)
				{
					if ( c == l || pFormData[c] == '&') //pFormData[c] == '\0')
					{
						logger->Write( FromWebServer, LogNotice, "Parsing form data: Assigning timingValue key=%i value=%i", key, atoi(value));
						
						if ( atoi(value) > 0)
						switch( key )
						{
							case 0:
								WAIT_FOR_SIGNALS = atoi(value);
								break;
							case 1:
								WAIT_CYCLE_READ = atoi(value);
								break;
							case 2:
								WAIT_CYCLE_WRITEDATA = atoi(value);
								break;
							case 3:
								WAIT_CYCLE_READ_BADLINE = atoi(value);
								break;
							case 4:
								WAIT_CYCLE_READ_VIC2 = atoi(value);
								break;
							case 5:
								WAIT_CYCLE_WRITEDATA_VIC2 = atoi(value);
								break;
							case 6:
								WAIT_CYCLE_MULTIPLEXER = atoi(value);
								break;
							case 7:
								WAIT_CYCLE_MULTIPLEXER_VIC2 = atoi(value);
								break;
							case 8:
								WAIT_TRIGGER_DMA = atoi(value);
								break;
							case 9:
								WAIT_RELEASE_DMA = atoi(value);
								break;
						}
						value = "";
						vl = 0;
						mode = 0;
					}
					else
					{
						value[vl] = pFormData[c];
						value[++vl] = '\0';
						mode = 2;
					}
				}
			}
		}
		else
		{
			pMsg = "Please change params to your needs.";
		}

		
		int timingValues[ 10 ] = { 40, 475, 470, 400, 425, 505, 200, 265, 600, 600 };

		timingValues[ 0 ] = WAIT_FOR_SIGNALS;
		timingValues[ 1 ] = WAIT_CYCLE_READ;
		timingValues[ 2 ] = WAIT_CYCLE_WRITEDATA;
		timingValues[ 3 ] = WAIT_CYCLE_READ_BADLINE;
		timingValues[ 4 ] = WAIT_CYCLE_READ_VIC2;
		timingValues[ 5 ] = WAIT_CYCLE_WRITEDATA_VIC2;
		timingValues[ 6 ] = WAIT_CYCLE_MULTIPLEXER;
		timingValues[ 7 ] = WAIT_CYCLE_MULTIPLEXER_VIC2;
		timingValues[ 8 ] = WAIT_TRIGGER_DMA;
		timingValues[ 9 ] = WAIT_RELEASE_DMA;

		CString formMarkup = "";
		formMarkup.Append("<pre>");
		formMarkup.Append(pMsg);
		formMarkup.Append("</pre>");
		for ( int i = 0; i < 10; i++ )
		{
			formMarkup.Append("<tr><td>");
			formMarkup.Append(timingNames[ i ]);
			formMarkup.Append("</td><td><input type=\"text\" name=\"");
			CString Number;
			Number.Format ("%01d", i);
			formMarkup.Append( Number );
			formMarkup.Append("\" value=\"");
			Number.Format ("%02d", timingValues[ i ]);
			formMarkup.Append(Number);
			formMarkup.Append("\"/></td></tr>\n");
		}

		assert (formMarkup != 0);
		String.Format (s_Tuning, (const char *) formMarkup );

		pContent = (const u8 *) (const char *) String;
		nLength = String.GetLength ();
		*ppContentType = "text/html; charset=UTF-8";
	}
	*/
	else if (strcmp (pPath, "/style.css") == 0)
	{
		pContent = s_Style;
		nLength = sizeof s_Style-1;
		*ppContentType = "text/css";
	}
	else if (strcmp (pPath, "/favicon.ico") == 0)
	{
		pContent = s_Favicon;
		nLength = sizeof s_Favicon;
		*ppContentType = "image/x-icon";
	}
	else if (strcmp (pPath, "/sidekick64_logo.png") == 0)
	{
		pContent = s_SK64Logo;
		nLength = sizeof s_SK64Logo;
		*ppContentType = "image/png";
	}
	else
	{
		return ::HTTPNotFound;
	}

	assert (pLength != 0);
	if (*pLength < nLength)
	{
		CLogger::Get ()->Write (FromWebServer, LogError,
					"Increase MAX_CONTENT_SIZE to at least %u", nLength);

		return ::HTTPInternalServerError;
	}

	assert (pBuffer != 0);
	assert (pContent != 0);
	assert (nLength > 0);
	memcpy (pBuffer, pContent, nLength);

	*pLength = nLength;

	return ::HTTPOK;
}
