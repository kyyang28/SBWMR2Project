
#include <stdio.h>
#include "serial_uart_impl.h"
#include "RCCTypes.h"
#include "io.h"
#include "rcc.h"
#include "nvic.h"
#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"

#define UART_RX_BUFFER_SIZE 512
#define UART_TX_BUFFER_SIZE 512

typedef enum UARTDevice {
    UARTDEV_1 = 0,
    UARTDEV_2 = 1,
    UARTDEV_3 = 2,
    UARTDEV_4 = 3,
    UARTDEV_5 = 4,
    UARTDEV_6 = 5
} UARTDevice;

typedef struct uartDevice_s {
    USART_TypeDef* dev;
    uartPort_t port;
    uint32_t DMAChannel;
    DMA_Stream_TypeDef *txDMAStream;
    DMA_Stream_TypeDef *rxDMAStream;
    ioTag_t rx;
    ioTag_t tx;
    volatile uint8_t rxBuffer[UART_RX_BUFFER_SIZE];
    volatile uint8_t txBuffer[UART_TX_BUFFER_SIZE];
    RccPeriphTag_t rcc_uart;
    uint8_t af;
    uint8_t rxIrq;
    uint32_t txPriority;
    uint32_t rxPriority;
} uartDevice_t;

#ifdef USE_UART1
static uartDevice_t uart1 = {
	.DMAChannel = DMA_Channel_4,
#ifdef USE_UART1_RX_DMA
	.rxDMAStream = DMA2_Stream5,
#endif
#ifdef USE_UART1_TX_DMA
	.txDMAStream = DMA2_Stream7,
#endif
	.dev = USART1,
	.rx = IO_TAG(UART1_RX_PIN),
	.tx = IO_TAG(UART1_TX_PIN),
	.af = GPIO_AF_USART1,
	.rcc_uart = RCC_APB2(USART1),			// RCC_APB2ENR_USART1EN 		((uint32_t)0x00000010)
	.rxIrq = USART1_IRQn,
	.txPriority = NVIC_PRIO_SERIALUART1_TXDMA,
	.rxPriority = NVIC_PRIO_SERIALUART1
};
#endif

#ifdef USE_UART2
static uartDevice_t uart2 = {
	.DMAChannel = DMA_Channel_4,
#ifdef USE_UART2_RX_DMA
	.rxDMAStream = DMA1_Stream5,
#endif
#ifdef USE_UART2_TX_DMA
	.txDMAStream = DMA1_Stream6,
#endif
	.dev = USART2,
	.rx = IO_TAG(UART2_RX_PIN),
	.tx = IO_TAG(UART2_TX_PIN),
	.af = GPIO_AF_USART2,
	.rcc_uart = RCC_APB1(USART2),			// RCC_APB1ENR_USART2EN 		((uint32_t)0x00020000)
	.rxIrq = USART2_IRQn,
	.txPriority = NVIC_PRIO_SERIALUART2_TXDMA,
	.rxPriority = NVIC_PRIO_SERIALUART2
};
#endif

#ifdef USE_UART3
static uartDevice_t uart3 = {
	.DMAChannel = DMA_Channel_4,
#ifdef USE_UART3_RX_DMA
	.rxDMAStream = DMA1_Stream1,
#endif
#ifdef USE_UART3_TX_DMA
	.txDMAStream = DMA1_Stream3,
#endif
	.dev = USART3,
	.rx = IO_TAG(UART3_RX_PIN),
	.tx = IO_TAG(UART3_TX_PIN),
	.af = GPIO_AF_USART3,
	.rcc_uart = RCC_APB1(USART3),			// RCC_APB1ENR_USART3EN 		((uint32_t)0x00040000)
	.rxIrq = USART3_IRQn,
	.txPriority = NVIC_PRIO_SERIALUART3_TXDMA,
	.rxPriority = NVIC_PRIO_SERIALUART3
};
#endif

