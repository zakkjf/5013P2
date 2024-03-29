//
/*****************************************************************************
 * Adapted from TI TivaWare User Guide, page 193 by Zach Farmer
 *
 * Redistribution, modification or use of this software in source or binary
 * forms is permitted by Texas Instruments as part of their product support
 * for the Tiva TMC1294XL Cortex M4F and other microcontrollers in
 * TI's ecosystem.
 *
*****************************************************************************/
/**
 * @file eth.c
 * @brief Ethernet MAC PHY HAL for the emac module on TMC1294XL Launchpad.
 * Requires TI driverlib.
 *
 * This is a short HAL driver for TI's emac module.
 *
 * @author Texas Instruments, modified by Zach Farmer
 * @date April 14 2018
 * @version 1.0
 *
 */
 
#include <stdint.h>
#include <stdbool.h>

// TivaWare includes
#include "driverlib/sysctl.h"
#include "driverlib/debug.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"
#include "driverlib/emac.h"
#include "driverlib/flash.h"
#include "driverlib/interrupt.h"
#include "inc/hw_memmap.h"
#include "inc/hw_emac.h"
#include "inc/hw_ints.h"
//*****************************************************************************
//
// Ethernet DMA descriptors.
//
// The MAC hardware needs a minimum of 3 receive descriptors to operate. The
// number used will be application-dependent and should be tuned for best
// performance.
//
//*****************************************************************************
#define NUM_TX_DESCRIPTORS 3
#define NUM_RX_DESCRIPTORS 3
tEMACDMADescriptor g_psRxDescriptor[NUM_TX_DESCRIPTORS];
tEMACDMADescriptor g_psTxDescriptor[NUM_RX_DESCRIPTORS];
uint32_t g_ui32RxDescIndex;
uint32_t g_ui32TxDescIndex;
//*****************************************************************************
//
// Transmit and receive buffers. These will typically be allocated within your
// network stack somewhere.
//
//*****************************************************************************
#define RX_BUFFER_SIZE 1536
uint8_t g_ppui8RxBuffer[NUM_RX_DESCRIPTORS][RX_BUFFER_SIZE];
//*****************************************************************************
//
// Read a packet from the DMA receive buffer and return the number of bytes
// read.
//
//*****************************************************************************
int32_t ProcessReceivedPacket(void)
{
    int_fast32_t i32FrameLen;
	//
	// By default, we assume we got a bad frame.
	//
    i32FrameLen = 0;
	//
	// Make sure that we own the receive descriptor.
	//
    if(!(g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus & DES0_RX_CTRL_OWN))
    {
		//
		// We own the receive descriptor so check to see if it contains a valid
		// frame.
		//
        if(!(g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus &
                DES0_RX_STAT_ERR))
        {
			//
			// We have a valid frame. First check that the "last descriptor"
			// flag is set. We sized the receive buffer such that it can
			// always hold a valid frame so this flag should never be clear at
			// this point but...
			//
            if(g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus &
                    DES0_RX_STAT_LAST_DESC)
            {
				//
				// What size is the received frame?
				//
                i32FrameLen =
                    ((g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus &
                      DES0_RX_STAT_FRAME_LENGTH_M) >>
                     DES0_RX_STAT_FRAME_LENGTH_S);
					//
					// Pass the received buffer up to the application to handle.
					//
               // ApplicationProcessFrame(i32FrameLen,
                //                        g_psRxDescriptor[g_ui32RxDescIndex].pvBuffer1);
            }
        }
		//
		// Now that we are finished dealing with this descriptor, hand
		// it back to the hardware. Note that we assume
		// ApplicationProcessFrame() is finished with the buffer at this point
		// so it is safe to reuse.
		//
        g_psRxDescriptor[g_ui32RxDescIndex].ui32CtrlStatus =
            DES0_RX_CTRL_OWN;
		//
		// Move on to the next descriptor in the chain.
		//
        g_ui32RxDescIndex++;
        if(g_ui32RxDescIndex == NUM_RX_DESCRIPTORS)
        {
            g_ui32RxDescIndex = 0;
        }
    }
	//
	// Return the Frame Length
	//
    return(i32FrameLen);
}
//*****************************************************************************
//
// The interrupt handler for the Ethernet interrupt.
//
//*****************************************************************************
void EthernetIntHandler(void)
{
    uint32_t ui32Temp;
	//
	// Read and Clear the interrupt.
	//
    ui32Temp = EMACIntStatus(EMAC0_BASE, true);
    EMACIntClear(EMAC0_BASE, ui32Temp);
	//
	// Check to see if an RX Interrupt has occurred.
	//
    if(ui32Temp & EMAC_INT_RECEIVE)
    {
		//
		// Indicate that a packet has been received.
		//
        ProcessReceivedPacket();
    }
}
//*****************************************************************************
//
// Transmit a packet from the supplied buffer. This function would be called
// directly by the application. pui8Buf points to the Ethernet frame to send
// and i32BufLen contains the number of bytes in the frame.
//
//*****************************************************************************
int32_t PacketTransmit(uint8_t *pui8Buf, int32_t i32BufLen)
{
	//
	// Wait for the transmit descriptor to free up.
	//
    while(g_psTxDescriptor[g_ui32TxDescIndex].ui32CtrlStatus & DES0_TX_CTRL_OWN)
    {
	//
	// Spin and waste time.
	//
    }
	//
	// Move to the next descriptor.
	//
    g_ui32TxDescIndex++;
    if(g_ui32TxDescIndex == NUM_TX_DESCRIPTORS)
    {
        g_ui32TxDescIndex = 0;
    }
	//
	// Fill in the packet size and pointer, and tell the transmitter to start
	// work.
	//
    g_psTxDescriptor[g_ui32TxDescIndex].ui32Count = (uint32_t)i32BufLen;
    g_psTxDescriptor[g_ui32TxDescIndex].pvBuffer1 = pui8Buf;
    g_psTxDescriptor[g_ui32TxDescIndex].ui32CtrlStatus =
        (DES0_TX_CTRL_LAST_SEG | DES0_TX_CTRL_FIRST_SEG |
         DES0_TX_CTRL_INTERRUPT | DES0_TX_CTRL_IP_ALL_CKHSUMS |
         DES0_TX_CTRL_CHAINED | DES0_TX_CTRL_OWN);
	//
	// Tell the DMA to reacquire the descriptor now that we'e filled it in.
	// This call is benign if the transmitter hasn't stalled and checking
	// the state takes longer than just issuing a poll demand so we do this
	// for all packets.
	//
    EMACTxDMAPollDemand(EMAC0_BASE);
	//
	// Return the number of bytes sent.
	//
    return(i32BufLen);
}
//*****************************************************************************
//
// Initialize the transmit and receive DMA descriptors.
//
//*****************************************************************************
void InitDescriptors(uint32_t ui32Base)
{
    uint32_t ui32Loop;
	//
	// Initialize each of the transmit descriptors. Note that we leave the
	// buffer pointer and size empty and the OWN bit clear here since we have
	// not set up any transmissions yet.
	//
    for(ui32Loop = 0; ui32Loop < NUM_TX_DESCRIPTORS; ui32Loop++)
    {
        g_psTxDescriptor[ui32Loop].ui32Count = DES1_TX_CTRL_SADDR_INSERT;
        g_psTxDescriptor[ui32Loop].DES3.pLink =
            (ui32Loop == (NUM_TX_DESCRIPTORS - 1)) ?
            g_psTxDescriptor : &g_psTxDescriptor[ui32Loop + 1];
        g_psTxDescriptor[ui32Loop].ui32CtrlStatus =
            (DES0_TX_CTRL_LAST_SEG | DES0_TX_CTRL_FIRST_SEG |
             DES0_TX_CTRL_INTERRUPT | DES0_TX_CTRL_CHAINED |
             DES0_TX_CTRL_IP_ALL_CKHSUMS);
    }
	//
	// Initialize each of the receive descriptors. We clear the OWN bit here
	// to make sure that the receiver doesn't start writing anything
	// immediately.
	//
    for(ui32Loop = 0; ui32Loop < NUM_RX_DESCRIPTORS; ui32Loop++)
    {
        g_psRxDescriptor[ui32Loop].ui32CtrlStatus = 0;
        g_psRxDescriptor[ui32Loop].ui32Count =
            (DES1_RX_CTRL_CHAINED |(RX_BUFFER_SIZE << DES1_RX_CTRL_BUFF1_SIZE_S));
        g_psRxDescriptor[ui32Loop].pvBuffer1 = g_ppui8RxBuffer[ui32Loop];
        g_psRxDescriptor[ui32Loop].DES3.pLink =
            (ui32Loop == (NUM_RX_DESCRIPTORS - 1)) ?
            g_psRxDescriptor : &g_psRxDescriptor[ui32Loop + 1];
    }
	//
	// Set the descriptor pointers in the hardware.
	//
    EMACRxDMADescriptorListSet(ui32Base, g_psRxDescriptor);
    EMACTxDMADescriptorListSet(ui32Base, g_psTxDescriptor);
	//
	// Start from the beginning of both descriptor chains. We actually set
	// the transmit descriptor index to the last descriptor in the chain
	// since it will be incremented before use and this means the first
	// transmission we perform will use the correct descriptor.
	//
    g_ui32RxDescIndex = 0;
    g_ui32TxDescIndex = NUM_TX_DESCRIPTORS - 1;
}
//*****************************************************************************
//
// This example demonstrates the use of the Ethernet Controller.
//
//*****************************************************************************
uint32_t initEthernet(uint32_t ui32SysClock)
{
    uint32_t ui32User0, ui32User1, ui32Loop;
    uint8_t ui8PHYAddr;
    uint8_t pui8MACAddr[6];

    /* handled externally
	//
	// Run from the PLL at 120 MHz.
	//
	ui32SysClock = SysCtlClockFreqSet((SYSCTL_XTAL_25MHZ |
                                       SYSCTL_OSC_MAIN |
                                       SYSCTL_USE_PLL |
                                       SYSCTL_CFG_VCO_480), 120000000);
									   */
	//
	// Read the MAC address from the user registers.
	//
    FlashUserGet(&ui32User0, &ui32User1);
    if((ui32User0 == 0xffffffff) || (ui32User1 == 0xffffffff))
    {
		//
		// We should never get here. This is an error if the MAC address has
		// not been programmed into the device. Exit the program.
		//
        while(1)
        {
        }
    }
	//
	// Convert the 24/24 split MAC address from NV ram into a 32/16 split MAC
	// address needed to program the hardware registers, then program the MAC
	// address into the Ethernet Controller registers.
	//
    pui8MACAddr[0] = ((ui32User0 >> 0) & 0xff);
    pui8MACAddr[1] = ((ui32User0 >> 8) & 0xff);
    pui8MACAddr[2] = ((ui32User0 >> 16) & 0xff);
    pui8MACAddr[3] = ((ui32User1 >> 0) & 0xff);
    pui8MACAddr[4] = ((ui32User1 >> 8) & 0xff);
    pui8MACAddr[5] = ((ui32User1 >> 16) & 0xff);
	//
	// Enable and reset the Ethernet modules.
	//
    SysCtlPeripheralEnable(SYSCTL_PERIPH_EMAC0);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_EPHY0);
    SysCtlPeripheralReset(SYSCTL_PERIPH_EMAC0);
    SysCtlPeripheralReset(SYSCTL_PERIPH_EPHY0);
	//
	// Wait for the MAC to be ready.
	//
    while(!SysCtlPeripheralReady(SYSCTL_PERIPH_EMAC0))
    {
    }
	//
	// Configure for use with the internal PHY.
	//
    ui8PHYAddr = 0;
    EMACPHYConfigSet(EMAC0_BASE,
                     (EMAC_PHY_TYPE_INTERNAL |
                      EMAC_PHY_INT_MDIX_EN |
                      EMAC_PHY_AN_100B_T_FULL_DUPLEX));
	//
	// Reset the MAC to latch the PHY configuration.
	//
    EMACReset(EMAC0_BASE);
	//
	// Initialize the MAC and set the DMA mode.
	//
    EMACInit(EMAC0_BASE, ui32SysClock,
             EMAC_BCONFIG_MIXED_BURST | EMAC_BCONFIG_PRIORITY_FIXED, 4, 4,
             0);
	//
	// Set MAC configuration options.
	//
    EMACConfigSet(EMAC0_BASE,
                  (EMAC_CONFIG_FULL_DUPLEX |
                   EMAC_CONFIG_CHECKSUM_OFFLOAD |
                   EMAC_CONFIG_7BYTE_PREAMBLE |
                   EMAC_CONFIG_IF_GAP_96BITS |
                   EMAC_CONFIG_USE_MACADDR0 |
                   EMAC_CONFIG_SA_FROM_DESCRIPTOR |
                   EMAC_CONFIG_BO_LIMIT_1024),
                  (EMAC_MODE_RX_STORE_FORWARD |
                   EMAC_MODE_TX_STORE_FORWARD |
                   EMAC_MODE_TX_THRESHOLD_64_BYTES |
                   EMAC_MODE_RX_THRESHOLD_64_BYTES), 0);
	//
	// Initialize the Ethernet DMA descriptors.
	//
    InitDescriptors(EMAC0_BASE);
	//
	// Program the hardware with its MAC address (for filtering).
	//
    EMACAddrSet(EMAC0_BASE, 0, pui8MACAddr);
	//
	// Wait for the link to become active.
	//
    while((EMACPHYRead(EMAC0_BASE, ui8PHYAddr, EPHY_BMSR) &
            EPHY_BMSR_LINKSTAT) == 0)
    {
    }
	//
	// Set MAC filtering options. We receive all broadcast and multicast
	// packets along with those addressed specifically for us.
	//
    EMACFrameFilterSet(EMAC0_BASE, (EMAC_FRMFILTER_SADDR |
                                    EMAC_FRMFILTER_PASS_MULTICAST |
                                    EMAC_FRMFILTER_PASS_NO_CTRL));
	//
	// Clear any pending interrupts.
	//
    EMACIntClear(EMAC0_BASE, EMACIntStatus(EMAC0_BASE, false));
	//
	// Mark the receive descriptors as available to the DMA to start
	// the receive processing.
	//
    for(ui32Loop = 0; ui32Loop < NUM_RX_DESCRIPTORS; ui32Loop++)
    {
        g_psRxDescriptor[ui32Loop].ui32CtrlStatus |= DES0_RX_CTRL_OWN;
    }
	//
	// Enable the Ethernet MAC transmitter and receiver.
	//
    EMACTxEnable(EMAC0_BASE);
    EMACRxEnable(EMAC0_BASE);
	//
	// Enable the Ethernet interrupt.
	//
    IntEnable(INT_EMAC0);
	//
	// Enable the Ethernet RX Packet interrupt source.
	//
    EMACIntEnable(EMAC0_BASE, EMAC_INT_RECEIVE);
	/*
	//
	// Application main loop continues....
	//
    uint64_t i = 0;
    uint8_t poo[49] = "What we've got here is a failure to communicate.";
    //for(i=0;i<32;i++)
    //{
   //     poo[i] = 0xFA;
   //a }

    while(1)
    {
        for(i=0;i<20000000UL;i++);
        PacketTransmit(poo, 49);

    }
	*/
	return 0;
}
