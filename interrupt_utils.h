/**
  ******************************************************************************
  * @file           : interrupt_utils.h
  * @brief          : Interrupt protection utility macros
  ******************************************************************************
  * @copyright (c) 2024-2026 Jennifer A. Gunn (with technological assistance).
  * email       : jennifer.a.gunn@outlook.com
  *
  * MIT License
  * Permission is hereby granted, free of charge, to any person obtaining a copy
  * of this software and associated documentation files (the "Software"), to deal
  * in the Software without restriction, including without limitation the rights
  * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  * copies of the Software, and to permit persons to whom the Software is
  * furnished to do so, subject to the following conditions:
  * The above copyright notice and this permission notice shall be included in all
  * copies or substantial portions of the Software.
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  * SOFTWARE.
  *****************************************************************************
  */

#ifndef INTERRUPT_UTILS_H
#define INTERRUPT_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* ARM Cortex-M CMSIS functions for interrupt control */
#include <cmsis_gcc.h>

/**
  * @brief Save interrupt state and disable all interrupts with memory barriers
  * 
  * Usage:
  *   ATOMIC_ENTER();
  *   // Critical code here
  *   ATOMIC_EXIT();
  * 
  * This pair protects code that must execute atomically without interruption,
  * while preserving the original interrupt state (in case interrupts were 
  * already disabled before entering the critical section).
  * 
  * Uses a local variable _atomic_primask to store the original PRIMASK value.
  * DSB ensures pending memory operations complete before disabling interrupts.
  * ISB ensures instruction fetch is synchronized after disabling interrupts.
  */
#define ATOMIC_ENTER() uint32_t _atomic_primask = __get_PRIMASK(); __disable_irq(); __DSB(); __ISB()

/**
  * @brief Restore the interrupt state saved by ATOMIC_ENTER() with memory barrier
  * 
  * Must be paired with ATOMIC_ENTER() in the same scope.
  * Restores the PRIMASK register to its value before ATOMIC_ENTER() was called.
  * DSB ensures all memory operations in critical section complete before 
  * re-enabling interrupts.
  */
#define ATOMIC_EXIT() __DSB(); __set_PRIMASK(_atomic_primask)

#ifdef __cplusplus
}
#endif

#endif /* INTERRUPT_UTILS_H */
