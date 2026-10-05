# EE186 Lab 2

## Push Button
(a)
An external interrupt allows the microcontroller to respond to an input event without continuously polling the input. 
In this implementation, SYSCFG connects PC13, the USER button pin, to EXTI13. When the button is pressed, EXTI sets 
the pending flag and sends an interrupt request to the NVIC. Once the interrupt is enabled and eligible
to execute, the processor temporarily suspends the main loop and runs EXTI15_10_IRQHandler. The handler checks the 
EXTI13 pending flag, clears it by writing a 1, and toggles the blue LED on PB7. After the handler finishes, execution
resumes in the main loop. If the pending flag were not cleared, the interrupt request would remain asserted, causing 
repeated execution of the handler and preventing normal main-loop progress.

(b)
Source Code (also in push_button.c file)
```
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
```

Video:

https://github.com/user-attachments/assets/2372e55c-0349-4cd3-8da0-5e8a53159a0a

## Timer

(a) 
TIM4 uses a counter to measure elapsed time. Its prescaler divides the timer’s input clock, 
and the TIM4_CNT register counts the resulting pulses. When the counter reaches the auto-reload value 
and wraps back to zero, an update event sets the UIF flag. If update interrupts are enabled in TIM4 and 
the NVIC, the processor temporarily suspends the main loop and executes TIM4_IRQHandler. The handler 
clears the interrupt flag and changes the active LED, then the processor resumes the main loop. No software 
function continuously counts time; the timer hardware performs the counting. A timer interrupt is triggered 
by a counting event.

(b)
The system clock is configured to 4 MHz. The AHB and APB1 prescalers are both set to 1, so TIM4 receives a 4 MHz input clock.

The counter frequency is:
f_counter = f_TIM4 / (PSC + 1)

Choosing PSC = 3999 gives:

f_counter = 4,000,000 / (3999 + 1)
          = 1,000 Hz

The counter therefore increments every 1 millisecond. Choosing ARR = 999 makes the counter count from 0 through 999, giving 1,000 counts per period.
T = (ARR + 1) / f_counter
  = 1,000 / 1,000
  = 1 second

The update interrupt frequency is therefore:

f_interrupt = 1 / T = 1 Hz

Thus, PSC = 3999 and ARR = 999 produce one interrupt per second, allowing each LED to remain on for one second.

(c)
Source Code: (also in timer.c file)
```
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
```

Video:

https://github.com/user-attachments/assets/29f0ec37-3cf0-4672-a815-b0534f7853c9

## Sorry to interrupt!

Source Code: (also in interrupt.c file)
```
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
```

Video:

https://github.com/user-attachments/assets/35d73b02-e7af-4b4b-8302-589975dd0e1e












