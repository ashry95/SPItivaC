/*
 * spi.c
 *
 *  Created on: Aug 19, 2019
 *      Author: Muhammad.Elzeiny
 */
/*================================================*
 * DEFINES
 * ==============================================*/
#define SPI_PRIVATE_CONFIG
#define Spi_NUMBER_OF_HW_UINTS       (4)
#define MAX_SPI_CLK_DIV               (254)
#define EVEN_NUM_MASK                (2)
/*================================================*
 * INCLUDES
 * ==============================================*/
#include "../../utils/Bit_Math.h"
#include "../../utils/STD_Types.h"
#include "../mcu_hw.h"

#include "spi_types.h"
#include "spi.h"

/*================================================*
 * LOCAL FUNCTIONS
 * ==============================================*/
static inline void Spi_SetBitRate(Spi_ChannelType SpiNum ,uint32 BitRate);
static inline void Spi_IntRoutine(Spi_ChannelType channel);
static inline void Spi_writeTxFifo(Spi_ChannelType Channel);
/*================================================*
 * EXTERNS
 * ==============================================*/
extern const Spi_ConfigType Spi_CfgArr[];

/*================================================*
 * LOCAL VARIABLES
 * ==============================================*/
static const uint32 Spi_BaseAddrArr[] = {SPI0_BASE_ADDR, SPI1_BASE_ADDR, SPI2_BASE_ADDR, SPI3_BASE_ADDR};
static Spi_ChannelParamType Spi_ChannelParam[Spi_NUMBER_OF_HW_UINTS];


uint32 tempBR;
/*================================================*
 * FUNCTIONS DEFINTIONS
 * ==============================================*/

/*=====================================================================================
 * NAME:        Spi_init
 * DESCRIBTION: Initial Configured Spi Channel with corresponding configuration Table
 * ===================================================================================*/
void Spi_init(void)
{
    uint8 i;

    Spi_ChannelType Channel;

    for (i = 0; i < Spi_NUM_OF_ACTIVATED_UNITS; ++i)
    {
        Channel = Spi_CfgArr[i].Spi_Channel;
        /* Disable Spi to init configuration */
        SSICR1(Spi_BaseAddrArr[Channel]).B.SSE = STD_low;

        /* Select whether the SSI is a master or slave  */
        SSICR1(Spi_BaseAddrArr[Channel]).B.MS = Spi_CfgArr[Channel].CFG_OprMode;

        /* configure loop back test */
        SSICR1(Spi_BaseAddrArr[Channel]).B.LBM = Spi_CfgArr[Channel].CFG_LoopBack;

        /* configure Tx Interrupt Mode  */
        SSICR1(Spi_BaseAddrArr[Channel]).B.EOT = Spi_CfgArr[Channel].CFG_TxIntMode;

        /* Configure the SSI clock source by writing to the SSICC register. */
        SSICC(Spi_BaseAddrArr[Channel]) = Spi_CfgArr[Channel].CFG_ClockSource;

        /*(4) Set Bit Rate with clock rate and clock prescaler */
        Spi_SetBitRate((Spi_ChannelType)Channel, Spi_CfgArr[i].CFG_BitRate);

        /* Configure Clock phase */
        SSICR0(Spi_BaseAddrArr[Channel]).B.SPH = Spi_CfgArr[Channel].CFG_DataCaptureClockEdge;

        /* Configure Clock  polarity */
        SSICR0(Spi_BaseAddrArr[Channel]).B.SPO = Spi_CfgArr[Channel].CFG_IdleCLockState;

        /* Configure Protocol mode  */
        SSICR0(Spi_BaseAddrArr[Channel]).B.FRF = Spi_CfgArr[Channel].CFG_FrameFormat;

        /* Configure Data Size */
        SSICR0(Spi_BaseAddrArr[Channel]).B.DSS = Spi_CfgArr[Channel].CFG_DataSize;

        /* Enable Spi to init configuration */
        SSICR1(Spi_BaseAddrArr[Channel]).B.SSE = STD_high;

        /* change SPi Unit status to idle*/
        Spi_ChannelParam[Channel].Status = SPI_IDLE;
    }
}
/*=================================================================================
 * NAME:        Spi_enInterrupt
 * DESCRIBTION: Enable all configured interrupts for given channel
 * =================================================================================*/
void Spi_enInterrupt(Spi_ChannelType Channel)
{
    /* Enable all configured interrupts for given channel */
    if(Spi_CfgArr[Channel].CFG_Interrupt_RxOverRun)
    {
        SSIIM(Spi_BaseAddrArr[Channel]).B.RORIM = STD_low;
    }
    if(Spi_CfgArr[Channel].CFG_Interrupt_RxTimeout)
    {
        SSIIM(Spi_BaseAddrArr[Channel]).B.RTIM  = STD_low;
    }
    if(Spi_CfgArr[Channel].CFG_Interrupt_Rxc)
    {
        SSIIM(Spi_BaseAddrArr[Channel]).B.RXIM  = STD_low;
    }
    if(Spi_CfgArr[Channel].CFG_Interrupt_Txc)
    {
        SSIIM(Spi_BaseAddrArr[Channel]).B.TXIM  = STD_low;
    }

}
/*=================================================================================
 * NAME:        Spi_diInterrupt
 * DESCRIBTION: Disable all interrupts in corresponding channel
 * =================================================================================*/
