/*
 * Copyright (C) 2014 Loci Controls Inc.
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     cpu_cc2538
 * @ingroup     drivers_periph_timer
 * @{
 *
 * @file
 * @brief       Low-level timer driver implementation for the CC2538 CPU
 *
 * @author      Ian Martin <ian@locicontrols.com>
 *
 * @}
 */

#include <assert.h>
#include <stdint.h>

#include "vendor/hw_gptimer.h"
#include "vendor/hw_memmap.h"

#include "board.h"
#include "cpu.h"
#include "periph/timer.h"
#include "periph_conf.h"

#define ENABLE_DEBUG (0)
#include "debug.h"
#if 0
#define LOAD_VALUE              (0x30D4)
#else
#define LOAD_VALUE              (0x1)
#endif

#define TIMER_A_IRQ_MASK        (0x000000ff)
#define TIMER_B_IRQ_MASK        (0x0000ff00)


/* GPTIMER_CTL Bits */
#define TBEN                    GPTIMER_CTL_TBEN
#define TAEN                    GPTIMER_CTL_TAEN

/* GPTIMER_TnMR Bits */
#define TNMIE                   GPTIMER_TAMR_TAMIE
#define TNCDIR                  GPTIMER_TAMR_TACDIR

typedef struct {
    uint16_t mask;
    uint16_t flag;
} _isr_cfg_t;

static const _isr_cfg_t chn_isr_cfg[] = {
    { .mask = TIMER_A_IRQ_MASK, .flag = GPTIMER_IMR_TAMIM | GPTIMER_IMR_TATOIM},
    { .mask = TIMER_B_IRQ_MASK, .flag = GPTIMER_IMR_TBMIM}
};

/**
 * @brief Timer state memory
 */
static timer_isr_ctx_t isr_ctx[TIMER_NUMOF];

/* enable timer interrupts */
static inline void _irq_enable(tim_t tim)
{
    DEBUG("%s(%u)\n", __FUNCTION__, tim);

    if (tim < TIMER_NUMOF) {
        IRQn_Type irqn = GPTIMER_0A_IRQn + (2 * tim);

        NVIC_SetPriority(irqn, TIMER_IRQ_PRIO);
        NVIC_EnableIRQ(irqn);

        if (timer_config[tim].chn == 2) {
            irqn++;
            NVIC_SetPriority(irqn, TIMER_IRQ_PRIO);
            NVIC_EnableIRQ(irqn);
        }
    }
}

static inline cc2538_gptimer_t *dev(tim_t tim)
{
    assert(tim < TIMER_NUMOF);

    return ((cc2538_gptimer_t *)(GPTIMER0_BASE | (((uint32_t)tim) << 12)));
}

/**
 * @brief Setup the given timer
 *
 */
int timer_init(tim_t tim, unsigned long freq, timer_cb_t cb, void *arg)
{
    DEBUG("%s(%u, %lu, %p, %p)\n", __FUNCTION__, tim, freq, cb, arg);

    if (tim >= TIMER_NUMOF) {
        return -1;
    }

    /* Save the callback function: */
    isr_ctx[tim].cb  = cb;
    isr_ctx[tim].arg = arg;

    /* Enable the clock for this timer: */
    SYS_CTRL->RCGCGPT |= (1 << tim);

    /* Disable this timer before configuring it: */
    dev(tim)->CTL = 0;

    uint32_t prescaler = 0;
    uint32_t chan_mode = TNMIE | GPTIMER_PERIODIC_MODE;
    if (timer_config[tim].cfg == GPTMCFG_32_BIT_TIMER) {
        /* Count up in periodic mode */
        chan_mode |= TNCDIR ;

        if (timer_config[tim].chn > 1) {
            DEBUG("Invalid timer_config. Multiple channels are available only in 16-bit mode.");
            return -1;
        }
#ifndef QEMU
        if (freq != sys_clock_freq()) {
            DEBUG("In 32-bit mode, the GPTimer frequency must equal the system clock frequency (%u).\n",
                  (unsigned)sys_clock_freq());
            return -1;
        }
#endif
	/* In 32 Bit counting mode, the timer will generate un interrupt
	after 1 ms */
        dev(tim)->TAILR = LOAD_VALUE;
    }
    else if (timer_config[tim].cfg == GPTMCFG_16_BIT_TIMER) {
        prescaler = sys_clock_freq();
        prescaler += freq / 2;
        prescaler /= freq;
        if (prescaler >   0) prescaler--;
        if (prescaler > 255) prescaler = 255;

        dev(tim)->TAPR = prescaler;
        dev(tim)->TAILR = LOAD_VALUE;
    } else if (timer_config[tim].cfg == GPTMCFG_32_BIT_REAL_TIME_CLOCK) {
        chan_mode |= TNCDIR ;
        dev(tim)->TAMATCHR = 0x2;
    }
    else {
        DEBUG("timer_init: invalid timer config must be 16 or 32Bit mode!\n");
        return -1;
    }

    dev(tim)->CFG = timer_config[tim].cfg;
    dev(tim)->CTL = TAEN;
    dev(tim)->TAMR = chan_mode;

    if (timer_config[tim].chn > 1) {
        dev(tim)->TBMR = chan_mode;
        dev(tim)->TBPR = prescaler;
        dev(tim)->TBILR = LOAD_VALUE;
        /* Enable the timer: */
        dev(tim)->CTL = TBEN | TAEN;
    } else {
	/* 32-bit count down mode, 32-bit realtime mode */
        dev(tim)->TBMR = chan_mode;
        /* Enable the timer: */
        dev(tim)->CTL = TBEN | TAEN;
        dev(tim)->TBILR = 0x0;
    }

    /* Enable interrupts for given timer: */
    _irq_enable(tim);

    return 0;
}

