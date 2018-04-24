#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include "drivers/pinout.h"
#include "utils/uartstdio.h"
#include "utils/eth.h"
#include "NetworkInterface.h"

// TivaWare includes
#include "driverlib/sysctl.h"
#include "driverlib/debug.h"
#include "driverlib/rom.h"
#include "driverlib/rom_map.h"

// FreeRTOS includes
#include "FreeRTOSConfig.h"
#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"
#include "FreeRTOS_Sockets.h"
#include "FreeRTOS_TCP_IP.h"

#define LED_BIT    0x01
#define LOG_BIT    0x02

QueueHandle_t xQueue1;

//Task declarations
void task1(void *pvParameters);
void task2(void *pvParameters);
void task3(void *pvParameters);

void vTimerCallback1(TimerHandle_t xTimer);
void vTimerCallback2(TimerHandle_t xTimer);

TaskHandle_t th1;
TaskHandle_t th2;
TaskHandle_t th3;
/* An array to hold handles to the created timers. */
TimerHandle_t xTimers[2];

uint32_t tick_counter;
// Main function
int main(void)
{
    // Initialize system clock to 120 MHz
    uint32_t output_clock_rate_hz;
    output_clock_rate_hz = ROM_SysCtlClockFreqSet(
                               (SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN |
                                SYSCTL_USE_PLL | SYSCTL_CFG_VCO_480),
                               SYSTEM_CLOCK);

    ASSERT(output_clock_rate_hz == SYSTEM_CLOCK);

    tick_counter = 0;
    xQueue1 = xQueueCreate( 10, sizeof(uint32_t) );
    //2 hz timer
    xTimers[1] = xTimerCreate("Timer1", 500, pdTRUE, ( void * ) 0, vTimerCallback2);
    //4 hz timer
    xTimers[0] = xTimerCreate("Timer0", 250, pdTRUE, ( void * ) 0, vTimerCallback1);

    if( xTimerStart( xTimers[0], 0 ) != pdPASS )
    {
        //The timer could not be set into the Active state.
    }
    if( xTimerStart( xTimers[1], 0 ) != pdPASS )
    {
        //The timer could not be set into the Active state.
    }
    // Initialize the GPIO pins for the Launchpad
    PinoutSet(false, false);

    const uint8_t ip[4] = {192,168,1,90};
    const uint8_t netmask[4] = {255,255,0,0};
    const uint8_t gate[4] = {192,168,1,2};
    const uint8_t dns[4] = NULL;
    const uint8_t mac[6] = {0x00,0x1A,0xB6,0x03,0x3A,0x16};

    xNetworkInterfaceInitialise();
    FreeRTOS_IPInit(ip, netmask, gate, dns, mac);

    //initialize ethernet PHY
    //initEthernet(output_clock_rate_hz);
    //vCreateTCPClientSocket();
    // Create tasks
    xTaskCreate(task1, (const portCHAR *)"LED1",
                configMINIMAL_STACK_SIZE, NULL, 1, &th1);
    xTaskCreate(task2, (const portCHAR *)"LED2",
                configMINIMAL_STACK_SIZE, NULL, 1, &th2);
    xTaskCreate(task3, (const portCHAR *)"LOGGER",
                configMINIMAL_STACK_SIZE, NULL, 1, &th3);
   // xTaskCreate(demoSerialTask, (const portCHAR *)"Serial",
    //            configMINIMAL_STACK_SIZE, NULL, 1, NULL);

    vTaskStartScheduler();
    return 0;
}

