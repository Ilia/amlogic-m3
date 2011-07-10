/*
Linux gpio.C

*/
#include <linux/module.h>
#include <mach/am_regs.h>
#include <mach/gpio.h>

struct gpio_addr {
    unsigned long mode_addr;
    unsigned long out_addr;
    unsigned long in_addr;
};
static struct gpio_addr gpio_addrs[] = {
    [PREG_PAD_GPIO0] = {PREG_PAD_GPIO0_EN_N, PREG_PAD_GPIO0_O, PREG_PAD_GPIO0_I},
    [PREG_PAD_GPIO1] = {PREG_PAD_GPIO1_EN_N, PREG_PAD_GPIO1_O, PREG_PAD_GPIO1_I},
    [PREG_PAD_GPIO2] = {PREG_PAD_GPIO2_EN_N, PREG_PAD_GPIO2_O, PREG_PAD_GPIO2_I},
    [PREG_PAD_GPIO3] = {PREG_PAD_GPIO3_EN_N, PREG_PAD_GPIO3_O, PREG_PAD_GPIO3_I},
    [PREG_PAD_GPIO4] = {PREG_PAD_GPIO4_EN_N, PREG_PAD_GPIO4_O, PREG_PAD_GPIO4_I},
    [PREG_PAD_GPIO5] = {PREG_PAD_GPIO5_EN_N, PREG_PAD_GPIO5_O, PREG_PAD_GPIO5_I},
	/*                  0xc8100024,          0xc8100024,        0xc8100028  */
	/*  #define CBUS_REG_OFFSET(reg) ((reg) << 2)                           */
	/*  #define CBUS_REG_ADDR(reg)   (IO_CBUS_BASE + CBUS_REG_OFFSET(reg))  */
	/*  (0xc8100024 - IO_CBUS_BASE) >> 2                                     */
#define TO_CBUS_OFFSET(reg)	(((reg) - IO_CBUS_BASE) >> 2) 
    [PREG_PAD_GPIOAO] = {TO_CBUS_OFFSET(0xc8100024), TO_CBUS_OFFSET(0xc8100024), TO_CBUS_OFFSET(0xc8100028)},
};

int set_gpio_mode(gpio_bank_t bank, int bit, gpio_mode_t mode)
{
    unsigned long addr = gpio_addrs[bank].mode_addr;
#ifdef CONFIG_EXGPIO
    if (bank >= EXGPIO_BANK0) {
        set_exgpio_mode(bank - EXGPIO_BANK0, bit, mode);
        return 0;
    }
#endif
    WRITE_CBUS_REG_BITS(addr, mode, bit, 1);
    return 0;
}

gpio_mode_t get_gpio_mode(gpio_bank_t bank, int bit)
{
    unsigned long addr = gpio_addrs[bank].mode_addr;
#ifdef CONFIG_EXGPIO
    if (bank >= EXGPIO_BANK0) {
        return get_exgpio_mode(bank - EXGPIO_BANK0, bit);
    }
#endif
    return (READ_CBUS_REG_BITS(addr, bit, 1) > 0) ? (GPIO_INPUT_MODE) : (GPIO_OUTPUT_MODE);
}


int set_gpio_val(gpio_bank_t bank, int bit, unsigned long val)
{
    unsigned long addr = gpio_addrs[bank].out_addr;
    unsigned int gpio_bit = 0;
#ifdef CONFIG_EXGPIO
    if (bank >= EXGPIO_BANK0) {
        set_exgpio_val(bank - EXGPIO_BANK0, bit, val);
        return 0;
    }
#endif
	 /* AO output: Because GPIO enable and output use the same register, we need shift 16 bit*/
	if(addr == TO_CBUS_OFFSET(0xc8100024)) { /* AO output need shift 16 bit*/
		gpio_bit = bit + 16;
	} else {
		gpio_bit = bit;
	}
    WRITE_CBUS_REG_BITS(addr, val ? 1 : 0, gpio_bit, 1);

    return 0;
}

unsigned long  get_gpio_val(gpio_bank_t bank, int bit)
{
    unsigned long addr = gpio_addrs[bank].in_addr;
#ifdef CONFIG_EXGPIO
    if (bank >= EXGPIO_BANK0) {
        return get_exgpio_val(bank - EXGPIO_BANK0, bit);
    }
#endif
    return READ_CBUS_REG_BITS(addr, bit, 1);
}

