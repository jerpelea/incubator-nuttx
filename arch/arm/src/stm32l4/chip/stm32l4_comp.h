/****************************************************************************************************
 * arch/arm/src/stm32l4/stm32l4_comp.h
 *
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************************************/

#ifndef __ARCH_ARM_SRC_STM32L4_CHIP_STM32L4_COMP_H
#define __ARCH_ARM_SRC_STM32L4_CHIP_STM32L4_COMP_H

/****************************************************************************************************
 * Pre-processor Definitions
 ****************************************************************************************************/

/* Register Offsets *********************************************************************************/

#define STM32L4_COMP_CSR_OFFSET(n) (((n)-1) << 2)
#define STM32L4_COMP1_CSR_OFFSET   0x0000  /* Comparator 1 control and status register */
#define STM32L4_COMP2_CSR_OFFSET   0x0004  /* Comparator 2 control and status register */

/* Register Addresses *******************************************************************************/

#define STM32L4_COMP_CSR(n)        (STM32L4_COMP_BASE+STM32L4_COMP_CSR_OFFSET(n))
#define STM32L4_COMP1_CSR          (STM32L4_COMP_BASE+STM32L4_COMP1_CSR_OFFSET)
#define STM32L4_COMP2_CSR          (STM32L4_COMP_BASE+STM32L4_COMP2_CSR_OFFSET)

/* Register Bitfield Definitions ********************************************************************/

#define COMP_CSR_EN                (1 << 0)  /* Bit 0:  Comparator enable bit */
                                             /* Bit 1: Reserved */
#define COMP_CSR_PWRMODE_SHIFT     (2)       /* Bits 2-3: Power Mode */
#define COMP_CSR_PWRMODE_MASK      (3 << COMP_CSR_PWRMODE_SHIFT)
#  define COMP_CSR_PWRMODE_HIGH    (0 << COMP_CSR_PWRMODE_SHIFT) /* High speed */
#  define COMP_CSR_PWRMODE_MEDIUM  (1 << COMP_CSR_PWRMODE_SHIFT) /* Medium speed */
#  define COMP_CSR_PWRMODE_LOW     (3 << COMP_CSR_PWRMODE_SHIFT) /* Ultra low power */
#define COMP_CSR_INMSEL_SHIFT      (4)       /* Bits 4-6: Input minus selection bits */
#define COMP_CSR_INMSEL_MASK       (7 << COMP_CSR_INMSEL_SHIFT)
#  define COMP_CSR_INMSEL_25PCT    (0 << COMP_CSR_INMSEL_SHIFT) /* 1/4 VREFINT */
#  define COMP_CSR_INMSEL_50PCT    (1 << COMP_CSR_INMSEL_SHIFT) /* 1/2 VREFINT */
#  define COMP_CSR_INMSEL_75PCT    (2 << COMP_CSR_INMSEL_SHIFT) /* 3/4 VREFINT */
#  define COMP_CSR_INMSEL_VREF     (3 << COMP_CSR_INMSEL_SHIFT) /* VREFINT */
#  define COMP_CSR_INMSEL_DAC1     (4 << COMP_CSR_INMSEL_SHIFT) /* DAC Channel1 */
#  define COMP_CSR_INMSEL_DAC2     (5 << COMP_CSR_INMSEL_SHIFT) /* DAC Channel2 */
#  define COMP1_CSR_INMSEL_PB1     (6 << COMP_CSR_INMSEL_SHIFT) /* PB1 */
#  define COMP1_CSR_INMSEL_PC4     (7 << COMP_CSR_INMSEL_SHIFT) /* PC4 */
#  define COMP2_CSR_INMSEL_PB3     (6 << COMP_CSR_INMSEL_SHIFT) /* PB3 */
#  define COMP2_CSR_INMSEL_PB7     (7 << COMP_CSR_INMSEL_SHIFT) /* PB7 */
#define COMP_CSR_INPSEL            (1 << 7)  /* Bit 7:  Input plus selection bit */
#  define COMP1_CSR_INPSEL_PC5     (0)
#  define COMP1_CSR_INPSEL_PB2     COMP_CSR_INPSEL
#  define COMP2_CSR_INPSEL_PB4     (0)
#  define COMP2_CSR_INPSEL_PB6     COMP_CSR_INPSEL
                                             /* Bits 8-14: Reserved */
