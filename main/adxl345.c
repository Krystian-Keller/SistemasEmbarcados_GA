#include "adxl345.h"

#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_log.h"

#define ADXL345_TAG "ADXL345"

#define ADXL345_I2C_PORT       I2C_NUM_0
#define ADXL345_I2C_SDA_GPIO   21
#define ADXL345_I2C_SCL_GPIO   22
#define ADXL345_I2C_FREQ_HZ    100000

#define ADXL345_I2C_ADDR       0x53

#define ADXL345_REG_DEVID      0x00
#define ADXL345_REG_BW_RATE    0x2C
#define ADXL345_REG_POWER_CTL  0x2D
#define ADXL345_REG_DATA_FORMAT 0x31
#define ADXL345_REG_DATAX0     0x32

#define ADXL345_DEVID_VALUE    0xE5

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_adxl_dev = NULL;

static esp_err_t adxl345_write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    return i2c_master_transmit(s_adxl_dev, data, sizeof(data), -1);
}

static esp_err_t adxl345_read_reg(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(s_adxl_dev, &reg, 1, value, 1, -1);
}

static esp_err_t adxl345_read_regs(uint8_t start_reg, uint8_t *buffer, size_t len)
{
    return i2c_master_transmit_receive(s_adxl_dev, &start_reg, 1, buffer, len, -1);
}

esp_err_t adxl345_init(void)
{
    esp_err_t err;

    i2c_master_bus_config_t bus_config = {
        .i2c_port = ADXL345_I2C_PORT,
        .sda_io_num = ADXL345_I2C_SDA_GPIO,
        .scl_io_num = ADXL345_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err != ESP_OK) {
        ESP_LOGE(ADXL345_TAG, "Erro ao criar barramento I2C: %s", esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = ADXL345_I2C_ADDR,
        .scl_speed_hz = ADXL345_I2C_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(s_i2c_bus, &dev_config, &s_adxl_dev);
    if (err != ESP_OK) {
        ESP_LOGE(ADXL345_TAG, "Erro ao adicionar ADXL345 no barramento: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t devid = 0;
    err = adxl345_read_reg(ADXL345_REG_DEVID, &devid);
    if (err != ESP_OK) {
        ESP_LOGE(ADXL345_TAG, "Erro ao ler DEVID: %s", esp_err_to_name(err));
        return err;
    }

    if (devid != ADXL345_DEVID_VALUE) {
        ESP_LOGE(ADXL345_TAG, "DEVID inesperado: 0x%02X", devid);
        return ESP_ERR_INVALID_RESPONSE;
    }

    err = adxl345_write_reg(ADXL345_REG_BW_RATE, 0x0A);
    if (err != ESP_OK) {
        ESP_LOGE(ADXL345_TAG, "Erro ao configurar BW_RATE: %s", esp_err_to_name(err));
        return err;
    }

    err = adxl345_write_reg(ADXL345_REG_DATA_FORMAT, 0x0B);
    if (err != ESP_OK) {
        ESP_LOGE(ADXL345_TAG, "Erro ao configurar DATA_FORMAT: %s", esp_err_to_name(err));
        return err;
    }

    err = adxl345_write_reg(ADXL345_REG_POWER_CTL, 0x08);
    if (err != ESP_OK) {
        ESP_LOGE(ADXL345_TAG, "Erro ao configurar POWER_CTL: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(ADXL345_TAG, "ADXL345 inicializado com sucesso");
    return ESP_OK;
}

esp_err_t adxl345_read_raw(int16_t *x, int16_t *y, int16_t *z)
{
    if (!x || !y || !z) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[6] = {0};

    esp_err_t err = adxl345_read_regs(ADXL345_REG_DATAX0, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }

    *x = (int16_t)((data[1] << 8) | data[0]);
    *y = (int16_t)((data[3] << 8) | data[2]);
    *z = (int16_t)((data[5] << 8) | data[4]);

    return ESP_OK;
}

esp_err_t adxl345_read_g(float *x_g, float *y_g, float *z_g)
{
    if (!x_g || !y_g || !z_g) {
        return ESP_ERR_INVALID_ARG;
    }

    int16_t x_raw, y_raw, z_raw;
    esp_err_t err = adxl345_read_raw(&x_raw, &y_raw, &z_raw);
    if (err != ESP_OK) {
        return err;
    }

    const float scale = 0.0039f;

    *x_g = x_raw * scale;
    *y_g = y_raw * scale;
    *z_g = z_raw * scale;

    return ESP_OK;
}