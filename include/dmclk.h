#ifndef DMCLK_H
#define DMCLK_H

#include <stdint.h>
#include "dmod.h"
#include "dmclk_defs.h"

/**
 * @brief Source of the clock signal
 */
typedef enum 
{
    dmclk_source_unkown = 0,        /**< Unknown clock source */
    dmclk_source_internal,          /**< Internal RC oscillator */
    dmclk_source_external,          /**< External crystal or oscillator */
    dmclk_source_hibernation,       /**< Low-power hibernation clock source */
} dmclk_source_t;

/**
 * @brief IOCTL commands for DMCLK device
 */
typedef enum 
{
    dmclk_ioctl_cmd_get_frequency = 1,   /**< Get current clock frequency */
    dmclk_ioctl_cmd_set_source,          /**< Set clock source */
    dmclk_ioctl_cmd_get_source,          /**< Get clock source */
    dmclk_ioctl_cmd_set_tolerance,       /**< Set frequency tolerance */
    dmclk_ioctl_cmd_get_tolerance,       /**< Get frequency tolerance */
    dmclk_ioctl_cmd_set_oscillator_frequency, /**< Set oscillator frequency */
    dmclk_ioctl_cmd_get_oscillator_frequency, /**< Get oscillator frequency */
    dmclk_ioctl_cmd_set_target_frequency,    /**< Set target frequency */
    dmclk_ioctl_cmd_get_target_frequency,    /**< Get target frequency */
    dmclk_ioctl_cmd_reconfigure,             /**< Reconfigure clock with current settings */

    dmclk_ioctl_cmd_max

} dmclk_ioctl_cmd_t;

#endif // DMCLK_H