static inline unsigned int _timer_read(tim_t tim)
{

    if (tim >= TIMER_NUMOF) {
        return 0;
    }

    if (timer_config[tim].cfg == GPTMCFG_32_BIT_TIMER) {
        return dev(tim)->TAV;
    } else if (timer_config[tim].cfg == GPTMCFG_32_BIT_REAL_TIME_CLOCK) {
	return dev(tim)->TAR;
    }
    else {
        return LOAD_VALUE - (dev(tim)->TAV & 0xffff);
    }
}
int timer_set_absolute(tim_t tim, int channel, unsigned int value)
{
    uint32_t offset;
    DEBUG("%s(%u, %u, %u)\n", __FUNCTION__, tim, channel, value);
#ifdef QEMU
    value = value / 1000;
#endif
    offset = value - _timer_read(tim);
    if ((tim >= TIMER_NUMOF) || (channel >= (int)timer_config[tim].chn) ) {
        return -1;
    }
    /* clear any pending match interrupts */
    dev(tim)->ICR = chn_isr_cfg[channel].flag;
    if (channel == 0) {
        if (timer_config[tim].cfg == GPTMCFG_32_BIT_TIMER) {
        	dev(tim)->TAILR =  offset;
	} else /* rtc count up */
        	dev(tim)->TAMATCHR = (timer_config[tim].cfg == GPTMCFG_32_BIT_TIMER) ?
                             value : (LOAD_VALUE - value);
    }
    else {
        dev(tim)->TBMATCHR = (LOAD_VALUE - value);
    }
    dev(tim)->IMR |= chn_isr_cfg[channel].flag;

    return 1;
}

int timer_clear(tim_t tim, int channel)
{
    DEBUG("%s(%u, %u)\n", __FUNCTION__, tim, channel);

    if ( (tim >= TIMER_NUMOF) || (channel >= (int)timer_config[tim].chn) ) {
        return -1;
    }
    /* clear interupt flags */
    dev(tim)->IMR &= ~(chn_isr_cfg[channel].flag);

    return 1;
}

/*
 * The timer channels 1 and 2 are configured to run with the same speed and
 * have the same value (they run in parallel), so only on of them is returned.
 */
unsigned int timer_read(tim_t tim)
{
    /* DEBUG("%s(%u)\n", __FUNCTION__, tim); */
    unsigned int res = _timer_read(tim);
#ifdef QEMU
    res *= 1000;
#endif
    return res;
}

/*
 * For stopping the counting of all channels.
 */
void timer_stop(tim_t tim)
{
    DEBUG("%s(%u)\n", __FUNCTION__, tim);

    if (tim < TIMER_NUMOF) {
        dev(tim)->CTL = 0;
    }
}

void timer_start(tim_t tim)
{
    DEBUG("%s(%u)\n", __FUNCTION__, tim);

    if (tim < TIMER_NUMOF) {
        if (timer_config[tim].chn == 1) {
            dev(tim)->CTL = TAEN;
        }
        else if (timer_config[tim].chn == 2) {
            dev(tim)->CTL = TBEN | TAEN;
        }
    }
}

/**
 * @brief   timer interrupt handler
 *
 * @param[in] num   GPT instance number
 * @param[in] chn   channel number (0=A, 1=B)
 */
static void gptm_irq_handler(tim_t tim, int channel)
{
  DEBUG("%s(%u,%d)\n", __FUNCTION__, tim, channel);
  assert(tim < TIMER_NUMOF);
  assert(channel < (int)timer_config[tim].chn);

  uint32_t mis;
  /* Latch the active interrupt flags */
  mis = dev(tim)->MIS & chn_isr_cfg[channel].mask;
  /* Clear the latched interrupt flags */
  dev(tim)->ICR = mis;

  if (mis & chn_isr_cfg[channel].flag) {
      /* Disable further match interrupts for this timer/channel */
      dev(tim)->IMR &= ~chn_isr_cfg[channel].flag;
      /* Invoke the callback function */
      isr_ctx[tim].cb(isr_ctx[tim].arg, channel);
  }

    cortexm_isr_end();
}

void isr_timer0_chan0(void) {gptm_irq_handler(0, 0);}
void isr_timer0_chan1(void) {gptm_irq_handler(0, 1);}
void isr_timer1_chan0(void) {gptm_irq_handler(1, 0);}
void isr_timer1_chan1(void) {gptm_irq_handler(1, 1);}
void isr_timer2_chan0(void) {gptm_irq_handler(2, 0);}
void isr_timer2_chan1(void) {gptm_irq_handler(2, 1);}
void isr_timer3_chan0(void) {gptm_irq_handler(3, 0);}
void isr_timer3_chan1(void) {gptm_irq_handler(3, 1);}
