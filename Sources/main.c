#include "stm32f1xx.h"
#include <stdint.h>

#define SERVO_MIN_ANGLE       0U
#define SERVO_MAX_ANGLE       180U
#define SERVO_MIN_PULSE_US    1000U
#define SERVO_MAX_PULSE_US    2000U
#define RX_BUFFER_SIZE        32U

static char rx_buffer[RX_BUFFER_SIZE];
static uint32_t rx_length;
static uint8_t rx_overflow;
static uint8_t rx_pending_cr;

/*
 * Clock tree:
 * HSI = 8 MHz, PLL input = HSI/2 = 4 MHz, PLL x16 = 64 MHz.
 * SYSCLK = 64 MHz, APB1 = 32 MHz, APB2 = 64 MHz.
 */
static void clock_init(void)
{
    volatile uint32_t timeout;

    RCC->CR |= RCC_CR_HSION;
    timeout = 1000000U;
    while (((RCC->CR & RCC_CR_HSIRDY) == 0U) && (timeout > 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        for (;;) {
            __NOP();
        }
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    timeout = 1000000U;
    while (((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) &&
           (timeout > 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        for (;;) {
            __NOP();
        }
    }

    RCC->CR &= ~RCC_CR_PLLON;
    timeout = 1000000U;
    while (((RCC->CR & RCC_CR_PLLRDY) != 0U) && (timeout > 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        for (;;) {
            __NOP();
        }
    }

    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2;

    RCC->CFGR &= ~(RCC_CFGR_HPRE |
                   RCC_CFGR_PPRE1 |
                   RCC_CFGR_PPRE2 |
                   RCC_CFGR_PLLSRC |
                   RCC_CFGR_PLLXTPRE |
                   RCC_CFGR_PLLMULL);
    RCC->CFGR |= RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_PLLMULL16;

    RCC->CR |= RCC_CR_PLLON;
    timeout = 1000000U;
    while (((RCC->CR & RCC_CR_PLLRDY) == 0U) && (timeout > 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        for (;;) {
            __NOP();
        }
    }

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    timeout = 1000000U;
    while (((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) &&
           (timeout > 0U)) {
        --timeout;
    }
    if (timeout == 0U) {
        for (;;) {
            __NOP();
        }
    }
}

static void gpio_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_AFIOEN;

    /* PA0: TIM2_CH1 AF push-pull; PA2: USART2_TX AF push-pull;
       PA3: USART2_RX floating input. */
    GPIOA->CRL &= ~((0xFU << 0U) | (0xFU << 8U) | (0xFU << 12U));
    GPIOA->CRL |= (0xBU << 0U) | (0xBU << 8U) | (0x4U << 12U);
}

static void usart2_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

    USART2->CR1 = 0U;
    USART2->CR2 = 0U;
    USART2->CR3 = 0U;
    /* PCLK1 = 32 MHz; 32000000 / 9600 ~= 3333 = 0x0D05. */
    USART2->BRR = 0x0D05U;
    USART2->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void usart2_write_char(char value)
{
    while ((USART2->SR & USART_SR_TXE) == 0U) {
    }
    USART2->DR = (uint16_t)(uint8_t)value;
}

static void usart2_write(const char *text)
{
    while (*text != '\0') {
        usart2_write_char(*text);
        ++text;
    }
    while ((USART2->SR & USART_SR_TC) == 0U) {
    }
}

static void write_unsigned_3(uint32_t value)
{
    usart2_write_char((char)('0' + ((value / 100U) % 10U)));
    usart2_write_char((char)('0' + ((value / 10U) % 10U)));
    usart2_write_char((char)('0' + (value % 10U)));
}

static void write_hex_byte(uint8_t value)
{
    static const char hex_digits[] = "0123456789ABCDEF";

    usart2_write("0x");
    usart2_write_char(hex_digits[(value >> 4U) & 0x0FU]);
    usart2_write_char(hex_digits[value & 0x0FU]);
    usart2_write_char(' ');
}

static void write_rx_diagnostic(void)
{
    uint32_t i;

    usart2_write("RX: ");
    for (i = 0U; i < rx_length; ++i) {
        write_hex_byte((uint8_t)rx_buffer[i]);
    }
    usart2_write("\r\n");
}

static void tim2_pwm_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    TIM2->CR1 = 0U;
    TIM2->PSC = 63U;
    TIM2->ARR = 19999U;
    TIM2->CCR1 = 1500U;

    TIM2->CCMR1 &= ~(TIM_CCMR1_CC1S | TIM_CCMR1_OC1M);
    TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 |
                   TIM_CCMR1_OC1PE;
    TIM2->CCER &= ~TIM_CCER_CC1P;
    TIM2->CCER |= TIM_CCER_CC1E;
    TIM2->CR1 |= TIM_CR1_ARPE;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0U;
    TIM2->CR1 |= TIM_CR1_CEN;
}

static void servo_set_angle(uint32_t angle)
{
    const uint32_t pulse = SERVO_MIN_PULSE_US +
        (angle * (SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US)) /
        SERVO_MAX_ANGLE;

    TIM2->CCR1 = pulse;
}

static int parse_command(const char *line, uint32_t length, uint32_t *angle)
{
    uint32_t i;
    uint32_t value = 0U;

    if (length != 9U) {
        return 0;
    }

    if (line[0] != 'S' || line[1] != 'E' || line[2] != 'T' ||
        line[3] != 'V' || line[4] != 'O' || line[5] != ' ') {
        return 0;
    }

    for (i = 6U; i < 9U; ++i) {
        if (line[i] < '0' || line[i] > '9') {
            return 0;
        }
        value = value * 10U + (uint32_t)(line[i] - '0');
    }

    if (value > SERVO_MAX_ANGLE) {
        return -1;
    }

    *angle = value;
    return 1;
}

static void process_command(void)
{
    uint32_t angle = 0U;
    const int result = parse_command(rx_buffer, rx_length, &angle);

    if (rx_overflow != 0U) {
        usart2_write("ERROR: command too long\r\n");
    } else if (result == 1) {
        servo_set_angle(angle);
        usart2_write("OK ");
        write_unsigned_3(angle);
        usart2_write("\r\n");
    } else if (result == -1) {
        usart2_write("ERROR: angle must be 000..180\r\n");
    } else {
        usart2_write("ERROR: use SETVO ddd\r\n");
    }
}

static void reset_rx_buffer(void)
{
    rx_length = 0U;
    rx_overflow = 0U;
    rx_pending_cr = 0U;
    rx_buffer[0] = '\0';
}

static void receive_character(char received)
{
    /* Accept CRLF, LF, or CR as a line terminator.
       If CR was already handled, discard the LF that may follow it. */
    if (rx_pending_cr != 0U) {
        rx_pending_cr = 0U;
        if (received == '\n') {
            return;
        }
    }

    if (received == '\r' || received == '\n') {
        if (rx_length > 0U || rx_overflow != 0U) {
            write_rx_diagnostic();
        }
        process_command();
        reset_rx_buffer();
        if (received == '\r') {
            rx_pending_cr = 1U;
        }
        return;
    }

    if (rx_length < (RX_BUFFER_SIZE - 1U)) {
        rx_buffer[rx_length] = received;
        ++rx_length;
        rx_buffer[rx_length] = '\0';
    } else {
        rx_overflow = 1U;
    }
}

static void usart2_receive_polling(void)
{
    if ((USART2->SR & USART_SR_RXNE) != 0U) {
        const char received = (char)(USART2->DR & 0xFFU);
        receive_character(received);
    }
}

int main(void)
{
    clock_init();
    gpio_init();
    usart2_init();
    tim2_pwm_init();

    usart2_write("Servo controller ready\r\n");

    for (;;) {
        usart2_receive_polling();
    }
}
