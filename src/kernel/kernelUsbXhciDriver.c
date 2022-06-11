//
//  Visopsys
//  Copyright (C) 1998-2014 J. Andrew McLaughlin
//
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation; either version 2 of the License, or (at your option)
//  any later version.
//
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
//  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
//  for more details.
//
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
//  kernelUsbXhciDriver.c
//

#include "kernelUsbXhciDriver.h"
#include "kernelUsbDriver.h"
#include "kernelCpu.h"
#include "kernelDebug.h"
#include "kernelError.h"
#include "kernelLog.h"
#include "kernelMalloc.h"
#include "kernelMemory.h"
#include "kernelMisc.h"
#include "kernelPage.h"
#include "kernelParameters.h"
#include "kernelPciDriver.h"
#include "kernelVariableList.h"
#include <stdio.h>
#include <stdlib.h>


#ifdef DEBUG
static inline void debugXhciCapRegs(usbXhciData *xhciData)
{
	kernelDebug(debug_usb, "XHCI capability registers:\n"
		"  capslen=0x%02x\n"
		"  hciver=0x%04x\n"
		"  hcsparams1=0x%08x\n"
		"  hcsparams2=0x%08x\n"
		"  hcsparams3=0x%08x\n"
		"  hccparams=0x%08x\n"
		"  dboffset=0x%08x\n"
		"  runtimeoffset=0x%08x",
		(xhciData->capRegs->capslenHciver & 0xFF),
		(xhciData->capRegs->capslenHciver >> 16),
		xhciData->capRegs->hcsparams1,
		xhciData->capRegs->hcsparams2,
		xhciData->capRegs->hcsparams3,
		xhciData->capRegs->hccparams,
		xhciData->capRegs->dboffset,
		xhciData->capRegs->runtimeoffset);
}

static inline void debugXhciOpRegs(usbXhciData *xhciData)
{
	kernelDebug(debug_usb, "XHCI operational registers:\n"
		"  cmd=0x%08x\n"
		"  stat=0x%08x\n"
		"  pagesz=0x%04x (%u)\n"
		"  dncntrl=0x%08x\n"
		"  cmdrctrl=0x...............%1x\n"
		"  dcbaap=0x%08x%08x\n"
		"  config=0x%08x",
		xhciData->opRegs->cmd, xhciData->opRegs->stat,
		xhciData->opRegs->pagesz,
		(xhciData->opRegs->pagesz << 12),
		xhciData->opRegs->dncntrl, xhciData->opRegs->cmdrctrlLo,
		xhciData->opRegs->dcbaapHi, xhciData->opRegs->dcbaapLo,
		xhciData->opRegs->config);
}

static inline void debugXhciHcsParams1(usbXhciData *xhciData)
{
	kernelDebug(debug_usb, "XHCI HCSParams1 register (0x%08x):\n"
		"  max ports=%d\n"
		"  max interrupters=%d\n"
		"  max device slots=%d", xhciData->capRegs->hcsparams1,
		((xhciData->capRegs->hcsparams1 & USBXHCI_HCSP1_MAXPORTS) >> 24),
		((xhciData->capRegs->hcsparams1 & USBXHCI_HCSP1_MAXINTRPTRS) >> 8),
		(xhciData->capRegs->hcsparams1 & USBXHCI_HCSP1_MAXDEVSLOTS));
}

static inline void debugXhciHcsParams2(usbXhciData *xhciData)
{
	kernelDebug(debug_usb, "XHCI HCSParams2 register (0x%08x):\n"
		"  max scratchpad buffers=%d\n"
		"  scratchpad restore=%d\n"
		"  event ring segment table max=%d\n"
		"  isochronous scheduling threshold=%d",
		xhciData->capRegs->hcsparams2,
		((xhciData->capRegs->hcsparams2 &
			USBXHCI_HCSP2_MAXSCRPBUFFS) >> 27),
		((xhciData->capRegs->hcsparams2 &
			USBXHCI_HCSP2_SCRATCHPREST) >> 26),
		((xhciData->capRegs->hcsparams2 & USBXHCI_HCSP2_ERSTMAX) >> 4),
		(xhciData->capRegs->hcsparams2 & USBXHCI_HCSP2_ISOCSCHDTHRS));
}

static inline void debugXhciHcsParams3(usbXhciData *xhciData)
{
	kernelDebug(debug_usb, "XHCI HCSParams3 register (0x%08x):\n"
		"  u2 device exit latency=%d\n"
		"  u1 device exit latency=%d", xhciData->capRegs->hcsparams3,
		((xhciData->capRegs->hcsparams3 & USBXHCI_HCSP3_U2DEVLATENCY) >> 16),
		(xhciData->capRegs->hcsparams3 & USBXHCI_HCSP3_U1DEVLATENCY));
}

static inline void debugXhciHccParams(usbXhciData *xhciData)
{
	kernelDebug(debug_usb, "XHCI HCCParams register(0x%08x):\n"
		"  extended caps ptr=0x%04x\n"
		"  max pri stream array size=%d\n"
		"  no sec sid support=%d\n"
		"  latency tolerance msg cap=%d\n"
		"  light hc reset cap=%d\n"
		"  port indicators=%d\n"
		"  port power control=%d\n"
		"  context size=%d\n"
		"  bandwidth neg cap=%d\n"
		"  64-bit addressing=%d", xhciData->capRegs->hccparams,
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_EXTCAPPTR) >> 16),
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_MAXPRISTRARSZ) >> 12),
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_NOSECSIDSUP) >> 7),
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_LATTOLMESSCAP) >> 6),
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_LIGHTHCRESET) >> 5),
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_PORTIND) >> 4),
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_PORTPOWER) >> 3),
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_CONTEXTSIZE) >> 2),
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_BANDNEGCAP) >> 1),
		(xhciData->capRegs->hccparams & USBXHCI_HCCP_64ADDRCAP));
}

static inline void debugXhciCmdStatRegs(usbXhciData *xhciData)
{
	kernelDebug(debug_usb, "XHCI command/status registers:\n"
		"  cmd=0x%08x\n"
		"  stat=0x%08x",
		xhciData->opRegs->cmd, xhciData->opRegs->stat);
}

static inline void debugXhciRuntimeRegs(usbXhciData *xhciData)
{
	int numIntrs = max(xhciData->numIntrs, 1);
	char *intrRegs = NULL;
	int count;

	intrRegs = kernelMalloc(kernelTextGetNumColumns() * numIntrs * 2);
	if (intrRegs)
	{
		// Read the interrupter register sets
		for (count = 0; count < numIntrs; count ++)
			sprintf((intrRegs + strlen(intrRegs)),
				"\n  inter%d intrMan=0x%08x intrMod=0x%08x "
				"evtRngSegTabSz=0x%08x"
				"\n  inter%d evtRngSegBase=0x%08x%08x "
				"evtRngDeqPtr=0x%08x%08x", count,
				xhciData->rtRegs->intrReg[count].intrMan,
				xhciData->rtRegs->intrReg[count].intrMod,
				xhciData->rtRegs->intrReg[count].evtRngSegTabSz, count,
				xhciData->rtRegs->intrReg[count].evtRngSegBaseHi,
				xhciData->rtRegs->intrReg[count].evtRngSegBaseLo,
				xhciData->rtRegs->intrReg[count].evtRngDeqPtrHi,
				xhciData->rtRegs->intrReg[count].evtRngDeqPtrLo);

		kernelDebug(debug_usb, "XHCI runtime registers:\n"
			"  mfindex=0x%08x%s", xhciData->rtRegs->mfindex, intrRegs);

		kernelFree(intrRegs);
	}
}

static const char *debugXhciTrbType2String(xhciTrb *trb)
{
	switch (trb->typeFlags & USBXHCI_TRBTYPE_MASK)
	{
		case USBXHCI_TRBTYPE_RESERVED:
			return "reserved";
		case USBXHCI_TRBTYPE_NORMAL:
			return "normal";
		case USBXHCI_TRBTYPE_SETUPSTG:
			return "setup stage";
		case USBXHCI_TRBTYPE_DATASTG:
			return "data stage";
		case USBXHCI_TRBTYPE_STATUSSTG:
			return "status stage";
		case USBXHCI_TRBTYPE_ISOCH:
			return "isochronous";
		case USBXHCI_TRBTYPE_LINK:
			return "link";
		case USBXHCI_TRBTYPE_EVENTDATA:
			return "event data";
		case USBXHCI_TRBTYPE_TRANSNOOP:
			return "transfer no-op";
		case USBXHCI_TRBTYPE_ENABLESLOT:
			return "enable slot";
		case USBXHCI_TRBTYPE_DISBLESLOT:
			return "disable slot";
		case USBXHCI_TRBTYPE_ADDRESSDEV:
			return "address device";
		case USBXHCI_TRBTYPE_CFGENDPT:
			return "configure endpoint";
		case USBXHCI_TRBTYPE_EVALCNTXT:
			return "evaluate context";
		case USBXHCI_TRBTYPE_RESETENDPT:
			return "reset endpoint";
		case USBXHCI_TRBTYPE_STOPENDPT:
			return "stop endpoint";
		case USBXHCI_TRBTYPE_SETTRDQ:
			return "set dequeue pointer";
		case USBXHCI_TRBTYPE_RESETDEV:
			return "reset device";
		case USBXHCI_TRBTYPE_FORCEEVNT:
			return "force event";
		case USBXHCI_TRBTYPE_NEGBNDWDTH:
			return "negotiate bandwidth";
		case USBXHCI_TRBTYPE_SETLATTVAL:
			return "set latency tolerance";
		case USBXHCI_TRBTYPE_GETPORTBW:
			return "get port bandwidth";
		case USBXHCI_TRBTYPE_FORCEHDR:
			return "force header";
		case USBXHCI_TRBTYPE_CMDNOOP:
			return "command no-op";
		case USBXHCI_TRBTYPE_TRANSFER:
			return "transfer event";
		case USBXHCI_TRBTYPE_CMDCOMP:
			return "command complete";
		case USBXHCI_TRBTYPE_PRTSTATCHG:
			return "port status change";
		case USBXHCI_TRBTYPE_BANDWREQ:
			return "bandwidth request";
		case USBXHCI_TRBTYPE_DOORBELL:
			return "doorbell";
		case USBXHCI_TRBTYPE_HOSTCONT:
			return "host controller event";
		case USBXHCI_TRBTYPE_DEVNOTIFY:
			return "device notification";
		case USBXHCI_TRBTYPE_MFIDXWRAP:
			return "mfindex wrap";
		default:
			return "unknown";
	}
}

static const char *debugXhciSpeed2String(xhciDevSpeed speed)
{
	switch (speed)
	{
		case xhcispeed_full:
			return "full";
		case xhcispeed_low:
			return "low";
		case xhcispeed_high:
			return "high";
		case xhcispeed_super:
			return "super";
		default:
			return "unknown";
	}
}

static inline void debugXhciPortStatus(usbXhciData *xhciData, int portNum)
{
	unsigned portsc = xhciData->opRegs->portRegSet[portNum].portsc;

	kernelDebug(debug_usb, "XHCI port %d status: 0x%08x\n"
		"  changes=0x%02x (%s%s%s%s%s%s%s)\n"
		"  indicator=%d\n"
		"  speed=%d\n"
		"  power=%d\n"
		"  linkState=0x%01x\n"
		"  reset=%d\n"
		"  overCurrent=%d\n"
		"  enabled=%d\n"
		"  connected=%d", portNum, portsc,
		((portsc & USBXHCI_PORTSC_CHANGES) >> 17),
		((portsc & USBXHCI_PORTSC_CONFERR_CH)? "conferr," : ""),
		((portsc & USBXHCI_PORTSC_LINKSTAT_CH)? "linkstat," : ""),
		((portsc & USBXHCI_PORTSC_RESET_CH)? "reset," : ""),
		((portsc & USBXHCI_PORTSC_OVERCURR_CH)? "overcurr," : ""),
		((portsc & USBXHCI_PORTSC_WARMREST_CH)? "warmreset," : ""),
		((portsc & USBXHCI_PORTSC_ENABLED_CH)? "enable," : ""),
		((portsc & USBXHCI_PORTSC_CONNECT_CH)? "connect," : ""),
		((portsc & USBXHCI_PORTSC_PORTIND) >> 14),
		((portsc & USBXHCI_PORTSC_PORTSPEED) >> 10),
		((portsc & USBXHCI_PORTSC_PORTPOWER) >> 9),
		((portsc & USBXHCI_PORTSC_LINKSTATE) >> 5),
		((portsc & USBXHCI_PORTSC_PORTRESET) >> 4),
		((portsc & USBXHCI_PORTSC_OVERCURRENT) >> 3),
		((portsc & USBXHCI_PORTSC_PORTENABLED) >> 1),
		(portsc & USBXHCI_PORTSC_CONNECTED));
}

static inline void debugXhciSlotCtxt(xhciSlotCtxt *ctxt)
{
	kernelDebug(debug_usb, "XHCI slot context:\n"
		"  contextEntries=%d\n"
		"  hub=%d\n"
		"  MTT=%d\n"
		"  speed=%d\n"
		"  routeString=0x%05x\n"
		"  numPorts=%d\n"
		"  portNum=%d\n"
		"  maxExitLatency=%d\n"
		"  interrupterTarget=%d\n"
		"  TTT=%d\n"
		"  ttPortNum=%d\n"
		"  ttHubSlotId=%d\n"
		"  slotState=%d\n"
		"  devAddr=%d",
		((ctxt->entFlagsSpeedRoute & USBXHCI_SLTCTXT_CTXTENTS) >> 27),
		((ctxt->entFlagsSpeedRoute & USBXHCI_SLTCTXT_HUB) >> 26),
		((ctxt->entFlagsSpeedRoute & USBXHCI_SLTCTXT_MTT) >> 25),
		((ctxt->entFlagsSpeedRoute & USBXHCI_SLTCTXT_SPEED) >> 20),
		(ctxt->entFlagsSpeedRoute & USBXHCI_SLTCTXT_ROUTESTRNG),
		((ctxt->numPortsPortLat & USBXHCI_SLTCTXT_NUMPORTS) >> 24),
		((ctxt->numPortsPortLat & USBXHCI_SLTCTXT_ROOTPRTNUM) >> 16),
		(ctxt->numPortsPortLat & USBXHCI_SLTCTXT_MAXEXITLAT),
		((ctxt->targetTT & USBXHCI_SLTCTXT_INTRTARGET) >> 22),
		((ctxt->targetTT & USBXHCI_SLTCTXT_TTT) >> 16),
		((ctxt->targetTT & USBXHCI_SLTCTXT_TTPORTNUM) >> 8),
		(ctxt->targetTT & USBXHCI_SLTCTXT_TTHUBSLOT),
		((ctxt->slotStateDevAddr & USBXHCI_SLTCTXT_SLOTSTATE) >> 27),
		(ctxt->slotStateDevAddr & USBXHCI_SLTCTXT_USBDEVADDR));
}

static inline void debugXhciTrb(xhciTrb *trb)
{
	kernelDebug(debug_usb, "XHCI TRB:\n"
		"  paramLo=0x%08x\n"
		"  paramHi=0x%08x\n"
		"  status=0x%08x\n"
		"  typeFlags=0x%04x (type=%s, flags=0x%03x)\n"
		"  control=0x%04x", trb->paramLo, trb->paramHi,
		trb->status, trb->typeFlags, debugXhciTrbType2String(trb),
		(trb->typeFlags & ~USBXHCI_TRBTYPE_MASK), trb->control);
}

