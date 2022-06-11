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
//  kernelUsbXhciDriver.h
//

#if !defined(_KERNELUSBXHCIDRIVER_H)

#include "kernelLinkedList.h"
#include "kernelUsbDriver.h"

// Global things
#define USBXHCI_PCI_PROGIF				0x30
#define USBXHCI_MAX_DEV_SLOTS			16
#define USBEHCI_MAX_ROOTPORTS			255
#define USBXHCI_COMMANDRING_SIZE		16
#define USBXHCI_EVENTRING_SIZE			16
#define USBXHCI_TRANSRING_SIZE			32
#define USBXHCI_TRB_MAXBYTES			0x10000

// Bitfields for the HCSPARAMS1 register
#define USBXHCI_HCSP1_MAXPORTS			0xFF000000
#define USBXHCI_HCSP1_MAXINTRPTRS		0x0007FF00
#define USBXHCI_HCSP1_MAXDEVSLOTS		0x000000FF

// Bitfields for the HCSPARAMS2 register
#define USBXHCI_HCSP2_MAXSCRPBUFFS		0xF8000000
#define USBXHCI_HCSP2_SCRATCHPREST		0x04000000
#define USBXHCI_HCSP2_ERSTMAX			0x000000F0
#define USBXHCI_HCSP2_ISOCSCHDTHRS		0x0000000F

// Bitfields for the HCSPARAMS3 register
#define USBXHCI_HCSP3_U2DEVLATENCY		0xFFFF0000
#define USBXHCI_HCSP3_U1DEVLATENCY		0x000000FF

// Bitfields for the HCCPARAMS register
#define USBXHCI_HCCP_EXTCAPPTR			0xFFFF0000
#define USBXHCI_HCCP_MAXPRISTRARSZ		0x0000F000
#define USBXHCI_HCCP_NOSECSIDSUP		0x00000080
#define USBXHCI_HCCP_LATTOLMESSCAP		0x00000040
#define USBXHCI_HCCP_LIGHTHCRESET		0x00000020
#define USBXHCI_HCCP_PORTIND			0x00000010
#define USBXHCI_HCCP_PORTPOWER			0x00000008
#define USBXHCI_HCCP_CONTEXTSIZE		0x00000004
#define USBXHCI_HCCP_BANDNEGCAP			0x00000002
#define USBXHCI_HCCP_64ADDRCAP			0x00000001

// Extended capability types
#define USBXHCI_EXTCAP_RESERVED			0x00
#define USBXHCI_EXTCAP_LEGACYSUPP		0x01
#define USBXHCI_EXTCAP_SUPPPROTO		0x02
#define USBXHCI_EXTCAP_EXTPOWERMAN		0x03
#define USBXHCI_EXTCAP_IOVIRT			0x04
#define USBXHCI_EXTCAP_MESSAGEINT		0x05
#define USBXHCI_EXTCAP_LOCALMEM			0x06
#define USBXHCI_EXTCAP_USBDEBUG			0x0A
#define USBXHCI_EXTCAP_EXTMESSINT		0x11

// Bitfields for the legacy support capability register
#define USBXHCI_LEGSUPCAP_BIOSOWND		0x00010000
#define USBXHCI_LEGSUPCAP_OSOWNED		0x01000000

// Bitfields for the command register
#define USBXHCI_CMD_ENBLU3MFIDXSTP		0x00000800
#define USBXHCI_CMD_ENBLWRAPEVENT		0x00000400
#define USBXHCI_CMD_CTRLRESTSTATE		0x00000200
#define USBXHCI_CMD_CTRLSAVESTATE		0x00000100
#define USBXHCI_CMD_LIGHTHCRESET		0x00000080
#define USBXHCI_CMD_HOSTSYSERRENBL		0x00000008
#define USBXHCI_CMD_INTERUPTRENBL		0x00000004
#define USBXHCI_CMD_HCRESET				0x00000002
#define USBXHCI_CMD_RUNSTOP				0x00000001

