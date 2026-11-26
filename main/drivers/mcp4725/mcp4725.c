#include "mcp4725.h"

/** @brief Comando Write DAC Register and EEPROM (C2=0, C1=1, C0=1) */
#define MCP4725_CMD_WRITE_DAC_EEPROM    0x60U

MCP4725_Status_t MCP4725_Init(MCP4725_Handle_t *handle, uint8_t i2cAddress, float vrefVolts)
{
    if (handle == NULL) {
        return MCP4725_STATUS_ERR_NULL_PTR;
    }

    if ((vrefVolts <= 0.0f) || (i2cAddress > 0x7FU)) {
        return MCP4725_STATUS_ERR_INVALID_ARG;
    }

    handle->i2cAddress = i2cAddress;
    handle->vrefVolts = vrefVolts;
    handle->lastRawValue = 0U;
    handle->powerMode = MCP4725_POWER_NORMAL;
    handle->txLength = 0U;

    for (size_t i = 0U; i < sizeof(handle->txBuffer); i++) {
        handle->txBuffer[i] = 0U;
    }

    for (size_t i = 0U; i < sizeof(handle->rxBuffer); i++) {
        handle->rxBuffer[i] = 0U;
    }

    return MCP4725_STATUS_OK;
}

MCP4725_Status_t MCP4725_BuildFastWriteFrame(
    MCP4725_Handle_t *handle,
    uint16_t rawValue,
    MCP4725_PowerMode_t powerMode)
{
    if (handle == NULL) {
        return MCP4725_STATUS_ERR_NULL_PTR;
    }

    if (rawValue > MCP4725_MAX_VALUE) {
        rawValue = MCP4725_MAX_VALUE;
    }

    /*
     * Formato Fast Mode (2 bytes apos endereco I2C):
     * Byte 0: [C2=0][C1=0][PD1][PD0][D11][D10][D9][D8]
     * Byte 1: [D7][D6][D5][D4][D3][D2][D1][D0]
     */
    handle->txBuffer[0] = (uint8_t)(((powerMode & 0x03U) << 4U) | ((rawValue >> 8U) & 0x0FU));
    handle->txBuffer[1] = (uint8_t)(rawValue & 0xFFU);
    handle->txLength = MCP4725_FRAME_SIZE_FAST_WRITE;

    handle->lastRawValue = rawValue;
    handle->powerMode = powerMode;

    return MCP4725_STATUS_OK;
}

MCP4725_Status_t MCP4725_BuildVoltageWriteFrame(
    MCP4725_Handle_t *handle,
    float voltageVolts)
{
    if (handle == NULL) {
        return MCP4725_STATUS_ERR_NULL_PTR;
    }

    float clampedVoltage = voltageVolts;
    if (clampedVoltage < 0.0f) {
        clampedVoltage = 0.0f;
    } else if (clampedVoltage > handle->vrefVolts) {
        clampedVoltage = handle->vrefVolts;
    }

    uint16_t rawValue = (uint16_t)(((clampedVoltage / handle->vrefVolts) * (float)MCP4725_MAX_VALUE) + 0.5f);

    return MCP4725_BuildFastWriteFrame(
        handle,
        rawValue,
        MCP4725_POWER_NORMAL
    );
}

MCP4725_Status_t MCP4725_BuildEepromWriteFrame(
    MCP4725_Handle_t *handle,
    uint16_t rawValue,
    MCP4725_PowerMode_t powerMode)
{
    if (handle == NULL) {
        return MCP4725_STATUS_ERR_NULL_PTR;
    }

    if (rawValue > MCP4725_MAX_VALUE) {
        rawValue = MCP4725_MAX_VALUE;
    }

    /*
     * Formato Write DAC Register & EEPROM (3 bytes apos endereco I2C):
     * Byte 0: [C2=0][C1=1][C0=1][X][X][PD1][PD0][X]
     * Byte 1: [D11][D10][D9][D8][D7][D6][D5][D4]
     * Byte 2: [D3][D2][D1][D0][X][X][X][X]
     */
    handle->txBuffer[0] = (uint8_t)(MCP4725_CMD_WRITE_DAC_EEPROM | ((powerMode & 0x03U) << 1U));
    handle->txBuffer[1] = (uint8_t)((rawValue >> 4U) & 0xFFU);
    handle->txBuffer[2] = (uint8_t)((rawValue & 0x0FU) << 4U);
    handle->txLength = MCP4725_FRAME_SIZE_WRITE_REG;

    handle->lastRawValue = rawValue;
    handle->powerMode = powerMode;

    return MCP4725_STATUS_OK;
}

MCP4725_Status_t MCP4725_ParseStatusFrame(
    const MCP4725_Handle_t *handle,
    MCP4725_DeviceStatus_t *outStatus)
{
    if ((handle == NULL) || (outStatus == NULL)) {
        return MCP4725_STATUS_ERR_NULL_PTR;
    }

    /*
     * Formato de leitura de 5 bytes do MCP4725:
     * Byte 0: [RDY/BSY][POR][X][X][X][PD1][PD0][X]
     * Byte 1: [DAC D11..D4]
     * Byte 2: [DAC D3..D0][X][X][X][X]
     * Byte 3: [X][PD1][PD0][X][EEPROM D11..D8]
     * Byte 4: [EEPROM D7..D0]
     */
    outStatus->eepromReady = ((handle->rxBuffer[0] & 0x80U) != 0U);
    outStatus->powerOnReset = ((handle->rxBuffer[0] & 0x40U) != 0U);
    outStatus->dacPowerMode = (MCP4725_PowerMode_t)((handle->rxBuffer[0] >> 1U) & 0x03U);
    outStatus->dacValue = (uint16_t)(((uint16_t)handle->rxBuffer[1] << 4U) | ((uint16_t)handle->rxBuffer[2] >> 4U));

    outStatus->eepromPowerMode = (MCP4725_PowerMode_t)((handle->rxBuffer[3] >> 5U) & 0x03U);
    outStatus->eepromValue = (uint16_t)((((uint16_t)handle->rxBuffer[3] & 0x0FU) << 8U) | (uint16_t)handle->rxBuffer[4]);

    return MCP4725_STATUS_OK;
}

MCP4725_Status_t MCP4725_RawToVoltage(
    const MCP4725_Handle_t *handle,
    uint16_t rawValue,
    float *outVoltageVolts)
{
    if ((handle == NULL) || (outVoltageVolts == NULL)) {
        return MCP4725_STATUS_ERR_NULL_PTR;
    }

    uint16_t clampedRaw = (rawValue > MCP4725_MAX_VALUE) ? MCP4725_MAX_VALUE : rawValue;
    *outVoltageVolts = ((float)clampedRaw / (float)MCP4725_MAX_VALUE) * handle->vrefVolts;

    return MCP4725_STATUS_OK;
}