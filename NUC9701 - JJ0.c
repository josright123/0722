#if defined(DUAL_PORT) && defined(NUC970)
//#if 1
/******************************************************************************
 *
 * Copyright (c)  2008 Nuvoton Tech. Corp.
 * All rights reserved.
 *
 * Module Name:	PACKETDRIVER.C
 *
 * Created by : 
 ******************************************************************************/
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
//	#include "..\fwvisa\CommonSrc\typedef.h"
//	#include "..\fwvisa\commonsrc\SWIcall.h"
//	#include "..\FwVisa\CommonSrc\OSAPI.h"
//	#include "..\FwVisa\CommonSrc\tcpcommon.h"
//	#include "..\fwvisa\pvos\tcp_wo_move\putInfoSocket.h"
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

//extern int m_LoopSign;

NETBUF  *_iqueue1_first, *_iqueue1_last;   /* incoming queue */


//static BOOL  Global_Tx1InProcess = FALSE;
int m_lastTime1 = 0;
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
int mac1_flowcontrl= 1;
int mac1_flowHighThread = 0;
int mac1_flowLowThread = 0;
int mac1_rxFrameCount = 0;
#endif

//#define MAX_AUTOCONFIGTIME 100000

//New way to get NC Memory
NETBUF* Net1RxBuf;
#if defined(NUC970)
#else
NETBUF rx1buf[MaxRxFrameDescriptors];

__align(16) static sFrameDescriptor Rx1FDBaseAddr[MaxRxFrameDescriptors];		//32
__align(16) static sFrameDescriptor Tx1FDBaseAddr[MaxTxFrameDescriptors];		//16
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

volatile unsigned int PHY1AD; //cmn, [2007/03/08]
//volatile bool link1Ok = false;
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
//#if defined(DM8603)
	volatile unsigned int GW1PHYAD[2]; //cmn,use in GW series
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
extern DWORD SystemTick;    		//SystemTick1ms

//int m_nSilent1Time; 

//void udelay (int delay)
//{
//	int i,j,u;
//	//for(j=0;j<i*2;j++)
//	//u=0;	
//	
//	 for (j = 0; j < delay; j++)
//    	//for (i=0; i < 1000; i++);
//    	for (i=0; i < 20; i++);
//	             u=0;
//}
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
//_DisplayDWord(55,0,GetTickCount());
				udelay(2000);
				_OS_ENTER_CRITICAL();
				
			}
		} else {
			if  (*mutexAddr == 0) {
				*mutexAddr = mutexType;
				break;
			} else {
//_DisplayDWord(50,0,GetTickCount());
//DisplayWord(50,0,GetTickCount());
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
	//Disp("**Mac1_EnableInt**\n");
    EnableInt(EMC1_TX_IRQn);
    EnableInt(EMC1_RX_IRQn);
    ETH1_TRIGGER_RX();
}
void Mac1_DisableInt()
{
	//Disp("**Mac1_DisableInt**\n");
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

// MII Interface Station Management Register Write
void Mii1StationWrite(UINT32 PhyInAddr, UINT32 PhyAddr, UINT32 PhyWrData)
{
	int volatile i = 1000;

	#if 1
		MIID1 = PhyWrData ;
		MIIDA1 = PhyInAddr | PhyAddr | PHYBUSY | PHYWR | MDCCR;
	    	
		while (i--) ;
		while ( (MIIDA1 & PHYBUSY) )  ;
	#else
		MIID1 = PhyWrData ;
		MIIDA1 = PhyInAddr | PhyAddr | 0xB0000;
	    	
		//while (i--) ;
		while ( (MIIDA1 & PHYBUSY) )  ;
	#endif
	
	
}

// MII Interface Station Management Register Read
UINT32 Mii1StationRead(UINT32 PhyInAddr, UINT32 PhyAddr)
{
 	UINT32 volatile PhyRdData ;

	#if 1
		MIIDA1 = PhyInAddr | PhyAddr | PHYBUSY | MDCCR;
		while( (MIIDA1 & PHYBUSY) )  ;
		PhyRdData = MIID1 ;  
	#else
		MIIDA1 = PhyInAddr | PhyAddr | 0xA0000;
		while( (MIIDA1 & PHYBUSY) )  ;
		PhyRdData = MIID1 ;  
	#endif

 	return PhyRdData ;
}

//////=====================================================
//#if	defined(DM8603) && defined(DUAL_PORT)
//void AutoDetectPhy1Addr()
//{
//    int i,j=0;
//    int flag;
//    unsigned int temp;
//    
//    flag = 0;
//   	GW1PHYAD[0] = GW1PHYAD[1] = 0;
//	for (i=0; i<8; i++)
//	{
//	
//		temp = Mii1StationRead(0, (i << 8));
//		if ((temp & 0xFFFF) == 0x3100)
//		{
//			//Disp("PhyAddr = %d j=%d \n",i,j);
//			GW1PHYAD[j++] = i << 8;			
//			flag = 1;			
//			//Disp("i= %d,PHY ADDR %xH = %xH\n", i,GW1PHYAD[j-1],temp);
//			//break;
//		}
//		//UART_printf("PHY ADDR %d = %x\n", i, Mii1StationRead(num, 0, (i << 8)));	
//
//	}
//	if (flag == 0) {
//		Disp("AutoDetectPhy1Addr() : Don't find PHY1 Addr!\n");
//	}
//	
//} /* end AutoDetectPhy1Addr */
//#else
//void AutoDetectPhy1Addr()
//{
//    int i;
//    int flag;
//    unsigned int temp;
//    
//    flag = 0;
//    PHY1AD = 0;
//	for (i=0; i<32; i++)
//	{
//	
//		temp = Mii1StationRead(0, (i << 8));
//		Disp("i1=%d, temp1=%xH \n",i,temp);
//		
//		if ((temp & 0xFFFF) == 0x3100)
//		{
//			
//			PHY1AD = i << 8;			
//			flag = 1;			
//			//break;
//		}
//		//UART_printf("PHY ADDR %d = %x\n", i, Mii1StationRead(num, 0, (i << 8)));	
//
//	}
//	Disp("PHY ADDR %d = %x\n", i, Mii1StationRead( 0, (i << 8)));	
//
//#ifdef _DEBUG
//	if (flag == 1) {
//		DisplayString(0,7,"AutoDetectPhy1Addr() : PHY ADDR = ");
//		DisplayDWord(40,7, i);
//	} else {
//		DisplayString(0,7,"AutoDetectPhy1Addr() : Don't find PHY Addr!");
//	}
//#endif
//} /* end AutoDetectPhy1Addr */
//#endif
////=====================================================

#if 1	//for S Series
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
				//Disp("PhyAddr = %d j=%d \n",i,j);
				GW1PHYAD[j++] = i << 8;			
				flag = 1;			
				//break;
			}
			//Disp("PHY1 ADDR %d = %x\n", i, Mii1StationRead(num, 0, (i << 8)));	
	
		}
		if (flag == 0) {
			Disp("AutoDetectPhy1Addr() : Don't find PHY1 Addr!\n");
		}
		else
		{
			Disp("GW1PHYAD[0]=%03xH,GW1PHYAD[1]=%03xH \n",GW1PHYAD[0],GW1PHYAD[1]);
		}
		
	}	//PHY1Chip==CHIP_KSZ8081
	else if(PHY1Chip==CHIP_KSZ8081 || PHY1Chip==CHIP_IP101A_INT)	
	{
	    flag = 0;
	    PHY1AD = 0;
		for (i=0; i<32; i++)
		{
		
			temp = Mii1StationRead(0, (i << 8));
			Disp("i1=%d, temp1=%xH \n",i,temp);
			
			if ((temp & 0xFFFF) == 0x3100)
			{
				
				PHY1AD = i << 8;			
				flag = 1;			
				break;
			}
			//UART_printf("PHY ADDR %d = %x\n", i, Mii1StationRead(num, 0, (i << 8)));	
	
		}
		Disp("PHY1 ADDR %d = %x\n", i, Mii1StationRead( 0, (i << 8)));	

		#ifdef _DEBUG
			if (flag == 1) {
				DisplayString(0,7,"AutoDetectPhy1Addr() : PHY ADDR = ");
				DisplayDWord(40,7, i);
			} else {
				DisplayString(0,7,"AutoDetectPhy1Addr() : Don't find PHY Addr!");
			}
		#endif

	}	//PHY1Chip==CHIP_DM8603
    else if(PHY1Chip==CHIP_YT8512C)
    {
		Disp("PHY1Chip==CHIP_YT8512C \n");
//
//		Disp("Reg0 = %xH \n",MiiStationRead(0, 3<<8));
//		Disp("Reg1 = %xH \n",MiiStationRead(1, 3<<8));
//		Disp("Reg2 = %xH \n",MiiStationRead(2, 3<<8));
//		Disp("Reg3 = %xH \n",MiiStationRead(3, 3<<8));	//ID 0128H
//		Disp("Reg4 = %xH \n",MiiStationRead(4, 3<<8));
//		Disp("Reg5 = %xH \n",MiiStationRead(5, 0<<8));
//		Disp("Reg6 = %xH \n",MiiStationRead(6, 0<<8));
//		Disp("Reg7 = %xH \n",MiiStationRead(7, 0<<8));
//		Disp("Reg8 = %xH \n",MiiStationRead(8, 0<<8));
//		Disp("Reg9 = %xH \n",MiiStationRead(9, 0<<8));
//		Disp("Reg10h = %xH \n",MiiStationRead(0x10, 0<<8));
//		Disp("Reg11h = %xH \n",MiiStationRead(0x11, 0<<8));
//		Disp("Reg15h = %xH \n",MiiStationRead(0x15, 0<<8));
//		Disp("Reg1Fh = %xH \n",MiiStationRead(0x1f, 0<<8));
		
		for (i=0; i<32; i++)
		{
			temp = Mii1StationRead(0, (i << 8));
			Disp("i=%d, temp=%xH \n",i,temp);
			if ((temp & 0xFFFF) == 0x1140 && (i!=0) )
			{
				PHY1AD = i << 8;			
				flag = 1;			
				break;
			}
			//UART_printf("PHY ADDR %d = %x\n", i, MiiStationRead(num, 0, (i << 8)));	
	
		}
    	
    }
    else Disp("!! PHY1 IC unknow !!\n");
	
}