static inline void debugXhciEndpointCtxt(xhciEndpointCtxt *ctxt)
{
	kernelDebug(debug_usb, "XHCI endpoint context:\n"
		"  interval=%d\n"
		"  linearStreamArray=%d\n"
		"  maxPrimaryStreams=%d\n"
		"  multiplier=%d\n"
		"  endpointState=%d\n"
		"  maxPacketSize=%d\n"
		"  maxBurstSize=%d\n"
		"  hostInitiateDisable=%d\n"
		"  endpointType=%d\n"
		"  errorCount=%d\n"
		"  trDequeuePtr=%p\n"
		"  maxEsitPayload=%d\n"
		"  avgTrbLen=%d",
		((ctxt->intvlLsaMaxPstrMultEpState & USBXHCI_EPCTXT_INTERVAL) >> 16),
		((ctxt->intvlLsaMaxPstrMultEpState & USBXHCI_EPCTXT_LINSTRARRAY) >> 15),
		((ctxt->intvlLsaMaxPstrMultEpState & USBXHCI_EPCTXT_MAXPRIMSTR) >> 10),
		((ctxt->intvlLsaMaxPstrMultEpState & USBXHCI_EPCTXT_MULT) >> 8),
		(ctxt->intvlLsaMaxPstrMultEpState & USBXHCI_EPCTXT_EPSTATE),
		((ctxt->maxPSizeMaxBSizeEpTypeCerr & USBXHCI_EPCTXT_MAXPKTSIZE) >> 16),
		((ctxt->maxPSizeMaxBSizeEpTypeCerr & USBXHCI_EPCTXT_MAXBRSTSIZE) >> 8),
		((ctxt->maxPSizeMaxBSizeEpTypeCerr & USBXHCI_EPCTXT_HSTINITDSBL) >> 7),
		((ctxt->maxPSizeMaxBSizeEpTypeCerr & USBXHCI_EPCTXT_ENDPNTTYPE) >> 3),
		((ctxt->maxPSizeMaxBSizeEpTypeCerr & USBXHCI_EPCTXT_CERR) >> 1),
		(void *) ctxt->trDeqPtrLo,
		((ctxt->maxEpEsitAvTrbLen & USBXHCI_EPCTXT_MAXESITPAYL) >> 16),
		(ctxt->maxEpEsitAvTrbLen & USBXHCI_EPCTXT_AVGTRBLEN));
}

static const char *debugXhciTrbCompletion2String(xhciTrb *trb)
{
	switch (trb->status & USBXHCI_TRBCOMP_MASK)
	{
		case USBXHCI_TRBCOMP_INVALID:
			return "invalid code";
		case USBXHCI_TRBCOMP_SUCCESS:
			return "success";
		case USBXHCI_TRBCOMP_DATABUFF:
			return "data buffer error";
		case USBXHCI_TRBCOMP_BABBLE:
			return "babble detected";
		case USBXHCI_TRBCOMP_TRANS:
			return "USB transaction error";
		case USBXHCI_TRBCOMP_TRB:
			return "TRB error";
		case USBXHCI_TRBCOMP_STALL:
			return "stall";
		case USBXHCI_TRBCOMP_RESOURCE:
			return "resource error";
		case USBXHCI_TRBCOMP_BANDWIDTH:
			return "bandwidth error";
		case USBXHCI_TRBCOMP_NOSLOTS:
			return "no slots available";
		case USBXHCI_TRBCOMP_INVALIDSTREAM:
			return "invalid stream type";
		case USBXHCI_TRBCOMP_SLOTNOTENAB:
			return "slot not enabled";
		case USBXHCI_TRBCOMP_ENDPTNOTENAB:
			return "endpoint not enabled";
		case USBXHCI_TRBCOMP_SHORTPACKET:
			return "short packet";
		case USBXHCI_TRBCOMP_RINGUNDERRUN:
			return "ring underrun";
		case USBXHCI_TRBCOMP_RINGOVERRUN:
			return "ring overrun";
		case USBXHCI_TRBCOMP_VFEVNTRINGFULL:
			return "VF event ring full";
		case USBXHCI_TRBCOMP_PARAMETER:
			return "parameter error";
		case USBXHCI_TRBCOMP_BANDWOVERRUN:
			return "bandwidth overrun";
		case USBXHCI_TRBCOMP_CONTEXTSTATE:
			return "context state error";
		case USBXHCI_TRBCOMP_NOPINGRESPONSE:
			return "no ping response";
		case USBXHCI_TRBCOMP_EVNTRINGFULL:
			return "event ring full";
		case USBXHCI_TRBCOMP_INCOMPATDEVICE:
			return "incompatible device";
		case USBXHCI_TRBCOMP_MISSEDSERVICE:
			return "missed service";
		case USBXHCI_TRBCOMP_CMDRINGSTOPPED:
			return "command ring stopped";
		case USBXHCI_TRBCOMP_COMMANDABORTED:
			return "command aborted";
		case USBXHCI_TRBCOMP_STOPPED:
			return "stopped";
		case USBXHCI_TRBCOMP_STOPPEDLENGTH:
			return "stopped - length invalid";
		case USBXHCI_TRBCOMP_MAXLATTOOLARGE:
			return "max exit latency";
		case USBXHCI_TRBCOMP_ISOCHBUFFOVER:
			return "isoch buffer overrun";
		case USBXHCI_TRBCOMP_EVENTLOST:
			return "event lost";
		case USBXHCI_TRBCOMP_UNDEFINED:
			return "undefined error";
		case USBXHCI_TRBCOMP_INVSTREAMID:
			return "invalid stream ID";
		case USBXHCI_TRBCOMP_SECBANDWIDTH:
			return "secondary bandwidth error";
		case USBXHCI_TRBCOMP_SPLITTRANS:
			return "split transaction error";
		default:
			return "(unknown)";
	}
}
#else
	#define debugXhciCapRegs(xhciData) do { } while (0)
	#define debugXhciOpRegs(xhciData) do { } while (0)
	#define debugXhciHcsParams1(xhciData) do { } while (0)
	#define debugXhciHcsParams2(xhciData) do { } while (0)
	#define debugXhciHcsParams3(xhciData) do { } while (0)
	#define debugXhciHccParams(xhciData) do { } while (0)
	#define debugXhciCmdStatRegs(xhciData) do { } while (0)
	#define debugXhciRuntimeRegs(xhciData) do { } while (0)
	#define debugXhciTrbType2String(trb) ""
	#define debugXhciSpeed2String(speed) ""
	#define debugXhciPortStatus(xhciData, portNum) do { } while (0)
	#define debugXhciSlotCtxt(ctxt) do { } while (0)
	#define debugXhciTrb(trb) do { } while (0)
	#define debugXhciEndpointCtxt(ctxt) do { } while (0)
	#define debugXhciTrbCompletion2String(trb) ""
#endif // DEBUG


static int processExtCaps(usbXhciData *xhciData)
{
	// If the controller has extended capabilities, such as legacy support that
	// requires a handover between the BIOS and the OS, we do that here.

	int status = 0;
	xhciExtendedCaps *extCap = NULL;
	xhciLegacySupport *legSupp = NULL;
	xhciSupportedProtocol *suppProto = NULL;
	int count;

	// Examine the extended capabilities
	extCap = ((void *) xhciData->capRegs +
		((xhciData->capRegs->hccparams & USBXHCI_HCCP_EXTCAPPTR) >> 14));

	while (1)
	{
		kernelDebug(debug_usb, "XHCI extended capability %d", extCap->id);

		// Is there legacy support?
		if (extCap->id == USBXHCI_EXTCAP_LEGACYSUPP)
		{
			kernelDebug(debug_usb, "XHCI legacy support implemented");

			legSupp = (xhciLegacySupport *) extCap;

			// Does the BIOS claim ownership of the controller?
			if (legSupp->legSuppCap & USBXHCI_LEGSUPCAP_BIOSOWND)
			{
				kernelDebug(debug_usb, "XHCI BIOS claims ownership, "
					"cap=0x%08x contStat=0x%08x", legSupp->legSuppCap,
					legSupp->legSuppContStat);

				// Attempt to take over ownership
				legSupp->legSuppCap |= USBXHCI_LEGSUPCAP_OSOWNED;

				// Wait for the BIOS to release ownership, if applicable
				for (count = 0; count < 200; count ++)
				{
					if ((legSupp->legSuppCap & USBXHCI_LEGSUPCAP_OSOWNED) &&
						!(legSupp->legSuppCap & USBXHCI_LEGSUPCAP_BIOSOWND))
					{
						kernelDebug(debug_usb, "XHCI OS ownership took %dms",
							count);
						break;
					}

					kernelCpuSpinMs(1);
				}

				// Do we have ownership?
				if (!(legSupp->legSuppCap & USBXHCI_LEGSUPCAP_OSOWNED) ||
					(legSupp->legSuppCap & USBXHCI_LEGSUPCAP_BIOSOWND))
				{
					kernelDebugError("BIOS did not release ownership");
					//kernelDebugStop();
				}
			}
			else
				kernelDebug(debug_usb, "XHCI BIOS does not claim ownership");

			// Make sure any SMIs are acknowledged and disabled
			legSupp->legSuppContStat = 0xE0000000;
			kernelDebug(debug_usb, "XHCI now cap=0x%08x, contStat=0x%08x",
				legSupp->legSuppCap, legSupp->legSuppContStat);
		}

		else if (extCap->id == USBXHCI_EXTCAP_SUPPPROTO)
		{
			char name[5];

			suppProto = (xhciSupportedProtocol *) extCap;

			strncpy(name, (char *) &suppProto->suppProtName, 4);
			name[4] = '\0';

			kernelDebug(debug_usb, "XHCI supported protocol \"%s\" %d.%d "
				"startPort=%d numPorts=%d",
				name, (suppProto->suppProtCap >> 24),
				((suppProto->suppProtCap >> 16) & 0xFF),
				((suppProto->suppProtPorts & 0xFF) - 1),
				((suppProto->suppProtPorts >> 8) & 0xFF));

			if (!strncmp(name, "USB ", 4))
			{
				for (count = ((suppProto->suppProtPorts & 0xFF) - 1);
					(count < (int)(((suppProto->suppProtPorts & 0xFF) - 1) +
						((suppProto->suppProtPorts >> 8) & 0xFF))); count ++)
				{
					if ((suppProto->suppProtCap >> 24) >= 2)
						xhciData->portProtos[count] = usbproto_usb2;
					if ((suppProto->suppProtCap >> 24) >= 3)
						xhciData->portProtos[count] = usbproto_usb3;

					kernelDebug(debug_usb, "XHCI port %d is protocol %d",
						count, xhciData->portProtos[count]);
				}
			}
		}

		if (extCap->next)
			extCap = ((void *) extCap + (extCap->next << 2));
		else
			break;
	}

	return (status = 0);
}


static int allocScratchPadBuffers(usbXhciData *xhciData,
	unsigned *scratchPadPhysical)
{
	int status = 0;
	int numScratchPads = ((xhciData->capRegs->hcsparams2 &
		USBXHCI_HCSP2_MAXSCRPBUFFS) >> 27);
	kernelIoMemory ioMem;
	unsigned long long *scratchPadBufferArray = NULL;
	unsigned buffer = NULL;
	int count;

	*scratchPadPhysical = NULL;

	if (!numScratchPads)
	{
		kernelDebug(debug_usb, "XHCI no scratchpad buffers required");
		return (status = 0);
	}

	kernelDebug(debug_usb, "XHCI allocating %d scratchpad buffers of %u",
		numScratchPads, xhciData->pageSize);

	// Allocate the array for pointers
	status = kernelMemoryGetIo((numScratchPads * sizeof(unsigned long long)),
		64 /* alignment */, &ioMem);
	if (status < 0)
		return (status);

	scratchPadBufferArray = ioMem.virtual;

	// Allocate each buffer.  We don't access these (they're purely for the
	// controller) so we don't need to allocate them as I/O memory.
	for (count = 0; count < numScratchPads; count ++)
	{
		buffer = kernelMemoryGetPhysical(xhciData->pageSize,
			xhciData->pageSize /* alignment */, "xhci scratchpad");
		if (!buffer)
		{
			status = ERR_MEMORY;
			goto out;
		}

		scratchPadBufferArray[count] = (unsigned long long) buffer;
	}

	*scratchPadPhysical = ioMem.physical;
	status = 0;

out:
	if ((status < 0) && scratchPadBufferArray)
	{
		for (count = 0; count < numScratchPads; count ++)
		{
			if (scratchPadBufferArray[count])
				kernelMemoryReleasePhysical((unsigned)
					scratchPadBufferArray[count]);
		}

		kernelMemoryReleaseIo(&ioMem);
	}

	return (status);
}


static void deallocTrbRing(xhciTrbRing *trbRing)
{
	// Dellocate a TRB ring.

	kernelIoMemory ioMem;

	if (trbRing->trbs)
	{
		ioMem.size = (trbRing->numTrbs * sizeof(xhciTrb));
		ioMem.physical = trbRing->trbsPhysical;
		ioMem.virtual = (void *) trbRing->trbs;
		kernelMemoryReleaseIo(&ioMem);
	}

	kernelFree(trbRing);
}


static xhciTrbRing *allocTrbRing(int numTrbs, int circular)
{
	// Allocate and link TRBs into a TRB ring, used for events, transfers, and
	// commands.

	xhciTrbRing *trbRing = NULL;
	unsigned memSize = 0;
	kernelIoMemory ioMem;

	kernelMemClear(&ioMem, sizeof(kernelIoMemory));

	// Allocate memory for the trbRing structure
	trbRing = kernelMalloc(sizeof(xhciTrbRing));
	if (!trbRing)
	{
		kernelError(kernel_error, "Couldn't get memory for TRB ring");
		//kernelDebugStop();
		goto err_out;
	}

	trbRing->numTrbs = numTrbs;
	trbRing->cycleState = USBXHCI_TRBFLAG_CYCLE;

	// How much memory do we need for TRBs?
	memSize = (numTrbs * sizeof(xhciTrb));

	// Request the memory
	if (kernelMemoryGetIo(memSize, 64 /* alignment */, &ioMem) < 0)
	{
		kernelError(kernel_error, "Couldn't get memory for TRBs");
		//kernelDebugStop();
		goto err_out;
	}

	trbRing->trbs = ioMem.virtual;
	trbRing->trbsPhysical = ioMem.physical;

	if (circular)
	{
		// Use the last TRB as a 'link' back to the beginning of the ring
		trbRing->trbs[trbRing->numTrbs - 1].paramLo = trbRing->trbsPhysical;
		trbRing->trbs[trbRing->numTrbs - 1].typeFlags =
			(USBXHCI_TRBTYPE_LINK | USBXHCI_TRBFLAG_TOGGLECYCL);
	}

	// Return success
	return (trbRing);

err_out:
	if (trbRing)
		deallocTrbRing(trbRing);

	return (trbRing = NULL);
}


