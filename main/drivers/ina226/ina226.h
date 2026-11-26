#ifndef INA226_H
#define INA226_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** @brief Endereco I2C padrao com pinos A0 e A1 em GND */
#define INA226_I2C_ADDR_DEFAULT         0x40U

/** @brief Tamanho do frame de escrita de registrador (1 byte endereco + 2 bytes dado) */
#define INA226_FRAME_SIZE_WRITE         3U

/** @brief Tamanho do frame de leitura de registrador (2 bytes dado) */
#define INA226_FRAME_SIZE_READ          2U

/** @brief Endereco dos registradores internos do INA226 */
#define INA226_REG_CONFIG               0x00U   /**< Registrador de configuracao (R/W) */
#define INA226_REG_SHUNT_VOLTAGE        0x01U   /**< Tensao no resistor shunt (R) - LSB = 2.5 uV */
#define INA226_REG_BUS_VOLTAGE          0x02U   /**< Tensao no barramento VBUS (R) - LSB = 1.25 mV */
#define INA226_REG_POWER                0x03U   /**< Potencia calculada (R) - LSB = 25 * Current_LSB */
#define INA226_REG_CURRENT              0x04U   /**< Corrente calculada (R) - LSB = Current_LSB */
#define INA226_REG_CALIBRATION          0x05U   /**< Registrador de calibracao (R/W) */
#define INA226_REG_MASK_ENABLE          0x06U   /**< Mascara de alertas e flags de conversao (R/W) */
#define INA226_REG_ALERT_LIMIT          0x07U   /**< Limite programavel para alerta (R/W) */
#define INA226_REG_MANUF_ID             0xFEU   /**< ID do fabricante (0x5449 = "TI") */
#define INA226_REG_DIE_ID               0xFFU   /**< ID do chip (0x2260) */

/** @brief Valores esperados dos identificadores de fabrica */
#define INA226_MANUF_ID_VALUE           0x5449U
#define INA226_DIE_ID_VALUE             0x2260U

/**
 * @brief Codigos de retorno das funcoes do driver INA226.
 */
typedef enum {
    INA226_STATUS_OK = 0,               /**< Operacao realizada com sucesso */
    INA226_STATUS_ERR_NULL_PTR,         /**< Ponteiro nulo passado como argumento */
    INA226_STATUS_ERR_INVALID_ARG,      /**< Parametro invalido ou fora da faixa */
    INA226_STATUS_ERR_CAL_OVERFLOW      /**< Valor de calibracao excedeu 15 bits (32767) */
} INA226_Status_t;

/**
 * @brief Numero de amostras para calculo da media interna (bits AVG[2:0] do reg 0x00).
 */
typedef enum {
    INA226_AVG_1 = 0x00U,               /**< 1 amostra (sem media) */
    INA226_AVG_4 = 0x01U,               /**< Media de 4 amostras */
    INA226_AVG_16 = 0x02U,              /**< Media de 16 amostras */
    INA226_AVG_64 = 0x03U,              /**< Media de 64 amostras */
    INA226_AVG_128 = 0x04U,             /**< Media de 128 amostras */
    INA226_AVG_256 = 0x05U,             /**< Media de 256 amostras */
    INA226_AVG_512 = 0x06U,             /**< Media de 512 amostras */
    INA226_AVG_1024 = 0x07U             /**< Media de 1024 amostras */
} INA226_Averaging_t;

/**
 * @brief Tempo de conversao ADC para tensao de barramento e shunt (bits VBUSCT e VSHCT).
 */
typedef enum {
    INA226_CONV_TIME_140US = 0x00U,     /**< 140 us */
    INA226_CONV_TIME_204US = 0x01U,     /**< 204 us */
    INA226_CONV_TIME_332US = 0x02U,     /**< 332 us */
    INA226_CONV_TIME_588US = 0x03U,     /**< 588 us */
    INA226_CONV_TIME_1100US = 0x04U,    /**< 1.1 ms (padrao de fabrica) */
    INA226_CONV_TIME_2116US = 0x05U,    /**< 2.116 ms */
    INA226_CONV_TIME_4156US = 0x06U,    /**< 4.156 ms */
    INA226_CONV_TIME_8244US = 0x07U     /**< 8.244 ms */
} INA226_ConvTime_t;

/**
 * @brief Modos de operacao do INA226 (bits MODE[2:0] do reg 0x00).
 */
typedef enum {
    INA226_MODE_POWER_DOWN = 0x00U,         /**< Desligado (baixo consumo) */
    INA226_MODE_SHUNT_TRIG = 0x01U,         /**< Disparo unico apenas tensao shunt */
    INA226_MODE_BUS_TRIG = 0x02U,           /**< Disparo unico apenas tensao barramento */
    INA226_MODE_SHUNT_BUS_TRIG = 0x03U,     /**< Disparo unico shunt e barramento */
    INA226_MODE_POWER_DOWN_ALT = 0x04U,     /**< Desligado (alternativo) */
    INA226_MODE_SHUNT_CONT = 0x05U,         /**< Continuo apenas tensao shunt */
    INA226_MODE_BUS_CONT = 0x06U,           /**< Continuo apenas tensao barramento */
    INA226_MODE_SHUNT_BUS_CONT = 0x07U      /**< Continuo shunt e barramento (padrao) */
} INA226_OperatingMode_t;

/**
 * @brief Estrutura de instancia do dispositivo INA226 (alocacao estatica).
 */
