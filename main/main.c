#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_ili9341.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "esp_lvgl_port.h"
#include "esp_log.h"
#include "esp_task_wdt.h" 

#include "ui/ui.h"
#include "ina226.h"


/* Definicoes de hardware do barramento I2C */
#define I2C_PORT            I2C_NUM_0
#define I2C_PIN_SDA         GPIO_NUM_21
#define I2C_PIN_SCL         GPIO_NUM_22
#define I2C_FREQ_HZ         400000
#define I2C_TIMEOUT_MS      100

/* Parametros do circuito de medicao */
#define INA226_ENDERECO     INA226_DEFAULT_ADDR  /* 0x40 (A1=GND, A0=GND) */
#define SHUNT_OHMS          0.1f                 /* Resistor shunt de 0.1 Ohm (R100) */
#define CORRENTE_MAX_A      3.2f                 /* Corrente maxima esperada em Amperes */

#define LCD_HOST           SPI2_HOST 
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)
#define LCD_H_RES          320
#define LCD_V_RES          240

#define PIN_NUM_MOSI       13
#define PIN_NUM_CLK        14
#define PIN_NUM_CS         15
#define PIN_NUM_DC         2
#define PIN_NUM_RST        -1 
#define PIN_NUM_BCKL       21 

#define TOUCH_HOST         SPI3_HOST
#define PIN_NUM_TOUCH_CLK  25
#define PIN_NUM_TOUCH_MOSI 32
#define PIN_NUM_TOUCH_MISO 39
#define PIN_NUM_TOUCH_CS   33

static const char *TAG = "MAIN";
static spi_device_handle_t touch_spi_handle = NULL;




// --- Callback de Leitura Direta e Segura do XPT2046 (Sem erros de polling do IDF v6) ---
static void my_touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (!touch_spi_handle) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    // Lê o eixo X (Control Byte: 0xD0)
    uint8_t tx_data_x[3] = {0xD0, 0x00, 0x00};
    uint8_t rx_data_x[3] = {0};
    spi_transaction_t trans_x = {
        .length = 24,
        .tx_buffer = tx_data_x,
        .rx_buffer = rx_data_x,
    };

    // Lê o eixo Y (Control Byte: 0x90)
    uint8_t tx_data_y[3] = {0x90, 0x00, 0x00};
    uint8_t rx_data_y[3] = {0};
    spi_transaction_t trans_y = {
        .length = 24,
        .tx_buffer = tx_data_y,
        .rx_buffer = rx_data_y,
    };

    if (spi_device_transmit(touch_spi_handle, &trans_x) == ESP_OK &&
        spi_device_transmit(touch_spi_handle, &trans_y) == ESP_OK) {
        
        uint32_t raw_x = ((rx_data_x[1] << 8) | rx_data_x[2]) >> 3;
        uint32_t raw_y = ((rx_data_y[1] << 8) | rx_data_y[2]) >> 3;

        // Valida se o toque está dentro da faixa útil do conversor A/D
        if (raw_x > 300 && raw_x < 3900 && raw_y > 300 && raw_y < 3900) {
            
            // 1. CRUZA OS EIXOS: Como a tela foi deitada, usamos raw_y no X e raw_x no Y
            uint32_t mapped_x = (raw_y - 300) * LCD_H_RES / 3500;
            uint32_t mapped_y = (raw_x - 300) * LCD_V_RES / 3500;

            // 2. ATRIBUI AS COORDENADAS E INVERTE 
            // DICA: Se ao arrastar o dedo para a direita o cursor for para a esquerda, 
            // mude para "data->point.x = mapped_x;" (retire a subtração). O mesmo vale para o Y.
            data->point.x = LCD_H_RES - mapped_x; 
            data->point.y = LCD_V_RES - mapped_y;

            data->state = LV_INDEV_STATE_PRESSED;
            return;
        }
    }
    
    data->state = LV_INDEV_STATE_RELEASED;
}

void app_main(void)
{
    // 1. Liga o Backlight
    gpio_set_direction(PIN_NUM_BCKL, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_BCKL, 1);

    // 2. Inicializa o Barramento SPI2 para o Display
    spi_bus_config_t lcd_buscfg = {
        .sclk_io_num = PIN_NUM_CLK,
        .mosi_io_num = PIN_NUM_MOSI,
        .miso_io_num = -1,
        .quadwp_io_num = -1, 
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 40 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &lcd_buscfg, SPI_DMA_CH_AUTO));

    // 3. IO do Painel LCD
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = PIN_NUM_DC,
        .cs_gpio_num = PIN_NUM_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 4
        
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));

    // 4. Painel ILI9341 (Ordem RGB padrão para remover a tela verde/roxa)
    esp_lcd_panel_handle_t panel_handle = NULL;
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = PIN_NUM_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

// <-- CORREÇÃO 2: Orientação da Tela (Modo Paisagem) -->
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true)); // Troca os eixos X e Y (Deita a tela)
    
    // Ajuste o espelhamento aqui. Se ficar de cabeça para baixo, troque para (true, false) ou (false, false)
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false)); 
    
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false)); 
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