//================================================
// Reset PHY, Auto-Negotiation Enable
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

	if(PHY1Chip == CHIP_KSZ8081 || PHY1Chip == CHIP_YT8512C || PHY1Chip==CHIP_IP101A_INT)
	{
		//Disp("** KSZ8081 MAC1_SetLinkOk **\n");
		if (ms == 0) {
	    	RdValue = Mii1StationRead(PHY_STATUS_REG, PHY1AD) ;
			if ((RdValue & AN_COMPLETE) != 0)
			{
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
					//DisplayString(0,22,"AN_COMPLETE OK!!");
					if (m_pEthInfo1->bUseInt == 0) 
					{
						m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED);
					}
					m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
	#if defined(VIRTUAL_NIC_SUPPORT)
					if (pVirAdapter) 
					{
						pVirAdapter->portStatus = m_pEthInfo1->portStatus;
					}
	#endif
					Mac1_EnableInt();
				}
			}
			else 
			{
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
	
					//return;
					Mac1_DisableInt();
					m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
					//if (m_pEthInfo1->bUseInt == 0) 
					//{
					//	m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
					//}
					m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
	#if defined(VIRTUAL_NIC_SUPPORT)
					if (pVirAdapter) 
					{
						pVirAdapter->portStatus = m_pEthInfo1->portStatus;
					}
	#endif
				}
			}
		}
		else 
		{
			t0 = GetTickCount1ms();
			while (1) 	 //wait for auto-negotiation complete 
		    {
		    	RdValue = Mii1StationRead(PHY_STATUS_REG, PHY1AD) ;
		    	//Disp("1 RdValue %xH\n",RdValue);
				//if ((RdValue & AN_LINKED) != 0)
				if ((RdValue & AN_COMPLETE) != 0)
				{
					//DisplayString(0,22,"AN_COMPLETE OK!!");
					//Disp("**AN_COMPLETE**\n");
					if (m_pEthInfo1->bUseInt == 0) 
					{
						m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED);
					}
					m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
	#if defined(VIRTUAL_NIC_SUPPORT)
					if (pVirAdapter) 
					{
						pVirAdapter->portStatus = m_pEthInfo1->portStatus;
					}
	#endif
					Mac1_EnableInt();
					break;
				}
				if ((GetTickCount1ms()-t0) >= ms )
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
	
					Mac1_DisableInt();
					m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
					//if (m_pEthInfo1->bUseInt == 0) 
					//{
					//	m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
					//}
					m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
	#if defined(VIRTUAL_NIC_SUPPORT)
					if (pVirAdapter) 
					{
						pVirAdapter->portStatus = m_pEthInfo1->portStatus;
					}
	#endif
					break;
				}
				//OSTimeDly(1);
		    }
		    //Disp("Total %d\n",GetTickCount1ms()-t0);
		}
	
		if (m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) {
			//Disp("link1Ok ms=%d\n",ms);
		    regANA   = Mii1StationRead(PHY_ANA_REG, PHY1AD);
		    regANLPA = Mii1StationRead(PHY_ANLPA_REG, PHY1AD);
		
		//DisplayDWord(0,23,regANA);
		//DisplayDWord(10,23,regANLPA);
		
		
		
		    if ((regANA & 0x100) && (regANLPA & 0x100)) //100Mb, Full duplex
		    { 
				//DisplayString(0,23,"100M, Full Duplex!!");
		    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
		    }
		    else if ((regANA & 0x80) && (regANLPA & 0x80)) //100Mb, Half duplex
		    {
				//DisplayString(0,23,"100M, Half Duplex!!");
		    	MCMDR1 |= MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;
		    } 
		    else if ((regANA & 0x40) && (regANLPA & 0x40)) //10Mb, Full duplex
		    {  	
				//DisplayString(0,23,"10M, Full Duplex!!");
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 |= MCMDR_FDUP;    
		    }
		    else //10Mb, half duplex
		    {   
				//DisplayString(0,23,"10M, Half Duplex, default !!");
		//    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;        
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;        
		    }
			//m_nJustLinkedin10s1 = 10;
			//m_nSilent1Time = SystemTick5ms;
		
		} 
		
	}
	else if(PHY1Chip == CHIP_DM8603)
	{
		Disp("** DM8603 MAC1_SetLinkOk **\n");
		if (ms == 0) {
			//Disp("MAC_SetLinkOk:time = %d \n", GetTickCount1ms());				
	    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[0]) ;	//reg 0x01  
	    	Disp("GW11 PHY_STATUS_REG = %xH\n",RdValue);    
			if ((RdValue & AN_COMPLETE) != 0)	//bit 5
			{
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
					GW1link1Ok = true;
					//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
					//Mac1_EnableInt();
				}
			} else {
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
					GW1link1Ok = false;
					//Mac1_DisableInt();
					//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
					//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
				}
			}
		    
			//PHY ADR2
	    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[1]) ;	//reg 0x01    
	    	Disp("GW12 PHY_STATUS_REG = %xH\n",RdValue);  
			if ((RdValue & AN_COMPLETE) != 0)	//bit 5
			{
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
					GW1link2Ok = true;
					//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
					//Mac1_EnableInt();
				}
			} else {
				if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
					GW1link2Ok = false;
					//Mac1_DisableInt();
					//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
					//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
				}
			}
		} else {
			t0 = GetTickCount1ms();
			while (1) 	 //wait for auto-negotiation complete 
		    {
		    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[0]) ;	//reg 0x01      
		      
				if ((RdValue & AN_COMPLETE) != 0)
				{
					//Disp("RdValue1 =%xH \n",RdValue);
					GW1link1Ok = true;
					//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
					//Disp("GW link1 Ok\n");	
					//m_pEthInfo1->portStatus = true;
					break;
				}
				//OSTimeDly1ms(1);
				if ((GetTickCount1ms()-t0) >= ms )			
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
		  			GW1link1Ok = false;
					//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
					//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
					break;
				}
				OSTimeDly1ms(10);
		    }
	
			t0 = GetTickCount1ms();
			while (1) 	 //wait for PHY ADR2 auto-negotiation complete 
		    {
				//PHY ADR2
		    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[1]) ;	//reg 0x01      
		    	
		//		if ((RdValue & AN_LINKED) != 0)	//bit2
				if ((RdValue & AN_COMPLETE) != 0)
				{
					//Disp("RdValue2 = %xH\n",RdValue);
					GW1link2Ok = true;
				//	Disp("GW link2 Ok\n");	
					//Disp("GW1link2Ok\n");
	//				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED);
					break;
				}
				//OSTimeDly1ms(1);
				if ((GetTickCount1ms()-t0) >= ms )			
				{
		  			MCMDR1 |= MCMDR_OPMOD;
		  			MCMDR1 |= MCMDR_FDUP;				
					GW1link2Ok = false;
	//				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
					break;
				}
				OSTimeDly1ms(10);
		    }
		    
		}
		//Disp(" 2 portStatus = %x , internalStatus = %x, \n", m_pEthInfo1->portStatus,m_pEthInfo1->internalStatus);	
	
	
		if (GW1link1Ok) {
	//	if (m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) {
		    regANA   = Mii1StationRead(PHY_ANA_REG, GW1PHYAD[0]);		//reg 0x04
		    regANLPA = Mii1StationRead(PHY_ANLPA_REG, GW1PHYAD[0]);	//reg 0x05
		
		    if ((regANA & 0x100) && (regANLPA & 0x100)) //100Mb, Full duplex,Bit8
		    { 
				//DisplayString(0,23,"100M, Full Duplex!!");
				//Disp("GW1 100M, Full Duplex!!\n");
		    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
		    }
		    else if ((regANA & 0x80) && (regANLPA & 0x80)) //100Mb, Half duplex
		    {
				//DisplayString(0,23,"100M, Half Duplex!!");
				//Disp("GW1 100M, Half Duplex!!\n");
		    	MCMDR1 |= MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;
		    } 
		    else if ((regANA & 0x40) && (regANLPA & 0x40)) //10Mb, Full duplex
		    {  	
				//DisplayString(0,23,"10M, Full Duplex!!");
				//Disp("GW1 10M, Full Duplex!!\n");
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 |= MCMDR_FDUP;    
		    }
		    else //10Mb, half duplex
		    {   
				//DisplayString(0,23,"10M, Half Duplex, default !!");
				//Disp("GW1 10M, Half Duplex!!\n");
		//    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;        
		    }
			//m_nJustLinkedin10s = 10;
		
		} 
		
		if (GW1link2Ok) {
		    regANA   = Mii1StationRead(PHY_ANA_REG, GW1PHYAD[1]);		//reg 0x04
		    regANLPA = Mii1StationRead(PHY_ANLPA_REG, GW1PHYAD[1]);	//reg 0x05
		
		    if ((regANA & 0x100) && (regANLPA & 0x100)) //100Mb, Full duplex,Bit8
		    { 
				//DisplayString(0,23,"100M, Full Duplex!!");
				//Disp("GW2 100M, Full Duplex!!\n");
		    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
		    }
		    else if ((regANA & 0x80) && (regANLPA & 0x80)) //100Mb, Half duplex
		    {
				//DisplayString(0,23,"100M, Half Duplex!!");
				//Disp("GW2 100M, Half Duplex!!\n");
		    	MCMDR1 |= MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;
		    } 
		    else if ((regANA & 0x40) && (regANLPA & 0x40)) //10Mb, Full duplex
		    {  	
				//DisplayString(0,23,"10M, Full Duplex!!");
				//Disp("GW2 10M, Full Duplex!!\n");
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 |= MCMDR_FDUP;    
		    }
		    else //10Mb, half duplex
		    {   
				//DisplayString(0,23,"10M, Half Duplex, default !!");
		//    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
				//Disp("GW2 10M, Half Duplex!!\n");
				MCMDR1 &= ~MCMDR_OPMOD;
				MCMDR1 &= ~MCMDR_FDUP;        
		    }
			//m_nJustLinkedin10s = 10;
		
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
				//Disp("**GW1link1Ok** \n");
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
				//if (m_pEthInfo1->bUseInt == 0) 
				//{
				//	m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
				//}
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
	//Disp(" 2 portStatus = %x , internalStatus = %x, GW1link1Ok=%d, GW1link2Ok=%d\n", m_pEthInfo1->portStatus,m_pEthInfo1->internalStatus, GW1link1Ok, GW1link2Ok);	
		}
	}
	
	return;	
}
#endif