#ifdef USE_UART6
static uartDevice_t uart6 = {
	.DMAChannel = DMA_Channel_5,
#ifdef USE_UART6_RX_DMA
	.rxDMAStream = DMA2_Stream1,
#endif
#ifdef USE_UART6_TX_DMA
	.txDMAStream = DMA2_Stream6,
#endif
	.dev = USART6,
	.rx = IO_TAG(UART6_RX_PIN),
	.tx = IO_TAG(UART6_TX_PIN),
	.af = GPIO_AF_USART6,
	.rcc_uart = RCC_APB2(USART6),			// RCC_APB1ENR_USART2EN 		((uint32_t)0x00020000)
	.rxIrq = USART6_IRQn,
	.txPriority = NVIC_PRIO_SERIALUART6_TXDMA,
	.rxPriority = NVIC_PRIO_SERIALUART6
};
#endif

static uartDevice_t *uartHardwareMap[] = {
#ifdef USE_UART1
	&uart1,
#else
	NULL,
#endif
#ifdef USE_UART2
	&uart2,
#else
	NULL,
#endif
#ifdef USE_UART3
	&uart3,
#else
	NULL,
#endif
#ifdef USE_UART4
//	&uart4,
#else
	NULL,
#endif
#ifdef USE_UART5
//	&uart5,
#else
	NULL,
#endif
#ifdef USE_UART6
	&uart6,
#else
	NULL,
#endif
};

uartPort_t *serialUART(UARTDevice device, uint32_t baudRate, portMode_t mode, portOptions_t options)
{
	uartPort_t *s;
	NVIC_InitTypeDef NVIC_InitStructure;
	
	uartDevice_t *uart = uartHardwareMap[device];
	if (!uart)
		return NULL;
	
	s = &(uart->port);
	s->port.vTable = uartVTable;
	s->port.baudRate = baudRate;
	s->port.rxBuffer = uart->rxBuffer;
	s->port.txBuffer = uart->txBuffer;
	s->port.rxBufferSize = sizeof(uart->rxBuffer);
	s->port.txBufferSize = sizeof(uart->txBuffer);
	
	s->USARTx = uart->dev;			// USART2 = ((USART_TypeDef *) USART2_BASE)
	if (uart->rxDMAStream) {
		s->rxDMAChannel = uart->DMAChannel;			// USART2: DMA_Channel_4
		s->rxDMAStream = uart->rxDMAStream;
//		dmaInit(dmaGetIdentifier(uart->rxDMAStream), OWNER_SERIAL_RX, RESOURCE_INDEX(device));
	}
	
	if (uart->txDMAStream) {
		s->txDMAChannel = uart->DMAChannel;
		s->txDMAStream = uart->txDMAStream;
//        const dmaIdentifier_e identifier = dmaGetIdentifier(uart->txDMAStream);
//        dmaInit(identifier, OWNER_SERIAL_TX, RESOURCE_INDEX(device));
//        // DMA TX Interrupt
//        dmaSetHandler(identifier, dmaIRQHandler, uart->txPriority, (uint32_t)uart);
	}
	
	s->txDMAPeripheralBaseAddr = (uint32_t)&s->USARTx->DR;
	s->rxDMAPeripheralBaseAddr = (uint32_t)&s->USARTx->DR;
	
	IO_t tx = IOGetByTag(uart->tx);
	IO_t rx = IOGetByTag(uart->rx);
	
	if (uart->rcc_uart) {
		RCC_ClockCmd(uart->rcc_uart, ENABLE);
	}
	
	if (options & SERIAL_BIDIR) {
		IOInit(tx, OWNER_SERIAL_TX, RESOURCE_INDEX(device));
		if (options & SERIAL_BIDIR_PP)
			IOConfigGPIOAF(tx, IOCFG_AF_PP, uart->af);
		else
			IOConfigGPIOAF(tx, IOCFG_AF_OD, uart->af);
	}else {
		if (mode & MODE_TX) {
			IOInit(tx, OWNER_SERIAL_TX, RESOURCE_INDEX(device));
			IOConfigGPIOAF(tx, IOCFG_AF_PP_UP, uart->af);
		}
		
		if (mode & MODE_RX) {
			IOInit(rx, OWNER_SERIAL_RX, RESOURCE_INDEX(device));
			IOConfigGPIOAF(rx, IOCFG_AF_PP_UP, uart->af);
		}
	}
	
	/* Use uart IRQ only if the uart DMA is disabled */
	if (!(s->rxDMAChannel)) {
		NVIC_InitStructure.NVIC_IRQChannel = uart->rxIrq;
		NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = NVIC_PRIORITY_BASE(uart->rxPriority);
		NVIC_InitStructure.NVIC_IRQChannelSubPriority = NVIC_PRIORITY_SUB(uart->rxPriority);
		NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
		NVIC_Init(&NVIC_InitStructure);
	}
	
	return s;
}

