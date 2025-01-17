//
// webserver.h
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
#ifndef _webserver_h
#define _webserver_h

#include <circle/net/httpdaemon.h>

class CWebServer; //forward declaration
#include "net.h"

extern CLogger *logger;

class CWebServer : public CHTTPDaemon
{
public:
	CWebServer (CNetSubSystem *pNetSubSystem,
							 u16		nPort,
							 unsigned	nMaxMultipartSize,
							 CSocket       *pSocket = 0,	// is 0 for 1st created instance (listener)				
							 CSidekickNet * pSidekickNet = 0
						 	);
	~CWebServer (void);
 
	boolean isRebootRequested();
 
	// creates an instance of our derived webserver class
	CHTTPDaemon *CreateWorker (CNetSubSystem *pNetSubSystem, CSocket *pSocket);

	// provides our content
	::THTTPStatus GetContent (const char  *pPath,		// path of the file to be sent
				const char  *pParams,		// parameters to GET ("" for none)
				const char  *pFormData, 	// form data from POST ("" for none)
			        u8	    *pBuffer,		// copy your content here
			        unsigned    *pLength,		// in: buffer size, out: content length
			        const char **ppContentType);	// set this if not "text/html"

private:
	u16	 m_nPort;
	unsigned m_nMaxMultipartSize;
  CSidekickNet * m_SidekickNet;
};

#endif