static int initInterrupter(usbXhciData *xhciData)
{
	// Set up the numbered interrupter

	int status = 0;
	kernelIoMemory ioMem;
	xhciEventRingSegTable *segTable = NULL;
	void *segTablePhysical = NULL;

	kernelMemClear(&ioMem, sizeof(kernelIoMemory));

	kernelDebug(debug_usb, "XHCI initialize interrupter %d (max=%d)",
		xhciData->numIntrs, ((xhciData->capRegs->hcsparams1 &
			USBXHCI_HCSP1_MAXINTRPTRS) >> 8));

	// Expand the array for holding pointers to event rings
	xhciData->eventRings = kernelRealloc(xhciData->eventRings,
		((xhciData->numIntrs + 1) * sizeof(xhciTrbRing *)));
	if (!xhciData->eventRings)
	{
		status = ERR_MEMORY;
		goto err_out;
	}

	// Allocate a TRB ring for events
	xhciData->eventRings[xhciData->numIntrs] =
		allocTrbRing(USBXHCI_EVENTRING_SIZE, 0 /* not circular */);
	if (!xhciData->eventRings[xhciData->numIntrs])
	{
		status = ERR_MEMORY;
		goto err_out;
	}

	kernelDebug(debug_usb, "XHCI eventRings[%d]->trbsPhysical=0x%08x",
		xhciData->numIntrs,
		xhciData->eventRings[xhciData->numIntrs]->trbsPhysical);

	// Get some aligned memory for the segment table
	status = kernelMemoryGetIo(sizeof(xhciEventRingSegTable),
		 64 /* alignment */, &ioMem);
	if (status < 0)
		goto err_out;

	segTable = ioMem.virtual;
	segTablePhysical = (void *) ioMem.physical;

	// Point the segment table to the TRB ring
	segTable->baseAddrLo =
		xhciData->eventRings[xhciData->numIntrs]->trbsPhysical;
	segTable->segSize = USBXHCI_EVENTRING_SIZE;

	// Update the interrupter's register set to point to the segment table
	xhciData->rtRegs->intrReg[xhciData->numIntrs].intrMod = 0x00000FA0; // 1ms
	xhciData->rtRegs->intrReg[xhciData->numIntrs].evtRngSegTabSz = 1;
	xhciData->rtRegs->intrReg[xhciData->numIntrs].evtRngDeqPtrLo =
		xhciData->eventRings[xhciData->numIntrs]->trbsPhysical;
	xhciData->rtRegs->intrReg[xhciData->numIntrs].evtRngDeqPtrHi = 0;
	xhciData->rtRegs->intrReg[xhciData->numIntrs].evtRngSegBaseLo =
		(unsigned) segTablePhysical;
	xhciData->rtRegs->intrReg[xhciData->numIntrs].evtRngSegBaseHi = 0;
	xhciData->rtRegs->intrReg[xhciData->numIntrs].intrMan =
		USBXHCI_IMAN_INTSENABLED;

	xhciData->numIntrs += 1;

	//debugXhciRuntimeRegs(controller);

	return (status = 0);

err_out:
	if (ioMem.virtual)
		kernelMemoryReleaseIo(&ioMem);

	if (xhciData->eventRings[xhciData->numIntrs])
		deallocTrbRing(xhciData->eventRings[xhciData->numIntrs]);

	return (status);
}


static int setup(usbXhciData *xhciData)
{
	// Allocate things, and set up any global controller registers prior to
	// changing the controller to the 'running' state

	int status = 0;
	unsigned devCtxtPhysPtrsMemSize = 0;
	kernelIoMemory ioMem;
	void *devCtxtPhysPtrsPhysical = NULL;
	unsigned scratchPadBufferArray = 0;

	kernelMemClear(&ioMem, sizeof(kernelIoMemory));

#if defined(DEBUG)
	// Check the sizes of some structures
	if (sizeof(xhciCtxt) != 32)
	{
		kernelDebugError("XHCI sizeof(xhciCtxt) is %u, not 32",
			sizeof(xhciCtxt));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciSlotCtxt) != 32)
	{
		kernelDebugError("XHCI sizeof(xhciSlotCtxt) is %u, not 32",
			sizeof(xhciSlotCtxt));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciEndpointCtxt) != 32)
	{
		kernelDebugError("XHCI sizeof(xhciEndpointCtxt) is %u, not 32",
			sizeof(xhciEndpointCtxt));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciInputCtrlCtxt) != 32)
	{
		kernelDebugError("XHCI sizeof(xhciInputCtrlCtxt) is %u, not 32",
			sizeof(xhciInputCtrlCtxt));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciInputCtxt) != 1056)
	{
		kernelDebugError("XHCI sizeof(xhciInputCtxt) is %u, not 1056",
			sizeof(xhciDevCtxt));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciDevCtxt) != 1024)
	{
		kernelDebugError("XHCI sizeof(xhciDevCtxt) is %u, not 1024",
			sizeof(xhciDevCtxt));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciTrb) != 16)
	{
		kernelDebugError("XHCI sizeof(xhciTrb) is %u, not 16", sizeof(xhciTrb));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciPortRegSet) != 16)
	{
		kernelDebugError("XHCI sizeof(xhciPortRegSet) is %u, not 16",
			sizeof(xhciPortRegSet));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciOpRegs) != 5120)
	{
		kernelDebugError("XHCI sizeof(xhciOpRegs) is %u, not 5120",
			sizeof(xhciOpRegs));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciCapRegs) != 28)
	{
		kernelDebugError("XHCI sizeof(xhciCapRegs) is %u, not 28",
			sizeof(xhciCapRegs));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciIntrRegSet) != 32)
	{
		kernelDebugError("XHCI sizeof(xhciIntrRegSet) is %u, not 32",
			sizeof(xhciIntrRegSet));
		status = ERR_ALIGN;
		goto err_out;
	}
	if (sizeof(xhciRuntimeRegs) != 32)

	{
		kernelDebugError("XHCI sizeof(xhciRuntimeRegs) is %u, not 32",

			sizeof(xhciRuntimeRegs));
		status = ERR_ALIGN;

		goto err_out;
	}

	if (sizeof(xhciDoorbellRegs) != 1024)
	{

		kernelDebugError("XHCI sizeof(xhciDoorbellRegs) is %u, not 1024",
			sizeof(xhciDoorbellRegs));

		status = ERR_ALIGN;
		goto err_out;

	}
	if (sizeof(xhciEventRingSegTable) != 16)
	{
		kernelDebugError("XHCI sizeof(xhciEventRingSegTable) is %u, not 16",
			sizeof(xhciEventRingSegTable));
		status = ERR_ALIGN;
		goto err_out;
	}
#endif

	// Program the max device slots enabled field in the config register to
	// enable the device slots that system software is going to use
	xhciData->opRegs->config = xhciData->numDevSlots;

	// Program the device context base address array pointer

	// How much memory is needed for the (64-bit) pointers to the device
	// contexts?
	devCtxtPhysPtrsMemSize = ((xhciData->numDevSlots + 1) *
		sizeof(unsigned long long));

	// Request memory for an aligned array of pointers to device contexts
	status = kernelMemoryGetIo(devCtxtPhysPtrsMemSize, 64 /* alignment */,
		&ioMem);
	if (status < 0)
		goto err_out;

	xhciData->devCtxtPhysPtrs = ioMem.virtual;
	devCtxtPhysPtrsPhysical = (void *) ioMem.physical;

	kernelDebug(debug_usb, "XHCI device context base array memory=%p",
		devCtxtPhysPtrsPhysical);

	// Allocate the scratchpad buffers requested by the controller
	status = allocScratchPadBuffers(xhciData, &scratchPadBufferArray);
	if (status < 0)
		goto err_out;

	if (scratchPadBufferArray)
		xhciData->devCtxtPhysPtrs[0] = scratchPadBufferArray;

	// Set the device context base address array pointer in the host
	// controller register
	xhciData->opRegs->dcbaapLo = (unsigned) devCtxtPhysPtrsPhysical;
	xhciData->opRegs->dcbaapHi = 0;

	// Allocate the command ring
	xhciData->commandRing = allocTrbRing(USBXHCI_COMMANDRING_SIZE,
		1 /* circular */);
	if (!xhciData->commandRing)
	{
		status = ERR_MEMORY;
		goto err_out;
	}

	kernelDebug(debug_usb, "XHCI commandRing->trbsPhysical=0x%08x",
		xhciData->commandRing->trbsPhysical);

	// Define the command ring dequeue pointer by programming the command ring
	// control register with the 64-bit address of the first TRB in the command
	// ring
	xhciData->opRegs->cmdrctrlLo =
		(xhciData->commandRing->trbsPhysical | USBXHCI_CRCR_RINGCYCSTATE);
	xhciData->opRegs->cmdrctrlHi = 0;

	// Initialize interrupts

	// Initialize each the 1st (primary) interrupter
	status = initInterrupter(xhciData);
	if (status < 0)
		goto err_out;

	// Enable the interrupts we're interested in, in the command register;
	// interrupter, and host system error
	xhciData->opRegs->cmd |=
		(USBXHCI_CMD_HOSTSYSERRENBL | USBXHCI_CMD_INTERUPTRENBL);

	// Return success
	return (status = 0);

err_out:
	if (xhciData->commandRing)
		deallocTrbRing(xhciData->commandRing);

	if (ioMem.virtual)
		kernelMemoryReleaseIo(&ioMem);

	return (status);
}


static inline void setPortStatusBits(usbXhciData *xhciData, int portNum,
	unsigned bits)
{
	// Set (or clear write-1-to-clear) the requested  port status bits, without
	// affecting the others

	xhciData->opRegs->portRegSet[portNum].portsc =
		((xhciData->opRegs->portRegSet[portNum].portsc &
			~(USBXHCI_PORTSC_ROMASK | USBXHCI_PORTSC_RW1CMASK)) | bits);
}


static int setPortPower(usbXhciData *xhciData, int portNum, int on)
{
	int status = 0;
	int count;

	kernelDebug(debug_usb, "XHCI power %s port %d", (on? "on" : "off"),
		portNum);

	if (on && !(xhciData->opRegs->portRegSet[portNum].portsc &
		USBXHCI_PORTSC_PORTPOWER))
	{
		// Set the power on bit and clear all port status 'change' bits
		setPortStatusBits(xhciData, portNum,
			(USBXHCI_PORTSC_CHANGES | USBXHCI_PORTSC_PORTPOWER));

		// Wait for it to read as set
		for (count = 0; count < 20; count ++)
		{
			if (xhciData->opRegs->portRegSet[portNum].portsc &
				USBXHCI_PORTSC_PORTPOWER)
			{
				kernelDebug(debug_usb, "XHCI powering up took %dms", count);
				break;
			}

			kernelCpuSpinMs(1);
		}

		if (!(xhciData->opRegs->portRegSet[portNum].portsc &
			USBXHCI_PORTSC_PORTPOWER))
		{
			kernelError(kernel_error, "XHCI: unable to power on port %d",
				portNum);
			//kernelDebugStop();
			return (status = ERR_IO);
		}
	}
	else if (!on)
	{
		// Would we ever need this?
		kernelDebugError("Port power off not implemented");
		//kernelDebugStop();
		return (status = ERR_NOTIMPLEMENTED);
	}

	return (status = 0);
}


static int startStop(usbXhciData *xhciData, int start)
{
	// Start or stop the XHCI controller

	int status = 0;
	int count;

	kernelDebug(debug_usb, "XHCI st%s controller", (start? "art" : "op"));

	if (start)
	{
		// Set the run/stop bit
		xhciData->opRegs->cmd |= USBXHCI_CMD_RUNSTOP;

		// Wait for not halted
		for (count = 0; count < 20; count ++)
		{
			if (!(xhciData->opRegs->stat & USBXHCI_STAT_HCHALTED))
			{
				kernelDebug(debug_usb, "XHCI starting took %dms", count);
				break;
			}

			kernelCpuSpinMs(1);
		}

		// Started?
		if (xhciData->opRegs->stat & USBXHCI_STAT_HCHALTED)
		{
			kernelError(kernel_error, "Couldn't clear controller halted bit");
			//kernelDebugStop();
			status = ERR_TIMEOUT;
			goto out;
		}
	}
	else // stop
	{
		// Make sure the command ring is stopped
		if (xhciData->opRegs->cmdrctrlLo & USBXHCI_CRCR_CMDRNGRUNNING)
		{
			kernelDebug(debug_usb, "XHCI stopping command ring");
			xhciData->opRegs->cmdrctrlLo = USBXHCI_CRCR_COMMANDABORT;
			xhciData->opRegs->cmdrctrlHi = 0;

			// Wait for stopped
			for (count = 0; count < 5000; count ++)
			{
				if (!(xhciData->opRegs->cmdrctrlLo &
					USBXHCI_CRCR_CMDRNGRUNNING))
				{
					kernelDebug(debug_usb, "XHCI stopping command ring took "
						"%dms", count);
					break;
				}

				kernelCpuSpinMs(1);
			}

			// Stopped?
			if (xhciData->opRegs->cmdrctrlLo & USBXHCI_CRCR_CMDRNGRUNNING)
			{
				kernelError(kernel_warn, "Couldn't stop command ring");
				//kernelDebugStop();
			}
		}

		// Clear the run/stop bit
		xhciData->opRegs->cmd &= ~USBXHCI_CMD_RUNSTOP;

		// Wait for halted
		for (count = 0; count < 20; count ++)
		{
			if (xhciData->opRegs->stat & USBXHCI_STAT_HCHALTED)
			{
				kernelDebug(debug_usb, "XHCI stopping controller took %dms",
					count);
				break;
			}

			kernelCpuSpinMs(1);
		}

		// Stopped?
		if (!(xhciData->opRegs->stat & USBXHCI_STAT_HCHALTED))
		{
			kernelError(kernel_error, "Couldn't set controller halted bit");
			//kernelDebugStop();
			status = ERR_TIMEOUT;
			goto out;
		}
	}

out:
	kernelDebug(debug_usb, "XHCI controller %sst%sed", (status? "not " : ""),
		(start? "art" : "opp"));

	return (status);
}


static inline void clearStatusBits(usbXhciData *xhciData, unsigned bits)
{
	// Clear the requested write-1-to-clear status bits, without affecting
	// the others

	xhciData->opRegs->stat =
		((xhciData->opRegs->stat &
			~(USBXHCI_STAT_ROMASK | USBXHCI_STAT_RW1CMASK)) | bits);
}


static inline unsigned trbPhysical(xhciTrbRing *ring, xhciTrb *trb)
{
	return (ring->trbsPhysical +
		(unsigned)((void *) trb - (void *) &ring->trbs[0]));
}


static inline int ringNextTrb(xhciTrbRing *transRing)
{
	int nextTrb = (transRing->nextTrb + 1);

	if ((nextTrb >= transRing->numTrbs) ||
		((transRing->trbs[nextTrb].typeFlags & USBXHCI_TRBTYPE_MASK) ==
			USBXHCI_TRBTYPE_LINK))
	{
		nextTrb = 0;
	}

	return (nextTrb);
}


static int getEvent(usbXhciData *xhciData, int intrNum, xhciTrb *destTrb,
	int consume)
{
	int status = 0;
	xhciIntrRegSet *regSet = NULL;
	xhciTrbRing *eventRing = NULL;
	xhciTrb *eventTrb = NULL;

	regSet = &(xhciData->rtRegs->intrReg[intrNum]);
	eventRing = xhciData->eventRings[intrNum];
	eventTrb = &(eventRing->trbs[eventRing->nextTrb]);

	if ((eventTrb->typeFlags & USBXHCI_TRBFLAG_CYCLE) == eventRing->cycleState)
	{
		kernelDebug(debug_usb, "XHCI next event TRB %d type=%d (%s) 0x%08x "
			"cyc=%d", eventRing->nextTrb,
			((eventTrb->typeFlags & USBXHCI_TRBTYPE_MASK) >> 10),
			debugXhciTrbType2String(eventTrb),
			trbPhysical(eventRing, eventTrb),
			(eventTrb->typeFlags & USBXHCI_TRBFLAG_CYCLE));

		// Copy it
		kernelMemCopy((void *) eventTrb, (void *) destTrb, sizeof(xhciTrb));

		if (consume)
		{
			kernelDebug(debug_usb, "XHCI consume event TRB %d type=%d (%s) "
				"0x%08x cyc=%d", eventRing->nextTrb,
				((eventTrb->typeFlags & USBXHCI_TRBTYPE_MASK) >> 10),
				debugXhciTrbType2String(eventTrb),
				trbPhysical(eventRing, eventTrb),
				(eventTrb->typeFlags & USBXHCI_TRBFLAG_CYCLE));

			// Move to the next TRB
			eventRing->nextTrb = ringNextTrb(eventRing);
			if (eventRing->nextTrb == 0)
				eventRing->cycleState ^= 1;

			// Update the controller's event ring dequeue TRB pointer to point
			// to the next one we expect to process, and clear the 'handler
			// busy' flag
			regSet->evtRngDeqPtrLo =
				(trbPhysical(eventRing, &eventRing->trbs[eventRing->nextTrb]) |
					USBXHCI_ERDP_HANDLERBUSY);
			regSet->evtRngDeqPtrHi = 0;
		}

		return (status = 0);
	}

	// No data
	return (status = ERR_NODATA);
}


