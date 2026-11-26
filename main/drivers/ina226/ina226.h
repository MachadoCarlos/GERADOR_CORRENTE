/**
 * @file ina226.h
 * @brief Driver puro de protocolo e conversao para o sensor de corrente e potencia INA226.
 *
 * Este modulo e independente de plataforma e barramento (sans-I/O). Ele monta os
 * frames de bytes no formato Big-Endian exigido pelo INA226 e decodifica as
 * leituras brutas em unidades de engenharia (Volts, Amperes, Watts).
 *
 * Caracteristicas principais do chip INA226 (Texas Instruments):
 * - Resolucao interna do ADC: 16 bits.
 * - Faixa de tensao no barramento (VBUS): 0 V a 36 V (LSB fixo de 1.25 mV).
 * - Faixa diferencial no shunt (VSHUNT): -81.9175 mV a +81.92 mV (LSB fixo de 2.5 uV).
 * - Interface: I2C compativel com modos Standard (100 kHz), Fast (400 kHz) e High-Speed (2.94 MHz).
 * - Endereco I2C de 7 bits: 0x40 a 0x4F, definido pelos pinos A1 e A0.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Mapa de Registradores Internos do INA226 (Todos de 16 bits, Big-Endian)
 * ============================================================================ */

/**
 * @brief Registrador de Configuracao (0x00) - Leitura e Escrita.
 *
 * Mapa de bits (15 a 0):
 * - Bit 15     : RST  - Reset do sistema (1 = gera reset interno e restaura padroes).
 * - Bits 14:12 : RSVD - Reservados (devem ser mantidos em 100b = 0x4000).
 * - Bits 11:9  : AVG  - Numero de amostras para media interna (000b = 1 ate 111b = 1024).
 * - Bits 8:6   : VBUSCT - Tempo de conversao da tensao de barramento (140 us a 8.244 ms).
 * - Bits 5:3   : VSHCT  - Tempo de conversao da tensao de shunt (140 us a 8.244 ms).
 * - Bits 2:0   : MODE - Modo de operacao (Power-Down, Triggered ou Continuo).
 *
 * Valor padrao apos reset: 0x4127 (Media=1, VBUSCT=1.1ms, VSHCT=1.1ms, Modo=Continuo).
 */
#define INA226_REG_CONFIG           0x00

/**
 * @brief Registrador de Tensao no Shunt (0x01) - Somente Leitura.
 *
 * Armazena a queda de tensao medida entre os pinos IN+ e IN-.
 * - Formato: Inteiro com sinal de 16 bits em complemento de dois (int16_t).
 * - Peso do bit menos significativo (LSB): 2.5 uV (0.0025 mV).
 * - Fundo de escala: -81.92 mV (0x8000) a +81.9175 mV (0x7FFF).
 */
#define INA226_REG_SHUNT_VOLTAGE    0x01

/**
 * @brief Registrador de Tensao do Barramento VBUS (0x02) - Somente Leitura.
 *
 * Armazena a tensao medida entre o pino VBUS e o GND.
 * - Formato: Inteiro sem sinal de 16 bits (uint16_t).
 * - Peso do bit menos significativo (LSB): 1.25 mV (0.00125 V).
 * - Fundo de escala: 0 V (0x0000) a 40.96 V teorico (limite fisico do pino e 36 V).
 */
#define INA226_REG_BUS_VOLTAGE      0x02

/**
 * @brief Registrador de Potencia (0x03) - Somente Leitura.
 *
 * Calcula internamente o produto da corrente pela tensao do barramento.
 * Exige que o registrador de calibracao (0x05) esteja previamente configurado.
 * - Formato: Inteiro sem sinal de 16 bits (uint16_t).
 * - Peso do bit menos significativo (LSB): Exatamente 25 vezes o Current_LSB.
 */
#define INA226_REG_POWER            0x03

/**
 * @brief Registrador de Corrente (0x04) - Somente Leitura.
 *
 * Calcula internamente a corrente multiplicando VSHUNT pelo valor do registrador
 * de calibracao (0x05) e dividindo por 2048.
 * - Formato: Inteiro com sinal de 16 bits em complemento de dois (int16_t).
 * - Peso do bit menos significativo (LSB): Definido pelo usuario via Current_LSB.
 */
#define INA226_REG_CURRENT          0x04

/**
 * @brief Registrador de Calibracao (0x05) - Leitura e Escrita.
 *
 * Armazena o fator de escala que relaciona o resistor shunt fisico com a corrente
 * maxima esperada. Se permanecer em 0x0000 (padrao de reset), os registradores
 * de Corrente (0x04) e Potencia (0x03) retornam sempre zero.
 * - Formato: Inteiro sem sinal de 15 bits (bit 15 deve ser 0).
 * - Formula do datasheet: CAL = trunc(0.00512 / (Current_LSB * R_shunt)).
 */
