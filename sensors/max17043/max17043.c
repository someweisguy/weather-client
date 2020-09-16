#include "max17043.h"

#include <math.h>
#include "i2c.h"

/** Datasheet can be found here:
 *   http://cdn.sparkfun.com/datasheets/Prototyping/MAX17043-MAX17044.pdf
*/

#define DEVICE_ADDRESS  0x36
#define VCELL_REG       0x02
#define SOC_REG         0x04
#define MODE_REG        0x06
#define VERSION_REG     0x08
#define CONFIG_REG      0x0c
#define COMMAND_REG     0xfe


esp_err_t max17043_start()
{
    // send power-on reset command
    const uint8_t por[2] = {0x54, 0x00};
    return i2c_write(DEVICE_ADDRESS, COMMAND_REG, por, 2, 5000);
}

float max17043_get_battery_millivolts()
{
    uint8_t vcell[2];
    if (i2c_read(DEVICE_ADDRESS, VCELL_REG, vcell, 2, 1000) == ESP_OK)
        return ((vcell[0] << 8 | vcell[1]) >> 4) * 1.25;
    else
        return NAN;
}

float max17043_get_battery_percentage()
{
    uint8_t soc[2];
    if (i2c_read(DEVICE_ADDRESS, SOC_REG, soc, 2, 1000) == ESP_OK)
        return (soc[0] << 8 | soc[1]) / 256.0;
    else
        return NAN;
}