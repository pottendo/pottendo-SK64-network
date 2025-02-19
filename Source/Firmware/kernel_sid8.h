/*
  _________.__    .___      __   .__        __          _________.___________   
 /   _____/|__| __| _/____ |  | _|__| ____ |  | __     /   _____/|   \______ \  
 \_____  \ |  |/ __ |/ __ \|  |/ /  |/ ___\|  |/ /     \_____  \ |   ||    |  \ 
 /        \|  / /_/ \  ___/|    <|  \  \___|    <      /        \|   ||    `   \
/_______  /|__\____ |\___  >__|_ \__|\___  >__|_ \    /_______  /|___/_______  /
        \/         \/    \/     \/       \/     \/            \/             \/ 
 
 kernel_sid8.h

 Sidekick64 - A framework for interfacing 8-Bit Commodore computers (C64/C128,C16/Plus4,VC20) and a Raspberry Pi Zero 2 or 3A+/3B+
            - Sidekick SID: a SID and SFX Sound Expander Emulation / SID-8 is a lazy modification to emulate 8 SIDs at once
		      (using reSID by Dag Lem)
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
#ifndef _kernel_sid8_h
#define _kernel_sid8_h

// use the OLED connected to the latch
#define USE_OLED

//
// choose whether to output sound via the headphone jack (PWM), otherwise HDMI audio will be used (higher delay)
//
#define HDMI_SOUND
#define USE_PWM_DIRECT
//#define USE_VCHIQ_SOUND

#define SID2_MASK (1<<A5)

#define USE_HDMI_VIDEO

#if defined(USE_OLED) && !defined(USE_LATCH_OUTPUT)
#define USE_LATCH_OUTPUT
#endif

#include <circle/startup.h>
#include <circle/bcm2835.h>
#include <circle/memio.h>
#include <circle/memory.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/sched/scheduler.h>
#include <circle/sound/soundbasedevice.h>
#include <circle/types.h>
#include <circle/gpioclock.h>
#include <circle/gpiopin.h>
#include <circle/gpiopinfiq.h>
#include <circle/gpiomanager.h>
#include <circle/sound/pwmsoundbasedevice.h>
#include <circle/sound/i2ssoundbasedevice.h>
#include <circle/util.h>

#ifdef USE_VCHIQ_SOUND
#include <vc4/vchiq/vchiqdevice.h>
#include <vc4/sound/vchiqsoundbasedevice.h>
#endif

#include "lowlevel_arm64.h"
#include "gpio_defs.h"
#include "latch.h"
#include "sound.h"
#include "helpers.h"

#ifdef USE_OLED
#include "oled.h"
#include "splash_sid.h"
#endif

#ifdef COMPILE_MENU
#include "kernel_menu.h"
void KernelSIDFIQHandler8( void *pParam );
#define FIQ_HANDLER	KernelSIDFIQHandler8
#define FIQ_PARENT	kernelMenu
#else
extern CLogger	*logger;
#define FIQ_HANDLER	(this->FIQHandler)
#define FIQ_PARENT	this
#endif


#ifndef min
#define min( a, b ) ( ((a)<(b))?(a):(b) )
#endif
#ifndef max
#define max( a, b ) ( ((a)>(b))?(a):(b) )
#endif

class CKernel
{
public:
	CKernel( void )
		: m_CPUThrottle( CPUSpeedMaximum ),
	#ifdef USE_HDMI_VIDEO
		m_Screen( m_Options.GetWidth(), m_Options.GetHeight() ),
	#endif
		m_Timer( &m_Interrupt ),
		m_Logger( m_Options.GetLogLevel(), &m_Timer ),
	#ifdef USE_VCHIQ_SOUND
		m_VCHIQ( &m_Memory, &m_Interrupt ),
	#endif
		m_pSound( 0 ),
		m_InputPin( PHI2, GPIOModeInput, &m_Interrupt )
	{
	}

	~CKernel( void )
	{
	}

	boolean Initialize( void );

	void Run( void );

private:
	static void FIQHandler( void *pParam );
	
	// do not change this order
	CMemorySystem		m_Memory;
	CKernelOptions		m_Options;
	CDeviceNameService	m_DeviceNameService;
	CCPUThrottle		m_CPUThrottle;
#ifdef USE_HDMI_VIDEO
	CScreenDevice		m_Screen;
#endif
	CInterruptSystem	m_Interrupt;
	CTimer				m_Timer;
	CLogger				m_Logger;
	CScheduler			m_Scheduler;
#ifdef USE_VCHIQ_SOUND
	CVCHIQDevice		m_VCHIQ;
#endif
	CSoundBaseDevice	*m_pSound;
	CGPIOPinFIQ			m_InputPin;
};

#endif