#define INA226_REG_CALIBRATION      0x05

/**
 * @brief Registrador de Mascara e Habilitacao de Alerta (0x06) - Leitura e Escrita.
 *
 * Configura qual condicao aciona o pino fisico ALERT (open-drain) e informa
 * flags de status da conversao:
 * - Bit 15 : SOL  - Shunt Over-Voltage (sobretensao no shunt).
 * - Bit 14 : SUL  - Shunt Under-Voltage (subtensao no shunt).
 * - Bit 13 : BOL  - Bus Over-Voltage (sobretensao no barramento).
 * - Bit 12 : BUL  - Bus Under-Voltage (subtensao no barramento).
 * - Bit 11 : POL  - Power Over-Limit (excesso de potencia).
 * - Bit 10 : CNVR - Conversion Ready (gera pulso no pino ALERT ao fim da conversao).
 * - Bit 4  : AFF  - Alert Function Flag (indica que um limite de alerta foi violado).
 * - Bit 3  : CVRF - Conversion Ready Flag (1 = nova amostra pronta para leitura).
 * - Bit 2  : OVF  - Math Overflow Flag (1 = estouro aritmetico no calculo de potencia).
 * - Bit 1  : APOL - Polaridade do pino ALERT (0 = ativo baixo, 1 = ativo alto).
 * - Bit 0  : LEN  - Alert Latch Enable (1 = trava o alerta ate este registrador ser lido).
 */
#define INA226_REG_MASK_ENABLE      0x06

/**
 * @brief Registrador de Limite de Alerta (0x07) - Leitura e Escrita.
 *
 * Armazena o limiar de comparacao para a funcao de alerta selecionada em 0x06.
 * A escala depende do tipo de alerta ativo (mesmo LSB de VSHUNT, VBUS ou POWER).
 */
#define INA226_REG_ALERT_LIMIT      0x07

/**
 * @brief Registrador de ID do Fabricante (0xFE) - Somente Leitura.
 *
 * Retorna sempre 0x5449 (codificacao ASCII para "TI" - Texas Instruments).
 */
#define INA226_REG_MANUF_ID         0xFE

/**
 * @brief Registrador de ID do Silicio (0xFF) - Somente Leitura.
 *
 * Retorna 0x2260, identificando a familia INA226 (bits 15:4 = 0x226, bits 3:0 = revisao).
 */
#define INA226_REG_DIE_ID           0xFF

/* ============================================================================
 * Constantes de Identificacao e Enderecamento
 * ============================================================================ */

/** @brief Endereco I2C padrao de 7 bits quando pinos A1 e A0 estao ligados ao GND. */
#define INA226_DEFAULT_ADDR         0x40

/** @brief Assinatura esperada no registrador 0xFE ("TI"). */
#define INA226_MANUF_ID_EXPECTED    0x5449

/** @brief Assinatura esperada no registrador 0xFF (INA226). */
#define INA226_DIE_ID_EXPECTED      0x2260

/* ============================================================================
 * Tipos de Dados para Configuracao (Registrador 0x00)
 * ============================================================================ */

/**
 * @brief Quantidade de amostras promediadas internamente antes de atualizar os registradores.
 *
 * Aumentar a media reduz o ruido da leitura, mas multiplica o tempo total de atualizacao:
 * Tempo_Total = Media * (Tempo_Conv_VBUS + Tempo_Conv_Shunt).
 */
typedef enum {
    INA226_AVG_1 = 0,    /**< 1 amostra (sem media) */
    INA226_AVG_4,        /**< Media de 4 amostras */
    INA226_AVG_16,       /**< Media de 16 amostras */
    INA226_AVG_64,       /**< Media de 64 amostras */
    INA226_AVG_128,      /**< Media de 128 amostras */
    INA226_AVG_256,      /**< Media de 256 amostras */
    INA226_AVG_512,      /**< Media de 512 amostras */
    INA226_AVG_1024      /**< Media de 1024 amostras */
} ina226_avg_t;

/**
 * @brief Tempo de integracao do ADC interno para cada conversao.
 *
 * Tempos maiores aumentam a rejeicao a ruido de linha e estabilizam a leitura.
 */