static int command(usbXhciData *xhciData, xhciTrb *cmdTrb)
{
	// Place a command in the command ring

	int status = 0;
	xhciTrb *nextTrb = NULL;
	xhciTrb eventTrb;
	int count;

	kernelDebug(debug_usb, "XHCI command %d (%s) position %d",
		((cmdTrb->typeFlags & USBXHCI_TRBTYPE_MASK) >> 10),
		debugXhciTrbType2String(cmdTrb), xhciData->commandRing->nextTrb);

	nextTrb = &(xhciData->commandRing->trbs[xhciData->commandRing->nextTrb]);

	kernelDebug(debug_usb, "XHCI use TRB with physical address=0x%08x",
		trbPhysical(xhciData->commandRing, nextTrb));

	// Set the cycle bit
	if (xhciData->commandRing->cycleState)
		cmdTrb->typeFlags |= USBXHCI_TRBFLAG_CYCLE;
	else
		cmdTrb->typeFlags &= ~USBXHCI_TRBFLAG_CYCLE;

	// Copy the command
	kernelMemCopy((void *) cmdTrb, (void *) nextTrb, sizeof(xhciTrb));

	// Ring the command doorbell
	xhciData->dbRegs->doorbell[0] = 0;

	// Wait until the command has completed
	for (count = 0; count < USB_STD_TIMEOUT_MS; count ++)
	{
		kernelMemClear((void *) &eventTrb, sizeof(xhciTrb));

		if (!getEvent(xhciData, 0, &eventTrb, 1) &&
			((eventTrb.typeFlags & USBXHCI_TRBTYPE_MASK) ==
				USBXHCI_TRBTYPE_CMDCOMP))
		{
			kernelDebug(debug_usb, "XHCI got command completion event for "
				"TRB 0x%08x", (eventTrb.paramLo & ~0xFU));

			kernelDebug(debug_usb, "XHCI completion code %d",
				((eventTrb.status & USBXHCI_TRBCOMP_MASK) >> 24));

			if ((eventTrb.paramLo & ~0xFU) ==
				trbPhysical(xhciData->commandRing, nextTrb))
			{
				break;
			}
		}

		kernelCpuSpinMs(1);
	}

	if (count >= USB_STD_TIMEOUT_MS)
	{
		kernelDebugError("No command event received");
		//kernelDebugStop();
		return (status = ERR_TIMEOUT);
	}

	// Copy the completion event TRB back to the command TRB
	kernelMemCopy((void *) &eventTrb, (void *) cmdTrb, sizeof(xhciTrb));

	// Advance the nextTrb 'enqueue pointer'
	xhciData->commandRing->nextTrb = ringNextTrb(xhciData->commandRing);
	if (xhciData->commandRing->nextTrb == 0)
	{
		// Update the cycle bit of the link TRB
		if (xhciData->commandRing->cycleState)
			xhciData->commandRing->trbs[xhciData->commandRing->numTrbs - 1]
				.typeFlags |= USBXHCI_TRBFLAG_CYCLE;
		else
			xhciData->commandRing->trbs[xhciData->commandRing->numTrbs - 1]
				.typeFlags &= ~USBXHCI_TRBFLAG_CYCLE;

		xhciData->commandRing->cycleState ^= 1;
	}

	return (status = 0);
}


static int deallocSlot(usbXhciData *xhciData, xhciSlot *slot)
{
	// Deallocate a slot, also releasing it in the controller, if applicable

	int status = 0;
	xhciTrb cmdTrb;
	kernelIoMemory ioMem;
	int count;

	kernelDebug(debug_usb, "XHCI de-allocate device slot");

	// Remove it from the controller's list
	status = kernelLinkedListRemove(&xhciData->slots, slot);
	if (status < 0)
		return (status);

	// Send a 'disable slot' command
	kernelMemClear((void *) &cmdTrb, sizeof(xhciTrb));
	cmdTrb.typeFlags = USBXHCI_TRBTYPE_DISBLESLOT;
	cmdTrb.control = (slot->num << 8);
	status = command(xhciData, &cmdTrb);
	if (status < 0)
		return (status);

	if ((cmdTrb.status & USBXHCI_TRBCOMP_MASK) != USBXHCI_TRBCOMP_SUCCESS)
	{
		kernelError(kernel_error, "Command error %d disabling device slot",
			((cmdTrb.status & USBXHCI_TRBCOMP_MASK) >> 24));
		return (status = ERR_IO);
	}

	// Delete the reference to the device context from the device context
	// base address array
	xhciData->devCtxtPhysPtrs[slot->num] = NULL;

	// Free memory

	if (slot->devCtxt)
	{
		ioMem.size = sizeof(xhciDevCtxt);
		ioMem.physical = slot->devCtxtPhysical;
		ioMem.virtual = (void *) slot->devCtxt;
		kernelMemoryReleaseIo(&ioMem);
	}

	for (count = 0; count < USB_MAX_ENDPOINTS; count ++)
		if (slot->transRings[count])
			deallocTrbRing(slot->transRings[count]);

	if (slot->inputCtxt)
	{
		ioMem.size = sizeof(xhciInputCtxt);
		ioMem.physical = slot->inputCtxtPhysical;
		ioMem.virtual = (void *) slot->inputCtxt;
		kernelMemoryReleaseIo(&ioMem);
	}

	status = kernelFree(slot);
	if (status < 0)
		return (status);

	return (status = 0);
}


static xhciDevSpeed usbSpeed2XhciSpeed(usbDevSpeed usbSpeed)
{
	switch (usbSpeed)
	{
		case usbspeed_full:
			return xhcispeed_full;
		case usbspeed_low:
			return xhcispeed_low;
		case usbspeed_high:
			return xhcispeed_high;
		case usbspeed_super:
			return xhcispeed_super;
		default:
			return xhcispeed_unknown;
	}
}


static int getHubSlotNum(usbXhciData *xhciData, usbDevice *usbDev)
{
	// For low/full-speed devices attached to high speed hubs, we need to get
	// the slot number of the hub, for use in the device's slot context

	int slotNum = 0;
	usbDevice *hubDev = usbDev->hub->usbDev;
	xhciSlot *slot = NULL;
	kernelLinkedListItem *iter = NULL;

	if (!hubDev)
		// It's probably on a root port
		return (slotNum = 0);

	slot = kernelLinkedListIterStart(&xhciData->slots, &iter);
	while (slot)
	{
		if (slot->usbDev == hubDev)
		{
			slotNum = slot->num;
			break;
		}

		slot = kernelLinkedListIterNext(&xhciData->slots, &iter);
	}

	return (slotNum);
}


static int allocEndpoint(xhciSlot *slot, int endpoint, int endpointType,
	int interval, int maxPacketSize, int maxBurst)
{
	// Allocate a transfer ring and initialize the endpoint context.

	int status = 0;
	xhciEndpointCtxt *inputEndpointCtxt = NULL;
	unsigned avgTrbLen = 0;

	kernelDebug(debug_usb, "XHCI initialize endpoint 0x%02x", endpoint);

	// Allocate the transfer ring for the endpoint
	slot->transRings[endpoint & 0xF] =
		allocTrbRing(USBXHCI_TRANSRING_SIZE, 1 /* circular */);
	if (!slot->transRings[endpoint & 0xF])
		return (status = ERR_MEMORY);

	// Get a pointer to the endpoint input context
	if (endpoint)
		inputEndpointCtxt = &slot->inputCtxt->devCtxt
			.endpointCtxt[(((endpoint & 0xF) * 2) - 1) + (endpoint >> 7)];
	else
		inputEndpointCtxt = &slot->inputCtxt->devCtxt.endpointCtxt[0];

	// Initialize the input endpoint context
	inputEndpointCtxt->intvlLsaMaxPstrMultEpState =
		((interval << 16) & USBXHCI_EPCTXT_INTERVAL);
	inputEndpointCtxt->maxPSizeMaxBSizeEpTypeCerr =
		(((maxPacketSize << 16) & USBXHCI_EPCTXT_MAXPKTSIZE) |
			((maxBurst << 8) & USBXHCI_EPCTXT_MAXBRSTSIZE) |
			((endpointType << 3) & USBXHCI_EPCTXT_ENDPNTTYPE) |
			((3 << 1) & USBXHCI_EPCTXT_CERR) /* cerr */);
	inputEndpointCtxt->trDeqPtrLo =
		(slot->transRings[endpoint & 0xF]->trbsPhysical |
			USBXHCI_TRBFLAG_CYCLE);

	switch (endpointType)
	{
		case USBXHCI_EPTYPE_CONTROL:
			avgTrbLen = 0x8;
			break;
		case USBXHCI_EPTYPE_INTR_OUT:
		case USBXHCI_EPTYPE_INTR_IN:
			avgTrbLen = 0x400;
			break;
		case USBXHCI_EPTYPE_ISOCH_OUT:
		case USBXHCI_EPTYPE_ISOCH_IN:
		case USBXHCI_EPTYPE_BULK_OUT:
		case USBXHCI_EPTYPE_BULK_IN:
			avgTrbLen = 0xC00;
			break;
	}

	inputEndpointCtxt->maxEpEsitAvTrbLen =
		(avgTrbLen & USBXHCI_EPCTXT_AVGTRBLEN);

	return (status = 0);
}


static xhciSlot *allocSlot(usbXhciData *xhciData, usbDevice *usbDev)
{
	// Ask the controller for a new device slot.  If we get one, allocate
	// data structures for it.

	xhciSlot *slot = NULL;
	xhciTrb cmdTrb;
	kernelIoMemory ioMem;
	int maxPacketSize0 = 0;

	kernelDebug(debug_usb, "XHCI allocate new device slot");

	// Send an 'enable slot' command to get a device slot
	kernelMemClear((void *) &cmdTrb, sizeof(xhciTrb));
	cmdTrb.typeFlags = USBXHCI_TRBTYPE_ENABLESLOT;
	if (command(xhciData, &cmdTrb) < 0)
		return (slot = NULL);

	if ((cmdTrb.status & USBXHCI_TRBCOMP_MASK) != USBXHCI_TRBCOMP_SUCCESS)
	{
		kernelError(kernel_error, "Command error %d enabling device slot",
			((cmdTrb.status & USBXHCI_TRBCOMP_MASK) >> 24));
		//kernelDebugStop();
		return (slot = NULL);
	}

	slot = kernelMalloc(sizeof(xhciSlot));
	if (!slot)
		return (slot);

	// Record the device slot number and device
	kernelMemClear(slot, sizeof(xhciSlot));
	slot->num = (cmdTrb.control >> 8);
	slot->usbDev = usbDev;

	kernelDebug(debug_usb, "XHCI got device slot %d from controller",
		slot->num);

	// Allocate I/O memory for the input context
	if (kernelMemoryGetIo(sizeof(xhciInputCtxt),
		xhciData->pageSize /* alignment on page boundary */, &ioMem) < 0)
	{
		goto err_out;
	}

	slot->inputCtxt = ioMem.virtual;
	slot->inputCtxtPhysical = ioMem.physical;

	// Set the A0 and A1 bits of the input control context
	slot->inputCtxt->inputCtrlCtxt.add = (BIT(0) | BIT(1));

	// Initialize the input slot context data structure
	slot->inputCtxt->devCtxt.slotCtxt.entFlagsSpeedRoute =
		(((1 << 27) & USBXHCI_SLTCTXT_CTXTENTS) /* context entries = 1 */ |
		((usbSpeed2XhciSpeed(usbDev->speed) << 20) &
			USBXHCI_SLTCTXT_SPEED) |
		(usbDev->routeString & USBXHCI_SLTCTXT_ROUTESTRNG));

	slot->inputCtxt->devCtxt.slotCtxt.numPortsPortLat =
		(((slot->usbDev->rootPort + 1) << 16) &
			USBXHCI_SLTCTXT_ROOTPRTNUM);

	if (usbDev->hub->usbDev && ((usbDev->speed == usbspeed_low) ||
		(usbDev->speed == usbspeed_full)))
	{
		slot->inputCtxt->devCtxt.slotCtxt.targetTT =
			((((usbDev->hubPort + 1) << 8) & USBXHCI_SLTCTXT_TTPORTNUM) |
				(getHubSlotNum(xhciData, usbDev) & USBXHCI_SLTCTXT_TTHUBSLOT));
	}

	// Super-speed, high-speed, and low-speed devices have fixed maximum
	// packet sizes.  Full-speed devices need to be queried, so start with
	// the minimum of 8.
	switch (usbDev->speed)
	{
		case usbspeed_super:
			maxPacketSize0 = 512;
			break;
		case usbspeed_high:
			maxPacketSize0 = 64;
			break;
		case usbspeed_low:
		default:
			maxPacketSize0 = 8;
			break;
	}

	// Allocate the control endpoint
	if (allocEndpoint(slot, 0, USBXHCI_EPTYPE_CONTROL, 0, maxPacketSize0,
		0) < 0)
	{
		goto err_out;
	}

	// Allocate I/O memory for the device context
	if (kernelMemoryGetIo(sizeof(xhciDevCtxt),
		xhciData->pageSize /* alignment on page boundary */, &ioMem) < 0)
	{
		goto err_out;
	}

	slot->devCtxt = ioMem.virtual;
	slot->devCtxtPhysical = ioMem.physical;

	// Record the physical address in the device context base address array
	xhciData->devCtxtPhysPtrs[slot->num] = slot->devCtxtPhysical;

	// Add it to the list
	if (kernelLinkedListAdd(&xhciData->slots, slot) < 0)
		goto err_out;

	return (slot);

err_out:

	if (slot)
		deallocSlot(xhciData, slot);

	return (slot = NULL);
}