void vTCPSend(char *pcBufferToTransmit, const size_t xTotalLengthToSend )
{
    Socket_t xSocket;
    struct freertos_sockaddr xRemoteAddress;
    BaseType_t xAlreadyTransmitted = 0, xBytesSent = 0;
    //TaskHandle_t xRxTask = NULL;
    size_t xLenToSend;

    /* Set the IP address (192.168.0.50) and port (1500) of the remote socket
    to which this client socket will transmit. */
    xRemoteAddress.sin_port = FreeRTOS_htons( 15000 );
    xRemoteAddress.sin_addr = FreeRTOS_inet_addr_quick( 192, 168, 1, 1 );

    /* Create a socket. */
    xSocket = FreeRTOS_socket( FREERTOS_AF_INET,
                               FREERTOS_SOCK_STREAM,/* FREERTOS_SOCK_STREAM for TCP. */
                               FREERTOS_IPPROTO_TCP );
    configASSERT( xSocket != FREERTOS_INVALID_SOCKET );

    /* Connect to the remote socket.  The socket has not previously been bound to
    a local port number so will get automatically bound to a local port inside
    the FreeRTOS_connect() function. */
    if( FreeRTOS_connect( xSocket, &xRemoteAddress, sizeof( xRemoteAddress ) ) == 0 );
   // {
        /* Keep sending until the entire buffer has been sent. */
        while( xAlreadyTransmitted < xTotalLengthToSend )
        {
            /* How many bytes are left to send? */
            xLenToSend = xTotalLengthToSend - xAlreadyTransmitted;
            xBytesSent = FreeRTOS_send( /* The socket being sent to. */
                                        xSocket,
                                        /* The data being sent. */
                                        &( pcBufferToTransmit[ xAlreadyTransmitted ] ),
                                        /* The remaining length of data to send. */
                                        xLenToSend,
                                        /* ulFlags. */
                                        0 );

            if( xBytesSent >= 0 )
            {
                /* Data was sent successfully. */
                xAlreadyTransmitted += xBytesSent;
            }
            else
            {
                /* Error - break out of the loop for graceful socket close. */
                break;
            }
        }
   // }

    /* Initiate graceful shutdown. */
    FreeRTOS_shutdown( xSocket, FREERTOS_SHUT_RDWR );

    /* Wait for the socket to disconnect gracefully (indicated by FreeRTOS_recv()
    returning a FREERTOS_EINVAL error) before closing the socket. */
    while( FreeRTOS_recv( xSocket, pcBufferToTransmit, xTotalLengthToSend, 0 ) >= 0 )
    {
        /* Wait for shutdown to complete.  If a receive block time is used then
        this delay will not be necessary as FreeRTOS_recv() will place the RTOS task
        into the Blocked state anyway. */
        vTaskDelay( pdMS_TO_TICKS(250));

        /* Note - real applications should implement a timeout here, not just
        loop forever. */
    }

    /* The socket has shut down and is safe to close. */
    FreeRTOS_closesocket( xSocket );
}

void vCreateTCPClientSocket( void )
{
    Socket_t xClientSocket;

    socklen_t xSize = sizeof(struct freertos_sockaddr);
    static const TickType_t xTimeOut = pdMS_TO_TICKS( 2000 );

    xClientSocket = FreeRTOS_socket( FREERTOS_AF_INET,
                                     FREERTOS_SOCK_STREAM,
                                     FREERTOS_IPPROTO_TCP );

    configASSERT( xClientSocket != FREERTOS_INVALID_SOCKET );

    FreeRTOS_setsockopt( xClientSocket,
                         0,
                         FREERTOS_SO_RCVTIMEO,
                         &xTimeOut,
                         sizeof( xTimeOut ) );

    FreeRTOS_setsockopt( xClientSocket,
                         0,
                         FREERTOS_SO_SNDTIMEO,
                         &xTimeOut,
                         sizeof( xTimeOut ) );


    FreeRTOS_bind( xClientSocket, NULL, xSize );
}

/**
 * @brief Contains lookup table for converting a byte from integer to ASCII
 *
 * @param num Number to convert to ASCII (one byte)
 *
 * @return ASCII character representation of byte
 */

uint8_t ascii_lut(int32_t num)
{
  switch(num)  {
    case 0:
      return '0';
    case 1:
      return '1';
    case 2:
      return '2';
    case 3:
      return '3';
    case 4:
      return '4';
    case 5:
      return '5';
    case 6:
      return '6';
    case 7:
      return '7';
    case 8:
      return '8';
    case 9:
      return '9';
    case 10:
      return 'A';
    case 11:
      return 'B';
    case 12:
      return 'C';
    case 13:
      return 'D';
    case 14:
      return 'E';
    case 15:
      return 'F';
    }

  return 'X'; // if digit falls out of range, return obviously wrong value
}

/**
 * @brief Converts from integer to ASCII, including numerical base conversion
 *
 * @param data Integer number to convert to ASCII
 * @param ptr Pointer to memory location containing converted ASCII string
 * @param base Number between [2, 16] indicating numerical base to convert to
 *
 * @return Length of converted ASCII array, in bytes
 */