// Bitfields for the status register
#define USBXHCI_STAT_HOSTCTRLERR		0x00001000	// RO
#define USBXHCI_STAT_CTRLNOTREADY		0x00000800	// RO
#define USBXHCI_STAT_SAVERESTERR		0x00000400	// RW1C
#define USBXHCI_STAT_RESTSTATE			0x00000200	// RO
#define USBXHCI_STAT_SAVESTATE			0x00000100	// RO
#define USBXHCI_STAT_PORTCHANGE			0x00000010	// RW1C
#define USBXHCI_STAT_EVENTINTR			0x00000008	// RW1C
#define USBXHCI_STAT_HOSTSYSERROR		0x00000004	// RW1C
#define USBXHCI_STAT_HCHALTED			0x00000001	// RO
#define USBXHCI_STAT_INTERRUPTMASK	\
	(USBXHCI_STAT_PORTCHANGE |		\
	USBXHCI_STAT_EVENTINTR |		\
	USBXHCI_STAT_HOSTSYSERROR)
#define USBXHCI_STAT_ROMASK			\
	(USBXHCI_STAT_HOSTCTRLERR |		\
	USBXHCI_STAT_CTRLNOTREADY |		\
	USBXHCI_STAT_RESTSTATE |		\
	USBXHCI_STAT_SAVESTATE |		\
	USBXHCI_STAT_HCHALTED)
#define USBXHCI_STAT_RW1CMASK		\
	(USBXHCI_STAT_SAVERESTERR |		\
	USBXHCI_STAT_PORTCHANGE |		\
	USBXHCI_STAT_EVENTINTR |		\
	USBXHCI_STAT_HOSTSYSERROR)

// Bitfields for the command ring control register
#define USBXHCI_CRCR_CMDRNGRUNNING		0x00000008
#define USBXHCI_CRCR_COMMANDABORT		0x00000004
#define USBXHCI_CRCR_COMMANDSTOP		0x00000002
#define USBXHCI_CRCR_RINGCYCSTATE		0x00000001

// Bitfields for port status/control registers
#define USBXHCI_PORTSC_WARMRESET		0x80000000	// RW1S
#define USBXHCI_PORTSC_DEVNOTREMV		0x40000000	// RO
#define USBXHCI_PORTSC_WAKEOVCREN		0x08000000	// RWS
#define USBXHCI_PORTSC_WAKEDISCEN		0x04000000	// RWS
#define USBXHCI_PORTSC_WAKECONNEN		0x02000000	// RWS
#define USBXHCI_PORTSC_COLDATTACH		0x01000000	// RO
#define USBXHCI_PORTSC_CHANGES			0x00FE0000	// (all RW1CS)
#define USBXHCI_PORTSC_CONFERR_CH		0x00800000	// RW1CS
#define USBXHCI_PORTSC_LINKSTAT_CH		0x00400000	// RW1CS
#define USBXHCI_PORTSC_RESET_CH			0x00200000	// RW1CS
#define USBXHCI_PORTSC_OVERCURR_CH		0x00100000	// RW1CS
#define USBXHCI_PORTSC_WARMREST_CH		0x00080000	// RW1CS
#define USBXHCI_PORTSC_ENABLED_CH		0x00040000	// RW1CS
#define USBXHCI_PORTSC_CONNECT_CH		0x00020000	// RW1CS
#define USBXHCI_PORTSC_LINKWSTROBE		0x00010000	// RW
#define USBXHCI_PORTSC_PORTIND			0x0000C000	// RWS
#define USBXHCI_PORTSC_PORTSPEED		0x00003C00	// ROS
#define USBXHCI_PORTSC_PORTPOWER		0x00000200	// RWS
#define USBXHCI_PORTSC_LINKSTATE		0x000001E0	// RWS
#define USBXHCI_PORTSC_PORTRESET		0x00000010	// RW1S
#define USBXHCI_PORTSC_OVERCURRENT		0x00000008	// RO
#define USBXHCI_PORTSC_PORTENABLED		0x00000002	// RW1CS
#define USBXHCI_PORTSC_CONNECTED		0x00000001	// ROS
#define USBXHCI_PORTSC_ROMASK		\
	(USBXHCI_PORTSC_DEVNOTREMV |	\
	USBXHCI_PORTSC_COLDATTACH |		\
	USBXHCI_PORTSC_PORTSPEED |		\
	USBXHCI_PORTSC_OVERCURRENT |	\
	USBXHCI_PORTSC_CONNECTED)