uint8_t driveForward, driveReverse, turnLeft, turnRight;

void uartIrqHandler(uartPort_t *s)
{
	/* rxDMAStream is disabled (NULL) AND there is incoming data in the RX line */
	if (!s->rxDMAStream && (USART_GetITStatus(s->USARTx, USART_IT_RXNE) == SET)) {
		if (s->port.rxCallback) {
			s->port.rxCallback(s->USARTx->DR);
		}else {
//			printf("data: %u\r\n", s->USARTx->DR);
			switch (s->USARTx->DR) {
				/* Stop */
				case 48:
					driveForward = 0;
					driveReverse = 0;
					turnLeft = 0;
					turnRight = 0;
					break;

				/* Forward */
				case 49:
					driveForward = 1;
					driveReverse = 0;
					turnLeft = 0;
					turnRight = 0;
					break;
				
				/* Reverse */
				case 50:
					driveForward = 0;
					driveReverse = 1;
					turnLeft = 0;
					turnRight = 0;
					break;
				
				/* Left */
				case 51:
					driveForward = 0;
					driveReverse = 0;
					turnLeft = 1;
					turnRight = 0;
					break;
				
				/* Right */
				case 52:
					driveForward = 0;
					driveReverse = 0;
					turnLeft = 0;
					turnRight = 1;
					break;
			}
			
			s->port.rxBuffer[s->port.rxBufferHead] = s->USARTx->DR;
			s->port.rxBufferHead = (s->port.rxBufferHead + 1) % s->port.rxBufferSize;
		}
	}
	
	/* txDMAStream is disabled (NULL) AND there is data to be sent in the TX line */
	if (!s->txDMAStream && (USART_GetITStatus(s->USARTx, USART_IT_TXE) == SET)) {
		if (s->port.txBufferTail != s->port.txBufferHead) {
			USART_SendData(s->USARTx, s->port.txBuffer[s->port.txBufferTail]);
			s->port.txBufferTail = (s->port.txBufferTail + 1) % s->port.txBufferSize;
		}else {
			USART_ITConfig(s->USARTx, USART_IT_TXE, DISABLE);
		}
	}
	
	if (USART_GetITStatus(s->USARTx, USART_FLAG_ORE) == SET) {
		USART_ClearITPendingBit(s->USARTx, USART_IT_ORE);
	}
}

#ifdef USE_UART1
uartPort_t *serialUART1(uint32_t baudRate, portMode_t mode, portOptions_t options)
{
	return serialUART(UARTDEV_1, baudRate, mode, options);
}

void USART1_IRQHandler(void)
{
	uartPort_t *s = &(uartHardwareMap[UARTDEV_1]->port);
	uartIrqHandler(s);
}
#endif

#ifdef USE_UART2
uartPort_t *serialUART2(uint32_t baudRate, portMode_t mode, portOptions_t options)
{
	return serialUART(UARTDEV_2, baudRate, mode, options);
}

void USART2_IRQHandler(void)
{
	uartPort_t *s = &(uartHardwareMap[UARTDEV_2]->port);
	uartIrqHandler(s);
}
#endif

#ifdef USE_UART3
uartPort_t *serialUART3(uint32_t baudRate, portMode_t mode, portOptions_t options)
{
	return serialUART(UARTDEV_3, baudRate, mode, options);
}

void USART3_IRQHandler(void)
{
	uartPort_t *s = &(uartHardwareMap[UARTDEV_3]->port);
	uartIrqHandler(s);
}
#endif

#ifdef USE_UART6
uartPort_t *serialUART6(uint32_t baudRate, portMode_t mode, portOptions_t options)
{
	return serialUART(UARTDEV_6, baudRate, mode, options);
}

void USART6_IRQHandler(void)
{
	uartPort_t *s = &(uartHardwareMap[UARTDEV_6]->port);
	uartIrqHandler(s);
}
#endif