void Spi_diInterrupt(Spi_ChannelType Channel)
{
    /* Disable all interrupt for this Channel*/
    SSIIM(Spi_BaseAddrArr[Channel]).B.RORIM = STD_high;
    SSIIM(Spi_BaseAddrArr[Channel]).B.RTIM  = STD_high;
    SSIIM(Spi_BaseAddrArr[Channel]).B.RXIM  = STD_high;
    SSIIM(Spi_BaseAddrArr[Channel]).B.TXIM  = STD_high;
}

/*=================================================================================
 * NAME:        Spi_GetStatus
 * DESCRIBTION: get status of given channel (Idle/Busy)
 * =================================================================================*/
Spi_StatusType Spi_GetStatus(Spi_ChannelType Channel)
{
    return Spi_ChannelParam[Channel].Status;
}

/*=================================================================================
 * NAME:        Spi_WriteIB
 * DESCRIBTION: The Function shall save the pointed data to the internal buffer
 * =================================================================================*/
Std_ReturnType Spi_WriteIB( Spi_ChannelType Channel, const uint16* DataBufferPtr,uint8 DataBufferSize)
{
    Std_ReturnType ret = E_OK;
    uint8 i;
    /* Save the given data to the internal buffer*/
    if(DataBufferSize <= Spi_TX_BUFFER_SIZE)
    {
        for(i = 0 ; i < DataBufferSize ; i++)
        {
            Spi_ChannelParam[Channel].TxBuffer[i] = DataBufferPtr[i];
        }
        Spi_ChannelParam[Channel].TxBufferIndex = 0;

    }
    else
    {
        ret = E_NOT_OK;
    }



    return ret;
}

/* =================================================================================
 * NAME:          Spi_ReadIB
 * DESCRIBTION:   The function Spi_ReadIB provides the service
 * for reading synchronously one or more data from an IB(Rx_Buffer)
 * =================================================================================*/
Std_ReturnType Spi_ReadIB( Spi_ChannelType Channel, uint16* DataBufferPtr, uint8* DataBufferSizePtr )
{
    uint8 i;
    Std_ReturnType ret = E_OK;
    /* Check if there is any Rx Data*/
    if(Spi_ChannelParam[Channel].RxBufferIndex != 0)
    {
        /*DO: Copy Data From RXBuffer to DataBufferPtr Location */
        for(i = 0 ; (Spi_ChannelParam[Channel].RxBuffer[i]) != '\0' ; i++ )
        {
            DataBufferPtr[i] = Spi_ChannelParam[Channel].RxBuffer[i];
        }

        /*DO: Write into DataBufferSizePtr location the last RxBufferIndex*/
        *DataBufferSizePtr = i;

        /* Reset RxBufferIndex */
        Spi_ChannelParam[Channel].RxBufferIndex = 0;
    }
    else
    {
        /* Return Not Ok if There is no Received Data */
        ret = E_NOT_OK;
    }
    return ret;
}

/*=================================================================================
 * NAME:        Spi_AsyncTransmit
 * DESCRIBTION: start transmission of Tx buffer asynchronously (Interrupt based)
 * =================================================================================*/
Std_ReturnType Spi_AsyncTransmit(Spi_ChannelType Channel)
{
    Std_ReturnType ret = E_OK;
    if(Spi_ChannelParam[Channel].Status == SPI_IDLE)
    {
       SSIDR(Spi_BaseAddrArr[Channel]) = Spi_ChannelParam[Channel].TxBuffer[Spi_ChannelParam[Channel].TxBufferIndex];
       Spi_ChannelParam[Channel].TxBufferIndex++;
    }
    else
    {
        ret = E_NOT_OK;
    }
    return ret;
}
/*=================================================================================
 * NAME:        Spi_SyncTransmit
 * DESCRIBTION: start transmission of Tx buffer synchronously (Polling based)
 * =================================================================================*/