#define USBXHCI_PORTSC_RW1CMASK		\
	(USBXHCI_PORTSC_CHANGES |		\
	USBXHCI_PORTSC_PORTENABLED)

// Bitfields for the interrupter register set
#define USBXHCI_IMAN_INTSENABLED		0x00000002
#define USBXHCI_IMAN_INTPENDING			0x00000001
#define USBXHCI_IMOD_COUNTER			0xFFFF0000
#define USBXHCI_IMOD_INTERVAL			0x0000FFFF
#define USBXHCI_ERSTSZ_TABLESIZE		0x0000FFFF
#define USBXHCI_ERDP_HANDLERBUSY		0x00000008
#define USBXHCI_ERDP_SEGINDEX			0x00000007

// Endpoint types
#define USBXHCI_EPTYPE_INVALID			0
#define USBXHCI_EPTYPE_ISOCH_OUT		1
#define USBXHCI_EPTYPE_BULK_OUT			2
#define USBXHCI_EPTYPE_INTR_OUT			3
#define USBXHCI_EPTYPE_CONTROL			4
#define USBXHCI_EPTYPE_ISOCH_IN			5
#define USBXHCI_EPTYPE_BULK_IN			6
#define USBXHCI_EPTYPE_INTR_IN			7

// Bitfields for the slot context structure
#define USBXHCI_SLTCTXT_CTXTENTS		0xF8000000
#define USBXHCI_SLTCTXT_HUB				0x04000000
#define USBXHCI_SLTCTXT_MTT				0x02000000
#define USBXHCI_SLTCTXT_SPEED			0x00F00000
#define USBXHCI_SLTCTXT_ROUTESTRNG		0x000FFFFF
#define USBXHCI_SLTCTXT_NUMPORTS		0xFF000000
#define USBXHCI_SLTCTXT_ROOTPRTNUM		0x00FF0000
#define USBXHCI_SLTCTXT_MAXEXITLAT		0x0000FFFF
#define USBXHCI_SLTCTXT_INTRTARGET		0xFFC00000
#define USBXHCI_SLTCTXT_TTT				0x00030000
#define USBXHCI_SLTCTXT_TTPORTNUM		0x0000FF00
#define USBXHCI_SLTCTXT_TTHUBSLOT		0x000000FF
#define USBXHCI_SLTCTXT_SLOTSTATE		0xF8000000
#define USBXHCI_SLTCTXT_USBDEVADDR		0x000000FF

// Bitfields for the endpoint context structure
#define USBXHCI_EPCTXT_INTERVAL			0x00FF0000
#define USBXHCI_EPCTXT_LINSTRARRAY		0x00008000
#define USBXHCI_EPCTXT_MAXPRIMSTR		0x00007C00
#define USBXHCI_EPCTXT_MULT				0x00000300
#define USBXHCI_EPCTXT_EPSTATE			0x00000007
#define USBXHCI_EPCTXT_MAXPKTSIZE		0xFFFF0000
#define USBXHCI_EPCTXT_MAXBRSTSIZE		0x0000FF00
#define USBXHCI_EPCTXT_HSTINITDSBL		0x00000080
#define USBXHCI_EPCTXT_ENDPNTTYPE		0x00000038
#define USBXHCI_EPCTXT_CERR				0x00000006
#define USBXHCI_EPCTXT_MAXESITPAYL		0xFFFF0000
#define USBXHCI_EPCTXT_AVGTRBLEN		0x0000FFFF