typedef enum {
    INA226_CONV_140US = 0,  /**< 140 microssegundos */
    INA226_CONV_204US,      /**< 204 microssegundos */
    INA226_CONV_332US,      /**< 332 microssegundos */
    INA226_CONV_588US,      /**< 588 microssegundos */
    INA226_CONV_1100US,     /**< 1.1 milissegundos (padrao de fabrica) */
    INA226_CONV_2116US,     /**< 2.116 milissegundos */
    INA226_CONV_4156US,     /**< 4.156 milissegundos */
    INA226_CONV_8244US      /**< 8.244 milissegundos */
} ina226_conv_time_t;

/**
 * @brief Modo de operacao do conversor A/D do INA226.
 */
typedef enum {
    INA226_MODE_POWER_DOWN = 0,     /**< Desligado (baixo consumo, ~0.5 uA) */
    INA226_MODE_SHUNT_TRIG,         /**< Disparo unico apenas na tensao de shunt */
    INA226_MODE_BUS_TRIG,           /**< Disparo unico apenas na tensao de barramento */
    INA226_MODE_SHUNT_BUS_TRIG,     /**< Disparo unico em shunt e barramento */
    INA226_MODE_POWER_DOWN_2,       /**< Desligado (equivalente ao modo 0) */
    INA226_MODE_SHUNT_CONT,         /**< Conversao continua apenas no shunt */
    INA226_MODE_BUS_CONT,           /**< Conversao continua apenas no barramento */
    INA226_MODE_SHUNT_BUS_CONT      /**< Conversao continua em shunt e barramento (padrao) */
} ina226_mode_t;

/**
 * @brief Parametros para montagem do registrador de configuracao (0x00).
 */
typedef struct {
    ina226_avg_t media;                 /**< Numero de amostras para media */
    ina226_conv_time_t tempoConvVbus;   /**< Tempo de conversao do canal VBUS */
    ina226_conv_time_t tempoConvShunt;  /**< Tempo de conversao do canal VSHUNT */
    ina226_mode_t modo;                 /**< Modo de operacao do ADC */
} ina226_config_t;

/**
 * @brief Estrutura de escala calculada durante a calibracao.
 *
 * Deve ser alocada estaticamente pela aplicacao e passada nas funcoes de
 * conversao de corrente e potencia para manter o driver 100% reentrante.
 */
typedef struct {
    float shuntOhms;    /**< Valor do resistor shunt em Ohms */
    float currentLsb;   /**< Resolucao de corrente em Amperes por bit (A/LSB) */
    float powerLsb;     /**< Resolucao de potencia em Watts por bit (W/LSB) */
} ina226_escala_t;

/* ============================================================================
 * Funcoes de Montagem de Pacotes (TX -> Microcontrolador transmite via I2C)
 * ============================================================================ */

/**
 * @brief Monta um frame de 3 bytes [REG, MSB, LSB] para escrita em qualquer registrador.
 *
 * O protocolo I2C do INA226 exige que o primeiro byte enviado seja o ponteiro do
 * registrador, seguido pelo byte mais significativo (MSB) e pelo byte menos
 * significativo (LSB) do dado de 16 bits.
 *
 * @param[in]  reg    Endereco do registrador interno (ex: INA226_REG_CONFIG).
 * @param[in]  valor  Palavra de 16 bits a ser gravada no registrador.
 * @param[out] outBuf Buffer de 3 bytes pronto para transmissao I2C.
 */
void INA226_MontarEscritaReg(uint8_t reg, uint16_t valor, uint8_t outBuf[3]);

/**
 * @brief Monta o frame de 3 bytes para configurar o registrador CONFIG (0x00).
 *
 * Combina os campos de media, tempos de conversao e modo de operacao, mantendo
 * o bit reservado 14 em nivel alto (0x4000) conforme exigido pelo datasheet.
 *
 * @param[in]  cfg    Ponteiro para a estrutura com os parametros desejados.
 * @param[out] outBuf Buffer de 3 bytes [0x00, MSB, LSB] pronto para envio.
 */
void INA226_MontarConfig(const ina226_config_t *cfg, uint8_t outBuf[3]);

/**
 * @brief Monta o frame de 3 bytes para executar um reset por software no INA226.
 *
 * Seta o bit 15 (RST) do registrador 0x00. O chip restaura todos os registradores
 * para seus valores padrao de fabrica e limpa automaticamente o bit RST.
 *
 * @param[out] outBuf Buffer de 3 bytes [0x00, 0x80, 0x00] pronto para envio.
 */
void INA226_MontarReset(uint8_t outBuf[3]);

