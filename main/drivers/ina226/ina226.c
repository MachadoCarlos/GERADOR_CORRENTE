#include "ina226.h"

/** @brief Constante interna fixa do datasheet INA226 (Equacao 1: 0.00512) */
#define INA226_CALIBRATION_CONST        0.00512f

/** @brief Divisor de resolucao de 15 bits para Current_LSB (2^15 = 32768) */
#define INA226_CURRENT_LSB_DIVISOR      32768.0f

/** @brief Valor maximo permitido no registrador de calibracao (15 bits) */
#define INA226_MAX_CALIBRATION_VAL      32767U

/** @brief LSB fixo da tensao de shunt: 2.5 uV */
#define INA226_SHUNT_VOLTAGE_LSB_V      0.0000025f

/** @brief LSB fixo da tensao de barramento: 1.25 mV */
#define INA226_BUS_VOLTAGE_LSB_V        0.00125f

/** @brief Multiplicador fixo de Power_LSB em relacao ao Current_LSB (25x) */
#define INA226_POWER_LSB_MULTIPLIER     25.0f

/** @brief Bit 15 de reset do registrador de configuracao */
#define INA226_CONFIG_RESET_BIT         0x8000U

/** @brief Bits reservados obrigatorios (bits 14..12 = 100b -> 0x4000) */
#define INA226_CONFIG_RESERVED_BITS     0x4000U

INA226_Status_t INA226_Init(
    INA226_Handle_t *handle,
    uint8_t i2cAddress,
    float shuntResistorOhms,
    float maxExpectedCurrentAmps)
{
    if (handle == NULL) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    if ((i2cAddress > 0x7FU) || (shuntResistorOhms <= 0.0f) || (maxExpectedCurrentAmps <= 0.0f)) {
        return INA226_STATUS_ERR_INVALID_ARG;
    }

    float currentLsb = maxExpectedCurrentAmps / INA226_CURRENT_LSB_DIVISOR;
    float calFloat = INA226_CALIBRATION_CONST / (currentLsb * shuntResistorOhms);

    if (calFloat > (float)INA226_MAX_CALIBRATION_VAL) {
        return INA226_STATUS_ERR_CAL_OVERFLOW;
    }

    handle->i2cAddress = i2cAddress;
    handle->shuntResistorOhms = shuntResistorOhms;
    handle->currentLsbAmps = currentLsb;
    handle->powerLsbWatts = currentLsb * INA226_POWER_LSB_MULTIPLIER;
    handle->calibrationReg = (uint16_t)calFloat;
    handle->txLength = 0U;

    for (size_t i = 0U; i < sizeof(handle->txBuffer); i++) {
        handle->txBuffer[i] = 0U;
    }

    for (size_t i = 0U; i < sizeof(handle->rxBuffer); i++) {
        handle->rxBuffer[i] = 0U;
    }

    return INA226_STATUS_OK;
}

INA226_Status_t INA226_BuildConfigFrame(
    INA226_Handle_t *handle,
    INA226_Averaging_t avg,
    INA226_ConvTime_t busConvTime,
    INA226_ConvTime_t shuntConvTime,
    INA226_OperatingMode_t mode)
{
    if (handle == NULL) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    /*
     * Mapa de bits do registrador 0x00 (Configuration):
     * Bit 15: RST (0)
     * Bits 14..12: Reservado (100b)
     * Bits 11..9: AVG[2:0]
     * Bits 8..6: VBUSCT[2:0]
     * Bits 5..3: VSHCT[2:0]
     * Bits 2..0: MODE[2:0]
     */
    uint16_t configWord = INA226_CONFIG_RESERVED_BITS
                        | (((uint16_t)avg & 0x07U) << 9U)
                        | (((uint16_t)busConvTime & 0x07U) << 6U)
                        | (((uint16_t)shuntConvTime & 0x07U) << 3U)
                        | ((uint16_t)mode & 0x07U);

    handle->txBuffer[0] = INA226_REG_CONFIG;
    handle->txBuffer[1] = (uint8_t)((configWord >> 8U) & 0xFFU);
    handle->txBuffer[2] = (uint8_t)(configWord & 0xFFU);
    handle->txLength = INA226_FRAME_SIZE_WRITE;

    return INA226_STATUS_OK;
}