//#if	defined(DM8603) && defined(DUAL_PORT)
//void MAC1_SetLinkOk(int ms) {
//   	
//   	int RdValue = 0;
//   	int t0;
//	UINT32 volatile regANA, regANLPA;
//#if defined(VIRTUAL_NIC_SUPPORT)
//	NET_ADAPTER_INFO *pVirAdapter = NULL;
//	
//	if (m_pEthInfo1->virtualPortSeq >= 1 && m_pEthInfo1->virtualPortSeq <= ETH_GetEthPortsOnHmi()) 
//	{
//		pVirAdapter = (NET_ADAPTER_INFO *)ETH_GetAdapterFrmPort(m_pEthInfo1->virtualPortSeq);
//	}
//	else 
//	{
//		pVirAdapter = NULL;
//	}
//#endif
//
//	//Disp("** MAC1_SetLinkOk **\n");
//	if (ms == 0) {
//		//Disp("MAC_SetLinkOk:time = %d \n", GetTickCount1ms());				
//    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[0]) ;	//reg 0x01  
//    	//Disp("ms=0 RdValue = %xH\n",RdValue);    
//		if ((RdValue & AN_COMPLETE) != 0)
//		{
//			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
//			{
//				GW1link1Ok = true;
//				//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
//				//Mac1_EnableInt();
//			}
//		} else {
//			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
//			{
//	  			MCMDR1 |= MCMDR_OPMOD;
//	  			MCMDR1 |= MCMDR_FDUP;				
//				GW1link1Ok = false;
//				//Mac1_DisableInt();
//				//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
//				//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
//			}
//		}
//	    
//		//PHY ADR2
//    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[1]) ;	//reg 0x01      
//		if ((RdValue & AN_COMPLETE) != 0)
//		{
//			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
//			{
//				GW1link2Ok = true;
//				//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
//				//Mac1_EnableInt();
//			}
//		} else {
//			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
//			{
//	  			MCMDR1 |= MCMDR_OPMOD;
//	  			MCMDR1 |= MCMDR_FDUP;				
//				GW1link2Ok = false;
//				//Mac1_DisableInt();
//				//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
//				//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
//			}
//		}
//	} else {
//		t0 = GetTickCount1ms();
//		while (1) 	 //wait for auto-negotiation complete 
//	    {
//	    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[0]) ;	//reg 0x01      
//	      
//			if ((RdValue & AN_COMPLETE) != 0)
//			{
//				//Disp("RdValue1 =%xH \n",RdValue);
//				GW1link1Ok = true;
//				//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
//				//Disp("GW link1 Ok\n");	
//				//m_pEthInfo1->portStatus = true;
//				break;
//			}
//			//OSTimeDly1ms(1);
//			if ((GetTickCount1ms()-t0) >= ms )			
//			{
//	  			MCMDR1 |= MCMDR_OPMOD;
//	  			MCMDR1 |= MCMDR_FDUP;				
//	  			GW1link1Ok = false;
//				//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
//				//m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
//				break;
//			}
//			OSTimeDly1ms(10);
//	    }
//
//		t0 = GetTickCount1ms();
//		while (1) 	 //wait for PHY ADR2 auto-negotiation complete 
//	    {
//			//PHY ADR2
//	    	RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[1]) ;	//reg 0x01      
//	    	
//	//		if ((RdValue & AN_LINKED) != 0)	//bit2
//			if ((RdValue & AN_COMPLETE) != 0)
//			{
//				//Disp("RdValue2 = %xH\n",RdValue);
//				GW1link2Ok = true;
//			//	Disp("GW link2 Ok\n");	
//				//Disp("GW1link2Ok\n");
////				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED);
//				break;
//			}
//			//OSTimeDly1ms(1);
//			if ((GetTickCount1ms()-t0) >= ms )			
//			{
//	  			MCMDR1 |= MCMDR_OPMOD;
//	  			MCMDR1 |= MCMDR_FDUP;				
//				GW1link2Ok = false;
////				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
//				break;
//			}
//			OSTimeDly1ms(10);
//	    }
//	    
//	}
//	//Disp(" 2 portStatus = %x , internalStatus = %x, \n", m_pEthInfo1->portStatus,m_pEthInfo1->internalStatus);	
//
//
//	if (GW1link1Ok) {
////	if (m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) {
//	    regANA   = Mii1StationRead(PHY_ANA_REG, GW1PHYAD[0]);		//reg 0x04
//	    regANLPA = Mii1StationRead(PHY_ANLPA_REG, GW1PHYAD[0]);	//reg 0x05
//	
//	    if ((regANA & 0x100) && (regANLPA & 0x100)) //100Mb, Full duplex,Bit8
//	    { 
//			//DisplayString(0,23,"100M, Full Duplex!!");
//			//Disp("GW1 100M, Full Duplex!!\n");
//	    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
//	    }
//	    else if ((regANA & 0x80) && (regANLPA & 0x80)) //100Mb, Half duplex
//	    {
//			//DisplayString(0,23,"100M, Half Duplex!!");
//			//Disp("GW1 100M, Half Duplex!!\n");
//	    	MCMDR1 |= MCMDR_OPMOD;
//			MCMDR1 &= ~MCMDR_FDUP;
//	    } 
//	    else if ((regANA & 0x40) && (regANLPA & 0x40)) //10Mb, Full duplex
//	    {  	
//			//DisplayString(0,23,"10M, Full Duplex!!");
//			//Disp("GW1 10M, Full Duplex!!\n");
//			MCMDR1 &= ~MCMDR_OPMOD;
//			MCMDR1 |= MCMDR_FDUP;    
//	    }
//	    else //10Mb, half duplex
//	    {   
//			//DisplayString(0,23,"10M, Half Duplex, default !!");
//			//Disp("GW1 10M, Half Duplex!!\n");
//	//    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
//			MCMDR1 &= ~MCMDR_OPMOD;
//			MCMDR1 &= ~MCMDR_FDUP;        
//	    }
//		//m_nJustLinkedin10s = 10;
//	
//	} 
//	
//	if (GW1link2Ok) {
//	    regANA   = Mii1StationRead(PHY_ANA_REG, GW1PHYAD[1]);		//reg 0x04
//	    regANLPA = Mii1StationRead(PHY_ANLPA_REG, GW1PHYAD[1]);	//reg 0x05
//	
//	    if ((regANA & 0x100) && (regANLPA & 0x100)) //100Mb, Full duplex,Bit8
//	    { 
//			//DisplayString(0,23,"100M, Full Duplex!!");
//			//Disp("GW2 100M, Full Duplex!!\n");
//	    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
//	    }
//	    else if ((regANA & 0x80) && (regANLPA & 0x80)) //100Mb, Half duplex
//	    {
//			//DisplayString(0,23,"100M, Half Duplex!!");
//			//Disp("GW2 100M, Half Duplex!!\n");
//	    	MCMDR1 |= MCMDR_OPMOD;
//			MCMDR1 &= ~MCMDR_FDUP;
//	    } 
//	    else if ((regANA & 0x40) && (regANLPA & 0x40)) //10Mb, Full duplex
//	    {  	
//			//DisplayString(0,23,"10M, Full Duplex!!");
//			//Disp("GW2 10M, Full Duplex!!\n");
//			MCMDR1 &= ~MCMDR_OPMOD;
//			MCMDR1 |= MCMDR_FDUP;    
//	    }
//	    else //10Mb, half duplex
//	    {   
//			//DisplayString(0,23,"10M, Half Duplex, default !!");
//	//    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
//			//Disp("GW2 10M, Half Duplex!!\n");
//			MCMDR1 &= ~MCMDR_OPMOD;
//			MCMDR1 &= ~MCMDR_FDUP;        
//	    }
//		//m_nJustLinkedin10s = 10;
//	
//	} 
//	
//	
//	if (GW1link1Ok || GW1link2Ok) {
//		if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
//		{
//			if (m_pEthInfo1->bUseInt == 0) 
//			{
//				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED);
//			}
//			m_pEthInfo1->dealMsg = 0;
//			m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
//			//Disp("**GW1link1Ok** \n");
//#if defined(VIRTUAL_NIC_SUPPORT)
//			if (pVirAdapter) 
//			{
//				pVirAdapter->portStatus = m_pEthInfo1->portStatus;
//			}
//#endif
//			Mac1_EnableInt();
//		}
//	} else {
//		if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
//		{
//			Mac1_DisableInt();
//			m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
//			//if (m_pEthInfo1->bUseInt == 0) 
//			//{
//			//	m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
//			//}
//			m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
//			m_pEthInfo1->dealMsg = 1;
//#if defined(VIRTUAL_NIC_SUPPORT)
//			if (pVirAdapter) 
//			{
//				pVirAdapter->portStatus = m_pEthInfo1->portStatus;
//			}
//#endif
//			ResetPhy1Chip();
//		}
////Disp(" 2 portStatus = %x , internalStatus = %x, GW1link1Ok=%d, GW1link2Ok=%d\n", m_pEthInfo1->portStatus,m_pEthInfo1->internalStatus, GW1link1Ok, GW1link2Ok);	
//	}
////Disp(" 3 portStatus = %x , internalStatus = %x, GW1link1Ok=%d, GW1link2Ok=%d\n", m_pEthInfo1->portStatus,m_pEthInfo1->internalStatus, GW1link1Ok, GW1link2Ok);	
////Disp(" 4 sndnumber = %x , sndinterupt = %x, rcvtime=%d, kkk=%d, curr=%d,sndlast=%d\n", sndnumber,sndinterupt, rcvtime, kkk, GetTickCount1ms(), m_pEthInfo1->portSilentTime);	
//	
//  	return;
//}
//#else
//void MAC1_SetLinkOk(int ms) {
//   	
//   	int RdValue = 0;
//   	int t0;
//	UINT32 volatile regANA, regANLPA;
//#if defined(VIRTUAL_NIC_SUPPORT)
//	NET_ADAPTER_INFO *pVirAdapter = NULL;
//	
//	if (m_pEthInfo1->virtualPortSeq >= 1 && m_pEthInfo1->virtualPortSeq <= ETH_GetEthPortsOnHmi()) 
//	{
//		pVirAdapter = (NET_ADAPTER_INFO *)ETH_GetAdapterFrmPort(m_pEthInfo1->virtualPortSeq);
//	}
//	else 
//	{
//		pVirAdapter = NULL;
//	}
//#endif
//	//for(i=0;i<1000000;i++)
//	//	j=j;		
//
//   	//Start auto-negotiation
////   	Mii1StationWrite(PHY_ANA_REG, PHY1AD, DR100_TX_FULL|DR100_TX_HALF|DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);
//    	
////   	RdValue = Mii1StationRead(PHY_CNTL_REG, PHY1AD) ;
//// 	RdValue = (RdValue | RESTART_AN | ENABLE_AN);
////   	Mii1StationWrite(PHY_CNTL_REG, PHY1AD, RdValue);
//
//		
//	if (ms == 0) {
//    	RdValue = Mii1StationRead(PHY_STATUS_REG, PHY1AD) ;
//		if ((RdValue & AN_COMPLETE) != 0)
//		{
//			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) != ETH_PORT_STATUS_NIGOTIATION_OK) 
//			{
//				//DisplayString(0,22,"AN_COMPLETE OK!!");
//				if (m_pEthInfo1->bUseInt == 0) 
//				{
//					m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED);
//				}
//				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
//#if defined(VIRTUAL_NIC_SUPPORT)
//				if (pVirAdapter) 
//				{
//					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
//				}
//#endif
//				Mac1_EnableInt();
//			}
//		}
//		else 
//		{
//			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) == ETH_PORT_STATUS_NIGOTIATION_OK) 
//			{
//	  			MCMDR1 |= MCMDR_OPMOD;
//	  			MCMDR1 |= MCMDR_FDUP;				
//
//				//return;
//				Mac1_DisableInt();
//				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
//				//if (m_pEthInfo1->bUseInt == 0) 
//				//{
//				//	m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
//				//}
//				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
//#if defined(VIRTUAL_NIC_SUPPORT)
//				if (pVirAdapter) 
//				{
//					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
//				}
//#endif
//			}
//		}
//	}
//	else 
//	{
//		t0 = GetTickCount1ms();
//		while (1) 	 //wait for auto-negotiation complete 
//	    {
//	    	RdValue = Mii1StationRead(PHY_STATUS_REG, PHY1AD) ;
//	    	//Disp("1 RdValue %xH\n",RdValue);
//			//if ((RdValue & AN_LINKED) != 0)
//			if ((RdValue & AN_COMPLETE) != 0)
//			{
//				//DisplayString(0,22,"AN_COMPLETE OK!!");
//				//Disp("**AN_COMPLETE**\n");
//				if (m_pEthInfo1->bUseInt == 0) 
//				{
//					m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED);
//				}
//				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_NIGOTIATION_OK);
//#if defined(VIRTUAL_NIC_SUPPORT)
//				if (pVirAdapter) 
//				{
//					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
//				}
//#endif
//				Mac1_EnableInt();
//				break;
//			}
//			if ((GetTickCount1ms()-t0) >= ms )
//			{
//	  			MCMDR1 |= MCMDR_OPMOD;
//	  			MCMDR1 |= MCMDR_FDUP;				
//
//				Mac1_DisableInt();
//				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
//				//if (m_pEthInfo1->bUseInt == 0) 
//				//{
//				//	m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
//				//}
//				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
//#if defined(VIRTUAL_NIC_SUPPORT)
//				if (pVirAdapter) 
//				{
//					pVirAdapter->portStatus = m_pEthInfo1->portStatus;
//				}
//#endif
//				break;
//			}
//			//OSTimeDly(1);
//	    }
//	    //Disp("Total %d\n",GetTickCount1ms()-t0);
//	}
//
//	if (m_pEthInfo1->portStatus&ETH_PORT_STATUS_NIGOTIATION_OK) {
//		//Disp("link1Ok ms=%d\n",ms);
//	    regANA   = Mii1StationRead(PHY_ANA_REG, PHY1AD);
//	    regANLPA = Mii1StationRead(PHY_ANLPA_REG, PHY1AD);
//	
//	//DisplayDWord(0,23,regANA);
//	//DisplayDWord(10,23,regANLPA);
//	
//	
//	
//	    if ((regANA & 0x100) && (regANLPA & 0x100)) //100Mb, Full duplex
//	    { 
//			//DisplayString(0,23,"100M, Full Duplex!!");
//	    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;           
//	    }
//	    else if ((regANA & 0x80) && (regANLPA & 0x80)) //100Mb, Half duplex
//	    {
//			//DisplayString(0,23,"100M, Half Duplex!!");
//	    	MCMDR1 |= MCMDR_OPMOD;
//			MCMDR1 &= ~MCMDR_FDUP;
//	    } 
//	    else if ((regANA & 0x40) && (regANLPA & 0x40)) //10Mb, Full duplex
//	    {  	
//			//DisplayString(0,23,"10M, Full Duplex!!");
//			MCMDR1 &= ~MCMDR_OPMOD;
//			MCMDR1 |= MCMDR_FDUP;    
//	    }
//	    else //10Mb, half duplex
//	    {   
//			//DisplayString(0,23,"10M, Half Duplex, default !!");
//	//    	MCMDR1 = MCMDR1 | MCMDR_OPMOD | MCMDR_FDUP;        
//			MCMDR1 &= ~MCMDR_OPMOD;
//			MCMDR1 &= ~MCMDR_FDUP;        
//	    }
//		//m_nJustLinkedin10s1 = 10;
//		//m_nSilent1Time = SystemTick5ms;
//	
//	} 
////while(1);
//  	return;
//}
//#endif