//    ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false)); 
//    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, false)); 
//    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

    // 5. Barramento SPI3 para o Touch (XPT2046) de forma isolada
    spi_bus_config_t touch_buscfg = {
        .sclk_io_num = PIN_NUM_TOUCH_CLK,
        .mosi_io_num = PIN_NUM_TOUCH_MOSI,
        .miso_io_num = PIN_NUM_TOUCH_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1, 
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TOUCH_HOST, &touch_buscfg, SPI_DMA_DISABLED));

    spi_device_interface_config_t touch_devcfg = {
        .clock_speed_hz = 1 * 1000 * 1000, 
        .mode = 0,
        .spics_io_num = PIN_NUM_TOUCH_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(TOUCH_HOST, &touch_devcfg, &touch_spi_handle));

    // 6. Inicialização do LVGL via esp_lvgl_port (Gerencia display e renderização perfeitamente)
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LCD_H_RES * 40,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .swap_bytes = true, // <-- CORREÇÃO: Resolve o fundo verde aqui!
        }
    };
    lv_display_t *disp = lvgl_port_add_disp(&disp_cfg);

    // 6.1 Registro manual e seguro do Touch
    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, disp);
    lv_indev_set_read_cb(indev, my_touch_read_cb);

    // 7. Inicializa UI do EEZ Studio
    lvgl_port_lock(0);
    ui_init();
    lvgl_port_unlock();

    ESP_LOGI(TAG, "LVGL inicializado com sucesso!");


    uint8_t txBuf[3] = {0};
    uint8_t rxBuf[2] = {0};
    uint8_t regAddr  = 0;
    ina226_escala_t escala = {0};

    /* 1. Inicializa o barramento I2C Master usando a API nativa do ESP-IDF 6 */
    i2c_master_bus_config_t busConfig = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_PORT,
        .scl_io_num = I2C_PIN_SCL,
        .sda_io_num = I2C_PIN_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t busHandle = NULL;
    ESP_ERROR_CHECK(i2c_new_master_bus(&busConfig, &busHandle));

    /* 2. Registra o dispositivo INA226 no barramento */
    i2c_device_config_t devConfig = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA226_ENDERECO,
        .scl_speed_hz = I2C_FREQ_HZ,
    };

    i2c_master_dev_handle_t ina226Handle = NULL;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(busHandle, &devConfig, &ina226Handle));

    /* 3. Verifica a identidade do chip no barramento (Manufacturer ID = 0x5449) */
    regAddr = INA226_REG_MANUF_ID;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(ina226Handle, &regAddr, 1, rxBuf, sizeof(rxBuf), I2C_TIMEOUT_MS));

    if (!INA226_ValidarManufId(rxBuf)) {
        ESP_LOGE(TAG, "INA226 nao encontrado no endereco 0x%02X", INA226_ENDERECO);
        return;
    }

    /* 4. Executa reset por software no INA226 */
    INA226_MontarReset(txBuf);
    ESP_ERROR_CHECK(i2c_master_transmit(ina226Handle, txBuf, sizeof(txBuf), I2C_TIMEOUT_MS));
    vTaskDelay(pdMS_TO_TICKS(10));

    /* 5. Envia a configuracao de operacao (Media 16x, 1.1 ms de conversao, modo continuo) */
    ina226_config_t inaConfig = {
        .media = INA226_AVG_16,
        .tempoConvVbus = INA226_CONV_1100US,
        .tempoConvShunt = INA226_CONV_1100US,
        .modo = INA226_MODE_SHUNT_BUS_CONT
    };
    INA226_MontarConfig(&inaConfig, txBuf);
    ESP_ERROR_CHECK(i2c_master_transmit(ina226Handle, txBuf, sizeof(txBuf), I2C_TIMEOUT_MS));

    /* 6. Calcula e grava o registrador de calibracao (0x05) */
    INA226_MontarCalibracao(SHUNT_OHMS, CORRENTE_MAX_A, &escala, txBuf);
    ESP_ERROR_CHECK(i2c_master_transmit(ina226Handle, txBuf, sizeof(txBuf), I2C_TIMEOUT_MS));

    ESP_LOGI(TAG, "INA226 inicializado (Shunt: %.3f Ohms | LSB: %.3f uA)",
             escala.shuntOhms, escala.currentLsb * 1000000.0f);
    
    while (1) {
        regAddr = INA226_REG_CURRENT;
        esp_err_t ret = i2c_master_transmit_receive(ina226Handle, &regAddr, 1, rxBuf, sizeof(rxBuf), I2C_TIMEOUT_MS);

        if (ret == ESP_OK) {
            float correnteA = INA226_ConverterCorrenteA(&escala, rxBuf);
            ESP_LOGI(TAG, "Corrente: %.4f A (%.2f mA)", correnteA, correnteA * 1000.0f);
        } else {
            ESP_LOGW(TAG, "Falha na leitura I2C: %s", esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}