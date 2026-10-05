#include <stdint.h>

#define REG32(address) (*(volatile uint32_t *)(address))

/* Clock control */
#define RCC_CR        REG32(0x40021000u)
#define RCC_CFGR      REG32(0x40021008u)
#define RCC_AHB2ENR   REG32(0x4002104Cu)
#define RCC_APB1ENR1  REG32(0x40021058u)

/* GPIOB: blue PB7 and red PB14 */
#define GPIOB_MODER   REG32(0x48000400u)
#define GPIOB_OTYPER  REG32(0x48000404u)
#define GPIOB_OSPEEDR REG32(0x48000408u)
#define GPIOB_PUPDR   REG32(0x4800040Cu)
#define GPIOB_BSRR    REG32(0x48000418u)

/* GPIOC: green PC7 */
#define GPIOC_MODER   REG32(0x48000800u)
#define GPIOC_OTYPER  REG32(0x48000804u)
#define GPIOC_OSPEEDR REG32(0x48000808u)
#define GPIOC_PUPDR   REG32(0x4800080Cu)
#define GPIOC_BSRR    REG32(0x48000818u)

/* TIM4 */
#define TIM4_CR1  REG32(0x40000800u)
#define TIM4_DIER REG32(0x4000080Cu)
#define TIM4_SR   REG32(0x40000810u)
#define TIM4_EGR  REG32(0x40000814u)
#define TIM4_CNT  REG32(0x40000824u)
#define TIM4_PSC  REG32(0x40000828u)
#define TIM4_ARR  REG32(0x4000082Cu)

/* NVIC */
#define NVIC_ISER0 REG32(0xE000E100u)

#define GREEN (1u << 7)
#define BLUE  (1u << 7)
#define RED   (1u << 14)

/* 0 = green, 1 = blue, 2 = red */
volatile uint32_t current_color = 0u;

static void configure_clock(void)
{
    /* Enable MSI and wait until ready. */
    RCC_CR |= (1u << 0);
    while ((RCC_CR & (1u << 1)) == 0u) {
    }

    /* Select MSI as the system clock. */
    RCC_CFGR &= ~3u;
    while ((RCC_CFGR & (3u << 2)) != 0u) {
    }

    /* Select MSI range 6: nominally 4 MHz. */
    RCC_CR = (RCC_CR & ~(0xFu << 4))
           | (6u << 4)
           | (1u << 3);

    /* AHB divider = 1, APB1 divider = 1. */
    RCC_CFGR &= ~((0xFu << 4) | (7u << 8));
}

static void show_color(uint32_t color)
{
    /* Turn all three LEDs off. */
    GPIOB_BSRR = (BLUE | RED) << 16;
    GPIOC_BSRR = GREEN << 16;

    if (color == 0u) {
        GPIOC_BSRR = GREEN;
    } else if (color == 1u) {
        GPIOB_BSRR = BLUE;
    } else {
        GPIOB_BSRR = RED;
    }
}

void TIM4_IRQHandler(void)
{
    /* Check the update interrupt flag, UIF. */
    if ((TIM4_SR & 1u) != 0u) {
        /* Clear timer flags. Only update interrupts are used. */
        TIM4_SR = 0u;

        /* Advance green -> blue -> red -> green. */
        current_color = (current_color + 1u) % 3u;
        show_color(current_color);
    }
}

int main(void)
{
    configure_clock();

    /* Enable GPIOB and GPIOC clocks. */
    RCC_AHB2ENR |= (1u << 1) | (1u << 2);
    (void)RCC_AHB2ENR;

    /* Initialize LED output levels to low. */
    GPIOB_BSRR = (BLUE | RED) << 16;
    GPIOC_BSRR = GREEN << 16;

    /* Push-pull output. */
    GPIOB_OTYPER &= ~(BLUE | RED);
    GPIOC_OTYPER &= ~GREEN;

    /* Low speed. */
    GPIOB_OSPEEDR &= ~((3u << 14) | (3u << 28));
    GPIOC_OSPEEDR &= ~(3u << 14);

    /* No pull-up/pull-down. */
    GPIOB_PUPDR &= ~((3u << 14) | (3u << 28));
    GPIOC_PUPDR &= ~(3u << 14);

    /* Configure PB7, PB14, and PC7 as outputs. */
    GPIOB_MODER &= ~((3u << 14) | (3u << 28));
    GPIOB_MODER |=  (1u << 14) | (1u << 28);

    GPIOC_MODER &= ~(3u << 14);
    GPIOC_MODER |=  (1u << 14);

    /* Start with green on. */
    show_color(current_color);

    /* Enable TIM4 clock: APB1ENR1 bit 2. */
    RCC_APB1ENR1 |= (1u << 2);
    (void)RCC_APB1ENR1;

    /* Stop timer and disable its interrupts during setup. */
    TIM4_CR1 = 0u;
    TIM4_DIER = 0u;

    /* 4 MHz / 4000 = 1000 Hz counter frequency. */
    TIM4_PSC = 3999u;

    /* 1000 counts = 1 second. */
    TIM4_ARR = 999u;
    TIM4_CNT = 0u;

    /* Generate an update to load the prescaler. */
    TIM4_EGR = 1u;

    /* Clear the flag generated during initialization. */
    TIM4_SR = 0u;

    /* Enable update interrupts: UIE bit 0. */
    TIM4_DIER = 1u;

    /* Enable TIM4 IRQ 30 in the NVIC. */
    NVIC_ISER0 = (1u << 30);

    /* Start counting: CEN bit 0. */
    TIM4_CR1 |= 1u;

    while (1) {
        /* TIM4 handles the timing independently. */
    }
}
