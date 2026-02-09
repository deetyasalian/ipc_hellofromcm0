#include "cy_pdl.h"
#include "ipc_communication.h"
#include "cyhal.h"
#include "cybsp.h"

static volatile uint8_t msg_cmd = 0;

static ipc_msg_t ipc_msg = {
    .client_id  = IPC_CM0_TO_CM4_CLIENT_ID,
    .cpu_status = 0,
    .intr_mask  = USER_IPC_PIPE_INTR_MASK,
    .cmd        = IPC_CMD_STATUS,
    .value      = 0
};

static void cm0p_msg_callback(uint32_t *msg);

int main(void)
{
    cy_rslt_t result;
    cy_en_ipc_pipe_status_t ipc_status;

    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    setup_ipc_communication_cm0();
    __enable_irq();

    ipc_status = Cy_IPC_Pipe_RegisterCallback(USER_IPC_PIPE_EP_ADDR,
                                             cm0p_msg_callback,
                                             IPC_CM4_TO_CM0_CLIENT_ID);
    if (ipc_status != CY_IPC_PIPE_SUCCESS)
    {
        CY_ASSERT(0);
    }

    Cy_SysEnableCM4(CY_CORTEX_M4_APPL_ADDR);

    for (;;)
    {
        Cy_SysPm_DeepSleep(CY_SYSPM_WAIT_FOR_INTERRUPT);

        switch (msg_cmd)
        {
            case IPC_CMD_START:
                // Step 2: CM0 -> CM4 : send 1
                ipc_msg.value = 1;

                ipc_status = Cy_IPC_Pipe_SendMessage(USER_IPC_PIPE_EP_ADDR_CM4,
                                                    USER_IPC_PIPE_EP_ADDR_CM0,
                                                    (uint32_t *)&ipc_msg, NULL);
                if (ipc_status != CY_IPC_PIPE_SUCCESS)
                {
                    CY_ASSERT(0);
                }
                break;

            case IPC_CMD_STOP:
                // CM0 received "2" from CM4, send ACK back
                ipc_msg.value = 2;

                ipc_status = Cy_IPC_Pipe_SendMessage(USER_IPC_PIPE_EP_ADDR_CM4,
                                                    USER_IPC_PIPE_EP_ADDR_CM0,
                                                    (uint32_t *)&ipc_msg, NULL);
                if (ipc_status != CY_IPC_PIPE_SUCCESS)
                {
                    CY_ASSERT(0);
                }
                break;


            default:
                break;
        }

        msg_cmd = 0;
    }
}

static void cm0p_msg_callback(uint32_t *msg)
{
    if (msg != NULL)
    {
        ipc_msg_t *ipc_recv_msg = (ipc_msg_t *) msg;
        msg_cmd = ipc_recv_msg->cmd;
    }
}

cy_rslt_t cybsp_register_custom_sysclk_pm_callback(void)
{
    return CY_RSLT_SUCCESS;
}