static int setDevAddress(usbXhciData *xhciData, xhciSlot *slot,
	usbDevice *usbDev)
{
	int status = 0;
	xhciTrb cmdTrb;
	xhciEndpointCtxt *inputEndpointCtxt = NULL;

	// If usbDev is NULL, that tells us we're only doing this to enable the
	// control endpoint on the controller, but that we don't want to send
	// a USB_SET_ADDRESS to the device.

	// Send an 'address device' command
	kernelMemClear((void *) &cmdTrb, sizeof(xhciTrb));
	cmdTrb.paramLo = slot->inputCtxtPhysical;
	cmdTrb.typeFlags = USBXHCI_TRBTYPE_ADDRESSDEV;
	if (!usbDev)
		cmdTrb.typeFlags|= USBXHCI_TRBFLAG_BLKSETADDR;
	cmdTrb.control = (slot->num << 8);

	status = command(xhciData, &cmdTrb);
	if (status < 0)
		return (status);

	if ((cmdTrb.status & USBXHCI_TRBCOMP_MASK) != USBXHCI_TRBCOMP_SUCCESS)
	{
		debugXhciSlotCtxt(&slot->inputCtxt->devCtxt.slotCtxt);
		kernelError(kernel_error, "Command error %d addressing device",
			((cmdTrb.status & USBXHCI_TRBCOMP_MASK) >> 24));
		//kernelDebugStop();
		return (status = ERR_IO);
	}

	//debugXhciSlotCtxt(&slot->devCtxt->slotCtxt);
	//debugXhciEndpointCtxt(&slot->devCtxt->endpointCtxt[0]);

	if (usbDev)
	{
		// Set the address in the USB device
		usbDev->address = (slot->devCtxt->slotCtxt.slotStateDevAddr &
			USBXHCI_SLTCTXT_USBDEVADDR);

		kernelDebug(debug_usb, "XHCI device address is now %d",
			usbDev->address);

		// If it's a full-speed device, now is the right time to set the control
		// endpoint packet size
		if (usbDev->speed == usbspeed_full)
		{
			inputEndpointCtxt = &slot->inputCtxt->devCtxt.endpointCtxt[0];

			inputEndpointCtxt->maxPSizeMaxBSizeEpTypeCerr &=
				~USBXHCI_EPCTXT_MAXPKTSIZE;
			inputEndpointCtxt->maxPSizeMaxBSizeEpTypeCerr |=
				((usbDev->deviceDesc.maxPacketSize0 << 16) &
					USBXHCI_EPCTXT_MAXPKTSIZE);

			// Set the 'add' bit of the input control context
			slot->inputCtxt->inputCtrlCtxt.add = BIT(1);
			slot->inputCtxt->inputCtrlCtxt.drop = 0;

			// Send the 'evaluate context' command
			kernelMemClear((void *) &cmdTrb, sizeof(xhciTrb));
			cmdTrb.paramLo = slot->inputCtxtPhysical;
			cmdTrb.typeFlags = USBXHCI_TRBTYPE_EVALCNTXT;
			cmdTrb.control = (slot->num << 8);

			status = command(xhciData, &cmdTrb);
			if (status < 0)
				return (status);

			if ((cmdTrb.status & USBXHCI_TRBCOMP_MASK) !=
				USBXHCI_TRBCOMP_SUCCESS)
			{
				kernelDebugError("Command error %d evaluating device context",
					((cmdTrb.status & USBXHCI_TRBCOMP_MASK) >> 24));
				//kernelDebugStop();
				return (status = ERR_IO);
			}
		}
	}

	return (status = 0);
}


static xhciSlot *getDevSlot(usbXhciData *xhciData, usbDevice *usbDev)
{
	// Return the a pointer to the slot structure belonging to a device.
	// First, search the list of existing ones.  If none is found, then
	// allocate and intitialize a new one.

	xhciSlot *slot = NULL;
	kernelLinkedListItem *iter = NULL;

	kernelDebug(debug_usb, "XHCI get device slot for device %d",
		usbDev->address);

	slot = kernelLinkedListIterStart(&xhciData->slots, &iter);
	while (slot)
	{
		if (slot->usbDev == usbDev)
			return (slot);

		slot = kernelLinkedListIterNext(&xhciData->slots, &iter);
	}

	// Not found.  Allocate a new one.
	slot = allocSlot(xhciData, usbDev);
	if (!slot)
		return (slot);

	// Do a setDevAddress() for the controller's sake (to enable the control
	// endpoint) but don't address the device.
	if (setDevAddress(xhciData, slot, NULL) < 0)
	{
		deallocSlot(xhciData, slot);
		return (slot = NULL);
	}

	return (slot);
}


static xhciTrb *queueIntrDesc(usbXhciData *xhciData, xhciSlot *slot,
	int endpoint, xhciTrb *srcTrb)
{
	// Enqueue the supplied interrupt on the transfer ring of the requested
	// endpoint.

	xhciTrbRing *transRing = NULL;
	xhciTrb *destTrb = NULL;

	transRing = slot->transRings[endpoint & 0xF];

	kernelDebug(debug_usb, "XHCI queue interrupt TRB, slot %d, endpoint "
		"0x%02x, position %d", slot->num, endpoint, transRing->nextTrb);

	destTrb = &transRing->trbs[transRing->nextTrb];

	kernelDebug(debug_usb, "XHCI use TRB with physical address=0x%08x",
		trbPhysical(transRing, destTrb));

	// Set the cycle bit
	if (transRing->cycleState)
		srcTrb->typeFlags |= USBXHCI_TRBFLAG_CYCLE;
	else
		srcTrb->typeFlags &= ~USBXHCI_TRBFLAG_CYCLE;

	// Copy the TRB
	kernelMemCopy((void *) srcTrb, (void *) destTrb, sizeof(xhciTrb));

	// Advance the nextTrb 'enqueue pointer'
	transRing->nextTrb = ringNextTrb(transRing);
	if (transRing->nextTrb == 0)
	{
		// Update the cycle bit of the link TRB
		if (transRing->cycleState)
			transRing->trbs[transRing->numTrbs - 1].typeFlags |=
				USBXHCI_TRBFLAG_CYCLE;
		else
			transRing->trbs[transRing->numTrbs - 1].typeFlags &=
				~USBXHCI_TRBFLAG_CYCLE;

		transRing->cycleState ^= 1;
	}

	// Ring the slot doorbell with the endpoint number
	kernelDebug(debug_usb, "XHCI ring endpoint 0x%02x doorbell", endpoint);
	if (endpoint)
		xhciData->dbRegs->doorbell[slot->num] =
			(((endpoint & 0xF) * 2) + (endpoint >> 7));
	else
		xhciData->dbRegs->doorbell[slot->num] = 1;

	return (destTrb);
}


static int transferEventInterrupt(usbXhciData *xhciData, xhciTrb *eventTrb)
{
	int slotNum = 0;
	kernelLinkedListItem *iter = NULL;
	xhciIntrReg *intrReg = NULL;
	xhciSlot *slot = NULL;
	unsigned bytes = 0;

	slotNum = (eventTrb->control >> 8);

	kernelDebug(debug_usb, "XHCI transfer event interrupt, slot %d", slotNum);

	// Loop through this controller's interrupt registrations, to find out
	// whether one of them caused this interrupt.
	intrReg = kernelLinkedListIterStart(&xhciData->intrRegs, &iter);
	while (intrReg)
	{
		if (intrReg->slot->num == slotNum)
		{
			slot = getDevSlot(xhciData, intrReg->usbDev);
			if (slot)
			{
				if ((eventTrb->paramLo & ~0xFU) ==
					trbPhysical(slot->transRings[intrReg->endpoint & 0xF],
						intrReg->queuedTrb))
				{
					bytes = (intrReg->dataLen - (eventTrb->status & 0xFFFFFF));

					kernelDebug(debug_usb, "XHCI found, device address %d, "
						"endpoint 0x%02x, %u bytes", intrReg->usbDev->address,
						intrReg->endpoint, bytes);

					if (intrReg->callback)
						intrReg->callback(intrReg->usbDev, intrReg->buffer,
							bytes);
					else
						kernelDebug(debug_usb, "XHCI no callback");

					// Re-queue the TRB
					intrReg->queuedTrb = queueIntrDesc(xhciData, intrReg->slot,
						intrReg->endpoint, &intrReg->trb);

					break;
				}
			}
		}

		intrReg = kernelLinkedListIterNext(&xhciData->intrRegs, &iter);
	}

	// If we did a callback, consume the event.  Otherwise, leave the event
	// in the ring for synchronous consumption.
	if (intrReg && intrReg->callback)
		return (1);
	else
		return (0);
}


static int portEventInterrupt(usbXhciData *xhciData, xhciTrb *eventTrb)
{
	// Port status changed.

	int portNum = ((eventTrb->paramLo >> 24) - 1);

	kernelDebug(debug_usb, "XHCI port %d event interrupt, portsc=%08x",
		portNum, xhciData->opRegs->portRegSet[portNum].portsc);

	xhciData->portChangedBitmap |= BIT(portNum);

	return (1);
}


static int eventInterrupt(usbXhciData *xhciData)
{
	// When an interrupt arrives that indicates an event has occurred, this
	// function is called to process it.

	int status = 0;
	xhciIntrRegSet *regSet = NULL;
	xhciTrb eventTrb;
	int consume = 0;
	int intrCount;

	kernelDebug(debug_usb, "XHCI process event interrupt");

	// Loop through the interrupters, to see which one(s) are interrupting
	for (intrCount = 0; intrCount < xhciData->numIntrs; intrCount ++)
	{
		regSet = &(xhciData->rtRegs->intrReg[intrCount]);

		if (!(regSet->intrMan & USBXHCI_IMAN_INTPENDING))
			continue;

		kernelDebug(debug_usb, "XHCI interrupter %d active", intrCount);

		// Clear the interrupt pending flag
		regSet->intrMan |= USBXHCI_IMAN_INTPENDING;

		kernelMemClear((void *) &eventTrb, sizeof(xhciTrb));

		if (!getEvent(xhciData, intrCount, &eventTrb, 0))
		{
			switch (eventTrb.typeFlags & USBXHCI_TRBTYPE_MASK)
			{
				case USBXHCI_TRBTYPE_TRANSFER:
					consume = transferEventInterrupt(xhciData, &eventTrb);
					if (!consume)
						kernelDebug(debug_usb, "XHCI ignore transfer event");
					break;

				case USBXHCI_TRBTYPE_CMDCOMP:
					kernelDebug(debug_usb, "XHCI ignore command completion "
						"event");
					break;

				case USBXHCI_TRBTYPE_PRTSTATCHG:
					consume = portEventInterrupt(xhciData, &eventTrb);
					break;

				case USBXHCI_TRBTYPE_HOSTCONT:
					// Host controller event (an error, we presume)
					kernelDebug(debug_usb, "XHCI host controller event, "
						"status=0x%02x (%s)", (eventTrb.status >> 24),
						(((eventTrb.status & USBXHCI_TRBCOMP_MASK) ==
							USBXHCI_TRBCOMP_SUCCESS)? "success" :
								"error"));
					//kernelDebugStop();
					consume = 1;
					status = ERR_IO;
					break;

				default:
					kernelError(kernel_error, "Unsupported event type %d",
						((eventTrb.typeFlags & USBXHCI_TRBTYPE_MASK) >> 10));
					//kernelDebugStop();
					status = ERR_IO;
					break;
			}
		}

		if (consume)
			getEvent(xhciData, intrCount, &eventTrb, 1);
	}

	return (status);
}


static int configDevSlot(usbXhciData *xhciData, xhciSlot *slot,
	usbDevice *usbDev)
{
	// 'configure' the supplied device slot

	int status = 0;
	usbEndpointDesc *endpointDesc = NULL;
	int ctxtIndex = 0;
	int endpointType = 0;
	int interval = 0;
	int contextEntries = 0;
	xhciTrb cmdTrb;
	int count;

	kernelDebug(debug_usb, "XHCI configure device slot %d", slot->num);

	slot->inputCtxt->inputCtrlCtxt.add = BIT(0);
	slot->inputCtxt->inputCtrlCtxt.drop = 0;

	// Loop through the endpoints (not including default endpoint 0) and set
	// up their endpoint contexts
	for (count = 1; count < usbDev->numEndpoints; count ++)
	{
		// Get the endpoint descriptor
		endpointDesc = usbDev->endpointDesc[count];

		if (endpointDesc->endpntAddress)
			ctxtIndex = ((((endpointDesc->endpntAddress & 0xF) * 2) - 1) +
				(endpointDesc->endpntAddress >> 7));

		kernelDebug(debug_usb, "XHCI configure endpoint 0x%02x, ctxtIndex=%d",
			endpointDesc->endpntAddress, ctxtIndex);

		// What kind of XHCI endpoint is it?
		switch (endpointDesc->attributes & USB_ENDP_ATTR_MASK)
		{
			case USB_ENDP_ATTR_CONTROL:
				endpointType = USBXHCI_EPTYPE_CONTROL;
				break;

			case USB_ENDP_ATTR_BULK:
				if (endpointDesc->endpntAddress & 0x80)
					endpointType = USBXHCI_EPTYPE_BULK_IN;
				else
					endpointType = USBXHCI_EPTYPE_BULK_OUT;
				break;

			case USB_ENDP_ATTR_ISOCHRONOUS:
				if (endpointDesc->endpntAddress & 0x80)
					endpointType = USBXHCI_EPTYPE_ISOCH_IN;
				else
					endpointType = USBXHCI_EPTYPE_ISOCH_OUT;
				break;

			case USB_ENDP_ATTR_INTERRUPT:
			{
				if (endpointDesc->endpntAddress & 0x80)
					endpointType = USBXHCI_EPTYPE_INTR_IN;
				else
					endpointType = USBXHCI_EPTYPE_INTR_OUT;
			}
		}

		kernelDebug(debug_usb, "XHCI endpoint interval value %d",
			endpointDesc->interval);

		// Interpret the endpoint interval value.  Expressed in frames
		// or microframes depending on the device operating speed (i.e.,
		// either 1 millisecond or 125 us units).
		interval = 0;
		if (usbDev->speed < usbspeed_high)
		{
			// Interval is expressed in frames
			while ((1 << (interval + 1)) <= (endpointDesc->interval << 3))
				interval += 1;
		}
		else
		{
			// Interval is expressed in microframes as a 1-based
			// exponent
			interval = (endpointDesc->interval - 1);
		}

		if (interval)
			kernelDebug(debug_usb, "XHCI interrupt interval at 2^%d "
				"microframes", interval);

		// Allocate things needed for the endpoint.
		status = allocEndpoint(slot, endpointDesc->endpntAddress, endpointType,
			interval, endpointDesc->maxPacketSize,
			usbDev->endpoint[count].maxBurst);
		if (status < 0)
			return (status);

		// Set the 'add' bit of the input control context
		slot->inputCtxt->inputCtrlCtxt.add |= BIT(ctxtIndex + 1);

		kernelDebug(debug_usb, "XHCI BIT(%d) now 0x%08x", (ctxtIndex + 1),
			slot->inputCtxt->inputCtrlCtxt.add);

		contextEntries = (ctxtIndex + 1);
	}

	// Update the input slot context data structure
	slot->inputCtxt->devCtxt.slotCtxt.entFlagsSpeedRoute &= 0x07FFFFFF;
	slot->inputCtxt->devCtxt.slotCtxt.entFlagsSpeedRoute |=
		((contextEntries << 27) & USBXHCI_SLTCTXT_CTXTENTS);

	kernelDebug(debug_usb, "XHCI contextEntries=%d now 0x%08x", contextEntries,
		slot->inputCtxt->devCtxt.slotCtxt.entFlagsSpeedRoute);

	// Send the 'configure endpoint' command
	kernelMemClear((void *) &cmdTrb, sizeof(xhciTrb));
	cmdTrb.paramLo = slot->inputCtxtPhysical;
	cmdTrb.typeFlags = USBXHCI_TRBTYPE_CFGENDPT;
	cmdTrb.control = (slot->num << 8);

	status = command(xhciData, &cmdTrb);
	if (status < 0)
		return (status);

	if ((cmdTrb.status & USBXHCI_TRBCOMP_MASK) != USBXHCI_TRBCOMP_SUCCESS)
	{
		kernelError(kernel_error, "Command error %d configuring device slot",
			((cmdTrb.status & USBXHCI_TRBCOMP_MASK) >> 24));
		//kernelDebugStop();
		return (status = ERR_IO);
	}

	return (status = 0);
}


