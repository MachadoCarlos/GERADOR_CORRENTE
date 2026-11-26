# GERADOR_CORRENTE — Calibrador de Corrente de Fuga (IEC 60601)

Equipamento de bancada baseado em **ESP32 (ESP-IDF v6.x)** desenvolvido para gerar correntes de baixa intensidade com alta resolução, destinado à **calibração e verificação metrológica de analisadores de segurança elétrica** de equipamentos eletromédicos, conforme os ensaios de **corrente de fuga ao paciente** exigidos pela norma **ABNT NBR IEC 60601-1**.

---

## 1. Objetivo e Princípio de Funcionamento

Nos ensaios de segurança elétrica segundo a IEC 60601, os analisadores medem correntes de fuga na ordem de microampères a poucos miliampères (tipicamente de **10 µA a 2 mA** sobre uma impedância de carga de referência de **1 kΩ**).

Para simular e calibrar essas correntes de forma controlada e rastreável, o sistema opera em **malha fechada**:

1. **Geração de Tensão (Atuador — MCP4725):** Um conversor Digital-Analógico (DAC) I2C de 12 bits gera uma tensão de controle (`0 V` a `5,0 V`), aplicando-a ao estágio de saída em série com o circuito sob ensaio.
2. **Medição de Corrente (Sensor — INA226):** Um monitor de corrente/potência I2C de 16 bits mede a queda de tensão diferencial sobre um **resistor shunt** conectado em série com a carga, obtendo o valor real da corrente drenada pelo analisador em calibração.
3. **Controle em Malha Fechada (Firmware):** O microcontrolador compara a corrente medida pelo **INA226** com o setpoint configurado pelo operador e ajusta dinamicamente a palavra digital enviada ao **MCP4725** até atingir e estabilizar a corrente alvo.
4. **Interface Homem-Máquina (LVGL — Em desenvolvimento):** Display gráfico para configuração do setpoint de corrente, visualização em tempo real da corrente medida, tensão aplicada e status de estabilidade.

---

## 2. Arquitetura de Hardware

| Bloco | Componente | Interface | Função no Sistema |
| :--- | :--- | :--- | :--- |
| **MCU** | ESP32 | — | Execução da malha de controle, comunicação I2C e interface gráfica |
| **DAC** | Microchip **MCP4725** (12 bits) | `I2C0` (`0x60`) | Geração da tensão de excitação (`0` a `Vref`) |
| **Sensor** | Texas Instruments **INA226** (16 bits) | `I2C0` (`0x40`) | Leitura de corrente em série via resistor shunt |
| **Display** | Módulo Display TFT/Touch | SPI / Paralelo | Interface gráfica com biblioteca **LVGL** *(próxima etapa)* |

### Barramento I2C0 (Configuração Padrão)
* **SDA:** `GPIO 21`
* **SCL:** `GPIO 22`
* **Frequência SCL:** `400 kHz` (Fast Mode)
* **Pull-ups:** Internos habilitados (recomendado pull-up externo de `4,7 kΩ` para maior imunidade a ruído)

---

## 3. Estrutura do Repositório

```text
GERADOR_CORRENTE/
├── CMakeLists.txt
├── sdkconfig
├── README.md
└── main/
    ├── CMakeLists.txt
    ├── main.c                  # Inicialização do barramento I2C0 e rotina de teste/controle
    └── drivers/
        ├── mcp4725/
        │   ├── mcp4725.h       # Interface e estruturas do driver DAC MCP4725
        │   └── mcp4725.c       # Implementação portável (sans-I/O) do MCP4725
        └── ina226/
            ├── ina226.h        # Interface e estruturas do driver sensor INA226
            └── ina226.c        # Implementação portável (sans-I/O) do INA226