//================================================
void ResetPhy1Chip(void)
{
	UINT32	volatile RdValue;
	UINT32	volatile uReg;
	
	int	i;
	int	t0;  	
	
	if(PHY1Chip == CHIP_KSZ8081 || PHY1Chip==CHIP_YT8512C || PHY1Chip==CHIP_IP101A_INT)
	{
		// Check the RMII bit is enabled or not
		RdValue = Mii1StationRead(16, PHY1AD) ;
		if (!(RdValue & 0x0100)) {
			//DisplayString(0,20,"WARNING, RMII is not enabled, set it by software !");
			RdValue |= 0x0100;
			Mii1StationWrite(16, PHY1AD, RdValue); 	 
		}
	    
	  	//Reset PHY
	
	//    	t0 = 1000000;
	// June 2, 2009, bo changed, the Rdvalue bad
		t0 = 100;
		Mii1StationWrite(PHY_CNTL_REG, PHY1AD, RESET_PHY); 
		while (1) 
		{
			RdValue = Mii1StationRead(PHY_CNTL_REG, PHY1AD) ;
	  		if ((RdValue&RESET_PHY)==0)
				break;
	     		 
	   		if (!(t0--)) 
	   	  	{
				//DisplayString(0,21,"Reset PHY FAILED!!");
				break;
			}
	 	}
	    
	   	//Start auto-negotiation
	   	Mii1StationWrite(PHY_ANA_REG, PHY1AD, DR100_TX_FULL|DR100_TX_HALF|DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);
	    	
	   	RdValue = Mii1StationRead(PHY_CNTL_REG, PHY1AD) ;
	 	RdValue = (RdValue | RESTART_AN | ENABLE_AN);
	// 	RdValue |= RESTART_AN;
	   	Mii1StationWrite(PHY_CNTL_REG, PHY1AD, RdValue);
	   	//Disp("Wait for auto-negotiation complete...");

		//setLink1OK();
		//Ron Test
	   	if(PHY1Chip == CHIP_KSZ8081 || PHY1Chip==CHIP_YT8512C || PHY1Chip==CHIP_IP101A_INT)
	   	{
	   		if(PHY1Chip == CHIP_KSZ8081)
	   		{
		   		RdValue = Mii1StationRead(PHY_INT_CNTL_REG, PHY1AD) ;
		   		//Disp("PHY_INT_CNTL_REG =%xH \n",RdValue);
		 		RdValue = (RdValue | 0x0500);
		   		Mii1StationWrite(PHY_INT_CNTL_REG, PHY1AD, RdValue);
			}
	   		else if(PHY1Chip == CHIP_YT8512C)
	   		{
		   		RdValue = Mii1StationRead(YT8512_PHY_INT_MASK_REG, PHYAD) ;
				//Disp("YT8512_PHY_INT_MASK_REG =%xH \n",RdValue);
		 		RdValue = (RdValue | LINK_FAILED_INT | LINK_SUCCEED_INT);		//link up and down enable;
		   		Mii1StationWrite(YT8512_PHY_INT_MASK_REG, PHYAD, RdValue);
		   		RdValue = Mii1StationRead(YT8512_PHY_INT_STATUS_REG, PHYAD) ;	//it need read Status Reg once,when plug in & Power on situation.
		   																		//otherwise Phy IC will not generate INT Signal 
	   		}
	   		else if(PHY1Chip==CHIP_IP101A_INT)
	   		{
	   			RdValue = Mii1StationRead(IP101_DIO_CNRL_REG, PHYAD);	//p16R29,page 28
	   			RdValue |= 0x04;	//INTR Function
	   			//RdValue &= ~(0x80);	//disable TXER/RXER function in RMII mode
	   			Mii1StationWrite(IP101_DIO_CNRL_REG, PHYAD, RdValue);
	   			
		   		RdValue = Mii1StationRead(IP101_PHY_INT_CNTL_REG, PHYAD) ;	//p16R17,page 26
				Disp("IP101_PHY_INT_CNTL_REG =%04xH \n",RdValue);
		 		
		 		RdValue |= IP101_INTR;
		 		//RdValue &= ~(IP101_ALL_MASK | IP101_SPEED_MASK | IP101_DUPLEX_MASK | IP101_LINK_MASK);
		 		RdValue &= ~(IP101_ALL_MASK | IP101_LINK_MASK);
				Disp("RdValue =%04xH \n",RdValue);
		 		//RdValue = (RdValue | LINK_FAILED_INT | LINK_SUCCEED_INT);		//link up and down enable;
		   		Mii1StationWrite(IP101_PHY_INT_CNTL_REG, PHYAD, RdValue);
		   																		 
	   			
	   		}
			
			//set interrupt
			outpw(REG_CLK_PCLKEN0,inpw(REG_CLK_PCLKEN0) | (1<<3)); //Enable GPIO engin clock.
	    	/* Set MFPH_GPF13 to EINT2 */
	    	outpw(REG_SYS_GPF_MFPH,(inpw(REG_SYS_GPF_MFPH) & ~(0xF<<20)) | (0xF<<20));	//p114
		
		
	    	/* Configure PF13 to input mode and pull-up */
			outpw(REG_GPIOF_DIR, inpw(REG_GPIOF_DIR) & ~(1<<13));			//set PF.13 as an input pin,p618
			outpw(REG_GPIOF_PUEN, inpw(REG_GPIOF_PUEN) | (1<<13));			//set PF.13 pull up,p628
		
	
	    	/* Confingure PF13 to falling-edge trigger */
	    	RdValue = inpw(REG_GPIOF_IMD);			//p621
	    	outpw(REG_GPIOF_IMD,RdValue & ~(1<<13));
	    
	    	RdValue = inpw(REG_GPIOF_IREN);			//p622
	    	outpw(REG_GPIOF_IREN,RdValue & ~(1<<13));
	    
	    	RdValue = inpw(REG_GPIOF_IFEN);			//p623
	    	outpw(REG_GPIOF_IFEN,RdValue  | (1<<13));	//p624
	
			//Disp("REG_GPIOH_DIR = %08XH\n",inpw(REG_GPIOH_DIR));
			//Disp("REG_GPIOH_PUEN = %08XH \n",inpw(REG_GPIOH_PUEN));
			//Disp("REG_GPIOH_IMD = %08XH \n",inpw(REG_GPIOH_IMD));
			//Disp("REG_GPIOH_IREN = %08XH \n",inpw(REG_GPIOH_IREN));
			//Disp("REG_GPIOH_IFEN = %08XH\n",inpw(REG_GPIOH_IFEN));
	
	    	/* Enable external 0 interrupt */
	    	SetIntVector((PVOID)GPIO_IRQHandler2,EINT2_IRQn,IRQ_LEVEL_7);
		}	
	}
	else if(CHIP_DM8603)
	{
		for(i=0;i<2;i++)
		{
			//Disp("Reset PHY%XH \n",GW1PHYAD[i]);
			t0 = 100;
			Mii1StationWrite(PHY_CNTL_REG, GW1PHYAD[i], RESET_PHY); 	//RESET_PHY=Bit15
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
		//Disp("Quit DM8603 Reset PHY \n");
    
	   	//Start auto-negotiation					//Bit8		  //Bit7        //Bit6		 //Bit5		  //1
	   	Mii1StationWrite(PHY_ANA_REG, GW1PHYAD[0], DR100_TX_FULL|DR100_TX_HALF|DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);
	   	RdValue = Mii1StationRead(PHY_CNTL_REG, GW1PHYAD[0]) ;
	 	RdValue = (RdValue | RESTART_AN | ENABLE_AN);
	   	Mii1StationWrite(PHY_CNTL_REG, GW1PHYAD[0], RdValue);
	
	   	//Start auto-negotiation					//Bit8		  //Bit7        //Bit6		 //Bit5		  //1
	   	Mii1StationWrite(PHY_ANA_REG, GW1PHYAD[1], DR100_TX_FULL|DR100_TX_HALF|DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);
	   	RdValue = Mii1StationRead(PHY_CNTL_REG, GW1PHYAD[1]) ;
	 	RdValue = (RdValue | RESTART_AN | ENABLE_AN);
	   	Mii1StationWrite(PHY_CNTL_REG, GW1PHYAD[1], RdValue);
		
	}
	return;
}

//#if	defined(DM8603) && defined(DUAL_PORT)
//void ResetPhy1Chip(void)
//{
//
//	UINT32	volatile RdValue;
//	UINT32	volatile uReg;
//	
////	UINT32 volatile regANA, regANLPA;
//	int	i;
//	int	t0;  	
//
//	//Disp("**ResetPhy1Chip**\n");
//   	//Reset PHY
//
////    	t0 = 1000000;
//// June 2, 2009, bo changed, the Rdvalue bad
//	for(i=0;i<2;i++)
//	{
//		//Disp("Reset PHY%XH \n",GW1PHYAD[i]);
//		t0 = 100;
//		Mii1StationWrite(PHY_CNTL_REG, GW1PHYAD[i], RESET_PHY); 	//RESET_PHY=Bit15
//		while (1) 
//		{
//			RdValue = Mii1StationRead(PHY_CNTL_REG, GW1PHYAD[i]) ;
//  			if ((RdValue&RESET_PHY)==0)
//				break;
//     		OSTimeDly1ms(1);
//   			if (!(t0--)) 
//   	  		{
//   	  			Disp("Reset PHY FAILED!!\n");
//				break;
//			}
//		}
// 	}
//	//Disp("Quit Reset PHY \n");
//    
//   	//Start auto-negotiation			//Bit8		  //Bit7        //Bit6		 //Bit5		  //1
//   	Mii1StationWrite(PHY_ANA_REG, GW1PHYAD[0], DR100_TX_FULL|DR100_TX_HALF|DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);
//   	RdValue = Mii1StationRead(PHY_CNTL_REG, GW1PHYAD[0]) ;
// 	RdValue = (RdValue | RESTART_AN | ENABLE_AN);
//   	Mii1StationWrite(PHY_CNTL_REG, GW1PHYAD[0], RdValue);
////	for(i=0;i<5000;i++)	//wait for auto-negotiation complete 
////	{
////		RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[0]) ;
////		if ((RdValue & AN_COMPLETE) != 0)
////			break;
////		OSTimeDly1ms(1);
////	}
//
//   	//Start auto-negotiation			//Bit8		  //Bit7        //Bit6		 //Bit5		  //1
//   	Mii1StationWrite(PHY_ANA_REG, GW1PHYAD[1], DR100_TX_FULL|DR100_TX_HALF|DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);
//   	RdValue = Mii1StationRead(PHY_CNTL_REG, GW1PHYAD[1]) ;
// 	RdValue = (RdValue | RESTART_AN | ENABLE_AN);
//   	Mii1StationWrite(PHY_CNTL_REG, GW1PHYAD[1], RdValue);
////	for(i=0;i<5000;i++)	//wait for auto-negotiation complete 
////	{
////		RdValue = Mii1StationRead(PHY_STATUS_REG, GW1PHYAD[1]) ;
////		if ((RdValue & AN_COMPLETE) != 0)
////			break;
////		OSTimeDly1ms(1);
////	}
//
//   	//Disp("Wait for auto-negotiation complete...\n");
//
// 	return;
//}
//#else
//void ResetPhy1Chip(void)
//{
//
//	UINT32	volatile RdValue;
////	UINT32 volatile regANA, regANLPA;
////	int   i,j;
//	int		t0;  	
//
//	
//	// Check the RMII bit is enabled or not
//	RdValue = Mii1StationRead(16, PHY1AD) ;
//	if (!(RdValue & 0x0100)) {
//		//DisplayString(0,20,"WARNING, RMII is not enabled, set it by software !");
//		RdValue |= 0x0100;
//		Mii1StationWrite(16, PHY1AD, RdValue); 	 
//	}
//    
//  	//Reset PHY
//
////    	t0 = 1000000;
//// June 2, 2009, bo changed, the Rdvalue bad
//	t0 = 100;
//	Mii1StationWrite(PHY_CNTL_REG, PHY1AD, RESET_PHY); 
//	while (1) 
//	{
//		RdValue = Mii1StationRead(PHY_CNTL_REG, PHY1AD) ;
//  		if ((RdValue&RESET_PHY)==0)
//			break;
//     		 
//   		if (!(t0--)) 
//   	  	{
//			//DisplayString(0,21,"Reset PHY FAILED!!");
//			break;
//		}
// 	}
//    
//#if 1
//    
//   	//Start auto-negotiation
//   	Mii1StationWrite(PHY_ANA_REG, PHY1AD, DR100_TX_FULL|DR100_TX_HALF|DR10_TX_FULL|DR10_TX_HALF|IEEE_802_3_CSMA_CD);
//    	
//   	RdValue = Mii1StationRead(PHY_CNTL_REG, PHY1AD) ;
// 	RdValue = (RdValue | RESTART_AN | ENABLE_AN);
//// 	RdValue |= RESTART_AN;
//   	Mii1StationWrite(PHY_CNTL_REG, PHY1AD, RdValue);
//   	//Disp("Wait for auto-negotiation complete...");
//#endif
//	//setLink1OK();
//	//Ron Test
//   	RdValue = Mii1StationRead(PHY_INT_CNTL_REG, PHY1AD) ;
//   	//Disp("PHY_INT_CNTL_REG =%xH \n",RdValue);
// 	RdValue = (RdValue | 0x0500);
//   	Mii1StationWrite(PHY_INT_CNTL_REG, PHY1AD, RdValue);
//
//	//set interrupt
//	outpw(REG_CLK_PCLKEN0,inpw(REG_CLK_PCLKEN0) | (1<<3)); //Enable GPIO engin clock.
//    /* Set MFPH_GPF13 to EINT2 */
//    outpw(REG_SYS_GPF_MFPH,(inpw(REG_SYS_GPF_MFPH) & ~(0xF<<20)) | (0xF<<20));	//p114
//
//	
//    /* Configure PF13 to input mode and pull-up */
//	outpw(REG_GPIOF_DIR, inpw(REG_GPIOF_DIR) & ~(1<<13));			//set PF.13 as an input pin,p618
//	outpw(REG_GPIOF_PUEN, inpw(REG_GPIOF_PUEN) | (1<<13));			//set PF.13 pull up,p628
//	
//
//    /* Confingure PF13 to falling-edge trigger */
//    RdValue = inpw(REG_GPIOF_IMD);			//p621
//    outpw(REG_GPIOF_IMD,RdValue & ~(1<<13));
//    
//    RdValue = inpw(REG_GPIOF_IREN);			//p622
//    outpw(REG_GPIOF_IREN,RdValue & ~(1<<13));
//    
//    RdValue = inpw(REG_GPIOF_IFEN);			//p623
//    outpw(REG_GPIOF_IFEN,RdValue  | (1<<13));	//p624
//
//	//Disp("REG_GPIOH_DIR = %08XH\n",inpw(REG_GPIOH_DIR));
//	//Disp("REG_GPIOH_PUEN = %08XH \n",inpw(REG_GPIOH_PUEN));
//	//Disp("REG_GPIOH_IMD = %08XH \n",inpw(REG_GPIOH_IMD));
//	//Disp("REG_GPIOH_IREN = %08XH \n",inpw(REG_GPIOH_IREN));
//	//Disp("REG_GPIOH_IFEN = %08XH\n",inpw(REG_GPIOH_IFEN));
//
//    /* Enable external 0 interrupt */
//    SetIntVector((PVOID)GPIO_IRQHandler2,EINT2_IRQn,IRQ_LEVEL_7);
// 	return;
//}
//#endif


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



// Set MAC Address to CAM
void SetMac1Addr(ethaddr mac)
{
 	int  i;
 	//char mac[6];

// 	GetMacAddress((char *)mac);

  /* Copy MAC Address to global variable */
  //Disp("SetMac1Addr %02xH,%02xH,%02xH,%02xH,%02xH,%02xH\n",mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
  
  for (i = 0; i < (int )MAC_ADDR_SIZE-2; i++)
    gEMAC1Cam0M = (gEMAC1Cam0M << 8) | mac[i] ;

  for (i = (int )(MAC_ADDR_SIZE-2); i < (int )MAC_ADDR_SIZE; i++)
    gEMAC1Cam0L = (gEMAC1Cam0L << 8) | mac[i] ;
    

    gEMAC1Cam0L = (gEMAC1Cam0L << 16) ;

    FillCam1Entry(0, gEMAC1Cam0M, gEMAC1Cam0L);
    
}



// Initialize Tx frame descriptor area-buffers.
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


	// Get Frame descriptor's base address.
#if defined(NUC970)
	TXDLSA1 = (UINT32)GetNcMem(sizeof(sFrameDescriptor)*MaxTxFrameDescriptors);
#else	
	TXDLSA1 = (UINT32)Tx1FDBaseAddr | 0x80000000;
#endif	
	gWTx1FDPtr = gCTx1FDPtr = TXDLSA1;
  
	// Generate linked list.
	pFrameDescriptor = (sFrameDescriptor *) gCTx1FDPtr;
	pStartFrameDescriptor = pFrameDescriptor;



	for(i = 0; i < MaxTxFrameDescriptors; i++) {
		if (pLastFrameDescriptor == NULL)
			pLastFrameDescriptor = pFrameDescriptor;
		else
			pLastFrameDescriptor->NextFrameDescriptor = (UINT32)pFrameDescriptor;

		pFrameDescriptor->Status1 = (PaddingMode | CRCMode | MACTxIntEn);
#if defined(NUC970)
  //#if defined(TCP_SPEEDUP)
		pFrameDescriptor->FrameDataPtr = (UINT32)0;
  //#else
//		pFrameDescriptor->FrameDataPtr = (UINT32)GetNcMem(1700);;
		
  //#endif
#else
		pFrameDescriptor->FrameDataPtr = (UINT32)0x0;
#endif		
		pFrameDescriptor->Status2 = (UINT32)0x0;
		pFrameDescriptor->NextFrameDescriptor = NULL;

		pLastFrameDescriptor = pFrameDescriptor;
		pFrameDescriptor++;
	}

	// Make Frame descriptor to ring buffer type.
	pFrameDescriptor--;
	pFrameDescriptor->NextFrameDescriptor = (UINT32)pStartFrameDescriptor;
}