int gpio_to_idx(unsigned gpio)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    int idx = -1;

    switch(bank) {
    case PREG_PAD_GPIO0:
        idx = GPIOA_IDX + bit;
		break;
    case PREG_PAD_GPIO1:
        idx = GPIOB_IDX + bit;
		break;
    case PREG_PAD_GPIO2:
        idx = GPIOC_IDX + bit;
		break;
    case PREG_PAD_GPIO3:
		if( bit < 20 ) {
            idx = GPIO_BOOT_IDX + bit;
		} else {
            idx = GPIOX_IDX + (bit + 12);
		}
		break;
    case PREG_PAD_GPIO4:
        idx = GPIOX_IDX + bit;
		break;
    case PREG_PAD_GPIO5:
		if( bit < 23 ) {
            idx = GPIOY_IDX + bit;
		} else {
            idx = GPIO_CARD_IDX + (bit - 23) ;
		}
		break;
    case PREG_PAD_GPIOAO:
        idx = GPIOAO_IDX + bit;
		break;
	}

    return idx;
}

/**
 * enable gpio edge interrupt
 *
 * @param [in] pin  index number of the chip, start with 0 up to 255
 * @param [in] flag rising(0) or falling(1) edge
 * @param [in] group  this interrupt belong to which interrupt group  from 0 to 7
 */
void gpio_enable_edge_int(int pin , int flag, int group)
{
	unsigned ireg = 0;
    int value = 0;

    group &= 7;
    ireg = GPIO_INTR_GPIO_SEL0 + (group >> 2);

    value = READ_CBUS_REG(ireg);
    value |= (pin << ((group & 3) << 3));
    SET_CBUS_REG_MASK(ireg, value);

    value = READ_CBUS_REG(GPIO_INTR_EDGE_POL);
    value |= ((flag << (16 + group)) | (1 << group));
    SET_CBUS_REG_MASK(GPIO_INTR_EDGE_POL, value);
    //  WRITE_CBUS_REG_BITS(A9_0_IRQ_IN2_INTR_STAT_CLR, 0, group, 1);
}
/**
 * enable gpio level interrupt
 *
 * @param [in] pin  index number of the chip, start with 0 up to 255
 * @param [in] flag high(0) or low(1) level
 * @param [in] group  this interrupt belong to which interrupt group  from 0 to 7
 */
void gpio_enable_level_int(int pin , int flag, int group)
{
	unsigned ireg = 0;

    group &= 7;
    ireg = GPIO_INTR_GPIO_SEL0 + (group >> 2);
    SET_CBUS_REG_MASK(ireg, pin << ((group & 3) << 3));
    SET_CBUS_REG_MASK(GPIO_INTR_EDGE_POL, ((flag << (16 + group)) | (0 << group)));
    //  WRITE_CBUS_REG_BITS(A9_0_IRQ_IN2_INTR_STAT_CLR, 0, group, 1);
}

/**
 * enable gpio interrupt filter
 *
 * @param [in] filter from 0~7(*222ns)
 * @param [in] group  this interrupt belong to which interrupt group  from 0 to 7
 */
void gpio_enable_int_filter(int filter, int group)
{
	unsigned ireg = 0;

    group &= 7;
    ireg = GPIO_INTR_FILTER_SEL0;
    SET_CBUS_REG_MASK(ireg, filter << (group << 2));
}

int gpio_is_valid(int number)
{
    return 1;
}

int gpio_request(unsigned gpio, const char *label)
{
    return 0;
}

void gpio_free(unsigned gpio)
{
}

int gpio_direction_input(unsigned gpio)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    set_gpio_mode(bank, bit, GPIO_INPUT_MODE);
    printk("set gpio%d.%d input\n", bank, bit);
    return 0;
}

int gpio_direction_output(unsigned gpio, int value)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    set_gpio_val(bank, bit, value ? 1 : 0);
    set_gpio_mode(bank, bit, GPIO_OUTPUT_MODE);
    printk("set gpio%d.%d output\n", bank, bit);
    return 0;
}

void gpio_set_value(unsigned gpio, int value)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    set_gpio_val(bank, bit, value ? 1 : 0);
}

int gpio_get_value(unsigned gpio)
{
    gpio_bank_t bank = (gpio_bank_t)(gpio >> 16);
    int bit = gpio & 0xFFFF;
    return (get_gpio_val(bank, bit));
}