#define COMP2_CSR_WINMODE          (1 << 9)  /* Bit 9:  Windows mode selection bit (COMP2 only) */
#  define COMP2_CSR_WINMODE_NOCONN (0)                /* Comparator 2 input not connected to Comparator 1 */
#  define COMP2_CSR_WINMODE_CONN   COMP2_CSR_WINMODE  /* Comparator 2 input connected to Comparator 1 */
#define COMP_CSR_POLARITY          (1 << 15) /* Bit 15: Polarity selection bit */
#  define COMP_CSR_POLARITY_NORMAL (0)
#  define COMP_CSR_POLARITY_INVERT COMP_CSR_POLARITY
#define COMP_CSR_HYST_SHIFT        (16)      /* Bits 16-17: Hysteresis selection bits */
#define COMP_CSR_HYST_MASK         (3 << COMP_CSR_HYST_SHIFT)
#  define COMP_CSR_HYST_NONE       (0 << COMP_CSR_HYST_SHIFT) /* No hysteresis */
#  define COMP_CSR_HYST_LOW        (1 << COMP_CSR_HYST_SHIFT) /* Low hysteresis */
#  define COMP_CSR_HYST_MEDIUM     (2 << COMP_CSR_HYST_SHIFT) /* Medium hysteresis */
#  define COMP_CSR_HYST_HIGH       (3 << COMP_CSR_HYST_SHIFT) /* High hysteresis */
#define COMP_CSR_BLANKING_SHIFT    (18)      /* Bits 18-20: Blanking source selection bits */
#define COMP_CSR_BLANKING_MASK     (7 << COMP_CSR_BLANKING_SHIFT)
#  define COMP_CSR_BLANKING_NONE      (0 << COMP_CSR_BLANKING_SHIFT) /* No blanking */
#  define COMP1_CSR_BLANKING_TIM1OC5  (1 << COMP_CSR_BLANKING_SHIFT) /* TIM1 OC5 is blanking source */
#  define COMP1_CSR_BLANKING_TIM2OC3  (2 << COMP_CSR_BLANKING_SHIFT) /* TIM2 OC3 is blanking source */
#  define COMP1_CSR_BLANKING_TIM3OC3  (4 << COMP_CSR_BLANKING_SHIFT) /* TIM3 OC3 is blanking source */
#  define COMP2_CSR_BLANKING_TIM3OC4  (1 << COMP_CSR_BLANKING_SHIFT) /* TIM3 OC4 is blanking source */
#  define COMP2_CSR_BLANKING_TIM8OC5  (2 << COMP_CSR_BLANKING_SHIFT) /* TIM8 OC5 is blanking source */
#  define COMP2_CSR_BLANKING_TIM15OC1 (4 << COMP_CSR_BLANKING_SHIFT) /* TIM15 OC1 is blanking source */
                                             /* Bit 21: Reserved */
#define COMP_CSR_BRGEN             (1 << 22) /* Bit 22: Scaler bridge enable */
#define COMP_CSR_SCALEN            (1 << 23) /* Bit 23: Voltage scaler enable bit */
                                             /* Bits 24-29: Reserved */
#define COMP_CSR_VALUE             (1 << 30) /* Bit 30: Comparator output status bit */
#define COMP_CSR_LOCK              (1 << 31) /* Bit 31: CSR register lock bit */
#  define COMP_CSR_LOCK_RW         (0)
#  define COMP_CSR_LOCK_RO         COMP_CSR_LOCK

#endif /* __ARCH_ARM_SRC_STM32L4_CHIP_STM32L4_COMP_H */
