#if defined(DUAL_PORT) && defined(NUC970)
#if defined(DIR_PATH_IN_PROJ)
	#include "typedef.h"
	#include "SWIcall.h"
	#include "OSAPI.h"
	#include "tcpcommon.h"
	#include "putInfoSocket.h"
#elif defined(S3C6410)	|| defined(S3C2416) || defined(NUC970)
	#include "typedef.h"
	#include "SWIcall.h"
	#include "OSAPI.h"
	#include "tcpcommon.h"
	#include "..\fwvisa\pvos\tcp_wo_move\putInfoSocket.h"
#else	
	#include "\fwvisa\CommonSrc\typedef.h"
	#include "\fwvisa\commonsrc\SWIcall.h"
	#include "\FwVisa\CommonSrc\OSAPI.h"
	#include "\FwVisa\CommonSrc\tcpcommon.h"
	#include "\fwvisa\pvos\tcp_wo_move\putInfoSocket.h"
#endif
#include <string.h>
#if defined(NUC970)
	#include "nuc970.h"
	#include "sys.h"
	#include "NUC970_MAC.h"
	#include "misc.h"
#else
	#include "w90p910.h"
	#include "W90X900_reg.h"
	#include "wbio.h"
#endif
#include "csphy.h"
NETBUF  *_iqueue1_first, *_iqueue1_last;   
int m_lastTime1 = 0;
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
int mac1_flowcontrl= 1;
int mac1_flowHighThread = 0;
int mac1_flowLowThread = 0;
int mac1_rxFrameCount = 0;
#endif
NETBUF* Net1RxBuf;
#if defined(NUC970)
#else
NETBUF rx1buf[MaxRxFrameDescriptors];
__align(16) static sFrameDescriptor Rx1FDBaseAddr[MaxRxFrameDescriptors];		
__align(16) static sFrameDescriptor Tx1FDBaseAddr[MaxTxFrameDescriptors];		
#endif
volatile UINT32 gMCMDR1 = MCMDR_FDUP | MCMDR_SPCRC | MCMDR_RXON | 
                           MCMDR_EnMDC ;
volatile UINT32 gMIEN1 = EnTXINTR | EnTXCP | EnRXINTR | EnTDU | EnRXGD | 
                          EnCRCE | EnRP | EnPTLE | EnRXOV |
                          EnLC | EnTXABT | EnNCS | EnEXDEF | EnTXEMP |
                          EnTxBErr | EnRxBErr | EnRDU | EnCFR;
volatile UINT32 gCAMCMR1 = CAM_ECMP;
volatile UINT32 gCTx1FDPtr, gWTx1FDPtr, gCRx1FDPtr;
sFrameSendBuffer* 	gWTx1DataStartPtr;
volatile UINT32 gEMAC1Cam0M = 0 , gEMAC1Cam0L = 0;
volatile int TDU1_Flag;
volatile char Tx1Ready=1;
int	m_nJustLinkedin10s1 = 0;
volatile unsigned int PHY1AD; 
NET_ADAPTER_INFO* m_pEthInfo1;
#if defined(DUAL_PORT)
	volatile char LinkPort=0,PreLinkPort=0;
	ipaddr PreMyIpAddr;
	ethaddr PreMyEthAddr;
	ipaddr PreServerIpAddr;
	ethaddr PreServerEthAddr;
	ipaddr PreNetMask;
	ipaddr PrePriDnsIpAddr;
	ipaddr PreAltDnsIpAddr;
	ipaddr PreDefDnsIpAddr;
	ipaddr PreNetMask;
	ethaddr PreMyEthAddr;
	int Debug_Length;
#endif
#if 1
	volatile unsigned int GW1PHYAD[2]; 
	volatile bool GW1link1Ok = false;
	volatile bool GW1link2Ok = false;
	volatile char PHY1Chip;
	void ResetPhy1Chip(void);
#endif
int r1frame = 0;
int w1frame = 0;
int w1frame1 = 0;
#if defined(W90P950)
	#if defined(ETHDEBUG)
		#define	Disp	UART_DebugString
	#else
		#define	Disp	
	#endif
#endif
#if defined(NUC970)
	#define ETHDEBUG
	#if defined(ETHDEBUG)
		#define	Disp	sysprintf
	#else
		#define	Disp	
	#endif
	#define ETH1_TRIGGER_RX()    outpw(REG_EMAC1_RSDR, 0)
	#define ETH1_TRIGGER_TX()    outpw(REG_EMAC1_TSDR, 0)
	#define ETH0_ENABLE_TX()     outpw(REG_EMAC1_MCMDR, inpw(REG_EMAC1_MCMDR) | 0x100)
	#define ETH0_ENABLE_RX()     outpw(REG_EMAC1_MCMDR, inpw(REG_EMAC1_MCMDR) | 0x1)
	#define ETH0_DISABLE_TX()    outpw(REG_EMAC1_MCMDR, inpw(REG_EMAC1_MCMDR) & ~0x100)
	#define ETH0_DISABLE_RX()    outpw(REG_EMAC1_MCMDR, inpw(REG_EMAC1_MCMDR) & ~0x1)
