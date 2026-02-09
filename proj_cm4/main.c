#include "cy_pdl.h"
#include "cyhal.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include "ipc_communication.h"
#include <stdio.h>

#define BTN_MSG_START   1u

#define SEND_IPC_MSG(x) ipc_msg.cmd = x; \
                        Cy_IPC_Pipe_SendMessage(USER_IPC_PIPE_EP_ADDR_CM0, \
                                                USER_IPC_PIPE_EP_ADDR_CM4, \
                                                (void *)&ipc_msg, 0);

static void cm4_msg_callback(uint32_t *msg);
static void button_isr_handler(void *arg, cyhal_gpio_event_t event);
static int handshake_count = 0;
static bool handshake_active = false;

static ipc_msg_t ipc_msg = {
    .client_id  = IPC_CM4_TO_CM0_CLIENT_ID,
    .cpu_status = 0,
    .intr_mask  = USER_IPC_PIPE_INTR_MASK,
    .cmd        = IPC_CMD_INIT,
};

static volatile bool msg_flag = false;
static volatile uint32_t msg_value;
static volatile uint32_t button_flag;
static volatile bool button_pressed = false;

static cyhal_gpio_callback_data_t cb_data =
{
    .callback = button_isr_handler,
    .callback_arg = NULL
};

int main(void)
{
    cy_rslt_t result;
    cy_en_ipc_pipe_status_t ipc_status;

    setup_ipc_communication_cm4();

    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    __enable_irq();

    result = cy_retarget_io_init(CYBSP_DEBUG_UART_TX,
                                 CYBSP_DEBUG_UART_RX,
                                 CY_RETARGET_IO_BAUDRATE);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    result = cyhal_gpio_init(CYBSP_USER_BTN, CYHAL_GPIO_DIR_INPUT,
                             CYHAL_GPIO_DRIVE_PULLUP, true);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    cyhal_gpio_enable_event(CYBSP_USER_BTN, CYHAL_GPIO_IRQ_FALL,
                            CYHAL_ISR_PRIORITY_DEFAULT, true);
    cyhal_gpio_register_callback(CYBSP_USER_BTN, &cb_data);

    ipc_status = Cy_IPC_Pipe_RegisterCallback(USER_IPC_PIPE_EP_ADDR,
                                             cm4_msg_callback,
                                             IPC_CM0_TO_CM4_CLIENT_ID);
    if (ipc_status != CY_IPC_PIPE_SUCCESS)
    {
        CY_ASSERT(0);
    }

    printf("\x1b[2J\x1b[;H");
    printf("IPC Handshake Demo\r\n");
    printf("<Press User Button to start handshake>\r\n");

    for (;;)
    {
        cyhal_syspm_sleep();

        if (msg_flag)
        {
            msg_flag = false;

            if (msg_value == 1)
            {
                printf("CM4: received 1 from CM0 (i=%d)\r\n", handshake_count + 1);

                // Send "2" to CM0
                SEND_IPC_MSG(IPC_CMD_STOP);
            }
            else if (msg_value == 2)
            {
                printf("CM0: received 2 from CM4 (i=%d)\r\n", handshake_count + 1);

                handshake_count++;

                if (handshake_active && handshake_count < 5)
                {
                    // Trigger next iteration
                    SEND_IPC_MSG(IPC_CMD_START);
                }
                else if (handshake_count >= 5)
                {
                    printf("Handshake loop completed (5 times)\r\n");
                    handshake_active = false;
                }
            }
        }

        // Step 1: Button press -> start loop
        if (BTN_MSG_START == button_flag && button_pressed)
        {
            printf("<---Start handshake loop--->\r\n");
            handshake_count = 0;
            handshake_active = true;

            SEND_IPC_MSG(IPC_CMD_START);

            button_pressed = false;
            button_flag = 0;
        }
    }

}

static void cm4_msg_callback(uint32_t *msg)
{
    if (msg != NULL)
    {
        ipc_msg_t *ipc_recv_msg = (ipc_msg_t *) msg;
        msg_value = ipc_recv_msg->value;
        msg_flag = true;
    }
}

static void button_isr_handler(void *arg, cyhal_gpio_event_t event)
{
    (void)arg;

    if (event & CYHAL_GPIO_IRQ_FALL)
    {
        button_flag = BTN_MSG_START;
        button_pressed = true;
    }
}