static int transfer(usbXhciData *xhciData, xhciSlot *slot, int endpoint,
	unsigned timeout, int numTrbs, xhciTrb *trbs)
{
	// Enqueue the supplied transaction on the transfer ring of the requested
	// endpoint.

	int status = 0;
	xhciTrbRing *transRing = NULL;
	xhciTrb *srcTrb = NULL;
	xhciTrb *destTrb = NULL;
	xhciTrb eventTrb;
	int trbCount;
	unsigned count;

	transRing = slot->transRings[endpoint & 0xF];

	kernelDebug(debug_usb, "XHCI queue transfer (%d TRBs) slot %d, "
		"endpoint 0x%02x, pos %d", numTrbs, slot->num, endpoint,
		transRing->nextTrb);

	for (trbCount = 0; trbCount < numTrbs; trbCount ++)
	{
		srcTrb = &trbs[trbCount];
		destTrb = &transRing->trbs[transRing->nextTrb];

		kernelDebug(debug_usb, "XHCI use TRB with physical address=0x%08x",
			trbPhysical(transRing, destTrb));

		// Copy the TRB
		kernelMemCopy((void *) srcTrb, (void *) destTrb, sizeof(xhciTrb));

		// Set the last TRB to interrupt
		if (trbCount == (numTrbs - 1))
			destTrb->typeFlags |= USBXHCI_TRBFLAG_INTONCOMP;

		// Set the cycle bit
		if (transRing->cycleState)
			destTrb->typeFlags |= USBXHCI_TRBFLAG_CYCLE;
		else
			destTrb->typeFlags &= ~USBXHCI_TRBFLAG_CYCLE;

		debugXhciTrb(destTrb);

		// Advance the nextTrb 'enqueue pointer'
		transRing->nextTrb = ringNextTrb(transRing);
		if (!transRing->nextTrb)
		{
			// Update the cycle bit of the link TRB
			if (transRing->cycleState)
				transRing->trbs[transRing->numTrbs - 1].typeFlags |=
					USBXHCI_TRBFLAG_CYCLE;
			else
				transRing->trbs[transRing->numTrbs - 1].typeFlags &=
					~USBXHCI_TRBFLAG_CYCLE;

			transRing->cycleState ^= 1;
		}
	}

	// Ring the slot doorbell with the endpoint number
	kernelDebug(debug_usb, "XHCI ring endpoint 0x%02x doorbell", endpoint);
	if (endpoint)
		xhciData->dbRegs->doorbell[slot->num] =
			(((endpoint & 0xF) * 2) + (endpoint >> 7));
	else
		xhciData->dbRegs->doorbell[slot->num] = 1;

	// Wait until the transfer has completed
	kernelDebug(debug_usb, "XHCI wait for transaction complete");
	for (count = 0; count < timeout; count ++)
	{
		kernelMemClear((void *) &eventTrb, sizeof(xhciTrb));

		if (!getEvent(xhciData, 0, &eventTrb, 1) &&
			((eventTrb.typeFlags & USBXHCI_TRBTYPE_MASK) ==
				USBXHCI_TRBTYPE_TRANSFER))
		{
			kernelDebug(debug_usb, "XHCI got transfer event for TRB 0x%08x",
				(eventTrb.paramLo & ~0xFU));

			kernelDebug(debug_usb, "XHCI completion code %d",
				((eventTrb.status & USBXHCI_TRBCOMP_MASK) >> 24));

			if ((eventTrb.status & USBXHCI_TRBCOMP_MASK) !=
				USBXHCI_TRBCOMP_SUCCESS)
			{
				kernelDebugError("TRB error: %d (%s)",
					((eventTrb.status & USBXHCI_TRBCOMP_MASK) >> 24),
					debugXhciTrbCompletion2String(&eventTrb));
			}

			if ((eventTrb.paramLo & ~0xFU) == trbPhysical(transRing, destTrb))
				break;
		}

		kernelCpuSpinMs(1);
	}

	if (count >= timeout)
	{
		kernelError(kernel_error, "No transfer event received");
		//kernelDebugStop();
		return (status = ERR_TIMEOUT);
	}

	kernelDebug(debug_usb, "XHCI transaction finished");

	// Copy the completion event TRB back to the last transfer TRB
	kernelMemCopy((void *) &eventTrb, (void *) &trbs[numTrbs - 1],
		sizeof(xhciTrb));

	return (status = 0);
}


static int controlBulkTransfer(usbXhciData *xhciData, xhciSlot *slot,
	usbTransaction *trans, unsigned maxPacketSize, unsigned timeout)
{
	int status = 0;
	unsigned numTrbs = 0;
	unsigned numDataTrbs = 0;
	xhciTrb trbs[USBXHCI_TRANSRING_SIZE];
	xhciSetupTrb *setupTrb = NULL;
	unsigned bytesToTransfer = 0;
	unsigned buffPtr = 0;
	unsigned doBytes = 0;
	unsigned remainingPackets = 0;
	xhciTrb *dataTrbs = NULL;
	xhciTrb *statusTrb = NULL;
	unsigned trbCount;

	kernelDebug(debug_usb, "XHCI control/bulk transfer for endpoint "
		"0x%02x, maxPacketSize=%u", trans->endpoint, maxPacketSize);

	// Figure out how many TRBs we're going to need for this transfer

	if (trans->type == usbxfer_control)
		// 2 TRBs for the setup and status phases
		numTrbs += 2;

	// Data descriptors?
	if (trans->length)
	{
		numDataTrbs = ((trans->length + (USBXHCI_TRB_MAXBYTES - 1)) /
			USBXHCI_TRB_MAXBYTES);

		kernelDebug(debug_usb, "XHCI data payload of %u requires %d "
			"descriptors", trans->length, numDataTrbs);

		numTrbs += numDataTrbs;
	}

	kernelDebug(debug_usb, "XHCI transfer requires %d descriptors", numTrbs);

	if (numTrbs > USBXHCI_TRANSRING_SIZE)
	{
		kernelDebugError("Number of TRBs exceeds maximum allowed per transfer "
			"(%d)", USBXHCI_TRANSRING_SIZE);
		//kernelDebugStop();
		return (status = ERR_RANGE);
	}

	kernelMemClear((void *) &trbs, (numTrbs * sizeof(xhciTrb)));

	if (trans->type == usbxfer_control)
	{
		// Set up the device request.  The setup stage is a single-TRB TD, so
		// it is not chained to the data or status stages

		// Get the TRB for the setup stage
		setupTrb = (xhciSetupTrb *) &trbs[0];

		// The device request goes straight into the initial part of the
		// setup TRB as immediate data
		status = kernelUsbSetupDeviceRequest(trans, (usbDeviceRequest *)
			&setupTrb->request);
		if (status < 0)
			return (status);

		setupTrb->intTargetTransLen = sizeof(usbDeviceRequest); // 8!!!
		setupTrb->typeFlags = (USBXHCI_TRBTYPE_SETUPSTG |
			USBXHCI_TRBFLAG_IMMEDDATA);

		// Transfer type depends on data stage and direction
		if (trans->length)
		{
			if (trans->pid == USB_PID_IN)
				setupTrb->control = 3;
			else
				setupTrb->control = 2;
		}
	}

	// If there is a data stage, set up the TRB(s) for the data.  The data
	// stage is its own TD, distinct from the setup and status stages (in the
	// control transfer case), so they are all chained together, but the last
	// TRB is not chained to anything.
	if (trans->length)
	{
		buffPtr = (unsigned) kernelPageGetPhysical((((unsigned) trans->buffer <
			KERNEL_VIRTUAL_ADDRESS)? kernelCurrentProcess->processId :
				KERNELPROCID), trans->buffer);
		if (!buffPtr)
		{
			kernelDebugError("Can't get physical address for buffer at %p",
				trans->buffer);
			return (status = ERR_MEMORY);
		}

		bytesToTransfer = trans->length;

		dataTrbs = &trbs[0];
		if (setupTrb)
			dataTrbs = &trbs[1];

		for (trbCount = 0; trbCount < numDataTrbs; trbCount ++)
		{
			doBytes = min(bytesToTransfer, USBXHCI_TRB_MAXBYTES);
			remainingPackets = (((bytesToTransfer - doBytes) +
				(maxPacketSize - 1)) / maxPacketSize);

			kernelDebug(debug_usb, "XHCI doBytes=%u remainingPackets=%u",
				doBytes, remainingPackets);

			if (doBytes)
			{
				// Set the data TRB
				dataTrbs[trbCount].paramLo = buffPtr;
				dataTrbs[trbCount].status =
					((min(remainingPackets, 31) << 17) | doBytes);
			}

			// Control transfers use 'data stage' TRBs for the first data TRB,
			// and 'normal' TRBs for the rest.  Bulk transfers use 'normal'
			// for all
			if ((trans->type == usbxfer_control) && !trbCount)
			{
				dataTrbs[trbCount].typeFlags = USBXHCI_TRBTYPE_DATASTG;
				dataTrbs[trbCount].control =
					((trans->pid == USB_PID_IN)? 1 : 0);
			}
			else
				dataTrbs[trbCount].typeFlags = USBXHCI_TRBTYPE_NORMAL;

			// Chain all but the last TRB
			if (trbCount < (numDataTrbs - 1))
				dataTrbs[trbCount].typeFlags |= USBXHCI_TRBFLAG_CHAIN;

			buffPtr += doBytes;
			bytesToTransfer -= doBytes;
		}
	}

	if (trans->type == usbxfer_control)
	{
		// Set up the TRB for the status stage

		statusTrb = &trbs[numTrbs - 1];

		statusTrb->typeFlags = USBXHCI_TRBTYPE_STATUSSTG;

		// Direction flag depends on data stage and direction
		if (trans->length)
		{
			// If there's data, status direction is opposite
			if (trans->pid == USB_PID_OUT)
				statusTrb->control = 1; // in
		}
		else
			// No data, status direction is always in
			statusTrb->control = 1; // in
	}

	// Queue the TRBs in the endpoint's transfer ring
	status = transfer(xhciData, slot, trans->endpoint, timeout, numTrbs, trbs);
	if (status < 0)
		return (status);

	if ((trbs[numTrbs - 1].status & USBXHCI_TRBCOMP_MASK) !=
		USBXHCI_TRBCOMP_SUCCESS)
	{
		// If it's bulk, we allow short packet
		if ((trans->type == usbxfer_bulk) &&
			(trbs[numTrbs - 1].status & USBXHCI_TRBCOMP_MASK) ==
				USBXHCI_TRBCOMP_SHORTPACKET)
		{
			kernelDebug(debug_usb, "XHCI short packet allowed");
		}
		else
		{
			kernelError(kernel_error, "Transfer failed, status=%d",
				((trbs[numTrbs - 1].status & USBXHCI_TRBCOMP_MASK) >> 24));
			//kernelDebugStop();
			return (status = ERR_IO);
		}
	}

	if (trans->length)
	{
		// Return the number of bytes transferred
		trans->bytes = (trans->length - (trbs[numTrbs - 1].status & 0xFFFFFF));
		kernelDebug(debug_usb, "XHCI transferred %u of %u requested bytes",
			trans->bytes, trans->length);
		//if (trans->bytes < trans->length)
		//	kernelDebugStop();
	}

	return (status = 0);
}


static int recordHubAttrs(usbXhciData *xhciData, xhciSlot *slot,
	usbHubDesc *hubDesc)
{
	// If we have discovered that a device is a hub, we need to tell the
	// controller about that.

	int status = 0;
	xhciTrb cmdTrb;

	kernelDebug(debug_usb, "XHCI record hub attributes");

	slot->inputCtxt->inputCtrlCtxt.add = BIT(0);
	slot->inputCtxt->inputCtrlCtxt.drop = 0;

	// Set the 'hub' flag
	slot->inputCtxt->devCtxt.slotCtxt.entFlagsSpeedRoute |= USBXHCI_SLTCTXT_HUB;

	// Set the number of ports
	slot->inputCtxt->devCtxt.slotCtxt.numPortsPortLat |=
		((hubDesc->numPorts << 24) & USBXHCI_SLTCTXT_NUMPORTS);

	// Set the TT Think Time
	slot->inputCtxt->devCtxt.slotCtxt.targetTT |=
		(((hubDesc->hubChars & USB_HUBCHARS_TTT) << 11) & USBXHCI_SLTCTXT_TTT);

	kernelDebug(debug_usb, "XHCI numPorts=%d", hubDesc->numPorts);

	// Send the 'configure endpoint' command
	kernelMemClear((void *) &cmdTrb, sizeof(xhciTrb));
	cmdTrb.paramLo = slot->inputCtxtPhysical;
	cmdTrb.typeFlags = USBXHCI_TRBTYPE_CFGENDPT;
	cmdTrb.control = (slot->num << 8);

	status = command(xhciData, &cmdTrb);
	if (status < 0)
		return (status);

	if ((cmdTrb.status & USBXHCI_TRBCOMP_MASK) != USBXHCI_TRBCOMP_SUCCESS)
	{
		kernelError(kernel_error, "Command error %d configuring device slot",
			((cmdTrb.status & USBXHCI_TRBCOMP_MASK) >> 24));
		//kernelDebugStop();
		return (status = ERR_IO);
	}

	return (status = 0);
}


static int controlTransfer(usbController *controller, usbDevice *usbDev,
	usbTransaction *trans, unsigned timeout)
{
	int status = 0;
	usbXhciData *xhciData = controller->data;
	xhciSlot *slot = NULL;
	unsigned maxPacketSize = 0;

	kernelDebug(debug_usb, "XHCI control transfer to controller %d, device %d",
		controller->num, usbDev->address);

	slot = getDevSlot(xhciData, usbDev);
	if (!slot)
	{
		kernelError(kernel_error, "Couldn't get device slot");
		return (status = ERR_NOSUCHENTRY);
	}

	// If this is a USB_SET_ADDRESS, we don't send it.  Instead, we tell the
	// controller to put the device into the addressed state.
	if (trans->control.request == USB_SET_ADDRESS)
	{
		kernelDebug(debug_usb, "XHCI skip sending USB_SET_ADDRESS");
		status = setDevAddress(xhciData, slot, usbDev);

		// Finished
		return (status);
	}

	// If we are at the stage of configuring the device, we also need to tell
	// the controller about it.
	if (trans->control.request == USB_SET_CONFIGURATION)
	{
		status = configDevSlot(xhciData, slot, usbDev);
		if (status < 0)
			return (status);

		// Carry on with the transfer
	}

	// Get the maximum packet size for the control endpoint
	maxPacketSize = usbDev->deviceDesc.maxPacketSize0;
	if (!maxPacketSize)
	{
		// If we haven't yet got the descriptors, etc., use 8 as the maximum
		// packet size
		kernelDebug(debug_usb, "XHCI using default maximum endpoint transfer "
			"size 8");
		maxPacketSize = 8;
	}

	status = controlBulkTransfer(xhciData, slot, trans, maxPacketSize,
		timeout);
	if (status < 0)
		return (status);

	// If this was a 'get hub descriptor' control transfer, we need to spy
	// on it to record a) the fact that it's a hub; b) the number of ports
	if (trans->control.request == USB_GET_DESCRIPTOR)
	{
		if (((trans->control.value >> 8) == USB_DESCTYPE_HUB) ||
			((trans->control.value >> 8) == USB_DESCTYPE_SSHUB))
		{
			recordHubAttrs(xhciData, slot, (usbHubDesc *) trans->buffer);
		}
	}

	return (status = 0);
}