typedef struct {
    uint8_t i2cAddress;                             /**< Endereco I2C de 7 bits do dispositivo */
    float shuntResistorOhms;                        /**< Valor do resistor shunt em Ohms */
    float currentLsbAmps;                           /**< Resolucao de corrente (Amperes por bit) */
    float powerLsbWatts;                            /**< Resolucao de potencia (Watts por bit) */
    uint16_t calibrationReg;                        /**< Valor calculado para o registrador 0x05 */
    uint8_t txBuffer[INA226_FRAME_SIZE_WRITE];      /**< Buffer interno dedicado para transmissao */
    size_t txLength;                                /**< Quantidade de bytes validos no txBuffer */
    uint8_t rxBuffer[INA226_FRAME_SIZE_READ];       /**< Buffer interno dedicado para recepcao */
} INA226_Handle_t;

/**
 * @brief Inicializa a instancia do INA226 e calcula o registrador de calibracao.
 *
 * @details Calcula Current_LSB = maxExpectedCurrentAmps / 32768 e
 *          CAL = trunc(0.00512 / (Current_LSB * shuntResistorOhms)).
 *
 * @param[out] handle Ponteiro para a instancia do dispositivo.
 * @param[in] i2cAddress Endereco I2C de 7 bits (ex: 0x40).
 * @param[in] shuntResistorOhms Valor do resistor shunt em Ohms (deve ser maior que zero).
 * @param[in] maxExpectedCurrentAmps Corrente maxima esperada em Amperes (maior que zero).
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_Init(
    INA226_Handle_t *handle,
    uint8_t i2cAddress,
    float shuntResistorOhms,
    float maxExpectedCurrentAmps
);

/**
 * @brief Monta o frame de 3 bytes para gravar o registrador de configuracao (0x00).
 *
 * @details Preenche handle->txBuffer e define handle->txLength = 3.
 *
 * @param[in,out] handle Ponteiro para a instancia do dispositivo.
 * @param[in] avg Quantidade de amostras para media interna.
 * @param[in] busConvTime Tempo de conversao para tensao VBUS.
 * @param[in] shuntConvTime Tempo de conversao para tensao VSHUNT.
 * @param[in] mode Modo de operacao do ADC.
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_BuildConfigFrame(
    INA226_Handle_t *handle,
    INA226_Averaging_t avg,
    INA226_ConvTime_t busConvTime,
    INA226_ConvTime_t shuntConvTime,
    INA226_OperatingMode_t mode
);

/**
 * @brief Monta o frame de 3 bytes para gravar o registrador de calibracao (0x05).
 *
 * @details Utiliza o valor calibrationReg calculado em INA226_Init.
 *          Preenche handle->txBuffer e define handle->txLength = 3.
 *
 * @param[in,out] handle Ponteiro para a instancia do dispositivo.
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_BuildCalibrationFrame(INA226_Handle_t *handle);

/**
 * @brief Monta o frame de 3 bytes para solicitar reset via software (bit RST = 1).
 *
 * @param[in,out] handle Ponteiro para a instancia do dispositivo.
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_BuildResetFrame(INA226_Handle_t *handle);

/**
 * @brief Monta o frame de 1 byte com o ponteiro do registrador para leitura posterior.
 *
 * @details Preenche handle->txBuffer[0] = regAddress e define handle->txLength = 1.
 *
 * @param[in,out] handle Ponteiro para a instancia do dispositivo.
 * @param[in] regAddress Endereco do registrador que sera lido.
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_BuildReadPointerFrame(INA226_Handle_t *handle, uint8_t regAddress);

/**
 * @brief Decodifica os 2 bytes de handle->rxBuffer como valor bruto de 16 bits sem sinal.
 *
 * @param[in] handle Ponteiro para a instancia contendo os dados em rxBuffer.
 * @param[out] outRawValue Ponteiro para armazenar o valor de 16 bits (MSB first).
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_ParseRawU16(const INA226_Handle_t *handle, uint16_t *outRawValue);

/**
 * @brief Decodifica handle->rxBuffer para tensao no shunt em Volts (LSB = 2.5 uV).
 *
 * @param[in] handle Ponteiro para a instancia contendo os dados em rxBuffer.
 * @param[out] outShuntVolts Ponteiro para armazenar a tensao shunt em Volts (com sinal).
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_ParseShuntVoltage(const INA226_Handle_t *handle, float *outShuntVolts);

/**
 * @brief Decodifica handle->rxBuffer para tensao no barramento VBUS em Volts (LSB = 1.25 mV).
 *
 * @param[in] handle Ponteiro para a instancia contendo os dados em rxBuffer.
 * @param[out] outBusVolts Ponteiro para armazenar a tensao de barramento em Volts.
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_ParseBusVoltage(const INA226_Handle_t *handle, float *outBusVolts);

/**
 * @brief Decodifica handle->rxBuffer para corrente em Amperes usando currentLsbAmps.
 *
 * @param[in] handle Ponteiro para a instancia contendo os dados em rxBuffer.
 * @param[out] outCurrentAmps Ponteiro para armazenar a corrente em Amperes (com sinal).
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_ParseCurrent(const INA226_Handle_t *handle, float *outCurrentAmps);

/**
 * @brief Decodifica handle->rxBuffer para potencia em Watts usando powerLsbWatts.
 *
 * @param[in] handle Ponteiro para a instancia contendo os dados em rxBuffer.
 * @param[out] outPowerWatts Ponteiro para armazenar a potencia em Watts.
 * @return INA226_Status_t Codigo de status da operacao.
 */
INA226_Status_t INA226_ParsePower(const INA226_Handle_t *handle, float *outPowerWatts);

#ifdef __cplusplus
}
#endif

#endif /* INA226_H */