// Initialize Rx frame descriptor area-buffers.
void Rx1FDInitialize(void)
{
 	sFrameDescriptor 	*pFrameDescriptor;
 	sFrameDescriptor 	*pStartFrameDescriptor;
 	sFrameDescriptor 	*pLastFrameDescriptor = NULL;
 	UINT32 			i;


	// Get Frame descriptor's base address.
#if defined(NUC970)
	RXDLSA1 = (UINT32)GetNcMem(sizeof(sFrameDescriptor)*MaxRxFrameDescriptors);
#else	
	RXDLSA1 = (UINT32)Rx1FDBaseAddr | 0x80000000;
#endif	
	gCRx1FDPtr = RXDLSA1;

	// Generate linked list.
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

	// Make Frame descriptor to ring buffer type.
	pFrameDescriptor--;
	pFrameDescriptor->NextFrameDescriptor = (UINT32)pStartFrameDescriptor;
}



// set Registers related with MAC.
void ReadyMac1(void)
{
	MIEN1 = gMIEN1 ;
	MCMDR1 = gMCMDR1 ;
}



// MAC Transfer Start for interactive mode
void Mac1TxGo(void)
{
 	// Enable MAC Transfer


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
	
	if(PHY1Chip==CHIP_KSZ8081 || PHY1Chip == CHIP_YT8512C || PHY1Chip==CHIP_IP101A_INT)
	{
		RdValue = Mii1StationRead(PHY_ID1_REG, PHY1AD) ;
		Disp("PHY1_ID1_REG = %xH \n",RdValue);
		RdValue = Mii1StationRead(PHY_ID2_REG, PHY1AD) ;
		Disp("PHY1_ID2_REG = %xH \n",RdValue>>4);
	}
	else if(PHY1Chip==CHIP_DM8603)
	{
		RdValue = Mii1StationRead(PHY_ID1_REG, GW1PHYAD[0]) ;
		Disp("PHY1_ID1_REG = %xH \n",RdValue);
		RdValue = Mii1StationRead(PHY_ID2_REG, GW1PHYAD[0]) ;
		Disp("PHY1_ID2_REG = %xH \n",RdValue);
	}
	
}


// Initialize MAC Controller
#if 1	//ok code
int MAC1_Initialize(NET_ADAPTER_INFO* pEthInfo)
{
	Disp("\n**Mac1_Initialize**\n");
	
	#if defined(S_SERIES) || defined(PT2100A) || defined(PK2070A) || defined(WOP107E)	//S BOX & PM2 had Info in System Parameter
		PHY1Chip=(RAM_HwInfo.ethernetType>>8) & 0xff;
	#else
		//other old model use DM8603 in PHY1 (define DM8603 or not)
		#if defined(DM8603)
			PHY1Chip=CHIP_DM8603;
		#else
			PHY1Chip=CHIP_KSZ8081;
		#endif
			
	#endif

	//PHY1Chip=(RAM_HwInfo.ethernetType>>8) & 0xff;

//#if defined(DM8603)	
//PHY1Chip = CHIP_DM8603;
//#endif
	
	//old model fix use 0x00(CHIP_IP101A), define to CHIP_KSZ8081
	if(PHY1Chip== CHIP_IP101A || PHY1Chip== CHIP_DM9000A)
	{
		PHY1Chip=CHIP_KSZ8081;
	}
	if(PHY1Chip == CHIP_KSZ8081)
		Disp("PHY1Chip=CHIP_KSZ8081 \n");	//CHIP_KSZ8081=0x03,CHIP_DM8603=0x02
	else if(PHY1Chip == CHIP_DM8603) 
		Disp("PHY1Chip=CHIP_DM8603 \n");	//CHIP_KSZ8081=0x03,CHIP_DM8603=0x02
	else if(PHY1Chip == CHIP_YT8512C) 
		Disp("PHY1Chip=CHIP_YT8512C \n");	//CHIP_KSZ8081=0x03,CHIP_DM8603=0x02,CHIP_YT8512C=0x04
	else if(PHY1Chip == CHIP_IP101A_INT) 
		Disp("PHY1Chip=CHIP_IP101A_INT \n");	//CHIP_KSZ8081=0x03,CHIP_DM8603=0x02,CHIP_YT8512C=0x04
	else  
		Disp("Unknow PHY0Chip \n");

	outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) | (1 << 17));            // EMAC1 clk
    outpw(REG_CLK_DIVCTL8, (inpw(REG_CLK_DIVCTL8) & ~0xFF) | 0xA0);     // MDC clk divider

    // Multi function pin setting
    outpw(REG_SYS_GPE_MFPL, (inpw(REG_SYS_GPE_MFPL) & ~0xFFFFFF00) | 0x11111100);
    outpw(REG_SYS_GPE_MFPH, (inpw(REG_SYS_GPE_MFPH) & ~0x0000FFFF) | 0x00001111);

	m_pEthInfo1 = pEthInfo;
	
//	m_pEthInfo1->portStatus = false;
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
	m_lastTime1 =0;
	mac1_flowcontrl= 0;
	mac1_flowHighThread = (MaxRxFrameDescriptors*2)/3;
	mac1_flowLowThread = (MaxRxFrameDescriptors*3)/100;
#endif
	m_lastTime1 =0;

    // Reset MAC
    //outpw(REG_EMAC1_MCMDR, 0x1000000);
	MCMDR1|=MCMDR_SWR;  //====
		
#if defined(NUC970)
	Net1RxBuf = (NETBUF*)GetNcMem(sizeof(NETBUF)*MaxRxFrameDescriptors);
#else	
	Net1RxBuf = (NETBUF*)((UINT32)rx1buf | 0x80000000);
#endif	
	_iqueue1_first = _iqueue1_last = NULL;





	// Set the Tx and Rx Frame Descriptor
	Tx1FDInitialize() ;
	Rx1FDInitialize() ;

 	SetMac1Addr(pEthInfo->myMacAddr) ;
	m_nJustLinkedin10s1 =10;

	// Set the CAM Control register and the MAC address value
	FillCam1Entry(0, gEMAC1Cam0M, gEMAC1Cam0L);
	CAMCMR1 = gCAMCMR1 ;

	Mac1_EnableBroadcast();	


	// Enable MAC Tx and Rx interrupt.
	//Enable_Int(EMCTXINT);
	//Enable_Int(EMCRXINT);

	TDU1_Flag=0;

	// Configure the MAC control registers.
	ReadyMac1() ;


 	// Set PHY operation mode
 	AutoDetectPhy1Addr() ; //[2007/03/08], added by cmn
	//GetPhy1ID();
 	ResetPhy1Chip() ;

 	SetMac1Addr(pEthInfo->myMacAddr) ;

	m_nP9701_Mutex = 0;
	m_pEthInfo1->portSilentTime = 0; //SystemTick;
 	
#if 0
	{
		int tmp;
		*((volatile UINT32 *)0x38)=(UINT32)IRQ_IntHandler;
		__asm
		{
			MRS	tmp, CPSR
			BIC	tmp, tmp, 0x80
			MSR	CPSR_c, tmp
		}
	}
