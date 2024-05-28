#include <asm/io.h>
#include "../../inc/address_map_arm.h"
#include "../../inc/interrupt_ID.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Takumi");
MODULE_DESCRIPTION("Intel SoC FPGA Countdown Timer");

#define TIM_CLK_RATE_HZ 100000000 //Hz
#define TIM_RES_MSEC 10 //msec
#define TIM_CLK_CNT_PER_RES ((TIM_CLK_RATE_HZ/1000)*TIM_RES_MSEC)
#define TIM_COUNTER_HIGH ((TIM_CLK_CNT_PER_RES >> 16) & 0xFFFF)
#define TIM_COUNTER_LOW ((TIM_CLK_CNT_PER_RES >> 0) & 0xFFFF)

void * LW_virtual; // Lightweight bridge base address
volatile int *HEX03_ptr; // virtual address for the HEX0-HEX3 port
volatile int *HEX45_ptr; // virtual address for the HEX4-HEX5 port
volatile int *Timer_ptr; // virtual address for the Timer port
volatile int *SW_ptr;

int time;
int key_select;
int pause;

// display codes of the text "Intel SoC FPGA" on HEX
const char hex_code[] = {
    0b00111111, //0
    0b00000110, //1
    0b01011001, //2
    0b01001111, //3
    0b01100110, //4
    0b01101101, //5
    0b01111101, //6
    0b00000111, //7
    0b01111111, //8
    0b01101111, //9
};

void segmentDecoder(int number){
    int donvi_d, chuc_d, donvi_m, chuc_m, donvi_s, chuc_s;
    donvi_d = (time >> 0) & 0xF;
    chuc_d = (time >> 4) & 0xF;
    donvi_m = (time >> 8) & 0xF;
    chuc_m = (time >> 12) & 0xF;
    donvi_s = (time >> 16) & 0xF;
    chuc_s = (time >> 20) & 0xF;

    *HEX03_ptr = (hex[chuc_m] << 24) | (hex[donvi_m] << 16) | (hex_code[chuc_d] << 8) | hex_code[donvi_d];
    *HEX45_ptr = (hex_code[chuc_s] << 8) | hex_code[donvi_m];
}

irq_handler_t key_irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
    key_select = *(KEY_ptr + 3);
    if (key_select & 0x1) {             // phan tram giay
        if (*SW_ptr > 0x99) {
            time  = 0x99;
        } else {
            time = *SW_ptr;
        }
    } else if (key_select & 0x2) {      // second
        if (*SW_ptr > 0x59) {
            time  = (0x59 << 8) | time;
        } else {
            time = (*SW_ptr << 8) | time;   
        }
    } else if (key_select & 0x3) {      // minute
        if (*SW_ptr > 0x59) {
            time  = (0x59 << 16) | time;
        } else {
            time = (*SW_ptr << 16) | time;   
        }
    } else {
        pause = !pause;
    }

    segmentDecoder(time);
    // Clear the edgecapture register (clears current interrupt)
    *(KEY_ptr + 3) = 0xF;
    return (irq_handler_t) IRQ_HANDLED;
}

// Timer interrupt handler
irqreturn_t timer_irq_handler(int irq, void *dev_id)
{
    if (pause) {
        // clear timer irq
        *Timer_ptr = *Timer_ptr & (~0x1);
        return IRQ_HANDLED;
    }

    if (--time == 0) {
        *HEX03_ptr = 0b00111111;
        *HEX45_ptr = 0b00111111;
        *Timer_ptr = *Timer_ptr & (~0x1);
        return IRQ_HANDLED;
    }

    segmentDecoder(time);

    // clear timer irq
    *Timer_ptr = *Timer_ptr & (~0x1);

    return IRQ_HANDLED;
}

// module init
static int __init initialize_txt_display(void)
{
    printk(KERN_INFO "init txt_display module\n");

    // init variables
    pause = 0;

    // generate a virtual address for the FPGA lightweight bridge
    LW_virtual = ioremap_nocache (LW_BRIDGE_BASE, LW_BRIDGE_SPAN);

    // init HEX pointer, clear all HEX
    HEX03_ptr = LW_virtual + HEX3_HEX0_BASE;
    HEX45_ptr = LW_virtual + HEX5_HEX4_BASE;
    *HEX03_ptr = 0;
    *HEX45_ptr = 0;

    // init key
    KEY_ptr = LW_virtual + KEY_BASE; // init virtual address for KEY port
    // Clear the PIO edgecapture register (clear any pending interrupt)
    *(KEY_ptr + 3) = 0xF;
    // Enable IRQ generation for the 4 buttons
    *(KEY_ptr + 2) = 0xF;

    // init SW
    SW_ptr = LW_virtual + SW_BASE;

    // init Timer pointer
    // start timer with continuous mode and interrupt every 100ms
    Timer_ptr = LW_virtual + TIMER0_BASE;
    *(Timer_ptr + 2) = TIM_COUNTER_LOW;
    *(Timer_ptr + 3) = TIM_COUNTER_HIGH;
    *(Timer_ptr + 1) = 0x7;

    // Register the interrupt handler.
    if (request_irq (KEYS_IRQ, (irq_handler_t) key_irq_handler, IRQF_SHARED,
        "pushbutton_irq_handler", (void *) (key_irq_handler))){
            printk(KERN_ERR "Failed to request Key\n");
            return -EBUSY;
    }

    if (request_irq (INTERVAL_TIMER_IRQi, timer_irq_handler, IRQF_SHARED,
    "timer_irq_handler", (void *) (timer_irq_handler))){
        printk(KERN_ERR "Failed to request Timer\n");
        free_irq (KEYS_IRQ, (void*) key_irq_handler);
        return -EBUSY;
    }

    return 0;
}

// module exit
static void __exit cleanup_txt_display(void)
{
    *(Timer_ptr + 1) = 0x8; // stop timer
    *HEX03_ptr = 0; // clear HEX display
    *HEX45_ptr = 0;
    iounmap (LW_virtual); // unmap the IO space
    free_irq (INTERVAL_TIMER_IRQi, (void*) timer_irq_handler); // unregister interrupt
    free_irq (KEYS_IRQ, (void*) key_irq_handler);

    printk(KERN_INFO "exit txt_display module\n");
}

module_init(initialize_txt_display);
module_exit(cleanup_txt_display);