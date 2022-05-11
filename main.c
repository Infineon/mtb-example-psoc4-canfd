/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the CANFD example
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2021-2022, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/

/* Header file includes */
#include <stdio.h>
#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"

/*******************************************************************************
* Macros
*******************************************************************************/
#define CANFD_NODE_1            1
#define CANFD_NODE_2            2
#define USE_CANFD_NODE          CANFD_NODE_1

#define CANFD_HW_CHANNEL        0
#define CANFD_BUFFER_INDEX      0

#define CANFD_DLC               8

#ifdef PSOC_6
#define CANFD_INTERRUPT         canfd_0_interrupts0_0_IRQn
#else
#define CANFD_INTERRUPT         canfd_interrupts0_0_IRQn
#endif
    
/*******************************************************************************
* Function Prototypes
*******************************************************************************/

/* canfd interrupt handler */
static void isr_canfd (void);

/* button press interrupt handler */
static void isr_button (void);

/*******************************************************************************
* Global Variables
*******************************************************************************/

/* This is a shared context structure, unique for each canfd channel */
static cy_stc_canfd_context_t canfd_context;

/* Variable which holds the button pressed status */
static volatile bool ButtonIntrFlag = false;

/* Array to store the data bytes of the CANFD frame */
static uint32_t canfd_dataBuffer[] =
{
    [CANFD_DATA_0] = 0x04030201U,
    [CANFD_DATA_1] = 0x08070605U,
};

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* This is the main function for CM0 CPU. It initializes the CANFD channel 
* and interrupt. User button and User LED are also initilalized. The main loop
* checks for the button pressed interrupt flag and when it is set, a CANFD frame
* is sent. Whenever a CANFD frame is received from other nodes, the user LED 
* toggles and the received data is logged over serial terminal.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{

    cy_rslt_t result;

    cy_en_canfd_status_t status;

    result = cybsp_init();

    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize retarget-io for uart logging */
    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX, CYBSP_DEBUG_UART_RX, 
                                CY_RETARGET_IO_BAUDRATE);

    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    printf("===============================================================\r\n");
    printf("Welcome to CANFD example\r\n");
    printf("===============================================================\r\n\n");

    printf("===============================================================\r\n");
    printf("CANFD Node-%d\r\n", USE_CANFD_NODE);
    printf("===============================================================\r\n\n");

    /* Populate the configuration structure for CANFD Interrupt */
    cy_stc_sysint_t canfd_irq_cfg =
    {   
        /* Source of interrupt signal */
        .intrSrc      = CANFD_INTERRUPT,
        /* Interrupt priority */
        .intrPriority = 1U,                      
    };

    /* Hook the interrupt service routine and enable the interrupt */
    (void) Cy_SysInt_Init(&canfd_irq_cfg, &isr_canfd);

    NVIC_EnableIRQ(CANFD_INTERRUPT);

    cy_stc_sysint_t switch_intr_config = 
    {
        /* Source of interrupt signal */
        .intrSrc = CYBSP_USER_BTN_IRQ, 
        /* Interrupt priority */  
        .intrPriority = 0U,                    
    };

    Cy_SysInt_Init(&switch_intr_config, isr_button);

    NVIC_EnableIRQ(CYBSP_USER_BTN_IRQ);

    /* Enable global interrupts */
    __enable_irq();

    /* Initialze CANFD Channel */
    status = Cy_CANFD_Init(CANFD_HW, CANFD_HW_CHANNEL, &CANFD_config, 
                           &canfd_context);

    if (status != CY_CANFD_SUCCESS)
    {
        CY_ASSERT(0);
    }

#if USE_CANFD_NODE == CANFD_NODE_1

    /* Setting Node-1 Identifier as 0x01 */
    CANFD_T0RegisterBuffer_0.id = USE_CANFD_NODE;

#elif USE_CANFD_NODE == CANFD_NODE_2

    /* Setting Node-2 Identifier as 0x02 */
    CANFD_T0RegisterBuffer_0.id = USE_CANFD_NODE;

#endif

    /* Assign the user defined data buffer to CANFD data area */
    CANFD_txBuffer_0.data_area_f = canfd_dataBuffer;

    for(;;)
    {
        
        if (true == ButtonIntrFlag)
        {
            /* Sending CANFD frame to other node */
            status = Cy_CANFD_UpdateAndTransmitMsgBuffer(CANFD_HW, 
                                                    CANFD_HW_CHANNEL, 
                                                    &CANFD_txBuffer_0,
                                                    CANFD_BUFFER_INDEX, 
                                                    &canfd_context);

            printf("CANFD Frame sent from Node-%d\r\n\r\n", USE_CANFD_NODE);

            ButtonIntrFlag = false;
        }
    }
}

/*******************************************************************************
* Function Name: isr_button
********************************************************************************
* Summary:
* This is the callback function for button press
*
* Parameters:
*    None
*    
*******************************************************************************/
static void isr_button (void)
{
    /* Clears the triggered pin interrupt */
    Cy_GPIO_ClearInterrupt(CYBSP_USER_BTN_PORT, CYBSP_USER_BTN_PIN);

    NVIC_ClearPendingIRQ(CYBSP_USER_BTN_IRQ);

    /* Set button interrupt flag */
    ButtonIntrFlag = true;
}

/*******************************************************************************
* Function Name: isr_canfd
********************************************************************************
* Summary:
* This is the interrupt handler function for the canfd interrupt.
*
* Parameters:
*  none
*    
*
*******************************************************************************/
static void isr_canfd(void)
{

    /* Just call the IRQ handler with the current channel number and context */
    Cy_CANFD_IrqHandler(CANFD_HW, CANFD_HW_CHANNEL, &canfd_context);
}

/*******************************************************************************
* Function Name: canfd_rx_callback
********************************************************************************
* Summary:
* This is the callback function for canfd reception
*
* Parameters:
*    msg_valid                     Message received properly or not
*    msg_buf_fifo_num              RxFIFO number of the received message
*    canfd_rx_buf                  Message buffer
*
*******************************************************************************/
void canfd_rx_callback (bool                        msg_valid, 
                        uint8_t                     msg_buf_fifo_num,
                        cy_stc_canfd_rx_buffer_t*   canfd_rx_buf)
{

    /* Array to hold the data bytes of the CANFD frame */
    uint8_t canfd_data_buffer[CANFD_DLC];
    /* Variable to hold the data length code of the CANFD frame */
    uint32_t canfd_dlc;
    /* Variable to hold the Identifier of the CANFD frame */
    uint32_t canfd_id;

    if (true == msg_valid)
    {
        /* Checking whether the frame received is a data frame */
        if(CY_CANFD_RTR_DATA_FRAME == canfd_rx_buf->r0_f->rtr) 
        {

            Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);

            canfd_dlc = canfd_rx_buf->r1_f->dlc;
            canfd_id  = canfd_rx_buf->r0_f->id;

            printf("%d bytes received from Node-%d with identifier %d\r\n\r\n", 
                                                        (int)canfd_dlc,
                                                        (int)canfd_id,
                                                        (int)canfd_id);

            memcpy(canfd_data_buffer,canfd_rx_buf->data_area_f,canfd_dlc);

            printf("Rx Data : ");

            for (uint8_t msg_idx = 0U; msg_idx < canfd_dlc ; msg_idx++)
            {
                printf(" %d ", canfd_data_buffer[msg_idx]);
            }
    
            printf("\r\n\r\n");
        }
    }

}

/* [] END OF FILE */