#endif

	//interrupt
	if(PHY1Chip != CHIP_DM8603)
	{
		SetIntVector((PVOID)MAC1_Tx_isr,EMC1_TX_IRQn,IRQ_LEVEL_1 | HIGH_LEVEL_SENSITIVE);
		SetIntVector((PVOID)MAC1_Rx_isr,EMC1_RX_IRQn,IRQ_LEVEL_1 | HIGH_LEVEL_SENSITIVE);
		//SetExtIsr(5,MAC1_Rx_isr);
		//SetExtIsr(6,MAC1_Tx_isr);
		Mac1_EnableInt();
	
		EnableInt(EINT2_IRQn);	//GPIO INT
	}
	//setLink1OK();
	return 1;
}
#else	//debug
int Mac1_Initialize(ethaddr MacAddr)
{
//	//bo added. for DMA 
////	*((unsigned volatile int *) 0xB0000200) |= 0x08000000;	//enable GDMA ?? clock
////	*((unsigned volatile int *) 0xB0000200) |= 0x20; 		//enable DMAC ??  clock
////	*((unsigned volatile int *) 0xB0000200) |= 0x80; 		//enable EMC ??  clock
//
//
//    outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) | (1 << 16));            // EMAC0 clk
//    outpw(REG_CLK_DIVCTL8, (inpw(REG_CLK_DIVCTL8) & ~0xFF) | 0xA0);     // MDC clk divider
//
//    // Multi function pin setting
//    outpw(REG_SYS_GPF_MFPL, 0x11111111);
//    outpw(REG_SYS_GPF_MFPH, (inpw(REG_SYS_GPF_MFPH) & ~0xFF) | 0x11);
//
//#if defined(DUAL_PORT)
//	outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) | (1 << 17));            // EMAC1 clk
//    outpw(REG_CLK_DIVCTL8, (inpw(REG_CLK_DIVCTL8) & ~0xFF) | 0xA0);     // MDC clk divider
//
//    // Multi function pin setting
//    outpw(REG_SYS_GPE_MFPL, (inpw(REG_SYS_GPE_MFPL) & ~0xFFFFFF00) | 0x11111100);
//    outpw(REG_SYS_GPE_MFPH, (inpw(REG_SYS_GPE_MFPH) & ~0x0000FFFF) | 0x00001111);
//#endif
//
//	m_pEthInfo1->portStatus = false;
	m_lastTime1 =0;
//
//    // Reset MAC
//    //outpw(REG_EMAC1_MCMDR, 0x1000000);
	MCMDR1|=MCMDR_SWR;  //====
//		
#if defined(NUC970)
	Net1RxBuf = (NETBUF*)GetNcMem(sizeof(NETBUF)*MaxRxFrameDescriptors);
#else	
	Net1RxBuf = (NETBUF*)((UINT32)rx1buf | 0x80000000);
#endif	
	_iqueue1_first = _iqueue1_last = NULL;
//
//
//
//
//
//	// Set the Tx and Rx Frame Descriptor
	Tx1FDInitialize() ;
	Rx1FDInitialize() ;

 	SetMac1Addr(MacAddr) ;
	m_nJustLinkedin10s1 =10;

	// Set the CAM Control register and the MAC address value
	FillCam1Entry(0, gEMAC1Cam0M, gEMAC1Cam0L);
	CAMCMR1 = gCAMCMR1 ;

	Mac1_EnableBroadcast();	


	// Enable MAC Tx and Rx interrupt.
	//Enable_Int(EMCTXINT);
	//Enable_Int(EMCRXINT);

	TDU1_Flag=0;

	// Configure the MAC control registers.
	ReadyMac1() ;


 	// Set PHY operation mode
 	AutoDetectPhy1Addr() ; //[2007/03/08], added by cmn
// 	ResetPhy1Chip() ;
//
 	SetMac1Addr(MacAddr) ;

	m_nP9701_Mutex = 0;
	m_pEthInfo1->portSilentTime = 0; //SystemTick;
// 	
//#if 0
//	{
//		int tmp;
//		*((volatile UINT32 *)0x38)=(UINT32)IRQ_IntHandler;
//		__asm
//		{
//			MRS	tmp, CPSR
//			BIC	tmp, tmp, 0x80
//			MSR	CPSR_c, tmp
//		}
//	}
//#endif
//
	//interrupt
	SetIntVector((PVOID)MAC1_Tx_isr,EMC1_TX_IRQn,IRQ_LEVEL_1 | HIGH_LEVEL_SENSITIVE);
	SetIntVector((PVOID)MAC1_Rx_isr,EMC1_RX_IRQn,IRQ_LEVEL_1 | HIGH_LEVEL_SENSITIVE);
	//SetExtIsr(5,MAC1_Rx_isr);
	//SetExtIsr(6,MAC1_Tx_isr);
	Mac1_EnableInt();

	//setLink1OK();
	return 1;
}

//extern ethaddr MyEthAddr;
void Enable_MAC0(void)
{
	//bo added. for DMA 
//	*((unsigned volatile int *) 0xB0000200) |= 0x08000000;	//enable GDMA ?? clock
//	*((unsigned volatile int *) 0xB0000200) |= 0x20; 		//enable DMAC ??  clock
//	*((unsigned volatile int *) 0xB0000200) |= 0x80; 		//enable EMC ??  clock

	//ethaddr MyEthAddr;
//	Disp("** Init_MAC0 **\n");	
    outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) | (1 << 16));            // EMAC0 clk
    outpw(REG_CLK_DIVCTL8, (inpw(REG_CLK_DIVCTL8) & ~0xFF) | 0xA0);     // MDC clk divider

    // Multi function pin setting
    outpw(REG_SYS_GPF_MFPL, 0x11111111);
    outpw(REG_SYS_GPF_MFPH, (inpw(REG_SYS_GPF_MFPH) & ~0xFF) | 0x11);
//
//#if defined(DUAL_PORT)
//	outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) | (1 << 17));            // EMAC1 clk
//    outpw(REG_CLK_DIVCTL8, (inpw(REG_CLK_DIVCTL8) & ~0xFF) | 0xA0);     // MDC clk divider
//
//    // Multi function pin setting
//    outpw(REG_SYS_GPE_MFPL, (inpw(REG_SYS_GPE_MFPL) & ~0xFFFFFF00) | 0x11111100);
//    outpw(REG_SYS_GPE_MFPH, (inpw(REG_SYS_GPE_MFPH) & ~0x0000FFFF) | 0x00001111);
//#endif
//	Disp("!! 1\n");
//	eth_getHwAddr(MyEthAddr);
//	link1Ok = false;
//	m_lastTime1 =0;

    // Reset MAC
    //outpw(REG_EMAC1_MCMDR, 0x1000000);
//	Disp("!! 2\n");

//	MCMDR1|=MCMDR_SWR;  //====
		
//#if defined(NUC970)
//	Net1RxBuf = (NETBUF*)GetNcMem(sizeof(NETBUF)*MaxRxFrameDescriptors);
//#else	
//	Net1RxBuf = (NETBUF*)((UINT32)rx1buf | 0x80000000);
//#endif	
//	_iqueue1_first = _iqueue1_last = NULL;
//	Disp("!! 3\n");





	// Set the Tx and Rx Frame Descriptor
//	Tx1FDInitialize() ;
//	Rx1FDInitialize() ;
//	Disp("!! 4\n");

// 	SetMac1Addr(MyEthAddr) ;
//	m_nJustLinkedin10s1 =10;

	// Set the CAM Control register and the MAC address value
//	FillCam1Entry(0, gEMAC1Cam0M, gEMAC1Cam0L);
//	CAMCMR1 = gCAMCMR1 ;
//	Disp("!! 5\n");

//	Mac1_EnableBroadcast();	
//	Disp("!! 6\n");


	// Enable MAC Tx and Rx interrupt.
	//Enable_Int(EMCTXINT);
	//Enable_Int(EMCRXINT);

//	TDU1_Flag=0;

	// Configure the MAC control registers.
//	ReadyMac1() ;


 	// Set PHY operation mode
 	AutoDetectPhy1Addr() ; //[2007/03/08], added by cmn
// 	ResetPhy1Chip() ;

// 	SetMac1Addr(MyEthAddr) ;
//	Disp("!! 7\n");

//	m_nP9701_Mutex = 0;
//	m_nSilent1Time = SystemTick5ms;
 	
#if 0
	{
		int tmp;
		*((volatile UINT32 *)0x38)=(UINT32)IRQ_IntHandler;
		__asm
		{
			MRS	tmp, CPSR
			BIC	tmp, tmp, 0x80
			MSR	CPSR_c, tmp
		}
	}
#endif

//	//interrupt
//	SetIntVector((PVOID)MAC1_Tx_isr,EMC1_TX_IRQn,IRQ_LEVEL_1 | HIGH_LEVEL_SENSITIVE);
//	SetIntVector((PVOID)MAC1_Rx_isr,EMC1_RX_IRQn,IRQ_LEVEL_1 | HIGH_LEVEL_SENSITIVE);
//	//SetExtIsr(5,MAC1_Rx_isr);
//	//SetExtIsr(6,MAC1_Tx_isr);
//	Mac1_EnableInt();

	//setLink1OK();
//	return 1;
//	Disp("**Init_MAC0 Quit**\n");
}

void Init_MAC0(void)
{
	//bo added. for DMA 
//	*((unsigned volatile int *) 0xB0000200) |= 0x08000000;	//enable GDMA ?? clock
//	*((unsigned volatile int *) 0xB0000200) |= 0x20; 		//enable DMAC ??  clock
//	*((unsigned volatile int *) 0xB0000200) |= 0x80; 		//enable EMC ??  clock

	//ethaddr MyEthAddr;
	//Disp("** Boot Init_MAC0 **\n");	
    outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) | (1 << 16));            // EMAC0 clk
    outpw(REG_CLK_DIVCTL8, (inpw(REG_CLK_DIVCTL8) & ~0xFF) | 0xA0);     // MDC clk divider

    // Multi function pin setting
    outpw(REG_SYS_GPF_MFPL, 0x11111111);
    outpw(REG_SYS_GPF_MFPH, (inpw(REG_SYS_GPF_MFPH) & ~0xFF) | 0x11);

#if defined(DUAL_PORT)
	outpw(REG_CLK_HCLKEN, inpw(REG_CLK_HCLKEN) | (1 << 17));            // EMAC1 clk
    outpw(REG_CLK_DIVCTL8, (inpw(REG_CLK_DIVCTL8) & ~0xFF) | 0xA0);     // MDC clk divider

    // Multi function pin setting
    outpw(REG_SYS_GPE_MFPL, (inpw(REG_SYS_GPE_MFPL) & ~0xFFFFFF00) | 0x11111100);
    outpw(REG_SYS_GPE_MFPH, (inpw(REG_SYS_GPE_MFPH) & ~0x0000FFFF) | 0x00001111);
#endif
//	Disp("!! 1\n");
//	eth_getHwAddr(MyEthAddr);
//	link1Ok = false;
//	m_lastTime1 =0;

    // Reset MAC
    //outpw(REG_EMAC1_MCMDR, 0x1000000);
//	Disp("!! 2\n");
	MCMDR1|=MCMDR_SWR;  //====
		
//#if defined(NUC970)
//	Net1RxBuf = (NETBUF*)GetNcMem(sizeof(NETBUF)*MaxRxFrameDescriptors);
//#else	
//	Net1RxBuf = (NETBUF*)((UINT32)rx1buf | 0x80000000);
//#endif	
//	_iqueue1_first = _iqueue1_last = NULL;
//	Disp("!! 3\n");


	// Set the Tx and Rx Frame Descriptor
//	Tx1FDInitialize() ;
//	Rx1FDInitialize() ;
//	Disp("!! 4\n");

// 	SetMac1Addr(MyEthAddr) ;
//	m_nJustLinkedin10s1 =10;

	// Set the CAM Control register and the MAC address value
//	FillCam1Entry(0, gEMAC1Cam0M, gEMAC1Cam0L);
	CAMCMR1 = gCAMCMR1 ;
//	Disp("!! 5\n");

//	Mac1_EnableBroadcast();	
//	Disp("!! 6\n");


	// Enable MAC Tx and Rx interrupt.
	//Enable_Int(EMCTXINT);
	//Enable_Int(EMCRXINT);

//	TDU1_Flag=0;

	// Configure the MAC control registers.
	ReadyMac1() ;


 	// Set PHY operation mode
 	AutoDetectPhy1Addr() ; //[2007/03/08], added by cmn
 	ResetPhy1Chip() ;

// 	SetMac1Addr(MyEthAddr) ;
//	Disp("!! 7\n");

//	m_nP9701_Mutex = 0;
//	m_nSilent1Time = SystemTick5ms;
 	
#if 0
	{
		int tmp;
		*((volatile UINT32 *)0x38)=(UINT32)IRQ_IntHandler;
		__asm
		{
			MRS	tmp, CPSR
			BIC	tmp, tmp, 0x80
			MSR	CPSR_c, tmp
		}
	}
#endif

//	//interrupt
//	SetIntVector((PVOID)MAC1_Tx_isr,EMC1_TX_IRQn,IRQ_LEVEL_1 | HIGH_LEVEL_SENSITIVE);
//	SetIntVector((PVOID)MAC1_Rx_isr,EMC1_RX_IRQn,IRQ_LEVEL_1 | HIGH_LEVEL_SENSITIVE);
//	//SetExtIsr(5,MAC1_Rx_isr);
//	//SetExtIsr(6,MAC1_Tx_isr);
//	Mac1_EnableInt();

	//setLink1OK();
//	return 1;
//	Disp("**Init_MAC0 Quit**\n");
}

#endif


void  Mac1_ShutDown()
{
#if 1 //CMN

	MCMDR1 &= ~(MCMDR_RXON|MCMDR_TXON) ;

#endif   
}       
//int iii=0;
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
	//rameDescriptor *psTxFD;
	char* TxData = NULL;
	sFrameSendBuffer* pTxDataPtr = gWTx1DataStartPtr;
	
	int i;

#if defined(TEST_BO)
DisplayWord(60,29,(int)Mac1jj5);
DisplayWord(30,27,Mac1jj1++);
#endif
	//Disp("Mac1_GetSendFramePt\n");
	Mac1_DisableInt();