static int bulkTransfer(usbController *controller, usbDevice *usbDev,
	usbTransaction *trans, unsigned timeout)
{
	int status = 0;
	usbXhciData *xhciData = controller->data;
	xhciSlot *slot = NULL;
	usbEndpointDesc *endpointDesc = NULL;
	unsigned maxPacketSize = 0;

	kernelDebug(debug_usb, "XHCI bulk transfer to controller %d, device %d, "
		"endpoint 0x%02x", controller->num, usbDev->address, trans->endpoint);

	slot = getDevSlot(xhciData, usbDev);
	if (!slot)
	{
		kernelError(kernel_error, "Couldn't get device slot");
		return (status = ERR_NOSUCHENTRY);
	}

	// Get the endpoint descriptor
	endpointDesc = kernelUsbGetEndpointDesc(usbDev, trans->endpoint);
	if (!endpointDesc)
	{
		kernelError(kernel_error, "No such endpoint 0x%02x", trans->endpoint);
		return (status = ERR_NOSUCHFUNCTION);
	}

	// Get the maximum packet size for the endpoint
	maxPacketSize = endpointDesc->maxPacketSize;
	if (!maxPacketSize)
	{
		kernelError(kernel_error, "Device endpoint 0x%02x has a max packet"
			"size of 0", trans->endpoint);
		return (status = ERR_BADDATA);
	}

	status = controlBulkTransfer(xhciData, slot, trans, maxPacketSize,
		timeout);

	return (status);
}


static xhciIntrReg *interruptTransfer(usbXhciData *xhciData, usbDevice *usbDev,
	int endpoint, unsigned dataLen,
	void (*callback)(usbDevice *, void *, unsigned))
{
	// This function is used to schedule an interrupt.

	int status = 0;
	xhciIntrReg *intrReg = NULL;
	kernelIoMemory ioMem;

	kernelDebug(debug_usb, "XHCI register interrupt transfer for endpoint "
		"0x%02x", endpoint);

	// Get memory to store information about the interrupt registration */
	intrReg = kernelMalloc(sizeof(xhciIntrReg));
	if (!intrReg)
	{
		status = ERR_MEMORY;
		goto out;
	}

	kernelMemClear(intrReg, sizeof(xhciIntrReg));
	intrReg->usbDev = usbDev;
	intrReg->endpoint = endpoint;

	// Get the device slot
	intrReg->slot = getDevSlot(xhciData, usbDev);
	if (!intrReg->slot)
	{
		status = ERR_NOSUCHENTRY;
		goto out;
	}

	// Get buffer memory
	status = kernelMemoryGetIo(dataLen, 0, &ioMem);
	if (status < 0)
		goto out;

	intrReg->buffer = ioMem.virtual;
	intrReg->dataLen = dataLen;

	// Set up the TRB
	intrReg->trb.paramLo = ioMem.physical;
	intrReg->trb.status = dataLen;
	intrReg->trb.typeFlags = (USBXHCI_TRBTYPE_NORMAL |
		USBXHCI_TRBFLAG_INTONCOMP);

	kernelDebug(debug_usb, "XHCI buffer=0x%08x len=%u flags=0x%04x",
		intrReg->trb.paramLo, intrReg->trb.status, intrReg->trb.typeFlags);

	intrReg->callback = callback;

	// Add the interrupt registration to the controller's list.
	status = kernelLinkedListAdd(&xhciData->intrRegs, intrReg);
	if (status < 0)
		goto out;

	// Enqueue the TRB
	intrReg->queuedTrb = queueIntrDesc(xhciData, intrReg->slot,
		intrReg->endpoint, &intrReg->trb);

	status = 0;

out:
	if (status < 0)
	{
		if (intrReg)
		{
			if (intrReg->buffer)
				kernelMemoryReleaseIo(&ioMem);

			kernelFree(intrReg);
			intrReg = NULL;
		}
	}

	return (intrReg);
}


static void unregisterInterrupt(usbXhciData *xhciData, xhciIntrReg *intrReg)
{
	// Remove an interrupt registration from the controller's list

	kernelIoMemory ioMem;

	kernelDebug(debug_usb, "XHCI remove interrupt registration for device %d, "
		"endpoint 0x%02x", intrReg->usbDev->address, intrReg->endpoint);

	// Remove it from the list
	kernelLinkedListRemove(&xhciData->intrRegs, intrReg);

	// Deallocate it
	if (intrReg->buffer)
	{
		ioMem.size = intrReg->dataLen;
		ioMem.physical = intrReg->trb.paramLo;
		ioMem.virtual = intrReg->buffer;
		kernelMemoryReleaseIo(&ioMem);
	}

	kernelFree(intrReg);
	return;
}


static int waitPortChangeEvent(usbXhciData *xhciData, int anyPort, int portNum,
	unsigned timeout)
{
	// Wait for, and consume, an expected port status change event

	unsigned count;

	kernelDebug(debug_usb, "XHCI try to wait for port change events");

	for (count = 0; count < timeout; count ++)
	{
		if (anyPort)
		{
			if (xhciData->portChangedBitmap)
			{
				kernelDebug(debug_usb, "XHCI got any port change event (%dms)",
					count);
				return (1);
			}
		}
		else
		{
			if (xhciData->portChangedBitmap & BIT(portNum))
			{
				kernelDebug(debug_usb, "XHCI got port %d change event (%dms)",
					portNum, count);
				xhciData->portChangedBitmap &= ~BIT(portNum);
				return (1);
			}
		}

		kernelCpuSpinMs(1);
	}

	// Timed out
	kernelDebugError("No port change event received");
	return (0);
}


static int portReset(usbXhciData *xhciData, int portNum)
{
	// Reset the port, with the appropriate delays, etc.

	int status = 0;
	int count;

	kernelDebug(debug_usb, "XHCI port reset");

	// Set the port 'reset' bit
	setPortStatusBits(xhciData, portNum, USBXHCI_PORTSC_PORTRESET);

	// Wait for it to read as clear
	for (count = 0; count < 100; count ++)
	{
		// Clear all port status 'change' bits
		setPortStatusBits(xhciData, portNum, USBXHCI_PORTSC_CHANGES);

		if (!(xhciData->opRegs->portRegSet[portNum].portsc &
			USBXHCI_PORTSC_PORTRESET))
		{
			kernelDebug(debug_usb, "XHCI port reset took %dms", count);
			break;
		}

		kernelCpuSpinMs(1);
	}

	// Clear?
	if (xhciData->opRegs->portRegSet[portNum].portsc & USBXHCI_PORTSC_PORTRESET)
	{
		kernelError(kernel_warn, "Port reset bit was not cleared");
		//kernelDebugStop();
		status = ERR_TIMEOUT;
		goto out;
	}

	// Try to wait for a 'port status change' event on the event ring.
	// Once we get this, we know that the port is enabled.
	if (!waitPortChangeEvent(xhciData, 0, portNum, 200))
		kernelDebugError("No port change event");

	// Delay 10ms
	kernelCpuSpinMs(10);

	//debugXhciPortStatus(controller, portNum);

	// Return success
	status = 0;

out:
	kernelDebug(debug_usb, "XHCI port reset %s",
		(status? "failed" : "success"));
	return (status);
}


static int portConnected(usbController *controller, int portNum, int hotPlug)
{
	// This function is called whenever we notice that a port has indicated
	// a new connection.

	int status = 0;
	usbXhciData *xhciData = controller->data;
	xhciDevSpeed xhciSpeed = xhcispeed_unknown;
	usbDevSpeed usbSpeed = usbspeed_unknown;
	int count;

	kernelDebug(debug_usb, "XHCI port %d connected", portNum);

	// Clear all port status 'change' bits
	setPortStatusBits(xhciData, portNum, USBXHCI_PORTSC_CHANGES);

	// USB3 devices should automatically transition the port to the 'enabled'
	// state, but older devices will need to go through the port reset
	// procedure.
	for (count = 0; count < 100; count ++)
	{
		if (xhciData->opRegs->portRegSet[portNum].portsc &
			USBXHCI_PORTSC_PORTENABLED)
		{
			kernelDebug(debug_usb, "XHCI port auto-enable took %dms", count);
			break;
		}

		kernelCpuSpinMs(1);
	}

	// Did the port become enabled?
	if (!(xhciData->opRegs->portRegSet[portNum].portsc &
		USBXHCI_PORTSC_PORTENABLED))
	{
		kernelDebug(debug_usb, "XHCI port did not auto-enable");

		status = portReset(xhciData, portNum);
		if (status < 0)
			return (status);

		// Clear all port status 'change' bits
		setPortStatusBits(xhciData, portNum, USBXHCI_PORTSC_CHANGES);

		for (count = 0; count < 100; count ++)
		{
			if (xhciData->opRegs->portRegSet[portNum].portsc &
				USBXHCI_PORTSC_PORTENABLED)
			{
				kernelDebug(debug_usb, "XHCI port enable took %dms", count);
				break;
			}

			kernelCpuSpinMs(1);
		}
	}

	// Did the port become enabled?
	if (!(xhciData->opRegs->portRegSet[portNum].portsc &
		USBXHCI_PORTSC_PORTENABLED))
	{
		kernelDebugError("Port did not transition to the enabled state");
		//kernelDebugStop();
		return (status = ERR_IO);
	}

	// Determine the speed of the device
	xhciSpeed = (xhciDevSpeed)((xhciData->opRegs->portRegSet[portNum].portsc &
		USBXHCI_PORTSC_PORTSPEED) >> 10);

	kernelDebug(debug_usb, "XHCI connection speed: %s",
		debugXhciSpeed2String(xhciSpeed));

	switch (xhciSpeed)
	{
		case xhcispeed_low:
			usbSpeed = usbspeed_low;
			break;
		case xhcispeed_full:
			usbSpeed = usbspeed_full;
			break;
		case xhcispeed_high:
			usbSpeed = usbspeed_high;
			break;
		case xhcispeed_super:
			usbSpeed = usbspeed_super;
			break;
		default:
			usbSpeed = usbspeed_unknown;
			break;
	}

	kernelDebug(debug_usb, "XHCI USB connection speed: %s",
		usbDevSpeed2String(usbSpeed));

	status = kernelUsbDevConnect(controller, &controller->hub, portNum,
		usbSpeed, hotPlug);
	if (status < 0)
		kernelError(kernel_error, "Error enumerating new USB device");

	return (status);
}


static void portDisconnected(usbController *controller, int portNum)
{
	// This function is called whenever we notice that a port has indicated
	// a device disconnection.

	kernelDebug(debug_usb, "XHCI port %d disconnected", portNum);

	// Tell the USB functions that the device disconnected.  This will call us
	// back to tell us about all affected devices - there might be lots if
	// this was a hub - so we can disable slots, etc., then.
	kernelUsbDevDisconnect(controller, &controller->hub, portNum);
}


static void detectPortChanges(usbController *controller, int portNum,
	int hotplug)
{
	usbXhciData *xhciData = controller->data;

	kernelDebug(debug_usb, "XHCI check port %d", portNum);

	if (!controller->hub.doneColdDetect ||
		(xhciData->opRegs->portRegSet[portNum].portsc &
			USBXHCI_PORTSC_CONNECT_CH))
	{
		if (xhciData->opRegs->portRegSet[portNum].portsc &
			USBXHCI_PORTSC_CONNECTED)
		{
			// Do port connection setup
			portConnected(controller, portNum, hotplug);
		}
		else
		{
			// Do port connection tear-down
			portDisconnected(controller, portNum);
		}
	}

	// Clear all port status 'change' bits
	setPortStatusBits(xhciData, portNum, USBXHCI_PORTSC_CHANGES);
}