Std_ReturnType Spi_SyncTransmit(Spi_ChannelType Channel)
{
    Std_ReturnType ret = E_OK;

    if(Spi_ChannelParam[Channel].Status == SPI_IDLE)
    {
        /* set channel status to be Busy*/
        Spi_ChannelParam[Channel].Status = SPI_BUSY;

        /*loop for TxBuffer */
        for (Spi_ChannelParam[Channel].TxBufferIndex = 0;
                Spi_ChannelParam[Channel].TxBufferIndex < Spi_ChannelParam[Channel].TxMsgSize ;
                Spi_ChannelParam[Channel].TxBufferIndex++)
        {
            /* send TxBuffer Frame By Frame */
            SSIDR(Spi_BaseAddrArr[Channel]) = Spi_ChannelParam[Channel].TxBuffer[Spi_ChannelParam[Channel].TxBufferIndex];
            /* wait for Data to be transmitted */
            while(SSISR(Spi_BaseAddrArr[Channel]).B.BSY == STD_high)
            {
                ;
            }
        }
        /*Reset Channel TxBufferIndex */
        Spi_ChannelParam[Channel].TxBufferIndex = 0;
        /* set channel status to be IDLE*/
        Spi_ChannelParam[Channel].Status = SPI_IDLE;
    }
    else
    {
        ret = E_NOT_OK;
    }
    return ret;
}
/*=================================================================================
 * NAME:        Spi_writeTxFifo
 * DESCRIBTION:
 * =================================================================================*/
static inline void Spi_writeTxFifo(Spi_ChannelType Channel)
{
    /* set channel status to be Busy*/
    Spi_ChannelParam[Channel].Status = SPI_BUSY;

    /*loop for TxBuffer */
    for (;
            Spi_ChannelParam[Channel].TxBufferIndex < Spi_ChannelParam[Channel].TxMsgSize  &&
            SSISR(Spi_BaseAddrArr[Channel]).B.TNF == STD_high ;/*Check TxFIFO is not full*/
            Spi_ChannelParam[Channel].TxBufferIndex++)
    {
        /* send TxBuffer Frame By Frame */
        SSIDR(Spi_BaseAddrArr[Channel]) = Spi_ChannelParam[Channel].TxBuffer[Spi_ChannelParam[Channel].TxBufferIndex];

        /* No need to wait as every Frame loaded in FIFO.
         * If FIFO is Full Interrupt will fire and continue to transmit the message */
    }
    if(Spi_ChannelParam[Channel].TxBufferIndex >= Spi_ChannelParam[Channel].TxMsgSize)
    {
        Spi_diInterrupt(Spi_Channel0);
        /*ResetTxBufferIndex */
        Spi_ChannelParam[Channel].TxBufferIndex = 0;
        /* set channel status to be IDLE*/
        Spi_ChannelParam[Channel].Status = SPI_IDLE;
    }
}
/*=================================================================================
 * NAME:        Spi_SetBitRate
 * DESCRIBTION: configure bit rate
 * =================================================================================*/
static inline void Spi_SetBitRate(Spi_ChannelType Channel ,uint32 u32SSInClk)
{
    /* Calculate CPSDVSR and SCR in SSICPSR and SSICR0 Register  */
    tempBR = u32SYS_CLOCK_Hz/(Spi_CfgArr[Channel].CFG_BitRate);
    if((tempBR <= MAX_SPI_CLK_DIV) && (!(tempBR % EVEN_NUM_MASK)))
    {

        SSICPSR(Spi_BaseAddrArr[Channel]) = tempBR;
        SSICR0(Spi_BaseAddrArr[Channel]).B.SCR = STD_low;
    }
    else
    {
        SSICR0(Spi_BaseAddrArr[Channel]).B.SCR = tempBR - STD_high;
        SSICPSR(Spi_BaseAddrArr[Channel]) = STD_high;
    }

}
/*=================================================================================
 * NAME:        Spi_IntRoutine
 * DESCRIBTION: ISR for all SPI Modules Interrupts
 * =================================================================================*/
static inline void Spi_IntRoutine(Spi_ChannelType Channel)
{
    if(SSIMIS(Spi_BaseAddrArr[Channel]).B.TXMIS == STD_high)
    {


    }

    if(SSIMIS(Spi_BaseAddrArr[Channel]).B.RXMIS == STD_high)
    {


    }

    if(SSIMIS(Spi_BaseAddrArr[Channel]).B.RTMIS == STD_high)
    {


    }

    if(SSIMIS(Spi_BaseAddrArr[Channel]).B.RORMIS == STD_high)
    {


    }

}

/*=================================================================================
 * NAME:        SPI0_IntHandler
 * DESCRIBTION: SPI_0 ISR
 * =================================================================================*/
void SPI0_IntHandler(void)
{
    Spi_IntRoutine(Spi_Channel0);
}
/*=================================================================================
 * NAME:        SPI1_IntHandler
 * DESCRIBTION: SPI_1 ISR
 * =================================================================================*/
void SPI1_IntHandler(void)
{
    Spi_IntRoutine(Spi_Channel1);
}
/*=================================================================================
 * NAME:        SPI2_IntHandler
 * DESCRIBTION: SPI_2 ISR
 * =================================================================================*/
void SPI2_IntHandler(void)
{
    Spi_IntRoutine(Spi_Channel2);
}
/*=================================================================================
 * NAME:        SPI3_IntHandler
 * DESCRIBTION: SPI_3 ISR
 * =================================================================================*/
void SPI3_IntHandler(void)
{
    Spi_IntRoutine(Spi_Channel3);
}