//	C970_mutex_on(SEND_MUTEX,0);


	for(i = 0; i < MaxTxFrameDescriptors; i++) {
		if ( (pTxDataPtr+i)->used == 0) 
		{
			(pTxDataPtr+i)->used = 1;
			(pTxDataPtr+i)->purpose = ori;
			TxData = (char*)(&(pTxDataPtr+i)->data);
			
//DisplayWord(0,Mac1jj4,GetTickCount1ms());
//DisplayDWord(10,Mac1jj4,(int)(pTxDataPtr+i) );			
//DisplayDWord(20,Mac1jj4,(int)(TxData) );			
//DisplayWord(35,Mac1jj4,(int)(ori) );			
//if (ori!=3) 
//{
//Mac1jj4++;
//if (Mac1jj4>= 10)
//Mac1jj4 = 1;
	
//}

//Mac1_DisableInt();
Mac1jj5++;
w1frame++;
w1frame1++;
//Mac1_EnableInt();
//DisplayWord(55,i,GetTickCount1ms());

			break;
		}
	}

//	C970_mutex_off(SEND_MUTEX,0);
	Mac1_EnableInt();

//DisplayWord(55,i,ori);

#if defined(TEST_BO)
DisplayWord(35,27,Mac1jj1++);
#endif

	if (!TxData) 
	{
		//resource full
//		bool bOk;
//		bOk = MAC1_isHwConnectionOk();
		
		//if (!bOk) 
		//{
			//release all resource
			for(i = 0; i < MaxTxFrameDescriptors; i++) {
				if ( (pTxDataPtr+i)->used != 0) 
				{
					(pTxDataPtr+i)->used = 0;
					(pTxDataPtr+i)->purpose = 0;
				}
			}
		//}
		//DisplayWord(50,0,GetTickCount1ms());	
		(pTxDataPtr+0)->used = 1;
		(pTxDataPtr+0)->purpose = ori;
		TxData = (char*)(&(pTxDataPtr+0)->data);

	}


	return TxData;

}
int Mac1_FreeSendFramePt(char* pData, int ori)
{
	//rameDescriptor *psTxFD;
	int ret = 0;
	sFrameSendBuffer* pTxDataPtr = gWTx1DataStartPtr;
	
	int i;
#if defined(TEST_BO)
DisplayWord(40,27,Mac1jj1++);
//DisplayWord(5,29,Mac1jj5);	
#endif

	if (ori !=5) 
	{
	Mac1_DisableInt();
//	C970_mutex_on(SEND_MUTEX,0);
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
//DisplayWord(60,i,GetTickCount1ms());

			break;
		}
	}

	if (ori !=5) 
	{
//	C970_mutex_off(SEND_MUTEX,0);
	Mac1_EnableInt();
	}

//DisplayWord(60,i,(pTxDataPtr+i)->purpose);


/*
if (w1frame != w1frame1) 
{
DisplayWord(55,20,(pTxDataPtr+0)->purpose);
DisplayWord(55,21,(pTxDataPtr+1)->purpose);
DisplayWord(55,22,(pTxDataPtr+2)->purpose);
DisplayWord(55,23,ori);
DisplayWord(60,20,w1frame);
DisplayWord(60,21,Mac1jj5);
}*/


#if defined(TEST_BO)
DisplayWord(45,27,Mac1jj1++);
#endif

	return ret;
}
int Mac1_SendPacketSpeedup(BYTE*  pbData, DWORD dwLength)
{
	sFrameDescriptor *psTxFD;

	//Mac1_MarkSentFramePt(pbData);

	//Disp("!1 SendPacketSpeedup\n");
	psTxFD = (sFrameDescriptor *)gWTx1FDPtr;

	// Cheange ownership to DMA
	psTxFD->FrameDataPtr=(UINT32)pbData;
	psTxFD->Status2 = (UINT32)(dwLength & 0xffff);
	psTxFD->Status1 |= TXfOwnership_DMA;

	Tx1Ready = 0;

	// Enable MAC Tx control register
	Mac1TxGo();

	// Change the Tx frame descriptor for next use
	gWTx1FDPtr = (UINT32)(psTxFD->NextFrameDescriptor);


	//C970_mutex_off(SEND_MUTEX,0);
	//Mac1_EnableInt();
}
#endif

int MAC1_SendPacket(BYTE*  pbData, DWORD dwLength)
{
/*
	sFrameDescriptor *psTxFD;
	//UINT32         *pTXFDStatus1;

	psTxFD = (sFrameDescriptor *)gWTx1FDPtr;

	// Cheange ownership to DMA
	psTxFD->FrameDataPtr=(UINT32)pbData;
	psTxFD->Status2 = (UINT32)(dwLength & 0xffff);
	psTxFD->Status1 |= TXfOwnership_DMA;

	Tx1Ready = 0;

	// Enable MAC Tx control register
	Mac1TxGo();

	// Change the Tx frame descriptor for next use
	gWTx1FDPtr = (UINT32)(psTxFD->NextFrameDescriptor);

*/	
	sFrameDescriptor *psTxFD;
	UINT32         *pTXFDStatus1;
	//Disp("! 1_SendPacket\n");

//	while(Global_Tx1InProcess==TRUE) {
//		OSTimeDly1ms(0);
//	}

//_DisplayDWord(0,4,Mac1jjj++);
//	Mac1_DisableInt();
//	C970_mutex_on(SEND_MUTEX,0);

	// Get Tx frame descriptor & data pointer
	psTxFD = (sFrameDescriptor *)gWTx1FDPtr ;
	

//	psTxFD->FrameDataPtr = (UINT32)gWTxDataPtr;


	pTXFDStatus1 = (UINT32 *)&psTxFD->Status1;

	//check ownership
//	if (psTxFD->Status1&TXfOwnership_CPU)
//		return 0;


#if defined(NUC970)
  //#if defined(TCP_SPEEDUP)
	//psTxFD->FrameDataPtr=(UINT32)pbData;
  //#else
//	m_memcpy(psTxFD->FrameDataPtr,(char*)pbData,(UINT32)(dwLength & 0xffff));
  //#endif	
  
	psTxFD->FrameDataPtr = (UINT32)MAC1_GetSendFramePt(5);
//DisplayWord(0,Mac1jj3,GetTickCount1ms());
//Mac1jj3++;
//if (Mac1jj3>=25)
//Mac1jj3 = 21;

	m_memcpy(psTxFD->FrameDataPtr,(char*)pbData,(UINT32)(dwLength & 0xffff));
#else	
	psTxFD->FrameDataPtr=(UINT32)pbData | 0x80000000;
#endif	
	psTxFD->Status2 = (UINT32)(dwLength & 0xffff);

//DisplayWord(5,Mac1jj3,GetTickCount1ms());
//Mac1jj3++;
//if (Mac1jj3>=25)
//Mac1jj3 = 21;

	// Cheange ownership to DMA
	psTxFD->Status1 |= TXfOwnership_DMA;

	Tx1Ready = 0;

	// Enable MAC Tx control register
	Mac1TxGo();

	// Change the Tx frame descriptor for next use
	gWTx1FDPtr = (UINT32)(psTxFD->NextFrameDescriptor);
	
//	gWTxDataPtr = (gWTxDataPtr+FRAME_TX_DATA_LEN);
//	if (gWTxDataPtr>= gWTx1DataStartPtr + FRAME_TX_DATA_LEN*MaxTxFrameDescriptors)
//		gWTxDataPtr = gWTx1DataStartPtr;


//	Global_Tx1InProcess=TRUE;


//	C970_mutex_off(SEND_MUTEX,0);
//	Mac1_EnableInt();

//_DisplayDWord(10,4,Mac1jjj++);
//Mac1cc2++;
//DisplayWord(15,23,Mac1cc2);


	return 1 ;
}
int Mac1_SendPacket_InINT(BYTE*  pbData, DWORD dwLength)
{
/*
	sFrameDescriptor *psTxFD;
	UINT32         *pTXFDStatus1;
	//NETBUF* netbuf;

	while(Global_Tx1InProcess==TRUE) {
		MAC1_Tx_isr();
		//OSTimeDly1ms(0);
	}

//	Mac1_DisableInt();

	// Get Tx frame descriptor & data pointer
	psTxFD = (sFrameDescriptor *)gWTx1FDPtr ;

	pTXFDStatus1 = (UINT32 *)&psTxFD->Status1;


#if defined(NUC970)
//	psTxFD->FrameDataPtr=(UINT32)pbData;
	m_memcpy(psTxFD->FrameDataPtr,(char*)pbData,(UINT32)(dwLength & 0xffff));
#else	
	psTxFD->FrameDataPtr=(UINT32)pbData | 0x80000000;
#endif	
	psTxFD->Status2 = (UINT32)(dwLength & 0xffff);


	// Cheange ownership to DMA
	psTxFD->Status1 |= TXfOwnership_DMA;

	Tx1Ready = 0;

	// Enable MAC Tx control register
	Mac1TxGo();

//DisplayWord(15,6,GetTickCount1ms());	
	// Change the Tx frame descriptor for next use
	gWTx1FDPtr = (UINT32)(psTxFD->NextFrameDescriptor);

	Global_Tx1InProcess=TRUE;


//DisplayWord(15,6,GetTickCount1ms());	
	//Mac1_EnableInt();

//	Mac1jjj--;
*/
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

	//Disp("MAC1_Tx_isr \n");
	RdValue = MISTA1;
	MISTA1 = RdValue&0xffff0000;

	if (RdValue & MISTA_TDU)
		TDU1_Flag = 1;
 
	if (RdValue & MISTA_TxBErr) {
		//FIFOTHD|=SWR;
		MCMDR1|=MCMDR_SWR;  //====    	
		//Mac1_Initialize();
	} else {
		//while(1) 
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

				// Clear Framedata pointer already used.
				pTxFDptr->Status2 = (UINT32)0x0;

				Mac1_FreeSendFramePt((char*)pTxFDptr->FrameDataPtr,5);

	       		gCTx1FDPtr = (UINT32)pTxFDptr->NextFrameDescriptor ;
				
			}
			else 
			{
				break;
				
				//1. Error Handling missing
				//2. Free buffer and adjust pointer
				//pTxFDptr->Status2 = (UINT32)0x0;
				//Mac1_FreeSendFramePt((char*)pTxFDptr->FrameDataPtr,5);
	       		//gCTx1FDPtr = (UINT32)pTxFDptr->NextFrameDescriptor ;
				//break;
			}

//			m_nWaitSendNumbers--;
			
//			if (gCTx1FDPtr == CTXDSA1) 
//			{
//				break;
//			}


			in++;
			if (in>MaxTxFrameDescriptors)
			{
				break;
			}
		} while(gCTx1FDPtr != CTXDSA1);
		
	}
	MISTA1 = 0xFFFF;



/*
while(1) 
{

	RdValue = MISTA1;
	MISTA1 = RdValue&0xffff0000;

	if (RdValue & MISTA_TDU)
		TDU1_Flag = 1;
 
	if (RdValue & MISTA_TxBErr) {
		//FIFOTHD|=SWR;
		MCMDR1|=MCMDR_SWR;  //====    	
		//Mac1_Initialize();
		break;
	} else {
		pTxFDptr = (sFrameDescriptor *) gCTx1FDPtr;

		Status = (pTxFDptr->Status2 >> 16) & 0xffff;
		if (Status & TXFD_TXCP) {
			Tx1Ready = 1;

			if (Status & ( TXFD_TXABT | TXFD_DEF | TXFD_PAU | TXFD_EXDEF |
					TXFD_NCS | TXFD_SQE | TXFD_LC | TXFD_TXHA) ) {
				;
			}

			// Clear Framedata pointer already used.
			pTxFDptr->Status2 = (UINT32)0x0;

			Mac1_FreeSendFramePt((char*)pTxFDptr->FrameDataPtr,5);

       		gCTx1FDPtr = (UINT32)pTxFDptr->NextFrameDescriptor ;

			if (gCTx1FDPtr == CTXDSA1) 
			{
				break;
			}
		}
		else 
		{
			break;
		}
		
	}
	MISTA1 = 0xFFFF;
	
	in++;
	if (in>MaxTxFrameDescriptors)
	{
		break;
	}
}
*/

//	m_pEthInfo1->portSilentTime = SystemTick;

#if defined(TEST_BO)
//_DisplayDWord(20,23,in);
 _DisplayDWord(40,28,Mac1jj1++);