/**
 * @brief Calcula a calibracao e monta o frame de 3 bytes para o registrador CALIBRATION (0x05).
 *
 * Aplica as equacoes oficiais da Texas Instruments:
 * - Current_LSB = Corrente_Maxima / 32768
 * - Power_LSB   = 25 * Current_LSB
 * - CAL         = trunc(0.00512 / (Current_LSB * R_shunt))
 *
 * @param[in]  shuntOhms     Resistencia do shunt em Ohms (deve ser maior que zero).
 * @param[in]  correnteMaxA  Corrente maxima esperada no circuito em Amperes (> 0).
 * @param[out] outEscala     Estrutura preenchida com os LSBs calculados para uso nas leituras.
 * @param[out] outBuf        Buffer de 3 bytes [0x05, MSB, LSB] pronto para envio.
 */
void INA226_MontarCalibracao(float shuntOhms, float correnteMaxA, ina226_escala_t *outEscala, uint8_t outBuf[3]);

/* ============================================================================
 * Funcoes de Decodificacao e Conversao (RX <- Microcontrolador leu 2 bytes)
 * ============================================================================ */

/**
 * @brief Converte 2 bytes Big-Endian lidos do barramento I2C em um inteiro de 16 bits.
 *
 * @param[in] rxBuf Buffer de 2 bytes recebidos [MSB, LSB].
 * @return Valor combinado de 16 bits sem sinal.
 */
uint16_t INA226_DecodificarU16(const uint8_t rxBuf[2]);

/**
 * @brief Verifica se os 2 bytes lidos do registrador 0xFE correspondem a Texas Instruments.
 *
 * @param[in] rxBuf Buffer de 2 bytes lidos de INA226_REG_MANUF_ID.
 * @return true se o valor for 0x5449 ("TI"), false caso contrario.
 */
bool INA226_ValidarManufId(const uint8_t rxBuf[2]);

/**
 * @brief Verifica se os 2 bytes lidos do registrador 0xFF correspondem ao chip INA226.
 *
 * @param[in] rxBuf Buffer de 2 bytes lidos de INA226_REG_DIE_ID.
 * @return true se o valor for 0x2260, false caso contrario.
 */
bool INA226_ValidarDieId(const uint8_t rxBuf[2]);

/**
 * @brief Converte a leitura bruta do registrador BUS_VOLTAGE (0x02) para Volts (V).
 *
 * Multiplica o valor sem sinal de 16 bits pelo LSB fixo de 1.25 mV e divide por 1000.
 *
 * @param[in] rxBuf Buffer de 2 bytes lidos de INA226_REG_BUS_VOLTAGE.
 * @return Tensao do barramento em Volts (0.0 V a 36.0 V).
 */
float INA226_ConverterTensaoBusV(const uint8_t rxBuf[2]);

/**
 * @brief Converte a leitura bruta do registrador SHUNT_VOLTAGE (0x01) para milivolts (mV).
 *
 * Interpreta os 16 bits em complemento de dois (suporta corrente bidirecional)
 * e multiplica pelo LSB fixo de 2.5 uV.
 *
 * @param[in] rxBuf Buffer de 2 bytes lidos de INA226_REG_SHUNT_VOLTAGE.
 * @return Queda de tensao no shunt em milivolts (-81.92 mV a +81.9175 mV).
 */
float INA226_ConverterTensaoShuntMv(const uint8_t rxBuf[2]);

/**
 * @brief Converte a leitura bruta do registrador CURRENT (0x04) para Amperes (A).
 *
 * Interpreta os 16 bits em complemento de dois e multiplica pelo fator currentLsb
 * previamente calculado na funcao INA226_MontarCalibracao().
 *
 * @param[in] escala Ponteiro para a estrutura de escala gerada na calibracao.
 * @param[in] rxBuf  Buffer de 2 bytes lidos de INA226_REG_CURRENT.
 * @return Corrente medida em Amperes (sinal positivo ou negativo conforme sentido).
 */
float INA226_ConverterCorrenteA(const ina226_escala_t *escala, const uint8_t rxBuf[2]);

/**
 * @brief Converte a leitura bruta do registrador POWER (0x03) para Watts (W).
 *
 * Multiplica o valor sem sinal de 16 bits pelo fator powerLsb (25 * currentLsb)
 * previamente calculado na funcao INA226_MontarCalibracao().
 *
 * @param[in] escala Ponteiro para a estrutura de escala gerada na calibracao.
 * @param[in] rxBuf  Buffer de 2 bytes lidos de INA226_REG_POWER.
 * @return Potencia ativa dissipada na carga em Watts (sempre >= 0.0 W).
 */
float INA226_ConverterPotenciaW(const ina226_escala_t *escala, const uint8_t rxBuf[2]);

#ifdef __cplusplus
}
#endif