// TRB types
#define USBXHCI_TRBTYPE_MASK			0xFC00
#define USBXHCI_TRBTYPE_RESERVED		(0 << 10)
#define USBXHCI_TRBTYPE_NORMAL			(1 << 10)	// Transfer ring
#define USBXHCI_TRBTYPE_SETUPSTG		(2 << 10)	// "
#define USBXHCI_TRBTYPE_DATASTG			(3 << 10)	// "
#define USBXHCI_TRBTYPE_STATUSSTG		(4 << 10)	// "
#define USBXHCI_TRBTYPE_ISOCH			(5 << 10)	// "
#define USBXHCI_TRBTYPE_LINK			(6 << 10)	// "
#define USBXHCI_TRBTYPE_EVENTDATA		(7 << 10)	// "
#define USBXHCI_TRBTYPE_TRANSNOOP		(8 << 10)	// "
#define USBXHCI_TRBTYPE_ENABLESLOT		(9 << 10)	// Command ring
#define USBXHCI_TRBTYPE_DISBLESLOT		(10 << 10)	// "
#define USBXHCI_TRBTYPE_ADDRESSDEV		(11 << 10)	// "
#define USBXHCI_TRBTYPE_CFGENDPT		(12 << 10)	// "
#define USBXHCI_TRBTYPE_EVALCNTXT		(13 << 10)	// "
#define USBXHCI_TRBTYPE_RESETENDPT		(14 << 10)	// "
#define USBXHCI_TRBTYPE_STOPENDPT		(15 << 10)	// "
#define USBXHCI_TRBTYPE_SETTRDQ			(16 << 10)	// "
#define USBXHCI_TRBTYPE_RESETDEV		(17 << 10)	// "
#define USBXHCI_TRBTYPE_FORCEEVNT		(18 << 10)	// "
#define USBXHCI_TRBTYPE_NEGBNDWDTH		(19 << 10)	// "
#define USBXHCI_TRBTYPE_SETLATTVAL		(20 << 10)	// "
#define USBXHCI_TRBTYPE_GETPORTBW		(21 << 10)	// "
#define USBXHCI_TRBTYPE_FORCEHDR		(22 << 10)	// "
#define USBXHCI_TRBTYPE_CMDNOOP			(23 << 10)	// "
#define USBXHCI_TRBTYPE_TRANSFER		(32 << 10)	// Event ring
#define USBXHCI_TRBTYPE_CMDCOMP			(33 << 10)	// "
#define USBXHCI_TRBTYPE_PRTSTATCHG		(34 << 10)	// "
#define USBXHCI_TRBTYPE_BANDWREQ		(35 << 10)	// "
#define USBXHCI_TRBTYPE_DOORBELL		(36 << 10)	// "
#define USBXHCI_TRBTYPE_HOSTCONT		(37 << 10)	// "
#define USBXHCI_TRBTYPE_DEVNOTIFY		(38 << 10)	// "
#define USBXHCI_TRBTYPE_MFIDXWRAP		(39 << 10)	// "

// TRB completion codes
#define USBXHCI_TRBCOMP_MASK			(0xFF << 24)
#define USBXHCI_TRBCOMP_INVALID			(0 << 24)
#define USBXHCI_TRBCOMP_SUCCESS			(1 << 24)
#define USBXHCI_TRBCOMP_DATABUFF		(2 << 24)
#define USBXHCI_TRBCOMP_BABBLE			(3 << 24)
#define USBXHCI_TRBCOMP_TRANS			(4 << 24)
#define USBXHCI_TRBCOMP_TRB				(5 << 24)
#define USBXHCI_TRBCOMP_STALL			(6 << 24)
#define USBXHCI_TRBCOMP_RESOURCE		(7 << 24)
#define USBXHCI_TRBCOMP_BANDWIDTH		(8 << 24)
#define USBXHCI_TRBCOMP_NOSLOTS			(9 << 24)
#define USBXHCI_TRBCOMP_INVALIDSTREAM	(10 << 24)
#define USBXHCI_TRBCOMP_SLOTNOTENAB		(11 << 24)
#define USBXHCI_TRBCOMP_ENDPTNOTENAB	(12 << 24)
#define USBXHCI_TRBCOMP_SHORTPACKET		(13 << 24)
#define USBXHCI_TRBCOMP_RINGUNDERRUN	(14 << 24)
#define USBXHCI_TRBCOMP_RINGOVERRUN		(15 << 24)
#define USBXHCI_TRBCOMP_VFEVNTRINGFULL	(16 << 24)
#define USBXHCI_TRBCOMP_PARAMETER		(17 << 24)
#define USBXHCI_TRBCOMP_BANDWOVERRUN	(18 << 24)
#define USBXHCI_TRBCOMP_CONTEXTSTATE	(19 << 24)
#define USBXHCI_TRBCOMP_NOPINGRESPONSE	(20 << 24)
#define USBXHCI_TRBCOMP_EVNTRINGFULL	(21 << 24)
#define USBXHCI_TRBCOMP_INCOMPATDEVICE	(22 << 24)
#define USBXHCI_TRBCOMP_MISSEDSERVICE	(23 << 24)
#define USBXHCI_TRBCOMP_CMDRINGSTOPPED	(24 << 24)
#define USBXHCI_TRBCOMP_COMMANDABORTED	(25 << 24)
#define USBXHCI_TRBCOMP_STOPPED			(26 << 24)
#define USBXHCI_TRBCOMP_STOPPEDLENGTH	(27 << 24)
#define USBXHCI_TRBCOMP_MAXLATTOOLARGE	(29 << 24)
#define USBXHCI_TRBCOMP_ISOCHBUFFOVER	(31 << 24)
#define USBXHCI_TRBCOMP_EVENTLOST		(32 << 24)
#define USBXHCI_TRBCOMP_UNDEFINED		(33 << 24)
#define USBXHCI_TRBCOMP_INVSTREAMID		(34 << 24)
#define USBXHCI_TRBCOMP_SECBANDWIDTH	(35 << 24)
#define USBXHCI_TRBCOMP_SPLITTRANS		(36 << 24)