static void doDetectDevices(usbController *controller, int hotplug)
{
	// This function gets called to check for device connections (either cold-
	// plugged ones at boot time, or hot-plugged ones during operations.

	usbXhciData *xhciData = controller->data;
	int count;

	kernelDebug(debug_usb, "XHCI check non-USB3 ports");

	// Check to see whether any non-USB3 ports are showing a connection
	for (count = 0; count < xhciData->numPorts; count ++)
	{
		if (xhciData->portProtos[count] >= usbproto_usb3)
			continue;

		detectPortChanges(controller, count, hotplug);
	}

	// It can happen that USB3 protocol ports suddenly show connections after
	// we have attempted to reset a corresponding USB2 protocol port.

	kernelDebug(debug_usb, "XHCI check USB3 ports");

	// Now check any USB3 protocol ports
	for (count = 0; count < xhciData->numPorts; count ++)
	{
		if (xhciData->portProtos[count] < usbproto_usb3)
			continue;

		detectPortChanges(controller, count, hotplug);
	}

	xhciData->portChangedBitmap = 0;

	return;
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Standard USB controller functions
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


static int reset(usbController *controller)
{
	// Do complete USB (controller and bus) reset

	int status = 0;
	usbXhciData *xhciData = NULL;
	int count;

	// Check params
	if (!controller)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	xhciData = controller->data;

	// Try to make sure the controller is stopped
	status = startStop(xhciData, 0);
	if (status < 0)
		return (status);

	kernelDebug(debug_usb, "XHCI reset controller");

	// Set host controller reset
	xhciData->opRegs->cmd |= USBXHCI_CMD_HCRESET;

	// Wait until the host controller clears it
	for (count = 0; count < 2000; count ++)
	{
		if (!(xhciData->opRegs->cmd & USBXHCI_CMD_HCRESET))
		{
			kernelDebug(debug_usb, "XHCI resetting controller took %dms",
				count);
			break;
		}

		kernelCpuSpinMs(1);
	}

	// Clear?
	if (xhciData->opRegs->cmd & USBXHCI_CMD_HCRESET)
	{
		kernelError(kernel_error, "Controller did not clear reset bit");
		//kernelDebugStop();
		status = ERR_TIMEOUT;
	}

	kernelDebug(debug_usb, "XHCI controller reset %s",
		(status? "failed" : "successful"));

	return (status);
}


static int interrupt(usbController *controller)
{
	// This function gets called when the controller issues an interrupt

	int gotInterrupt = 0;
	usbXhciData *xhciData = controller->data;

	// See whether the status register indicates any of the interrupts we
	// enabled
	if (xhciData->opRegs->stat &
		(USBXHCI_STAT_HOSTCTRLERR | USBXHCI_STAT_INTERRUPTMASK))
	{
		kernelDebug(debug_usb, "XHCI controller %d interrupt, status=0x%08x",
			controller->num, xhciData->opRegs->stat);
	}

	if (xhciData->opRegs->stat & USBXHCI_STAT_HOSTSYSERROR)
	{
		kernelError(kernel_error, "Host system error interrupt");
		debugXhciOpRegs(xhciData);

		// Clear the host system error bit
		clearStatusBits(xhciData, USBXHCI_STAT_HOSTSYSERROR);

		gotInterrupt = 1;
	}

	else if (xhciData->opRegs->stat & USBXHCI_STAT_EVENTINTR)
    {
		kernelDebug(debug_usb, "XHCI event interrupt");

		// Clear the event interrupt bit before processing the interrupters
		clearStatusBits(xhciData, USBXHCI_STAT_EVENTINTR);

		eventInterrupt(xhciData);

		gotInterrupt = 1;
	}

	else if (xhciData->opRegs->stat & USBXHCI_STAT_HOSTCTRLERR)
    {
		kernelError(kernel_error, "Host controller error");
		debugXhciOpRegs(xhciData);
	}

	if (gotInterrupt)
		return (0);
	else
		return (ERR_NODATA);
}


static int queue(usbController *controller, usbDevice *usbDev,
	usbTransaction *trans, int numTrans)
{
	// This function contains the intelligence necessary to initiate a set of
	// transactions (all phases)

	int status = 0;
	unsigned timeout = 0;
	int transCount;

	kernelDebug(debug_usb, "XHCI queue %d transaction%s for controller %d, "
		"device %d", numTrans, ((numTrans > 1)? "s" : ""), controller->num,
		usbDev->address);

	if (!controller || !usbDev || !trans)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	// Loop for each transaction
	for (transCount = 0; transCount < numTrans; transCount ++)
	{
		timeout = trans[transCount].timeout;
		if (!timeout)
			timeout = USB_STD_TIMEOUT_MS;

		switch (trans[transCount].type)
		{
			case usbxfer_control:
				status = controlTransfer(controller, usbDev,
					&trans[transCount], timeout);
				break;

			case usbxfer_bulk:
				status = bulkTransfer(controller, usbDev,
					&trans[transCount], timeout);
				break;

			default:
				kernelError(kernel_error, "Illegal transaction type for "
					"queueing");
				status = ERR_INVALID;
				break;
		}

		if (status < 0)
			break;
	}

	return (status);
}


static int schedInterrupt(usbController *controller, usbDevice *usbDev,
	unsigned char endpoint, int interval __attribute__((unused)),
	unsigned maxLen, void (*callback)(usbDevice *, void *, unsigned))
{
	// This function is used to schedule an interrupt.

	int status = 0;
	usbXhciData *xhciData = NULL;

	// Check params
	if (!controller || !usbDev || !callback)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	kernelDebug(debug_usb, "XHCI schedule interrupt for address %d endpoint "
		"%02x len %u", usbDev->address, endpoint, maxLen);

	xhciData = controller->data;

	if (!interruptTransfer(xhciData, usbDev, endpoint, maxLen, callback))
		return (status = ERR_IO);

	return (status = 0);
}


static int deviceRemoved(usbController *controller, usbDevice *usbDev)
{
	int status = 0;
	usbXhciData *xhciData = controller->data;
	xhciSlot *slot = NULL;
	xhciIntrReg *intrReg = NULL;
	kernelLinkedListItem *iter = NULL;

	// Check params
	if (!controller || !usbDev)
	{
		kernelError(kernel_error, "NULL parameter");
		return (status = ERR_NULLPARAMETER);
	}

	kernelDebug(debug_usb, "XHCI device %d removed", usbDev->address);

	xhciData = controller->data;

	// Get the device slot
	slot = getDevSlot(xhciData, usbDev);
	if (!slot)
		return (status = ERR_NOSUCHENTRY);

	// Disable the slot
	status = deallocSlot(xhciData, slot);
	if (status < 0)
		return (status);

	// Remove any interrupt registrations for the device
	intrReg = kernelLinkedListIterStart(&xhciData->intrRegs, &iter);
	while (intrReg)
	{
		if (intrReg->usbDev == usbDev)
			unregisterInterrupt(xhciData, intrReg);

		intrReg = kernelLinkedListIterNext(&xhciData->intrRegs, &iter);
	}

	return (status = 0);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Standard USB hub functions
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


static void detectDevices(usbHub *hub, int hotplug)
{
	// This function gets called once at startup to detect 'cold-plugged'
	// devices.

	usbController *controller = NULL;
	usbXhciData *xhciData = NULL;
	xhciTrb cmdTrb;

	kernelDebug(debug_usb, "XHCI initial device detection, hotplug=%d",
		hotplug);

	// Check params
	if (!hub)
	{
		kernelError(kernel_error, "NULL parameter");
		return;
	}

	controller = hub->controller;
	if (!controller)
	{
		kernelError(kernel_error, "Hub controller is NULL");
		return;
	}

	xhciData = controller->data;

	// Do a no-op command.  Helps the port change interrupt to arrive on
	// time, and demonstrates that the command ring and interrupter are
	// working properly.
	kernelMemClear((void *) &cmdTrb, sizeof(xhciTrb));
	cmdTrb.typeFlags = USBXHCI_TRBTYPE_CMDNOOP;
	if (command(xhciData, &cmdTrb) < 0)
		kernelDebugError("No-op command failed");

	// Try to wait for a 'port status change' event on the event ring.  Once
	// we get this, we know that any connected ports should be showing their
	// connections
	if (!waitPortChangeEvent(xhciData, 1, 0, 150))
		kernelDebugError("No port change event");

	doDetectDevices(controller, hotplug);

	hub->doneColdDetect = 1;
}


static void threadCall(usbHub *hub)
{
	// This function gets called periodically by the USB thread, to give us
	// an opportunity to detect connections/disconnections, or whatever else
	// we want.

	usbController *controller = NULL;
	usbXhciData *xhciData = NULL;

	// Check params
	if (!hub)
	{
		kernelError(kernel_error, "NULL parameter");
		return;
	}

	// Only continue if we've already completed 'cold' device connection
	// detection.  Don't want to interfere with that.
	if (!hub->doneColdDetect)
		return;

	controller = hub->controller;
	if (!controller)
	{
		kernelError(kernel_error, "Hub controller is NULL");
		return;
	}

	xhciData = controller->data;

	if (xhciData->portChangedBitmap)
		doDetectDevices(controller, 1 /* hotplug */);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


kernelDevice *kernelUsbXhciDetect(kernelBusTarget *busTarget,
	kernelDriver *driver)
{
	// This function is used to detect and initialize a potential XHCI USB
	// controller, as well as registering it with the higher-level interfaces.

	int status = 0;
	pciDeviceInfo pciDevInfo;
	usbController *controller = NULL;
	usbXhciData *xhciData = NULL;
	unsigned physMemSpace;
	unsigned physMemSpaceHi;
	unsigned memSpaceSize;
	unsigned hciver = 0;
	kernelDevice *dev = NULL;
	char value[32];
	int count;

	// Get the PCI device header
	status = kernelBusGetTargetInfo(busTarget, &pciDevInfo);
	if (status < 0)
		goto err_out;

	// Make sure it's a non-bridge header
	if ((pciDevInfo.device.headerType & ~PCI_HEADERTYPE_MULTIFUNC) !=
		PCI_HEADERTYPE_NORMAL)
	{
		kernelDebugError("XHCI headertype not 'normal' (%02x)",
			(pciDevInfo.device.headerType &
				~PCI_HEADERTYPE_MULTIFUNC));
		goto err_out;
	}

	// Make sure it's an XHCI controller (programming interface is 0x30 in
	// the PCI header)
	if (pciDevInfo.device.progIF != USBXHCI_PCI_PROGIF)
		goto err_out;

	// After this point, we believe we have a supported device.

	kernelDebug(debug_usb, "XHCI controller found");

	// Try to enable bus mastering
	if (pciDevInfo.device.commandReg & PCI_COMMAND_MASTERENABLE)
		kernelDebug(debug_usb, "XHCI bus mastering already enabled");
	else
		kernelBusSetMaster(busTarget, 1);

	// Disable the device's memory access and I/O decoder, if applicable
	kernelBusDeviceEnable(busTarget, 0);

	// Re-read target info
	status = kernelBusGetTargetInfo(busTarget, &pciDevInfo);
	if (status < 0)
		goto err_out;

	if (!(pciDevInfo.device.commandReg & PCI_COMMAND_MASTERENABLE))
		kernelDebugError("Couldn't enable bus mastering");
	else
		kernelDebug(debug_usb, "XHCI bus mastering enabled in PCI");

	// Make sure the BAR refers to a memory decoder
	if (pciDevInfo.device.nonBridge.baseAddress[0] & 0x1)
	{
		kernelDebugError("ABAR is not a memory decoder");
		goto err_out;
	}

	// Allocate memory for the controller
	controller = kernelMalloc(sizeof(usbController));
	if (!controller)
		goto err_out;

	// Set the controller type
	controller->type = usb_xhci;

	// Get the USB version number
	controller->usbVersion = kernelBusReadRegister(busTarget, 0x60, 8);

	// Get the interrupt number.
	controller->interruptNum = pciDevInfo.device.nonBridge.interruptLine;

	kernelLog("USB: XHCI controller USB %d.%d interrupt %d",
		((controller->usbVersion & 0xF0) >> 4),
		(controller->usbVersion & 0xF), controller->interruptNum);

	// Allocate memory for the XHCI data
	controller->data = kernelMalloc(sizeof(usbXhciData));
	if (!controller->data)
		goto err_out;

	xhciData = controller->data;

	// Get the memory range address
	physMemSpace = (pciDevInfo.device.nonBridge.baseAddress[0] & 0xFFFFFFF0);

	kernelDebug(debug_usb, "XHCI physMemSpace=0x%08x", physMemSpace);

	physMemSpaceHi = (pciDevInfo.device.nonBridge.baseAddress[1] & 0xFFFFFFF0);

	if (physMemSpaceHi)
	{
		kernelError(kernel_error, "Register memory must be mapped in 32-bit "
			"address space");
		status = ERR_NOTIMPLEMENTED;
		goto err_out;
	}

	// Determine the memory space size.  Write all 1s to the register.
	kernelBusWriteRegister(busTarget, PCI_CONFREG_BASEADDRESS0_32, 32,
		0xFFFFFFFF);

	memSpaceSize = (~(kernelBusReadRegister(busTarget,
		PCI_CONFREG_BASEADDRESS0_32, 32) & ~0xF) + 1);

	kernelDebug(debug_usb, "XHCI memSpaceSize=0x%08x", memSpaceSize);

	// Restore the register we clobbered.
	kernelBusWriteRegister(busTarget, PCI_CONFREG_BASEADDRESS0_32, 32,
		pciDevInfo.device.nonBridge.baseAddress[0]);

	// Map the physical memory address of the controller's registers into
	// our virtual address space.

	// Map the physical memory space pointed to by the decoder.
	status = kernelPageMapToFree(KERNELPROCID, physMemSpace,
		(void **) &(xhciData->capRegs), memSpaceSize);
	if (status < 0)
	{
		kernelDebugError("Error mapping memory");
		goto err_out;
	}

	// Make it non-cacheable, since this memory represents memory-mapped
	// hardware registers.
	status = kernelPageSetAttrs(KERNELPROCID, 1 /* set */,
		PAGEFLAG_CACHEDISABLE, (void *) xhciData->capRegs, memSpaceSize);
	if (status < 0)
	{
		kernelDebugError("Error setting page attrs");
		goto err_out;
	}

	// Enable memory mapping access
	if (pciDevInfo.device.commandReg & PCI_COMMAND_MEMORYENABLE)
		kernelDebug(debug_usb, "XHCI memory access already enabled");
	else
		kernelBusDeviceEnable(busTarget, PCI_COMMAND_MEMORYENABLE);

	// Re-read target info
	kernelBusGetTargetInfo(busTarget, &pciDevInfo);

	if (!(pciDevInfo.device.commandReg & PCI_COMMAND_MEMORYENABLE))
	{
		kernelDebugError("Couldn't enable memory access");
		goto err_out;
	}
	kernelDebug(debug_usb, "XHCI memory access enabled in PCI");

	// Warn if the controller is pre-release
	hciver = (xhciData->capRegs->capslenHciver >> 16);
	if (hciver < 0x0100)
		kernelLog("USB: XHCI warning, version is older than 1.0 (%d.%d%d)",
			((hciver >> 8) & 0xFF), ((hciver >> 4) & 0xF), (hciver & 0xF));

	//debugXhciCapRegs(controller);
	//debugXhciHcsParams1(controller);
	//debugXhciHcsParams2(controller);
	//debugXhciHcsParams3(controller);
	//debugXhciHccParams(controller);

	xhciData->numPorts =
		((xhciData->capRegs->hcsparams1 & USBXHCI_HCSP1_MAXPORTS) >> 24);
	kernelDebug(debug_usb, "XHCI number of ports=%d", xhciData->numPorts);

	// Record the address of the operational registers
	xhciData->opRegs = ((void *) xhciData->capRegs +
		(xhciData->capRegs->capslenHciver & 0xFF));

	//debugXhciOpRegs(controller);

	// Record the address of the doorbell registers
	xhciData->dbRegs = ((void *) xhciData->capRegs +
		(xhciData->capRegs->dboffset & ~0x3UL));

	// Record the address of the runtime registers
	xhciData->rtRegs = ((void *) xhciData->capRegs +
		(xhciData->capRegs->runtimeoffset & ~0x1FUL));

	// Record the maximum number of device slots
	xhciData->numDevSlots = min(USBXHCI_MAX_DEV_SLOTS,
		(xhciData->capRegs->hcsparams1 & USBXHCI_HCSP1_MAXDEVSLOTS));
	kernelDebug(debug_usb, "XHCI number of device slots=%d (max=%d)",
		xhciData->numDevSlots,
		(xhciData->capRegs->hcsparams1 & USBXHCI_HCSP1_MAXDEVSLOTS));

	// Calculate and record the controller's notion of a 'page size'
	xhciData->pageSize = (xhciData->opRegs->pagesz << 12);

	// Look out for 64-bit contexts - not yet supported
	if (xhciData->capRegs->hccparams & USBXHCI_HCCP_CONTEXTSIZE)
	{
		kernelError(kernel_error, "Controller is using 64-bit contexts");
		status = ERR_NOTIMPLEMENTED;
		goto err_out;
	}

	// Does the controller have any extended capabilities?
	if (xhciData->capRegs->hccparams & USBXHCI_HCCP_EXTCAPPTR)
	{
		kernelDebug(debug_usb, "XHCI controller has extended capabilities");

		// Process them
		status = processExtCaps(xhciData);
		if (status < 0)
			goto err_out;
	}

	// Reset the controller
	status = reset(controller);
	if (status < 0)
		goto err_out;

	// Set up the controller's registers, data structures, etc.
	status = setup(xhciData);
	if (status < 0)
		goto err_out;

	//debugXhciOpRegs(controller);

	// If port power is software-controlled, make sure they're all powered on
	if (xhciData->capRegs->hccparams & USBXHCI_HCCP_PORTPOWER)
	{
		for (count = 0; count < xhciData->numPorts; count ++)
			setPortPower(xhciData, count, 1);

		// The spec says we need to wait 20ms for port power to stabilize
		// (only do it once though, after they've all been turned on)
		kernelCpuSpinMs(20);
	}

	// Start the controller
	status = startStop(xhciData, 1);
	if (status < 0)
		goto err_out;

	// Allocate memory for the kernel device
	dev = kernelMalloc(sizeof(kernelDevice));
	if (!dev)
		goto err_out;

	// Set controller function calls
	controller->reset = &reset;
	controller->interrupt = &interrupt;
	controller->queue = &queue;
	controller->schedInterrupt = &schedInterrupt;
	controller->deviceRemoved = &deviceRemoved;

	// The controller's root hub
	controller->hub.controller = controller;

	// Set hub function calls
	controller->hub.detectDevices = &detectDevices;
	controller->hub.threadCall = &threadCall;

	// Set up the kernel device
	dev->device.class = kernelDeviceGetClass(DEVICECLASS_BUS);
	dev->device.subClass = kernelDeviceGetClass(DEVICESUBCLASS_BUS_USB);
	dev->driver = driver;
	dev->data = (void *) controller;

	// Initialize the variable list for attributes of the controller
	status = kernelVariableListCreate(&dev->device.attrs);
	if (status >= 0)
	{
		kernelVariableListSet(&dev->device.attrs, "controller.type", "XHCI");
		snprintf(value, 32, "%d", xhciData->numPorts);
		kernelVariableListSet(&dev->device.attrs, "controller.numPorts", value);
	}

	status = kernelDeviceAdd(busTarget->bus->dev, dev);
	if (status < 0)
		goto err_out;
	else
		return (dev);

err_out:
	if (dev)
		kernelFree(dev);
	if (controller)
	{
		if (controller->data)
			kernelFree(controller->data);
		kernelFree((void *) controller);
	}

	return (dev = NULL);
}

