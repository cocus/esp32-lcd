#ifndef I2S_PARALLEL_H
#define I2S_PARALLEL_H

#include <stdint.h>
#include <stddef.h>
#include "soc/i2s_struct.h"

typedef enum
{
    I2S_PARALLEL_BITS_8 = 8,
    I2S_PARALLEL_BITS_16 = 16,
    I2S_PARALLEL_BITS_32 = 32,
} i2s_parallel_cfg_bits_t;

typedef struct
{
    void *memory;
    size_t size;
} i2s_parallel_buffer_desc_t;

typedef struct
{
    int gpio;
    int inverted;
} i2s_parallel_gpio_cfg_t;

typedef enum
{
    I2S_PARALLEL_PIN_D0 = 0,
    I2S_PARALLEL_PIN_D1 = 1,
    I2S_PARALLEL_PIN_D2 = 2,
    I2S_PARALLEL_PIN_D3 = 3,

    I2S_PARALLEL_PIN_HS = 4,
    I2S_PARALLEL_PIN_VS = 5,
    I2S_PARALLEL_PIN_CLK_GATE = 6,
} i2s_parallel_cfg_pins_t;

typedef struct
{
    i2s_parallel_gpio_cfg_t gpio_bus[7];
    int gpio_clk;
    int clkspeed_hz;
    i2s_parallel_cfg_bits_t bits;
    i2s_parallel_buffer_desc_t *bufa;
    i2s_parallel_buffer_desc_t *bufb;
} i2s_parallel_config_t;

void i2s_parallel_setup(i2s_dev_t *dev, const i2s_parallel_config_t *cfg);
void i2s_parallel_flip_to_buffer(i2s_dev_t *dev, int bufid);

#endif