uint8_t my_itoa(int32_t data, uint8_t * ptr, uint32_t base)
{
  uint8_t length = 0; // store length of converted ASCII String
  uint8_t tmp = 0;    // store current digit in temp location

  if(base < 2 || base > 16)  // only bases in range [2,16] supported
    return 255;

  while(data != 0)  {  // perform repeated divides by base to perform conversion
    *(ptr+length) = ascii_lut(data % base);
    data /= base;
    length++;  // count length of ASCII string (number of digits)
  }

  uint8_t i;
  for(i=0; i<(length/2); i++) { // reverse ASCII string to correct
    tmp = *(ptr+i);                     // digit order
    *(ptr+i) = *(ptr+length-i-1);
    *(ptr+length-i-1) = tmp;
  }

  *(ptr+length) = '\0';   // terminate with null character

  return length;  // return length of ASCII string
}

// Flash the LEDs on the launchpad
void task1(void *pvParameters)
{

    for (;;)
    {

        vTaskSuspend(th1);
    }
}

// Flash the LEDs on the launchpad
void task2(void *pvParameters)
{
    for (;;)
    {
        char poo[49] = "What we've got here is a failure to communicate.";
        //PacketTransmit(poo, 49);
        vTCPSend(poo, 49);
        vTaskSuspend(th2);
    }
}


void task3(void *pvParameters)
{
    UARTStdioConfig(0, 57600, SYSTEM_CLOCK);
    const TickType_t xMaxBlockTime = 1000;
    BaseType_t xResult;
    uint32_t ulNotifiedValue;
    char toggleLED = 0;
    for( ;; )
    {
        /* Wait to be notified of an interrupt. */
        xResult = xTaskNotifyWait( 0x0000,    /* Don't clear bits on entry. */
                                    0xFFFF,        /* Clear all bits on exit. */
                                    &ulNotifiedValue, /* Stores the notified value. */
                                    xMaxBlockTime );

        if( xResult == pdPASS )
        {
            /* A notification was received.  See which bits were set. */
            if( ( ulNotifiedValue & LOG_BIT ) != 0 )
            {
                uint32_t poo;
                /* The TX ISR has set a bit. */
                if( xQueue1 != 0 )
                {

                    // Receive a message on the created queue.  Block for 10 ticks if a
                    // message is not immediately available.
                    if( xQueueReceive( xQueue1, &poo, ( TickType_t ) 10 ) )
                    {
                        // pcRxedMessage now points to the struct AMessage variable posted
                        // by vATask.
                    }
                }
                UARTprintf("\r\nThe ticker done ticked ");
                char val[12];
                my_itoa(poo, val, 10);
                UARTprintf(val);
                UARTprintf(" times since the ticker done started tickin'.");
            }

            if( ( ulNotifiedValue & LED_BIT ) != 0 )
            {
                /* The RX ISR has set a bit. */
                //Toggle LED
                toggleLED ^= 1;
                LEDWrite(0x01, toggleLED);
            }
        }
        else
        {
            /* Did not receive a notification within the expected time. */
            //prvCheckForErrors();
        }
    }
    /*
    // Set up the UART which is connected to the virtual COM port
    UARTStdioConfig(0, 57600, SYSTEM_CLOCK);


    for (;;)
    {
        UARTprintf("\r\nHello, world from FreeRTOS 9.0!");
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
    */
}


/* Define a callback function that will be used by multiple timer
instances.  The callback function does nothing but count the number
of times the associated timer expires, and stop the timer once the
timer has expired 10 times.  The count is saved as the ID of the
timer. */

void vTimerCallback1( TimerHandle_t xTimer )
{
    xTaskNotify( th3, LED_BIT, eSetBits);
    vTaskResume(th1);
}

void vTimerCallback2( TimerHandle_t xTimer )
{
    tick_counter++;
    if( xQueue1 != 0 )
    {
        /* Send an unsigned long.  Wait for 10 ticks for space to become
        available if necessary. */
        if( xQueueSend( xQueue1,
                       ( void * ) &tick_counter,
                       ( TickType_t ) 10 ) != pdPASS )
        {
            /* Failed to post the message, even after 10 ticks. */
        }
    }
    xTaskNotify(th3, LOG_BIT, eSetBits);
    vTaskResume(th2);
}


/*  ASSERT() Error function
 *
 *  failed ASSERTS() from driverlib/debug.h are executed in this function
 */
void __error__(char *pcFilename, uint32_t ui32Line)
{
    // Place a breakpoint here to capture errors until logging routine is finished
    while (1)
    {
    }
}