// TRB flags
#define USBXHCI_TRBFLAG_BLKSETADDR		0x0200
#define USBXHCI_TRBFLAG_IMMEDDATA		0x0040
#define USBXHCI_TRBFLAG_INTONCOMP		0x0020
#define USBXHCI_TRBFLAG_CHAIN			0x0010
#define USBXHCI_TRBFLAG_INTONSHORT		0x0004
#define USBXHCI_TRBFLAG_EVALNEXT		0x0002
#define USBXHCI_TRBFLAG_TOGGLECYCL		0x0002
#define USBXHCI_TRBFLAG_CYCLE			0x0001

// Bit of symbolism to improve readability when manipulating anonymous bits,
// because I don't like constants in the code
#define BIT(num) (1 << (num))

typedef enum {
	xhcispeed_unknown = 0,
	xhcispeed_full = 1,
	xhcispeed_low = 2,
	xhcispeed_high = 3,
	xhcispeed_super = 4

} xhciDevSpeed;

// Generic context structure
typedef volatile struct {
	unsigned dwords[8];

} __attribute__((packed)) xhciCtxt;

// Input control context structure
typedef volatile struct {
	unsigned entFlagsSpeedRoute;
	unsigned numPortsPortLat;
	unsigned targetTT;
	unsigned slotStateDevAddr;
	unsigned res[4];

} __attribute__((packed)) xhciSlotCtxt;

// Endpoint context structure
typedef volatile struct {
	unsigned intvlLsaMaxPstrMultEpState;
	unsigned maxPSizeMaxBSizeEpTypeCerr;
	unsigned trDeqPtrLo;
	unsigned trDeqPtrHi;
	unsigned maxEpEsitAvTrbLen;
	unsigned res[3];

} __attribute__((packed)) xhciEndpointCtxt;

// Slot context structure
typedef volatile struct {
	unsigned drop;
	unsigned add;
	unsigned res[6];

} __attribute__((packed)) xhciInputCtrlCtxt;

// Device context structure
typedef volatile struct {
	xhciSlotCtxt slotCtxt;
	xhciEndpointCtxt endpointCtxt[31];

} __attribute__((packed)) xhciDevCtxt;

// Device context structure
typedef volatile struct {
	xhciInputCtrlCtxt inputCtrlCtxt;
	xhciDevCtxt devCtxt;

} __attribute__((packed)) xhciInputCtxt;

typedef volatile struct {
	unsigned baseAddrLo;
	unsigned baseAddrHi;
	unsigned segSize;
	unsigned res;

} __attribute__((packed)) xhciEventRingSegTable;

// TRB (Transfer Request Block) structure
typedef volatile struct {
	unsigned paramLo;
	unsigned paramHi;
	unsigned status;
	unsigned short typeFlags;
	unsigned short control;

} __attribute__((packed)) xhciTrb;

// Setup TRB for control transfers
typedef volatile struct {
	usbDeviceRequest request;
	unsigned intTargetTransLen;
	unsigned short typeFlags;
	unsigned short control;

} __attribute__((packed)) xhciSetupTrb;

// Port register set
typedef volatile struct {
	unsigned portsc;
	unsigned portpmsc;
	unsigned portli;
	unsigned res;

} __attribute__((packed)) xhciPortRegSet;