#endif
#define INTERRUPT_MUTEX	0x01
#define SEND_MUTEX		0x02
#define RECEIVE_MUTEX	0x03
int m_nP9701_Mutex = 0;
extern DWORD SystemTick5ms;
extern DWORD SystemTick;    		
void C970_mutex_on(int mutexType, int* mutexAddr)
{
	_OS_ENTER_CRITICAL();
	while (1) {
		if 	(mutexAddr == 0) {
			if (m_nP9701_Mutex == 0) {
				m_nP9701_Mutex = mutexType;
				break;
			} else {
				_OS_EXIT_CRITICAL();	
				udelay(2000);
				_OS_ENTER_CRITICAL();
			}
		} else {
			if  (*mutexAddr == 0) {
				*mutexAddr = mutexType;
				break;
			} else {
				OSTimeDly1ms(0);
			}
		}
	}
	_OS_EXIT_CRITICAL();	
}
void C970_mutex_off(int mutexType, int* mutexAddr)
{
	_OS_ENTER_CRITICAL();
	if 	(mutexAddr == 0) {
		if (m_nP9701_Mutex == mutexType) {
			m_nP9701_Mutex = 0;
		} else {
		}
	} else {
		if (*mutexAddr == mutexType) {
			*mutexAddr = 0;
		} else {
		}
	}
	_OS_EXIT_CRITICAL();	
}
void Mac1_EnableInt()
{
    EnableInt(EMC1_TX_IRQn);
    EnableInt(EMC1_RX_IRQn);
    ETH1_TRIGGER_RX();
}
void Mac1_DisableInt()
{
    DisableInt(EMC1_TX_IRQn);
    DisableInt(EMC1_RX_IRQn);
}
void Mac1_EnableBroadcast()
{
	CAMCMR1 |= CAM_ABP ;
}
void Mac1_DisableBroadcast()
{
	CAMCMR1 &= ~CAM_ABP ;
}
void Mac1_EnableMulticast()
{
	CAMCMR1 |= CAM_AMP ;
}
void Mac1_DisableMulticast()
{
	CAMCMR1 &= ~CAM_AMP ;
}
void Mac1_EnableUnicast()
{
	CAMCMR1 |= CAM_AUP ;
}
void Mac1_DisableUnicast()
{
	CAMCMR1 &= ~CAM_AUP ;
}
void Mii1StationWrite(UINT32 PhyInAddr, UINT32 PhyAddr, UINT32 PhyWrData)
{
	int volatile i = 1000;
		MIID1 = PhyWrData ;
		MIIDA1 = PhyInAddr | PhyAddr | PHYBUSY | PHYWR | MDCCR;
		while (i--) ;
		while ( (MIIDA1 & PHYBUSY) )  ;
}
UINT32 Mii1StationRead(UINT32 PhyInAddr, UINT32 PhyAddr)
{
 	UINT32 volatile PhyRdData ;
		MIIDA1 = PhyInAddr | PhyAddr | PHYBUSY | MDCCR;
		while( (MIIDA1 & PHYBUSY) )  ;
		PhyRdData = MIID1 ;  
 	return PhyRdData ;
}
#if 1	
void AutoDetectPhy1Addr()
{
    int i,j=0;
    int flag;
    unsigned int temp;
	if(PHY1Chip==CHIP_DM8603)
	{
	    flag = 0;
	   	GW1PHYAD[0] = GW1PHYAD[1] =0;
		for (i=0; i<8; i++)
		{
			temp = Mii1StationRead(0, (i << 8));
			Disp("i=%d, temp=%xH \n",i,temp);
			if ((temp & 0xFFFF) == 0x3100)
			{
				GW1PHYAD[j++] = i << 8;			
				flag = 1;			
			}
		}
		if (flag == 0) {
			Disp("AutoDetectPhy1Addr() : Don't find PHY1 Addr!\n");
		}
		else
		{
			Disp("GW1PHYAD[0]=%03xH,GW1PHYAD[1]=%03xH \n",GW1PHYAD[0],GW1PHYAD[1]);
		}
	}
    else Disp("!! PHY1 IC unknow !!\n");
}
void MAC1_SetLinkOk(int ms) 
{
   	int RdValue = 0;
   	int t0;
	UINT32 volatile regANA, regANLPA;
#if defined(VIRTUAL_NIC_SUPPORT)
	NET_ADAPTER_INFO *pVirAdapter = NULL;
	if (m_pEthInfo1->virtualPortSeq >= 1 && m_pEthInfo1->virtualPortSeq <= ETH_GetEthPortsOnHmi()) 
	{
		pVirAdapter = (NET_ADAPTER_INFO *)ETH_GetAdapterFrmPort(m_pEthInfo1->virtualPortSeq);
	}
	else 
	{
		pVirAdapter = NULL;
	}
#endif
	if(PHY1Chip == CHIP_DM8603)
	{
		Disp("** DM8603 MAC1_SetLinkOk **\n");
		if (ms == 0) {
	    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[0]) ;	
	    	Disp("GW11 PHY_STATUS_REG = %xH\n",RdValue);    
			if ((RdValue & AN_COMPLETE) != 0)	
			{
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
					GW1link1Ok = true;
				}
			} else {
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
					GW1link1Ok = false;
				}
			}
	    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[1]) ;	
	    	Disp("GW12 PHY_STATUS_REG = %xH\n",RdValue);  
			if ((RdValue & AN_COMPLETE) != 0)	
			{
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
					GW1link2Ok = true;
				}
			} else {
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
					GW1link2Ok = false;
				}
			}
		} else {
			t0 = GetTickCount1ms();
			while (1) 	 
		    {
		    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[0]) ;	
				if ((RdValue & AN_COMPLETE) != 0)
				{
					GW1link1Ok = true;
					break;
				}
				if ((GetTickCount1ms()-t0) >= ms )			
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
		  			GW1link1Ok = false;
					break;
				}
				OSTimeDly1ms(10);
		    }
			t0 = GetTickCount1ms();
			while (1) 	 
		    {
		    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[1]) ;	
				if ((RdValue & AN_COMPLETE) != 0)
				{
					GW1link2Ok = true;
					break;
				}
				if ((GetTickCount1ms()-t0) >= ms )			
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
					GW1link2Ok = false;
					break;
				}
				OSTimeDly1ms(10);
		    }
		}
		if (GW1link1Ok) {
		    regANA   = Mii1StationRead(PHY_ANA_REG, GW1PHYAD[0]);		
		    regANLPA = Mii1StationRead(PHY_ANLPA_REG, GW1PHYAD[0]);	
		    if ((regANA & 0x100) && (regANLPA & 0x100)) 
		    { 
		    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
		    }
		    else if ((regANA & 0x80) && (regANLPA & 0x80)) 
		    {
		    	MCMDR1 |= MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;
		    } 
		    else if ((regANA & 0x40) && (regANLPA & 0x40)) 
		    {  	
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 |= MCMDR_FDUP;    
		    }
		    else 
		    {   
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;        
		    }
		} 
		if (GW1link2Ok) {
		    regANA   = Mii1StationRead(PHY_ANA_REG, GW1PHYAD[1]);		
		    regANLPA = Mii1StationRead(PHY_ANLPA_REG, GW1PHYAD[1]);	
		    if ((regANA & 0x100) && (regANLPA & 0x100)) 
		    { 
		    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
		    }
		    else if ((regANA & 0x80) && (regANLPA & 0x80)) 
		    {
		    	MCMDR1 |= MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;
		    } 
		    else if ((regANA & 0x40) && (regANLPA & 0x40)) 
		    {  	
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 |= MCMDR_FDUP;    
		    }
		    else 
		    {   
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;        
		    }
		} 
		if (GW1link1Ok || GW1link2Ok) {
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
			{
				if (m_pEthInfo1->bUseInt == 0) 
				{
					m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED);
				}
				m_pEthInfo1->dealMsg = 0;
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
	#if defined(VIRTUAL_NIC_SUPPORT)
				if (pVirAdapter) 
				{
					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
				}
	#endif
				Mac1_EnableInt();
			}
		} else {
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
			{
				Mac1_DisableInt();
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
				m_pEthInfo1->dealMsg = 1;
	#if defined(VIRTUAL_NIC_SUPPORT)
				if (pVirAdapter) 
				{
					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
				}
	#endif
				ResetPhy1Chip();
			}
		}
	}
	return;	
}
#endif
void ResetPhy1Chip(void)
{
	UINT32	volatile RdValue;
	UINT32	volatile uReg;
	int	i;
	int	t0;  	
	if(PHY1Chip == CHIP_DM8603)
	{
		for(i=0;i<2;i++)
		{
			t0 = 100;
			Mii1StationWrite(PHY_CNTL_REG, GW1PHYAD[i], RESET_PHY); 	
			while (1) 
			{
				RdValue = Mii1StationRead(PHY_CNTL_REG, GW1PHYAD[i]) ;
	  			if ((RdValue&RESET_PHY)==0)
					break;
	     		OSTimeDly1ms(1);
	   			if (!(t0--)) 
	   	  		{
	   	  			Disp("Reset DM8603 PHY FAILED!!\n");
					break;
				}
			}
	 	}
	   	Mii1StationWrite(PHY_ANA_REG, GW1PHYAD[0], DR100_TX_FULL|DR100_TX_HALF|DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);
	   	RdValue = Mii1StationRead(PHY_CNTL_REG, GW1PHYAD[0]) ;
	 	RdValue = (RdValue | RESTART_AN | ENABLE_AN);
	   	Mii1StationWrite(PHY_CNTL_REG, GW1PHYAD[0], RdValue);
	   	Mii1StationWrite(PHY_ANA_REG, GW1PHYAD[1], DR100_TX_FULL|DR100_TX_HALF|DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);
	   	RdValue = Mii1StationRead(PHY_CNTL_REG, GW1PHYAD[1]) ;
	 	RdValue = (RdValue | RESTART_AN | ENABLE_AN);
	   	Mii1StationWrite(PHY_CNTL_REG, GW1PHYAD[1], RdValue);
	}
	return;
}
void EnableCam1Entry(int entry)
{
  	CAMEN1 |= 0x00000001 << entry ;
}
void DisableCam1Entry(int entry)
{
  	CAMEN1 &= ~(0x00000001 << entry) ;
}
void FillCam1Entry(int entry, UINT32 msw, UINT32 lsw)
{
  	CAM1xM_Reg(entry) = msw;
  	CAM1xL_Reg(entry) = lsw;
 	EnableCam1Entry(entry);
}
#if defined(VIRTUAL_NIC_SUPPORT)
void MAC1_SetVirtualMacAddr(int entry, ethaddr mac)
{
	int i;
	for (i = 0; i < (int )MAC_ADDR_SIZE-2; i++)
		gEMAC1Cam0M = (gEMAC1Cam0M << 8) | mac[i] ;
	for (i = (int )(MAC_ADDR_SIZE-2); i < (int )MAC_ADDR_SIZE; i++)
		gEMAC1Cam0L = (gEMAC1Cam0L << 8) | mac[i] ;
	gEMAC1Cam0L = (gEMAC1Cam0L << 16) ;
    FillCam1Entry(entry, gEMAC1Cam0M, gEMAC1Cam0L);
    EnableCam1Entry(entry);
}
void MAC1_ResetVirtualMacAddr(int entry)
{
    FillCamEntry(entry, 0, 0);
    DisableCamEntry(entry);
}
#endif
void SetMac1Addr(ethaddr mac)
{
 	int  i;
  for (i = 0; i < (int )MAC_ADDR_SIZE-2; i++)
    gEMAC1Cam0M = (gEMAC1Cam0M << 8) | mac[i] ;
  for (i = (int )(MAC_ADDR_SIZE-2); i < (int )MAC_ADDR_SIZE; i++)
    gEMAC1Cam0L = (gEMAC1Cam0L << 8) | mac[i] ;
    gEMAC1Cam0L = (gEMAC1Cam0L << 16) ;
    FillCam1Entry(0, gEMAC1Cam0M, gEMAC1Cam0L);
}
void Tx1FDInitialize(void)
{
 	sFrameDescriptor 	*pFrameDescriptor;
 	sFrameDescriptor 	*pStartFrameDescriptor;
 	sFrameDescriptor 	*pLastFrameDescriptor = NULL;
 	UINT32 			i;
#if defined(NUC970)
	sFrameSendBuffer* pTxDataPtr;
	gWTx1DataStartPtr = (sFrameSendBuffer* )GetNcMem(sizeof(sFrameSendBuffer)*MaxTxFrameDescriptors);
	pTxDataPtr = gWTx1DataStartPtr;
	for(i = 0; i < MaxTxFrameDescriptors; i++) {
		(pTxDataPtr + i)->used = 0;
		(pTxDataPtr + i)->purpose = 0;
		(pTxDataPtr + i)->next = NULL;
	}
#endif
#if defined(NUC970)
	TXDLSA1 = (UINT32)GetNcMem(sizeof(sFrameDescriptor)*MaxTxFrameDescriptors);
#else	
	TXDLSA1 = (UINT32)Tx1FDBaseAddr | 0x80000000;
#endif	
	gWTx1FDPtr = gCTx1FDPtr = TXDLSA1;
	pFrameDescriptor = (sFrameDescriptor *) gCTx1FDPtr;
	pStartFrameDescriptor = pFrameDescriptor;
	for(i = 0; i < MaxTxFrameDescriptors; i++) {
		if (pLastFrameDescriptor == NULL)
			pLastFrameDescriptor = pFrameDescriptor;
		else
			pLastFrameDescriptor->NextFrameDescriptor = (UINT32)pFrameDescriptor;
		pFrameDescriptor->Status1 = (PaddingMode | CRCMode | MACTxIntEn);
#if defined(NUC970)
		pFrameDescriptor->FrameDataPtr = (UINT32)0;
#else
		pFrameDescriptor->FrameDataPtr = (UINT32)0x0;
#endif		
		pFrameDescriptor->Status2 = (UINT32)0x0;
		pFrameDescriptor->NextFrameDescriptor = NULL;
		pLastFrameDescriptor = pFrameDescriptor;
		pFrameDescriptor++;
	}
	pFrameDescriptor--;
	pFrameDescriptor->NextFrameDescriptor = (UINT32)pStartFrameDescriptor;
}
void Rx1FDInitialize(void)
{
 	sFrameDescriptor 	*pFrameDescriptor;
 	sFrameDescriptor 	*pStartFrameDescriptor;
 	sFrameDescriptor 	*pLastFrameDescriptor = NULL;
 	UINT32 			i;
#if defined(NUC970)
	RXDLSA1 = (UINT32)GetNcMem(sizeof(sFrameDescriptor)*MaxRxFrameDescriptors);
#else	
	RXDLSA1 = (UINT32)Rx1FDBaseAddr | 0x80000000;
#endif	
	gCRx1FDPtr = RXDLSA1;
	pFrameDescriptor = (sFrameDescriptor *) gCRx1FDPtr;
	pStartFrameDescriptor = pFrameDescriptor;
	for (i = 0; i < MaxRxFrameDescriptors; i++) {
		if (pLastFrameDescriptor == NULL)
			pLastFrameDescriptor = pFrameDescriptor;
		else
			pLastFrameDescriptor->NextFrameDescriptor = (UINT32)pFrameDescriptor;
		pFrameDescriptor->Status1 = RXfOwnership_DMA;
		pFrameDescriptor->FrameDataPtr = (UINT32)(Net1RxBuf+i);
		pFrameDescriptor->Status2 = (UINT32)0x0;
		pFrameDescriptor->NextFrameDescriptor = NULL;
		pLastFrameDescriptor = pFrameDescriptor;
		pFrameDescriptor++;
	}
	pFrameDescriptor--;
	pFrameDescriptor->NextFrameDescriptor = (UINT32)pStartFrameDescriptor;
}
void ReadyMac1(void)
{
	MIEN1 = gMIEN1 ;
	MCMDR1 = gMCMDR1 ;
}
void Mac1TxGo(void)
{
	if (!(MCMDR1&MCMDR_TXON))
		MCMDR1 |= MCMDR_TXON ;
	if (TDU1_Flag==1)
	{         
     TDU1_Flag = 0;
     TSDR1 = 0;  
	}         
}
void GetPhy1ID(void)
{
	UINT32	RdValue;
	if(PHY1Chip==CHIP_DM8603)
	{
		RdValue = Mii1StationRead(PHY_ID1_REG, GW1PHYAD[0]) ;
		Disp("PHY1_ID1_REG = %xH \n",RdValue);
		RdValue = Mii1StationRead(PHY_ID2_REG, GW1PHYAD[0]) ;
		Disp("PHY1_ID2_REG = %xH \n",RdValue);
	}
}
#if 1	
int MAC1_Initialize(NET_ADAPTER_INFO* pEthInfo)
{
	Disp("\n**Mac1_Initialize**\n");
	#if defined(S_SERIES) || defined(PT2100A) || defined(PK2070A) || defined(WOP107E)	
		PHY1Chip=(RAM_HwInfo.ethernetType>>8) & 0xff;
	#else
		#if defined(DM8603)
			PHY1Chip=CHIP_DM8603;
		#endif
	#endif
	if(PHY1Chip== CHIP_IP101A || PHY1Chip== CHIP_DM9000A)
	{
		PHY1Chip=CHIP_KSZ8081;
	}
	if(PHY1Chip == CHIP_KSZ8081)
		Disp("PHY1Chip=CHIP_KSZ8081 \n");	
	else if(PHY1Chip == CHIP_DM8603) 
		Disp("PHY1Chip=CHIP_DM8603 \n");	
	else if(PHY1Chip == CHIP_YT8512C) 
		Disp("PHY1Chip=CHIP_YT8512C \n");	
	else if(PHY1Chip == CHIP_IP101A_INT) 
		Disp("PHY1Chip=CHIP_IP101A_INT \n");	
	else  
		Disp("Unknow PHY0Chip \n");
	outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) | (1 << 17));            
    outpw(REG_CLK_DIVCTL8, (inpw(REG_CLK_DIVCTL8) & ~0xFF) | 0xA0);     
    outpw(REG_SYS_GPE_MFPL, (inpw(REG_SYS_GPE_MFPL) & ~0xFFFFFF00) | 0x11111100);
    outpw(REG_SYS_GPE_MFPH, (inpw(REG_SYS_GPE_MFPH) & ~0x0000FFFF) | 0x00001111);
	m_pEthInfo1 = pEthInfo;
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
	m_lastTime1 =0;
	mac1_flowcontrl= 0;
	mac1_flowHighThread = (MaxRxFrameDescriptors*2)/3;
	mac1_flowLowThread = (MaxRxFrameDescriptors*3)/100;
