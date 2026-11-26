#ifndef MCP4725_H
#define MCP4725_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** @brief Endereco I2C padrao com pino A0 em GND (versao A0) */
#define MCP4725_I2C_ADDR_DEFAULT        0x60U

/** @brief Endereco I2C alternativo com pino A0 em VCC (versao A0) */
#define MCP4725_I2C_ADDR_ALT            0x61U

/** @brief Resolucao maxima do DAC de 12 bits (0 a 4095) */
#define MCP4725_MAX_VALUE               4095U

/** @brief Tamanho do frame de escrita no modo rapido (Fast Mode) */
#define MCP4725_FRAME_SIZE_FAST_WRITE   2U

/** @brief Tamanho do frame de escrita no registrador DAC e EEPROM */
#define MCP4725_FRAME_SIZE_WRITE_REG    3U

/** @brief Tamanho do frame de leitura de status e registradores */
#define MCP4725_FRAME_SIZE_READ_STATUS  5U

/**
 * @brief Codigos de retorno das funcoes do driver MCP4725.
 */
typedef enum {
    MCP4725_STATUS_OK = 0,              /**< Operacao realizada com sucesso */
    MCP4725_STATUS_ERR_NULL_PTR,        /**< Ponteiro nulo passado como argumento */
    MCP4725_STATUS_ERR_INVALID_ARG      /**< Parametro fora da faixa permitida */
} MCP4725_Status_t;

/**
 * @brief Modos de operacao e Power-Down (bits PD1 e PD0 do datasheet).
 */
typedef enum {
    MCP4725_POWER_NORMAL = 0x00U,       /**< Operacao normal (saida ativa) */
    MCP4725_POWER_DOWN_1K = 0x01U,      /**< Saida conectada ao GND via resistor de 1 kOhm */
    MCP4725_POWER_DOWN_100K = 0x02U,    /**< Saida conectada ao GND via resistor de 100 kOhm */
    MCP4725_POWER_DOWN_500K = 0x03U     /**< Saida conectada ao GND via resistor de 500 kOhm */
} MCP4725_PowerMode_t;

/**
 * @brief Estrutura de instancia do dispositivo MCP4725 (alocacao estatica).
 */
typedef struct {
    uint8_t i2cAddress;                                 /**< Endereco I2C de 7 bits do dispositivo */
    float vrefVolts;                                    /**< Tensao de referencia VDD em Volts */
    uint16_t lastRawValue;                              /**< Ultimo valor bruto de 12 bits configurado */
    MCP4725_PowerMode_t powerMode;                      /**< Modo de operacao atual configurado */
    uint8_t txBuffer[MCP4725_FRAME_SIZE_WRITE_REG];     /**< Buffer interno dedicado para transmissao */
    size_t txLength;                                    /**< Quantidade de bytes validos no txBuffer */
    uint8_t rxBuffer[MCP4725_FRAME_SIZE_READ_STATUS];   /**< Buffer interno dedicado para recepcao */
} MCP4725_Handle_t;

/**
 * @brief Estrutura com os dados decodificados da leitura de status do chip.
 */
typedef struct {
    bool eepromReady;                   /**< Flag RDY/BSY: true se ciclo de escrita EEPROM finalizou */
    bool powerOnReset;                  /**< Flag POR: true se o circuito de reset foi acionado */
    MCP4725_PowerMode_t dacPowerMode;   /**< Modo de energia ativo no registrador DAC */
    uint16_t dacValue;                  /**< Valor atual de 12 bits no registrador DAC */
    MCP4725_PowerMode_t eepromPowerMode;/**< Modo de energia armazenado na EEPROM */
    uint16_t eepromValue;               /**< Valor de 12 bits armazenado na EEPROM */
} MCP4725_DeviceStatus_t;

/**
 * @brief Inicializa a estrutura de controle do MCP4725 sem tocar no hardware.
 *
 * @param[out] handle Ponteiro para a instancia do dispositivo.
 * @param[in] i2cAddress Endereco I2C de 7 bits (ex: 0x60).
 * @param[in] vrefVolts Tensao de referencia VDD em Volts (deve ser maior que zero).
 * @return MCP4725_Status_t Codigo de status da operacao.
 */
MCP4725_Status_t MCP4725_Init(MCP4725_Handle_t *handle, uint8_t i2cAddress, float vrefVolts);

/**
 * @brief Monta o frame interno de 2 bytes para escrita rapida no DAC (volatil).
 *
 * @details Preenche handle->txBuffer e define handle->txLength = 2.
 *
 * @param[in,out] handle Ponteiro para a instancia do dispositivo.
 * @param[in] rawValue Valor digital de 12 bits (0 a 4095).
 * @param[in] powerMode Modo de operacao ou Power-Down desejado.
 * @return MCP4725_Status_t Codigo de status da operacao.
 */
MCP4725_Status_t MCP4725_BuildFastWriteFrame(
    MCP4725_Handle_t *handle,
    uint16_t rawValue,
    MCP4725_PowerMode_t powerMode
);

/**
 * @brief Converte uma tensao desejada em Volts e monta o frame interno de escrita rapida.
 *
 * @details Preenche handle->txBuffer e define handle->txLength = 2.
 *
 * @param[in,out] handle Ponteiro para a instancia do dispositivo.
 * @param[in] voltageVolts Tensao de saida desejada em Volts (0.0 a vrefVolts).
 * @return MCP4725_Status_t Codigo de status da operacao.
 */
MCP4725_Status_t MCP4725_BuildVoltageWriteFrame(
    MCP4725_Handle_t *handle,
    float voltageVolts
);

/**
 * @brief Monta o frame interno de 3 bytes para gravar no DAC e na EEPROM interna.
 *
 * @details Preenche handle->txBuffer e define handle->txLength = 3.
 *          Apos transmitir este frame, aguarde eepromReady ficar true (ate 50 ms).
 *
 * @param[in,out] handle Ponteiro para a instancia do dispositivo.
 * @param[in] rawValue Valor digital de 12 bits (0 a 4095).
 * @param[in] powerMode Modo de operacao ou Power-Down desejado.
 * @return MCP4725_Status_t Codigo de status da operacao.
 */
MCP4725_Status_t MCP4725_BuildEepromWriteFrame(
    MCP4725_Handle_t *handle,
    uint16_t rawValue,
    MCP4725_PowerMode_t powerMode
);

/**
 * @brief Decodifica os 5 bytes armazenados em handle->rxBuffer apos leitura I2C.
 *
 * @param[in] handle Ponteiro para a instancia contendo os dados em rxBuffer.
 * @param[out] outStatus Ponteiro para a estrutura que recebera os dados decodificados.
 * @return MCP4725_Status_t Codigo de status da operacao.
 */
MCP4725_Status_t MCP4725_ParseStatusFrame(
    const MCP4725_Handle_t *handle,
    MCP4725_DeviceStatus_t *outStatus
);

/**
 * @brief Converte um valor bruto de 12 bits para tensao em Volts usando vrefVolts.
 *
 * @param[in] handle Ponteiro para a instancia do dispositivo.
 * @param[in] rawValue Valor digital de 12 bits (0 a 4095).
 * @param[out] outVoltageVolts Ponteiro para armazenar a tensao calculada.
 * @return MCP4725_Status_t Codigo de status da operacao.
 */
MCP4725_Status_t MCP4725_RawToVoltage(
    const MCP4725_Handle_t *handle,
    uint16_t rawValue,
    float *outVoltageVolts
);

#ifdef __cplusplus
}
#endif

#endif /* MCP4725_H */