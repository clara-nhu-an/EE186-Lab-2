#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

/* Clock control */
#define RCC_AHB2ENR REG32(0x4002104Cu)
#define RCC_APB2ENR REG32(0x40021060u)

/* GPIOB: blue LED on PB7 */
#define GPIOB_MODER   REG32(0x48000400u)
#define GPIOB_OTYPER  REG32(0x48000404u)
#define GPIOB_OSPEEDR REG32(0x48000408u)
#define GPIOB_PUPDR   REG32(0x4800040Cu)
#define GPIOB_ODR     REG32(0x48000414u)
#define GPIOB_BSRR    REG32(0x48000418u)

/* GPIOC: USER button on PC13 */
#define GPIOC_MODER REG32(0x48000800u)
#define GPIOC_PUPDR REG32(0x4800080Cu)

/* External interrupt routing */
#define SYSCFG_EXTICR4 REG32(0x40010014u)

/* External interrupt controller */
#define EXTI_IMR1  REG32(0x40010400u)
#define EXTI_RTSR1 REG32(0x40010408u)
#define EXTI_FTSR1 REG32(0x4001040Cu)
#define EXTI_PR1   REG32(0x40010414u)

/* NVIC interrupt set-enable register 1 */
#define NVIC_ISER1 REG32(0xE000E104u)

#define BLUE_LED    (1u << 7)
#define BUTTON_LINE (1u << 13)

/* Called automatically when an EXTI10-15 interrupt occurs. */
void EXTI15_10_IRQHandler(void)
{
    /* Check whether EXTI13 caused the interrupt. */
    if ((EXTI_PR1 & BUTTON_LINE) != 0u) {

        /* Clear EXTI13's pending flag by writing a 1. */
        EXTI_PR1 = BUTTON_LINE;

        /* Toggle PB7: on becomes off, off becomes on. */
        GPIOB_ODR ^= BLUE_LED;
    }
}

int main(void)
{
    /* Enable GPIOB and GPIOC clocks. */
    RCC_AHB2ENR |= (1u << 1) | (1u << 2);
    (void)RCC_AHB2ENR;

    /* Enable SYSCFG clock. */
    RCC_APB2ENR |= (1u << 0);
    (void)RCC_APB2ENR;

    /* Start with blue LED off. */
    GPIOB_BSRR = BLUE_LED << 16;

    /* PB7: push-pull, low speed, no pull-up/pull-down. */
    GPIOB_OTYPER &= ~BLUE_LED;
    GPIOB_OSPEEDR &= ~(3u << 14);
    GPIOB_PUPDR &= ~(3u << 14);

    /* PB7: general-purpose output mode, 01. */
    GPIOB_MODER &= ~(3u << 14);
    GPIOB_MODER |=  (1u << 14);

    /* PC13: input mode, 00. */
    GPIOC_MODER &= ~(3u << 26);

    /* Use the board's external pull-down resistor. */
    GPIOC_PUPDR &= ~(3u << 26);

    /* Keep EXTI13 masked while configuring it. */
    EXTI_IMR1 &= ~BUTTON_LINE;

    /* Route EXTI13 to port C: EXTICR4 bits 7:4 = 0010. */
    SYSCFG_EXTICR4 &= ~(0xFu << 4);
    SYSCFG_EXTICR4 |=  (0x2u << 4);

    /* Interrupt on button press: rising edge only. */
    EXTI_RTSR1 |= BUTTON_LINE;
    EXTI_FTSR1 &= ~BUTTON_LINE;

    /* Clear any pending EXTI13 interrupt before enabling. */
    EXTI_PR1 = BUTTON_LINE;

    /* Unmask EXTI13. */
    EXTI_IMR1 |= BUTTON_LINE;

    /* IRQ 40 belongs to ISER1, bit 40 - 32 = 8. */
    NVIC_ISER1 = (1u << 8);

    while (1) {
        /* Button presses are handled by the ISR. */
    }
}
