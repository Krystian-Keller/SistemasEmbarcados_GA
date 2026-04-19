#ifndef ADXL345_H
#define ADXL345_H

#include "esp_err.h"

esp_err_t adxl345_init(void);
esp_err_t adxl345_read_raw(int16_t *x, int16_t *y, int16_t *z);
esp_err_t adxl345_read_g(float *x_g, float *y_g, float *z_g);

#endif