#endif
 
}
/*
bool MAC1_isHwConnectionOk(void) 
{
	int status;
	
	status=Mii1StationRead(PHY_STATUS_REG, PHY1AD);
	if ((status & AN_LINKED) == 0) {
		m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
		m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
		return false;
	} else {
		m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED);
		return true;
	}

}*/
bool Mac1isHWConnectionOk_Simple(void)
{
/*
	int status;

	status=Mii1StationRead(PHY_STATUS_REG, PHY1AD);
	if ((status & AN_LINKED) == 0) {
		return false;
	} else {
		return true;
	}
*/
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

//#endif
int Mac1per=0;
int MAC1_ReceivePacket( BYTE *pbData, UINT16 *pwLength,WORD* iPosition)
{
	//UDP_PACKET  *udp;
	NETBUF      *buffer;
      
      //Disp("MCA1_ReceivePacket %d \n",*pwLength);
//	if (w1frame >= 1)
//		Mac1TxGo();
	
	//#if defined(GATEWAY)
	//if(!GW1link1Ok && !GW1link2Ok) {
	//#else	      

//DisplayWord(50,Mac1per,GetTickCount1ms());

/*
	if (!m_pEthInfo1->portStatus) {
		//link not OK. check it/every 1s
		if (IsTimeOut(GetTickCount(), m_lastTime1 ,200)) {		//1s =200*5
			MAC1_SetLinkOk(0);
			m_lastTime1 = GetTickCount();
		} 
		return 0;
	}
*/	 
/*	
	else {
		//link OK. check it/every 6s
		if (m_nJustLinkedin10s1>0) {
			//Force read negotiation data /every 1s. until 10s
			if (IsTimeOut(GetTickCount(), m_lastTime1 ,200)) {		//1s =200*5
				setLink1OK(0);
				m_lastTime1 = GetTickCount();
				m_nJustLinkedin10s1--;
			}	 
		} else 
		{
			if (IsTimeOut(GetTickCount(), m_lastTime1 ,1200)) {		//60s =1200*5
				if (isHWConnectionOk()) {
				} else {
					m_nJustLinkedin10s1 = 10;
					link1Ok = false;
					//#if defined(GATEWAY)
					//	GW1link1Ok = GW1link2Ok =false;
					//#endif
				}
				m_lastTime1 = GetTickCount();
			}
		}
	}
  */    
	if (_iqueue1_first == NULL) {
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
		mac1_rxFrameCount = 0;	
		mac1_flowcontrl = Mac1_SetRxFlowControl(mac1_rxFrameCount);
#endif
		return false;
	} else {
		Mac1_DisableInt();
//		C970_mutex_on(RECEIVE_MUTEX,0);
		buffer = (NETBUF*)_iqueue1_first;
		_iqueue1_first = _iqueue1_first->next;
		if (_iqueue1_first == NULL)  
			_iqueue1_last = NULL;
//		C970_mutex_off(RECEIVE_MUTEX,0);
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
		mac1_rxFrameCount--;
		mac1_flowcontrl = Mac1_SetRxFlowControl(mac1_rxFrameCount);
#endif	
		Mac1_EnableInt();

		*pwLength = buffer->len;
		
//DisplayWord(55,Mac1per,GetTickCount1ms());
//DisplayWord(60,Mac1per,buffer->len);
//Mac1per++;
//if (Mac1per>28)
//Mac1per = 0;


		//Mac1PutInTskQueue(buffer->packet, *pwLength,iPosition);
		//PutInTskQueue(buffer->packet, *pwLength,iPosition);
		PutInTskQueue(buffer->packet, *pwLength,iPosition, m_pEthInfo1->portSeq); 
		
			if (*iPosition == PACKET_WILL_DISCARD || *iPosition == PACKET_WILL_FORWARD) 
			{
				//Disp("MAC1_ReceivePacket false \n");
				return false;
			}
			else if (*iPosition == PACKET_UNDECIDED_YET) {
	#if defined(NUC970)
				m_memcpy(pbData, buffer->packet, *pwLength);
	#else
				memcpy(pbData, buffer->packet, *pwLength);
	#endif
				//Disp("MAC1_ReceivePacket true1 \n");
				return true;
			}
			else 
			{
				//PACKET_WILL_DO
				//Disp("MAC1_ReceivePacket true2 \n");
				return true;
			}
/*		
{
int i,j;
sFrameSendBuffer* pTxDataPtr = gWTx1DataStartPtr;
i=0;
j=0;


for(i = 0; i < MaxTxFrameDescriptors; i++) {
	if ((pTxDataPtr+i)->used != 0) 
	{
		j++;
	}
}
DisplayWord(25,0,w1frame);
DisplayWord(30,0,w1frame1);
DisplayWord(35,0,j);
}
*/
//DisplayWord(20,0,r1frame);
/*
		#if defined(DUAL_PORT)
			//Disp("MCA1_ReceivePacket %d %d\n",*pwLength,*iPosition);
			LinkPort=1;
			Debug_Length=*pwLength;
			if(LinkPort != PreLinkPort)
			{
				//memcpy((char*)&hmiId.ipAddr,MyIpAddr,4);
				m_memcpy(PreMyIpAddr, MyIpAddr, 4);	//backup Mac0 ip
				m_memcpy(MyIpAddr, MyIp1Addr, 4);	//put mac1 ip to use
				
				m_memcpy(PreServerIpAddr, ServerIpAddr, 4);	//backup Mac0 Server Ip
				m_memcpy(ServerIpAddr, Server1IpAddr, 4);	//put mac1 Server ip to use
				
				m_memcpy(PrePriDnsIpAddr, PriDnsIpAddr, 4);	//backup Mac0 PriDns Ip
				m_memcpy(PriDnsIpAddr, PriDns1IpAddr, 4);	//put mac1 PriDns ip to use
				
				m_memcpy(PreAltDnsIpAddr, AltDnsIpAddr, 4);	//backup Mac0 AltDnsIpAddr Ip
				m_memcpy(AltDnsIpAddr, AltDns1IpAddr, 4);	//put mac1 AltDnsIpAddr ip to use

				m_memcpy(PreDefDnsIpAddr, DefDnsIpAddr, 4);	//backup Mac0 DefDnsIpAddr Ip
				m_memcpy(DefDnsIpAddr, DefDns1IpAddr, 4);	//put mac1 DefDnsIpAddr ip to use
				
				m_memcpy(PreNetMask, NetMask, 4);	//backup Mac0 NetMask Ip
				m_memcpy(NetMask, Net1Mask, 4);	//put mac1 NetMask ip to use
				
				m_memcpy(PreMyEthAddr, MyEthAddr, 6);	//backup Mac0 Mac address
				m_memcpy(MyEthAddr, MyEth1Addr, 6);	//put mac1 Mac address to use
				
				
				PreLinkPort=LinkPort;
			}
		#endif
*/
		return true;	   
	}   
}


void MAC1_Rx_isr(void)
{
	sFrameDescriptor 	*pRxFDptr ;
	UINT32 				RxStatus ;
	UINT32 				CRxPtr;
	UINT32 				RdValue;
	//NETBUF   			*netbuf;
	int ret;
int i=0;

	RdValue = MISTA1;
	MISTA1 = RdValue&0x0000ffff;

#if defined(TEST_BO)
_DisplayDWord(30,26,Mac1jj1++);
#endif

 	if (RdValue & MISTA_RxBErr)  {
		MCMDR1|=MCMDR_SWR;  //====
		//Disp("~~~ MISTA_RxBErr\n");
		//Mac1_Initialize();
//_DisplayDWord(0,2,1);
	} else {
		if (RdValue & (MISTA_CFR | MISTA_CRCE | MISTA_PTLE | MISTA_ALIE | MISTA_RP)) {
			//Disp("L2081\n");
			// DisplayString(0,4,"Rx error, status:%x\n",RdValue) ;
//_DisplayDWord(0,2,2);
		} else {
			// Get current frame descriptor
			CRxPtr = CRXDSA1 ;
//_DisplayDWord(5,Mac1jj1,1);
			//Disp("~~~ CRxPtr=%08xH\n",CRxPtr);
			do {
				// Get Rx Frame Descriptor
				pRxFDptr = (sFrameDescriptor *)gCRx1FDPtr;

				if ((pRxFDptr->Status1|RXfOwnership_CPU)==RXfOwnership_CPU) {
					RxStatus = (pRxFDptr->Status1 >> 16) & 0xffff;

					// If Rx frame is good, then process received frame
					if (RxStatus & RXFD_RXGD)  {
						//ret = icmpIsrSvr((char*)pRxFDptr->FrameDataPtr, pRxFDptr->Status1 & 0xffff);
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
//_DisplayDWord(35,Mac1jj1,GetTickCount1ms());
//_DisplayDWord(45,Mac1jj1,_iqueue1_last->len);
						}
//_DisplayDWord(15,Mac1jj1,gCRx1FDPtr);	
					} else {
//_DisplayDWord(15,Mac1jj1,gCRx1FDPtr);	
					}
       			} else {
//_DisplayDWord(0,2,3);
					break;
				}

      			// Change ownership to DMA for next use
				pRxFDptr->Status1 = RXfOwnership_DMA;
	
   	  			// Get Next Frame Descriptor pointer to process
				gCRx1FDPtr = (UINT32)(pRxFDptr->NextFrameDescriptor) ;
#if defined(TCP_SPEEDUP) && ( defined(NUC970) || defined(W90P950) )
				mac1_rxFrameCount++;
#endif

//i++;
//_DisplayDWord(0,Mac1jj1,i);	
//if (gCRx1FDPtr != CRxPtr)
//{
//_DisplayDWord(25,Mac1jj1,gCRx1FDPtr);	
//}
//Mac1jj1++;
//if (Mac1jj1>28)
//Mac1jj1=1;


     		
			} while (CRxPtr != gCRx1FDPtr);
		}

		if (RdValue & MISTA_RDU)
			RSDR1 = 0;
	}
	
	
//	m_pEthInfo1->portSilentTime = 0; //SystemTick;
	if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_OK) == ETH_PORT_STATUS_OK)
	{
		m_pEthInfo1->portSilentTime = 0; //SystemTick;
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
	   	//Disp("RdValue =%xH \n",RdValue);
	 	//RdValue = (RdValue | 0xf0);
	   	//MiiStationWrite(PHY_INT_CNTL_REG, PHYAD, RdValue);
		if (RdValue&1)
		{
			//bit0 ==1, link_up, plug in
			m_pEthInfo1->bUseInt = 1;		//interrupt working
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) != ETH_PORT_STATUS_CABLE_LINKED) 
			{
		 		m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED );
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	//BOOTP_NULL;
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
			//link_down
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) == ETH_PORT_STATUS_CABLE_LINKED) 
			{
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	//BOOTP_NULL;
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
			//link partner acknoledge, not use
		//	_DisplayDWord(20,5,in1++);	
		}
		else if (RdValue&0x80) 
		{
			//jabber, not use 
		//	_DisplayDWord(30,5,in1++);	
		}
   	}
   	else if(PHY1Chip == CHIP_YT8512C)
   	{
   		RdValue = Mii1StationRead(YT8512_PHY_INT_STATUS_REG, PHY1AD);
		if (RdValue & LINK_SUCCEED_INT)
		{
			//bit0 ==1, link_up, plug in
			m_pEthInfo1->bUseInt = 1;		//interrupt working
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) != ETH_PORT_STATUS_CABLE_LINKED) 
			{
		 		m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED );
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	//BOOTP_NULL;
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
			//link_down
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) == ETH_PORT_STATUS_CABLE_LINKED) 
			{
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	//BOOTP_NULL;
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
		RdValue = Mii1StationRead(IP101_PHY_INT_CNTL_REG, PHYAD);	//need read once to clear 
	   	RdValue = Mii1StationRead(IP101_PHY_INT_STATUS_REG, PHYAD);
	   	Disp("IP101 INT RdValue =%xH \n",RdValue);

	   	if((RdValue & LINK_SUCCEED_INT) == LINK_SUCCEED_INT)
		{
			//bit0 ==1, link_up, plug in
			m_pEthInfo1->bUseInt = 1;		//interrupt working
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) != ETH_PORT_STATUS_CABLE_LINKED) 
			{
		 		m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus | ETH_PORT_STATUS_CABLE_LINKED );
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	//BOOTP_NULL;
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
			//link_down
			if ((m_pEthInfo1->portStatus&ETH_PORT_STATUS_CABLE_LINKED) == ETH_PORT_STATUS_CABLE_LINKED) 
			{
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_PROT_GETIPED);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_NIGOTIATION_OK);
				m_pEthInfo1->portStatus = (m_pEthInfo1->portStatus & ~ETH_PORT_STATUS_CABLE_LINKED);
				m_pEthInfo1->dealMsg = 1;
				m_pEthInfo1->internalStatus = 0;	//BOOTP_NULL;
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
    //Disp("GPIOF_ISR =%xH \n",inpw(REG_GPIOF_ISR));
    outpw(REG_GPIOF_ISR, (1<<13));	//p626
//    if(gpioCfg.EINTIRQCallback[1] != NULL) {
//        gpioCfg.EINTIRQCallback[1](reg, gpioCfg.EINTIRQUserData[1]);
//    }
    outpw(REG_AIC_SCCR, mask);   /* Clear interrupt */
}

#endif