#endif
	m_lastTime1 =0;
	MCMDR1|=MCMDR_SWR;  
#if defined(NUC970)
	Net1RxBuf = (NETBUF*)GetNcMem(sizeof(NETBUF)*MaxRxFrameDescriptors);
#else	
	Net1RxBuf = (NETBUF*)((UINT32)rx1buf | 0x80000000);
#endif	
	_iqueue1_first = _iqueue1_last = NULL;
	Tx1FDInitialize() ;
	Rx1FDInitialize() ;
 	SetMac1Addr(pEthInfo->myMacAddr) ;
	m_nJustLinkedin10s1 =10;
	FillCam1Entry(0, gEMAC1Cam0M, gEMAC1Cam0L);
	CAMCMR1 = gCAMCMR1 ;
	Mac1_EnableBroadcast();	
	TDU1_Flag=0;
	ReadyMac1() ;
 	AutoDetectPhy1Addr() ; 
 	ResetPhy1Chip() ;
 	SetMac1Addr(pEthInfo->myMacAddr) ;
	m_nP9701_Mutex = 0;
	m_pEthInfo1->portSilentTime = 0; 
	return 1;
}
#else	
int Mac1_Initialize(ethaddr MacAddr)
{
}
#endif
void  Mac1_ShutDown()
{
	MCMDR1 &= ~(MCMDR_RXON|MCMDR_TXON) ;
}       
int Mac1jj1=0;
int Mac1jj2 = 11;
int Mac1jj3= 21;
int Mac1jj4 = 1;
int Mac1jj5 = 0;
int Mac1cc1= 0;
int Mac1cc2= 0;
int Mac1cc3 = 0;
#if defined(TCP_SPEEDUP) && defined(NUC970)
char* MAC1_GetSendFramePt(int ori)
{
	char* TxData = NULL;
	sFrameSendBuffer* pTxDataPtr = gWTx1DataStartPtr;
	int i;
#if defined(TEST_BO)
DisplayWord(60,29,(int)Mac1jj5);
DisplayWord(30,27,Mac1jj1++);
#endif
	Mac1_DisableInt();
	for(i = 0; i < MaxTxFrameDescriptors; i++) {
		if ( (pTxDataPtr+i)->used == 0) 
		{
			(pTxDataPtr+i)->used = 1;
			(pTxDataPtr+i)->purpose = ori;
			TxData = (char*)(&(pTxDataPtr+i)->data);
Mac1jj5++;
w1frame++;
w1frame1++;
			break;
		}
	}
	Mac1_EnableInt();
#if defined(TEST_BO)
DisplayWord(35,27,Mac1jj1++);
#endif
	if (!TxData) 
	{
			for(i = 0; i < MaxTxFrameDescriptors; i++) {
				if ( (pTxDataPtr+i)->used != 0) 
				{
					(pTxDataPtr+i)->used = 0;
					(pTxDataPtr+i)->purpose = 0;
				}
			}
		(pTxDataPtr+0)->used = 1;
		(pTxDataPtr+0)->purpose = ori;
		TxData = (char*)(&(pTxDataPtr+0)->data);
	}
	return TxData;
}
int Mac1_FreeSendFramePt(char* pData, int ori)
{
	int ret = 0;
	sFrameSendBuffer* pTxDataPtr = gWTx1DataStartPtr;
	int i;
#if defined(TEST_BO)
DisplayWord(40,27,Mac1jj1++);
#endif
	if (ori !=5) 
	{
	Mac1_DisableInt();
	}
Mac1jj5--;		
w1frame1 = w1frame1 -1;
	for(i = 0; i < MaxTxFrameDescriptors; i++) {
		if ((char*)(&(pTxDataPtr+i)->data) == pData) 
		{
			(pTxDataPtr+i)->used=0;
			(pTxDataPtr+i)->purpose = 0;
			ret = 1;
w1frame = w1frame -1;	
			break;
		}
	}
	if (ori !=5) 
	{
	Mac1_EnableInt();
	}
#if defined(TEST_BO)
DisplayWord(45,27,Mac1jj1++);
#endif
	return ret;
}
int Mac1_SendPacketSpeedup(BYTE*  pbData, DWORD dwLength)
{
	sFrameDescriptor *psTxFD;
	psTxFD = (sFrameDescriptor *)gWTx1FDPtr;
	psTxFD->FrameDataPtr=(UINT32)pbData;
	psTxFD->Status2 = (UINT32)(dwLength & 0xffff);
	psTxFD->Status1 |= TXfOwnership_DMA;
	Tx1Ready = 0;
	Mac1TxGo();
	gWTx1FDPtr = (UINT32)(psTxFD->NextFrameDescriptor);
}
#endif
int MAC1_SendPacket(BYTE*  pbData, DWORD dwLength)
{
	sFrameDescriptor *psTxFD;
	UINT32         *pTXFDStatus1;
	psTxFD = (sFrameDescriptor *)gWTx1FDPtr ;
	pTXFDStatus1 = (UINT32 *)&psTxFD->Status1;
#if defined(NUC970)
	psTxFD->FrameDataPtr = (UINT32)MAC1_GetSendFramePt(5);
	m_memcpy(psTxFD->FrameDataPtr,(char*)pbData,(UINT32)(dwLength & 0xffff));
#else	
	psTxFD->FrameDataPtr=(UINT32)pbData | 0x80000000;
#endif	
	psTxFD->Status2 = (UINT32)(dwLength & 0xffff);
	psTxFD->Status1 |= TXfOwnership_DMA;
	Tx1Ready = 0;
	Mac1TxGo();
	gWTx1FDPtr = (UINT32)(psTxFD->NextFrameDescriptor);
	return 1 ;
}
int Mac1_SendPacket_InINT(BYTE*  pbData, DWORD dwLength)
{
	return 1 ;
}
int Mac1jjj = 0;
void MAC1_Tx_isr(void)
{   
	sFrameDescriptor 	*pTxFDptr;
	UINT32 			Status, RdValue;
	int in = 0;
#if defined(TEST_BO)
 _DisplayDWord(30,28,Mac1jj1++);
#endif
	RdValue = MISTA1;
	MISTA1 = RdValue&0xffff0000;
	if (RdValue & MISTA_TDU)
		TDU1_Flag = 1;
	if (RdValue & MISTA_TxBErr) {
		MCMDR1|=MCMDR_SWR;  
	} else {
		do
		{
			pTxFDptr = (sFrameDescriptor *) gCTx1FDPtr;
			Status = (pTxFDptr->Status2 >> 16) & 0xffff;
			if (Status & TXFD_TXCP) {
				Tx1Ready = 1;
				if (Status & ( TXFD_TXABT | TXFD_DEF | TXFD_PAU | TXFD_EXDEF |
						TXFD_NCS | TXFD_SQE | TXFD_LC | TXFD_TXHA) ) {
					;
				}
				pTxFDptr->Status2 = (UINT32)0x0;
				Mac1_FreeSendFramePt((char*)pTxFDptr->FrameDataPtr,5);
	       		gCTx1FDPtr = (UINT32)pTxFDptr->NextFrameDescriptor ;
			}
			else 
			{
				break;
			}
			in++;
			if (in>MaxTxFrameDescriptors)
			{
				break;
			}
		} while(gCTx1FDPtr != CTXDSA1);
	}
	MISTA1 = 0xFFFF;
#if defined(TEST_BO)
 _DisplayDWord(40,28,Mac1jj1++);
#endif
}
bool Mac1isHWConnectionOk_Simple(void)
{
	return m_pEthInfo1->portStatus;
}
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
int Mac1_GetRxFlowControl(int rxFrameCount)
{
	return mac1_flowcontrl;
}
int Mac1_SetRxFlowControl(int rxFrameCount)
{
	int flowCtrl = mac1_flowcontrl;
	if (rxFrameCount>=mac1_flowHighThread) 
	{
		flowCtrl = 1;
	}
	else if (rxFrameCount<=mac1_flowLowThread) 
	{
		flowCtrl = 0;
	}
	else 
	{
		flowCtrl = mac1_flowcontrl;
	}
	return flowCtrl;
}
#endif
int Mac1per=0;
int MAC1_ReceivePacket( BYTE *pbData, UINT16 *pwLength,WORD* iPosition)
{
	NETBUF      *buffer;
	if (_iqueue1_first == NULL) {
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
		mac1_rxFrameCount = 0;	
		mac1_flowcontrl = Mac1_SetRxFlowControl(mac1_rxFrameCount);
#endif
		return false;
	} else {
		Mac1_DisableInt();
		buffer = (NETBUF*)_iqueue1_first;
		_iqueue1_first = _iqueue1_first->next;
		if (_iqueue1_first == NULL)  
			_iqueue1_last = NULL;
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
		mac1_rxFrameCount--;
		mac1_flowcontrl = Mac1_SetRxFlowControl(mac1_rxFrameCount);
#endif	
		Mac1_EnableInt();
		*pwLength = buffer->len;
		PutInTskQueue(buffer->packet, *pwLength,iPosition, m_pEthInfo1->portSeq); 
			if (*iPosition == PACKET_WILL_DISCARD || *iPosition == PACKET_WILL_FORWARD) 
			{
				return false;
			}
			else if (*iPosition == PACKET_UNDECIDED_YET) {
	#if defined(NUC970)
				m_memcpy(pbData, buffer->packet, *pwLength);
	#else
				memcpy(pbData, buffer->packet, *pwLength);
	#endif
				return true;
			}
			else 
			{
				return true;
			}
		return true;	   
	}   
}
void MAC1_Rx_isr(void)
{
	sFrameDescriptor 	*pRxFDptr ;
	UINT32 				RxStatus ;
	UINT32 				CRxPtr;
	UINT32 				RdValue;
	int ret;
int i=0;
	RdValue = MISTA1;
	MISTA1 = RdValue&0x0000ffff;
#if defined(TEST_BO)
_DisplayDWord(30,26,Mac1jj1++);
#endif
 	if (RdValue & MISTA_RxBErr)  {
		MCMDR1|=MCMDR_SWR;  
	} else {
		if (RdValue & (MISTA_CFR | MISTA_CRCE | MISTA_PTLE | MISTA_ALIE | MISTA_RP)) {
		} else {
			CRxPtr = CRXDSA1 ;
			do {
				pRxFDptr = (sFrameDescriptor *)gCRx1FDPtr;
				if ((pRxFDptr->Status1|RXfOwnership_CPU)==RXfOwnership_CPU) {
					RxStatus = (pRxFDptr->Status1 >> 16) & 0xffff;
					if (RxStatus & RXFD_RXGD)  {
						ret = 0;
						if (ret==0) {
							if (_iqueue1_last == NULL) {
								_iqueue1_last = (NETBUF *)pRxFDptr->FrameDataPtr;
								_iqueue1_first = _iqueue1_last;
							} else {
								_iqueue1_last->next = (NETBUF *)pRxFDptr->FrameDataPtr;
								_iqueue1_last = _iqueue1_last->next;
							}
							_iqueue1_last->len = pRxFDptr->Status1 & 0xffff;
							_iqueue1_last->next = NULL;             
						}
					} else {
					}
       			} else {
					break;
				}
				pRxFDptr->Status1 = RXfOwnership_DMA;
				gCRx1FDPtr = (UINT32)(pRxFDptr->NextFrameDescriptor) ;
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
				mac1_rxFrameCount++;
#endif
			} while (CRxPtr != gCRx1FDPtr);
		}
		if (RdValue & MISTA_RDU)
			RSDR1 = 0;
	}
	if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_OK) == ETH_PORT_STATUS_OK)
	{
		m_pEthInfo1->portSilentTime = 0; 
	}
