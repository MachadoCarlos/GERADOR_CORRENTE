/**
 * @file ina226.c
 * @brief Implementacao das funcoes de protocolo e calculo do driver INA226.
 */

#include <math.h>
#include "ina226.h"

/* ============================================================================
 * Constantes Internas Definidas pelo Datasheet do INA226
 * ============================================================================ */

/** @brief Mascara do bit 15 (RST) do registrador 0x00 para reset por software. */
#define INA226_CFG_RESET_BIT        (1U << 15)

/** @brief Bit 14 do registrador 0x00 deve ser mantido em 1 conforme datasheet (0x4000). */
#define INA226_CFG_FIXED_BIT        (1U << 14)

/** @brief Resolucao fixa do ADC diferencial do shunt: 2.5 microvolts por bit. */
#define INA226_SHUNT_V_LSB_UV       2.5f

/** @brief Resolucao fixa do ADC de tensao de barramento: 1.25 milivolts por bit. */
#define INA226_BUS_V_LSB_MV         1.25f

/** @brief Constante interna fixa de escalonamento da equacao de calibracao do chip. */
#define INA226_CALIB_CONST          0.00512f

/* ============================================================================
 * Implementacao das Funcoes de Montagem de Pacotes (TX)
 * ============================================================================ */

void INA226_MontarEscritaReg(uint8_t reg, uint16_t valor, uint8_t outBuf[3])
{
    if (!outBuf) {
        return;
    }

    /* O INA226 utiliza protocolo Big-Endian: primeiro o byte mais significativo */
    outBuf[0] = reg;
    outBuf[1] = (uint8_t)((valor >> 8) & 0xFFU);
    outBuf[2] = (uint8_t)(valor & 0xFFU);
}

void INA226_MontarConfig(const ina226_config_t *cfg, uint8_t outBuf[3])
{
    if (!cfg || !outBuf) {
        return;
    }

    /* Monta a palavra de 16 bits deslocando cada campo para sua posicao no registrador 0x00 */
    uint16_t regVal = INA226_CFG_FIXED_BIT |
                      ((uint16_t)(cfg->media & 0x07U) << 9) |
                      ((uint16_t)(cfg->tempoConvVbus & 0x07U) << 6) |
                      ((uint16_t)(cfg->tempoConvShunt & 0x07U) << 3) |
                      ((uint16_t)(cfg->modo & 0x07U));

    INA226_MontarEscritaReg(INA226_REG_CONFIG, regVal, outBuf);
}

void INA226_MontarReset(uint8_t outBuf[3])
{
    INA226_MontarEscritaReg(INA226_REG_CONFIG, INA226_CFG_RESET_BIT, outBuf);
}

void INA226_MontarCalibracao(float shuntOhms, float correnteMaxA, ina226_escala_t *outEscala, uint8_t outBuf[3])
{
    if (!outEscala || !outBuf || shuntOhms <= 0.0f || correnteMaxA <= 0.0f) {
        return;
    }

    /* Divide a corrente maxima esperada pelos 15 bits uteis de magnitude (2^15 = 32768) */
    outEscala->shuntOhms = shuntOhms;
    outEscala->currentLsb = correnteMaxA / 32768.0f;

    /* Pela arquitetura interna do INA226, o LSB de potencia e sempre 25x o LSB de corrente */
    outEscala->powerLsb = 25.0f * outEscala->currentLsb;

    /* Calcula o valor inteiro de 15 bits a ser gravado no registrador 0x05 */
    uint16_t calVal = (uint16_t)truncf(INA226_CALIB_CONST / (outEscala->currentLsb * shuntOhms));

    INA226_MontarEscritaReg(INA226_REG_CALIBRATION, calVal, outBuf);
}

/* ============================================================================
 * Implementacao das Funcoes de Decodificacao e Conversao (RX)
 * ============================================================================ */

uint16_t INA226_DecodificarU16(const uint8_t rxBuf[2])
{
    if (!rxBuf) {
        return 0;
    }

    /* Reconstroi a palavra de 16 bits a partir dos dois bytes recebidos em Big-Endian */
    return ((uint16_t)rxBuf[0] << 8) | (uint16_t)rxBuf[1];
}

bool INA226_ValidarManufId(const uint8_t rxBuf[2])
{
    return (INA226_DecodificarU16(rxBuf) == INA226_MANUF_ID_EXPECTED);
}

bool INA226_ValidarDieId(const uint8_t rxBuf[2])
{
    return (INA226_DecodificarU16(rxBuf) == INA226_DIE_ID_EXPECTED);
}

float INA226_ConverterTensaoBusV(const uint8_t rxBuf[2])
{
    uint16_t raw = INA226_DecodificarU16(rxBuf);

    /* Converte de unidades de 1.25 mV para Volts */
    return ((float)raw * INA226_BUS_V_LSB_MV) / 1000.0f;
}

float INA226_ConverterTensaoShuntMv(const uint8_t rxBuf[2])
{
    /* Cast explicito para int16_t preserva o sinal negativo (complemento de dois) */
    int16_t raw = (int16_t)INA226_DecodificarU16(rxBuf);

    /* Converte de unidades de 2.5 uV para milivolts */
    return ((float)raw * INA226_SHUNT_V_LSB_UV) / 1000.0f;
}

float INA226_ConverterCorrenteA(const ina226_escala_t *escala, const uint8_t rxBuf[2])
{
    if (!escala) {
        return 0.0f;
    }

    /* O registrador 0x04 e assinado (int16_t) para permitir medicoes bidirecionais */
    int16_t raw = (int16_t)INA226_DecodificarU16(rxBuf);

    return (float)raw * escala->currentLsb;
}

float INA226_ConverterPotenciaW(const ina226_escala_t *escala, const uint8_t rxBuf[2])
{
    if (!escala) {
        return 0.0f;
    }

    /* O registrador 0x03 e sempre sem sinal (uint16_t) */
    uint16_t raw = INA226_DecodificarU16(rxBuf);

    return (float)raw * escala->powerLsb;
}