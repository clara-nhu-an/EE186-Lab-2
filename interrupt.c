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

/* USER button on PC13 */
#define GPIOC_IDR REG32(0x48000810u)

/* SYSCFG clock and external interrupt routing */
#define RCC_APB2ENR    REG32(0x40021060u)
#define SYSCFG_EXTICR4 REG32(0x40010014u)

/* External interrupt controller */
#define EXTI_IMR1  REG32(0x40010400u)
#define EXTI_RTSR1 REG32(0x40010408u)
#define EXTI_FTSR1 REG32(0x4001040Cu)
#define EXTI_PR1   REG32(0x40010414u)

/* EXTI15_10 is interrupt 40: ISER1 bit 8 */
#define NVIC_ISER1 REG32(0xE000E104u)

#define BUTTON_LINE (1u << 13)

/* 1 = button held; 0 = button released */
volatile uint32_t button_held = 0u;

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
    if ((TIM4_SR & 1u) != 0u) {

        /* Always acknowledge the timer interrupt. */
        TIM4_SR = 0u;

        /* Advance only when the button is released.
           The pin check also catches a press whose
           button interrupt has not yet been serviced. */
        if ((button_held == 0u) &&
            ((GPIOC_IDR & BUTTON_LINE) == 0u)) {

            current_color = (current_color + 1u) % 3u;
            show_color(current_color);

            /* Enable SYSCFG clock. */
            RCC_APB2ENR |= (1u << 0);
            (void)RCC_APB2ENR;

            /* PC13: input mode.
               Preserve PC7's configuration for the green LED. */
            GPIOC_MODER &= ~(3u << 26);

            /* No internal pull: use the board's pull-down resistor. */
            GPIOC_PUPDR &= ~(3u << 26);

            /* Mask EXTI13 during setup. */
            EXTI_IMR1 &= ~BUTTON_LINE;

            /* Connect EXTI13 to PC13: port C = 0010. */
            SYSCFG_EXTICR4 &= ~(0xFu << 4);
            SYSCFG_EXTICR4 |=  (0x2u << 4);

            /* Detect BOTH press and release. */
            EXTI_RTSR1 |= BUTTON_LINE;  /* Rising edge: press */
            EXTI_FTSR1 |= BUTTON_LINE;  /* Falling edge: release */

            /* Clear any pending interrupt. */
            EXTI_PR1 = BUTTON_LINE;

            /* Initialize state, including if held at startup. */
            button_held =
                ((GPIOC_IDR & BUTTON_LINE) != 0u) ? 1u : 0u;

            /* Enable EXTI13 and its NVIC interrupt. */
            EXTI_IMR1 |= BUTTON_LINE;
            NVIC_ISER1 = (1u << 8);
        }
    }
}

void EXTI15_10_IRQHandler(void)
{
    if ((EXTI_PR1 & BUTTON_LINE) != 0u) {

        /* Acknowledge the button interrupt. */
        EXTI_PR1 = BUTTON_LINE;

        /* Read the current button level:
           high = pressed, low = released. */
        button_held =
            ((GPIOC_IDR & BUTTON_LINE) != 0u) ? 1u : 0u;
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