#if defined(TEST_BO)
_DisplayDWord(40,26,Mac1jj1++);
#endif
}
void Check_PHY1_INT_CNTL_REG(void)
{
	UINT32	RdValue;
#if defined(VIRTUAL_NIC_SUPPORT)
	NET_ADAPTER_INFO *pVirAdapter = NULL;
	if (m_pEthInfo1->virtualPortSeq >= 1 && m_pEthInfo1->virtualPortSeq <= ETH_GetEthPortsOnHmi()) 
	{
		pVirAdapter = (NET_ADAPTER_INFO *)ETH_GetAdapterFrmPort(m_pEthInfo1->virtualPortSeq);
	}
	else 
	{
		pVirAdapter = NULL;
	}
#endif
	if(PHY1Chip == CHIP_KSZ8081)
	{
		RdValue = Mii1StationRead(PHY_INT_CNTL_REG, PHY1AD) ;
		if (RdValue&1)
		{
			m_pEthInfo1->bUseInt = 1;		
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) != ETH_PORT_STATUS_CABLE_LINKED) 
			{
		 		m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED );
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	
				m_pEthInfo1->portSilentTime =0;
	#if defined(VIRTUAL_NIC_SUPPORT)
				if (pVirAdapter) 
				{
					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
				}
	#endif
			}
		}
		else if (RdValue&4) 
		{
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) == ETH_PORT_STATUS_CABLE_LINKED) 
			{
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	
				m_pEthInfo1->portSilentTime =0;
	#if defined(VIRTUAL_NIC_SUPPORT)
				if (pVirAdapter) 
				{
					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
				}
	#endif
			}
		}
		else if (RdValue&8) 
		{
		}
		else if (RdValue&0x80) 
		{
		}
   	}
   	else if(PHY1Chip == CHIP_YT8512C)
   	{
   		RdValue = Mii1StationRead(YT8512_PHY_INT_STATUS_REG, PHY1AD);
		if (RdValue & LINK_SUCCEED_INT)
		{
			m_pEthInfo1->bUseInt = 1;		
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) != ETH_PORT_STATUS_CABLE_LINKED) 
			{
		 		m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED );
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	
				m_pEthInfo1->portSilentTime =0;
	#if defined(VIRTUAL_NIC_SUPPORT)
				if (pVirAdapter) 
				{
					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
				}
	#endif
			}
		}
		else if (RdValue & LINK_FAILED_INT) 
		{
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) == ETH_PORT_STATUS_CABLE_LINKED) 
			{
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	
				m_pEthInfo1->portSilentTime =0;
	#if defined(VIRTUAL_NIC_SUPPORT)
				if (pVirAdapter) 
				{
					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
				}
	#endif
			}
		}
   	}
	else if(PHY1Chip == CHIP_IP101A_INT)
	{
		RdValue = Mii1StationRead(IP101_PHY_INT_CNTL_REG, PHYAD);	
	   	RdValue = Mii1StationRead(IP101_PHY_INT_STATUS_REG, PHYAD);
	   	Disp("IP101 INT RdValue =%xH \n",RdValue);
	   	if((RdValue & LINK_SUCCEED_INT) == LINK_SUCCEED_INT)
		{
			m_pEthInfo1->bUseInt = 1;		
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) != ETH_PORT_STATUS_CABLE_LINKED) 
			{
		 		m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED );
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	
				m_pEthInfo1->portSilentTime =0;
	#if defined(VIRTUAL_NIC_SUPPORT)
				if (pVirAdapter) 
				{
					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
				}
	#endif
			}
		}
		else if( (RdValue & LINK_SUCCEED_INT) != LINK_SUCCEED_INT)	
		{
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) == ETH_PORT_STATUS_CABLE_LINKED) 
			{
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	
				m_pEthInfo1->portSilentTime =0;
	#if defined(VIRTUAL_NIC_SUPPORT)
				if (pVirAdapter) 
				{
					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
				}
	#endif
			}
		}   	
	}
}
void GPIO_IRQHandler2(void)
{
    UINT32 reg;
    INT32 mask;
    reg = inpw(REG_AIC_ISR);
    mask = 0x40;
    Check_PHY1_INT_CNTL_REG();
    outpw(REG_GPIOF_ISR, (1<<13));	
    outpw(REG_AIC_SCCR, mask);   
}
#endif