// Interrupter register set
typedef volatile struct {
	unsigned intrMan;
	unsigned intrMod;
	unsigned evtRngSegTabSz;
	unsigned res;
	unsigned evtRngSegBaseLo;
	unsigned evtRngSegBaseHi;
	unsigned evtRngDeqPtrLo;
	unsigned evtRngDeqPtrHi;

} __attribute__((packed)) xhciIntrRegSet;

// Runtime register set
typedef volatile struct {
	unsigned mfindex;
	char res[28];
	xhciIntrRegSet intrReg[];

} __attribute__((packed)) xhciRuntimeRegs;

// Doorbell register set
typedef volatile struct {
	unsigned doorbell[256];

} __attribute__((packed)) xhciDoorbellRegs;

// Extended capability pointer register
typedef volatile struct {
	unsigned char id;
	unsigned char next;
	unsigned short capSpec;

} __attribute__((packed)) xhciExtendedCaps;

// Legacy support capability register set
typedef volatile struct {
	unsigned legSuppCap;
	unsigned legSuppContStat;

} __attribute__((packed)) xhciLegacySupport;

typedef volatile struct {
	unsigned suppProtCap;
	unsigned suppProtName;
	unsigned suppProtPorts;

} __attribute__((packed)) xhciSupportedProtocol;

// Operational registers
typedef volatile struct {
	unsigned cmd;					// 0x00-0x03
	unsigned stat;					// 0x04-0x07
	unsigned pagesz;				// 0x08-0x0B
	char res1[8];					// 0x0C-0x13
	unsigned dncntrl;				// 0x14-0x17
	unsigned cmdrctrlLo;			// 0x18-0x1B
	unsigned cmdrctrlHi;			// 0x1C-0x1F
	char res2[16];					// 0x20-0x2F
	unsigned dcbaapLo;				// 0x30-0x33
	unsigned dcbaapHi;				// 0x34-0x37
	unsigned config;				// 0x38-0x3B
	char res3[964];					// 0x3C-0x3FF
	xhciPortRegSet portRegSet[256];	// 0x400-0x13FF

} __attribute__((packed)) xhciOpRegs;

// Capability registers
typedef volatile struct {
	unsigned capslenHciver;
	unsigned hcsparams1;
	unsigned hcsparams2;
	unsigned hcsparams3;
	unsigned hccparams;
	unsigned dboffset;
	unsigned runtimeoffset;

} __attribute__((packed)) xhciCapRegs;

// A structure for managing transfer rings (event, transfer, and command)
typedef struct {
	int numTrbs;
	int nextTrb;
	int cycleState;
	unsigned trbsPhysical;
	xhciTrb *trbs;

} xhciTrbRing;

// A structure to store information about a device slot
typedef struct {
	int num;
	usbDevice *usbDev;
	xhciInputCtxt *inputCtxt;
	unsigned inputCtxtPhysical;
	xhciDevCtxt *devCtxt;
	unsigned devCtxtPhysical;
	xhciTrbRing *transRings[USB_MAX_ENDPOINTS];

} xhciSlot;

// For keeping track of interrupt registrations
typedef struct {
	usbDevice *usbDev;
	int endpoint;
	xhciSlot *slot;
	void *buffer;
	unsigned dataLen;
	xhciTrb trb;
	xhciTrb *queuedTrb;
	void (*callback)(usbDevice *, void *, unsigned);

} xhciIntrReg;

// Our main structure for storing information about the controller
typedef struct {
	xhciCapRegs *capRegs;
	xhciOpRegs *opRegs;
	xhciDoorbellRegs *dbRegs;
	xhciRuntimeRegs *rtRegs;
	unsigned pageSize;
	int numPorts;
	usbProtocol portProtos[USBEHCI_MAX_ROOTPORTS];
	int numDevSlots;
	int numIntrs;
	unsigned long long *devCtxtPhysPtrs;
	xhciTrbRing *commandRing;
	xhciTrbRing **eventRings;
	kernelLinkedList slots;
	kernelLinkedList intrRegs;
	unsigned portChangedBitmap;

} usbXhciData;

#define _KERNELUSBXHCIDRIVER_H
#endif