INA226_Status_t INA226_BuildCalibrationFrame(INA226_Handle_t *handle)
{
    if (handle == NULL) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    handle->txBuffer[0] = INA226_REG_CALIBRATION;
    handle->txBuffer[1] = (uint8_t)((handle->calibrationReg >> 8U) & 0xFFU);
    handle->txBuffer[2] = (uint8_t)(handle->calibrationReg & 0xFFU);
    handle->txLength = INA226_FRAME_SIZE_WRITE;

    return INA226_STATUS_OK;
}

INA226_Status_t INA226_BuildResetFrame(INA226_Handle_t *handle)
{
    if (handle == NULL) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    uint16_t resetWord = INA226_CONFIG_RESET_BIT | INA226_CONFIG_RESERVED_BITS;

    handle->txBuffer[0] = INA226_REG_CONFIG;
    handle->txBuffer[1] = (uint8_t)((resetWord >> 8U) & 0xFFU);
    handle->txBuffer[2] = (uint8_t)(resetWord & 0xFFU);
    handle->txLength = INA226_FRAME_SIZE_WRITE;

    return INA226_STATUS_OK;
}

INA226_Status_t INA226_BuildReadPointerFrame(INA226_Handle_t *handle, uint8_t regAddress)
{
    if (handle == NULL) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    handle->txBuffer[0] = regAddress;
    handle->txLength = 1U;

    return INA226_STATUS_OK;
}

INA226_Status_t INA226_ParseRawU16(const INA226_Handle_t *handle, uint16_t *outRawValue)
{
    if ((handle == NULL) || (outRawValue == NULL)) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    *outRawValue = (uint16_t)(((uint16_t)handle->rxBuffer[0] << 8U) | (uint16_t)handle->rxBuffer[1]);

    return INA226_STATUS_OK;
}

INA226_Status_t INA226_ParseShuntVoltage(const INA226_Handle_t *handle, float *outShuntVolts)
{
    if ((handle == NULL) || (outShuntVolts == NULL)) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    int16_t rawSigned = (int16_t)(((uint16_t)handle->rxBuffer[0] << 8U) | (uint16_t)handle->rxBuffer[1]);
    *outShuntVolts = (float)rawSigned * INA226_SHUNT_VOLTAGE_LSB_V;

    return INA226_STATUS_OK;
}

INA226_Status_t INA226_ParseBusVoltage(const INA226_Handle_t *handle, float *outBusVolts)
{
    if ((handle == NULL) || (outBusVolts == NULL)) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    uint16_t rawUnsigned = (uint16_t)(((uint16_t)handle->rxBuffer[0] << 8U) | (uint16_t)handle->rxBuffer[1]);
    *outBusVolts = (float)rawUnsigned * INA226_BUS_VOLTAGE_LSB_V;

    return INA226_STATUS_OK;
}

INA226_Status_t INA226_ParseCurrent(const INA226_Handle_t *handle, float *outCurrentAmps)
{
    if ((handle == NULL) || (outCurrentAmps == NULL)) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    int16_t rawSigned = (int16_t)(((uint16_t)handle->rxBuffer[0] << 8U) | (uint16_t)handle->rxBuffer[1]);
    *outCurrentAmps = (float)rawSigned * handle->currentLsbAmps;

    return INA226_STATUS_OK;
}

INA226_Status_t INA226_ParsePower(const INA226_Handle_t *handle, float *outPowerWatts)
{
    if ((handle == NULL) || (outPowerWatts == NULL)) {
        return INA226_STATUS_ERR_NULL_PTR;
    }

    uint16_t rawUnsigned = (uint16_t)(((uint16_t)handle->rxBuffer[0] << 8U) | (uint16_t)handle->rxBuffer[1]);
    *outPowerWatts = (float)rawUnsigned * handle->powerLsbWatts;

    return INA226_STATUS_OK;
}