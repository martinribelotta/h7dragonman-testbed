#include <execute.h>

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <bsp_driver_sd.h>
#include <fatfs.h>
#include <ff.h>
#include <lwip.h>
#include <main.h>
#include <sfud.h>
#include <usbd_cdc_if.h>

#include <stm32h7xx_hal_qspi.h>

/* Read Operations */
#define READ_CMD 0x03
#define FAST_READ_CMD 0x0B
#define DUAL_OUT_FAST_READ_CMD 0x3B
#define DUAL_INOUT_FAST_READ_CMD 0xBB
#define QUAD_OUT_FAST_READ_CMD 0x6B
#define QUAD_INOUT_FAST_READ_CMD 0xEB

#define N25Q128A_DUMMY_CYCLES_READ 8
#define N25Q128A_DUMMY_CYCLES_READ_QUAD 10

#define CMDFUNC(name) int name(int argc, const char *const *argv)
#define uart485 huart8

static int retcode = 0;

extern UART_HandleTypeDef huart8;
extern UART_HandleTypeDef huart1;

static CMDFUNC(cmd_gpio);
static CMDFUNC(cmd_help);
static CMDFUNC(cmd_mrd);
static CMDFUNC(cmd_mwr);
static CMDFUNC(cmd_reset);
static CMDFUNC(cmd_sdinfo);
static CMDFUNC(cmd_sdls);
static CMDFUNC(cmd_qspi);
static CMDFUNC(cmd_usb);
static CMDFUNC(cmd_eth);
static CMDFUNC(cmd_485);
static CMDFUNC(cmd_can);

static CMDFUNC(cmd_retcode)
{
    printf("%d\r\n", retcode);
    return retcode;
}

struct {
    const char *cmd;
    int (*func)(int, const char *const *);
    const char *help;
} commands[] = {
    {"$?", cmd_retcode, "show last return code"},
    {"help", cmd_help, "show this help"},
    {"mrd", cmd_mrd, "Memory read"},
    {"mwr", cmd_mwr, "Memory write"},
    {"reset", cmd_reset, "System reset"},
    {"gpio", cmd_gpio, "gpio subsystem"},
    {"sdinfo", cmd_sdinfo, "show sd information"},
    {"sdls", cmd_sdls, "ls on SDCard"},
    {"qspi", cmd_qspi, "qspi subsystem"},
    {"usb", cmd_usb, "usb subsystem"},
    {"eth", cmd_eth, "ethernet subsystem"},
    {"485", cmd_485, "rs485 subsystem"},
    {"can", cmd_can, "CANBus subsystem"},
};

static int readKey()
{
    uint8_t c;
    if (HAL_UART_Receive(&huart1, &c, 1, 0) == HAL_OK)
        return c;
    return -1;
}

static CMDFUNC(cmd_gpio)
{
    static const struct {
        const char *name;
        uint32_t mode;
        const char *help;
    } modemap[] = {
        {"out", GPIO_MODE_OUTPUT_PP, "Output push pull"},    {"in", GPIO_MODE_INPUT, "Input"},
        {"outod", GPIO_MODE_OUTPUT_OD, "Output open drain"}, {"alt", GPIO_MODE_AF_PP, "Alternate push pull"},
        {"altod", GPIO_MODE_AF_OD, "Alternate open drain"},  {"analog", GPIO_MODE_ANALOG, "Analog"},
    };

    if (argc >= 2) {
        char port;
        int pin;
        int val;
        if (sscanf(argv[1], "P%c%d", &port, &pin) == 2) {
            if (port >= 'A' && port <= 'K' && pin >= 0 && pin <= 15) {
                static GPIO_TypeDef *const portMap[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF,
                                                        GPIOG, GPIOH, GPIOI, GPIOJ, GPIOK};
                GPIO_TypeDef *portPtr = portMap[port - 'A'];
                if (argc >= 3) {
                    if (sscanf(argv[2], "%d", &val) == 1) {
                        // write port value
                        printf("Wrinting %d to %s\r\n", val, argv[1]);
                        HAL_GPIO_WritePin(portPtr, 1 << pin, val);
                        return 0;
                    } else {
                        for (int i = 0; i < (sizeof(modemap) / sizeof(*modemap)); i++) {
                            if (strcmp(modemap[i].name, argv[2]) == 0) {
                                /* Configure IO Direction mode (Input, Output, Alternate or Analog) */
                                printf("Setting %s to %s\r\n", argv[1], modemap[i].name);
                                uint32_t temp = portPtr->MODER;
                                temp &= ~(GPIO_MODER_MODE0 << (pin * 2U));
                                temp |= ((modemap[i].mode & 3) << (pin * 2U));
                                portPtr->MODER = temp;
                                return 0;
                            }
                        }
                    }
                } else {
                    // read port and info
                    static const char *const mode_text[] = {"Input", "Output", "Alternate", "Analog"};
                    val = HAL_GPIO_ReadPin(portPtr, 1 << pin);
                    int mode = (portPtr->MODER >> (pin * 2)) & 0x3;
                    printf("%s = %d (%s", argv[1], val, mode_text[mode]);
                    if (mode == 1) {
                        static const char *const speed_text[] = {"low", "medium", "high", "very high"};
                        int speed = (portPtr->OSPEEDR >> (pin * 2)) & 0x3;
                        if ((portPtr->OTYPER >> pin) & 1)
                            printf(" open drain");
                        printf(" %s speed", speed_text[speed]);
                    } else if (mode == 2) {
                        int hl = (pin >> 3) & 1;
                        int alt = (portPtr->AFR[hl] >> (pin & 0x7)) & 0xF;
                        printf(" AF%d", alt);
                    }
                    static const char *const pull_text[] = {"", "pull up", "pull down", ""};
                    int pull = (portPtr->PUPDR >> (pin * 2)) & 0x03;
                    if (pull > 0)
                        printf(" %s", pull_text[pull]);
                    printf(")\r\n");
                    return 0;
                }
            }
        }
    }
    printf("usage: %s P[A..K][0..15] (0|1|<mode>)\r\nWhere mode is one of:\r\n", argv[0]);
    for (int i = 0; i < (sizeof(modemap) / sizeof(*modemap)); i++)
        printf("  %-15s %s\r\n", modemap[i].name, modemap[i].help);
    return 0;
}

static const char *fpuType()
{
    static const char *typeText[] = {
        "no fpu",
        "sp fpu",
        "dp fpu",
    };
    return typeText[SCB_GetFPUType()];
}

static CMDFUNC(cmd_help)
{
    uint32_t freq = HAL_RCC_GetSysClockFreq();
    int freq_mhz = freq / ((int)1e6);
    int freq_frac = freq % ((int)1e6);
    printf("Build at %s %s\r\nwith " __VERSION__ "\r\n"
           "CPU Cortex-M%d running at %d.%d MHz with %s\r\n\r\n"
           "Availables commands:\r\n",
           __DATE__, __TIME__, __CORTEX_M, freq_mhz, freq_frac, fpuType());
    for (int i = 0; i < (sizeof(commands) / sizeof(*commands)); i++)
        printf("  %-30s %s\r\n", commands[i].cmd, commands[i].help);
    return 0;
}

static int printMemoryCell(int address, int bytes)
{
    switch (bytes) {
    default:
    case 1:
        printf("%02X ", *((uint8_t *)address));
        return 2 + 1;
    case 2:
        printf("%04X ", *((uint16_t *)address));
        return 4 + 1;
    case 4:
        printf("%08lX ", *((uint32_t *)address));
        return 8 + 1;
    case 8:
        printf("%016llX ", *((uint64_t *)address));
        return 16 + 1;
    }
}

static CMDFUNC(cmd_mrd)
{
    // mrd <addr> <count> <size>?
    if (argc < 3)
        goto usage;

    uint32_t address;
    uint32_t count;
    int size = 8;

    if (sscanf(argv[1], "%li", &address) != 1)
        goto usage;

    if (sscanf(argv[2], "%li", &count) != 1)
        goto usage;
    if (argc > 3)
        if (sscanf(argv[3], "%i", &size) != 1)
            goto usage;
    size /= 8;

    printf("\r\n%08lX ", address);
    int nchars = 8 + 1;
    for (int i = 0; i < count; i++) {
        nchars += printMemoryCell(address, size);
        address += size;
        if (nchars > 60) {
            printf("\r\n%08lX ", address);
            nchars = 8 + 1;
        }
    }
    printf("\r\n");
    return 0;

usage:
    printf("Usage: %s <addr> <count> [size:8|16|32|64]\r\n", argv[0]);
    return -1;
}

static CMDFUNC(cmd_mwr)
{
    // mwr <addr> <count> <size>?
    if (argc < 3)
        goto usage;

    uint32_t address;
    uint64_t data;
    int size = 8;

    if (sscanf(argv[1], "%li", &address) != 1)
        goto usage;

    if (sscanf(argv[2], "%lli", &data) != 1)
        goto usage;
    if (argc > 3)
        if (sscanf(argv[3], "%i", &size) != 1)
            goto usage;

    switch (size) {
    default:
    case 8:
        *((uint8_t *)address) = data;
        break;
    case 16:
        *((uint16_t *)address) = data;
        break;
    case 32:
        *((uint32_t *)address) = data;
        break;
    case 64:
        *((uint64_t *)address) = data;
        break;
    }
    printf("Write done\r\n");
    return 0;

usage:
    printf("usage: %s <addr> <data> [size:8|16|32]\r\n", argv[0]);
    return -1;
}

static CMDFUNC(cmd_reset)
{
    NVIC_SystemReset();
    while (1)
        __WFI();
    return 0;
}

static const char *card_type[] = {
    "SDSC",
    "SDHC/SDXC",
    "unknown",
    "secured",
};

static CMDFUNC(cmd_sdinfo)
{
    if (BSP_SD_Init() == MSD_OK) {
        HAL_SD_CardInfoTypeDef info;
        BSP_SD_GetCardInfo(&info);
        printf("CardType:     %s\r\n", card_type[info.CardType]);
        printf("CardVersion:  %" PRIu32 "\r\n", info.CardVersion);
        printf("Class:        %" PRIu32 "\r\n", info.Class);
        printf("RelCardAdd:   %" PRIu32 "\r\n", info.RelCardAdd);
        printf("BlockNbr:     %" PRIu32 "\r\n", info.BlockNbr);
        printf("BlockSize:    %" PRIu32 "\r\n", info.BlockSize);
        printf("LogBlockNbr:  %" PRIu32 "\r\n", info.LogBlockNbr);
        printf("LogBlockSize: %" PRIu32 "\r\n", info.LogBlockSize);
        printf("CardSpeed:    %" PRIu32 "\r\n", info.CardSpeed);
    } else {
        printf("Error init SD\r\n");
    }
    return 0;
}

static CMDFUNC(cmd_sdls)
{
    static DIR dir;
    if (f_mount(&SDFatFS, SDPath, 1) != FR_OK) {
        printf("Error mounting SD\r\n");
        return -1;
    }
    if (f_opendir(&dir, SDPath) == FR_OK) {
        static FILINFO inf;
        while (f_readdir(&dir, &inf) == FR_OK) {
            if (inf.fname[0] == 0)
                break;
            printf("  %s\r\n", inf.fname);
        }
    } else {
        printf("Fail to open SD\r\n");
    }
    if (f_mount(NULL, SDPath, 1) != FR_OK) {
        printf("Error unmounting SD\r\n");
        return -1;
    }
    return 0;
}

static void sfud_demo(uint32_t addr, size_t size, uint8_t *data)
{
    sfud_err result = SFUD_SUCCESS;
    const sfud_flash *flash = sfud_get_device(SFUD_W25_DEVICE_INDEX);
    size_t i;
    /* prepare write data */
    for (i = 0; i < size; i++) {
        data[i] = i;
    }
    /* erase test */
    result = sfud_erase(flash, addr, size);
    if (result == SFUD_SUCCESS) {
        printf("Erase the %s flash data finish. Start from 0x%08X, size is %u.\r\n", flash->name, (unsigned int)addr,
               size);
    } else {
        printf("Erase the %s flash data failed.\r\n", flash->name);
        return;
    }
    /* write test */
    result = sfud_write(flash, addr, size, data);
    if (result == SFUD_SUCCESS) {
        printf("Write the %s flash data finish. Start from 0x%08X, size is %u.\r\n", flash->name, (unsigned int)addr,
               size);
    } else {
        printf("Write the %s flash data failed.\r\n", flash->name);
        return;
    }
    /* read test */
    result = sfud_read(flash, addr, size, data);
    if (result == SFUD_SUCCESS) {
        printf("Read the %s flash data success. Start from 0x%08X, size is %u. The data is:\r\n", flash->name,
               (unsigned int)addr, size);
        printf("Offset (h) 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\r\n");
        for (i = 0; i < size; i++) {
            if (i % 16 == 0) {
                printf("[%08X] ", (unsigned int)(addr + i));
            }
            printf("%02X ", data[i]);
            if (((i + 1) % 16 == 0) || i == size - 1) {
                printf("\r\n");
            }
        }
        printf("\r\n");
    } else {
        printf("Read the %s flash data failed.\r\n", flash->name);
    }
    /* data check */
    for (i = 0; i < size; i++) {
        if (data[i] != i % 256) {
            printf("Read and check write data has an error. Write the %s flash data failed.\r\n", flash->name);
            break;
        }
    }
    if (i == size) {
        printf("The %s flash test is success.\r\n", flash->name);
    }
}

#define SFUD_DEMO_TEST_BUFFER_SIZE 1024
static uint8_t sfud_demo_test_buf[SFUD_DEMO_TEST_BUFFER_SIZE];
static volatile bool qspi_inited = false;

static void sfud_printRet(const char *func, sfud_err errorCode)
{
    static const char *sfud_error_text[] = {
        "success",       "not found or not supported",    "write error", "read error",
        "timeout error", "address is out of flash bound",
    };
    printf("SFUD %s return: %s\r\n", func, sfud_error_text[errorCode]);
}

uint8_t BSP_QSPI_MemoryMappedMode(void)
{
    extern QSPI_HandleTypeDef hqspi;
    QSPI_CommandTypeDef s_command;
    QSPI_MemoryMappedTypeDef s_mem_mapped_cfg;

    /* Configure the command for the read instruction */
    s_command.InstructionMode = QSPI_INSTRUCTION_1_LINE;
    s_command.Instruction = 0x03;
    s_command.AddressMode = QSPI_ADDRESS_1_LINE;
    s_command.AddressSize = QSPI_ADDRESS_24_BITS;
    s_command.AlternateByteMode = QSPI_ALTERNATE_BYTES_NONE;
    s_command.DataMode = QSPI_DATA_1_LINE;
    s_command.DummyCycles = 0;
    s_command.DdrMode = QSPI_DDR_MODE_DISABLE;
    s_command.DdrHoldHalfCycle = QSPI_DDR_HHC_ANALOG_DELAY;
    s_command.SIOOMode = QSPI_SIOO_INST_EVERY_CMD;
    s_command.Address = 0;
    s_command.AlternateBytes = 0;
    s_command.AlternateBytesSize = QSPI_ALTERNATE_BYTES_8_BITS;
    s_command.NbData = 0;

    /* Configure the memory mapped mode */
    s_mem_mapped_cfg.TimeOutActivation = QSPI_TIMEOUT_COUNTER_ENABLE;
    s_mem_mapped_cfg.TimeOutPeriod = 1;

    if (HAL_QSPI_MemoryMapped(&hqspi, &s_command, &s_mem_mapped_cfg) != HAL_OK) {
        return -1;
    }

    SCB_CleanInvalidateDCache_by_Addr((uint32_t *)QSPI_BASE, 1 << hqspi.Init.FlashSize);

    return 0;
}

static void testQPSIMemMap(void)
{
    const int size = 32;
    volatile const uint8_t *buffer = (volatile const uint8_t *)(QSPI_BASE);
    for (int i = 0; i < size; i++) {
        if ((i % 16) == 0) {
            printf("\n%04X ", i);
        }
        printf("%02X ", buffer[i]);
    }
    printf("\n");
}

static CMDFUNC(cmd_qspi)
{
    if (argc == 1)
        goto usage;

    if (strcmp(argv[1], "mmap") == 0) {
        if (strcmp(argv[2], "on") == 0) {
            printf("Enter in mmap mode\n");
            if (BSP_QSPI_MemoryMappedMode() == 0) {
                puts("QSPI in mmap\nTesting mmap:");
                return 0;
            } else {
                puts("QSPI enter mmap error\n");
                return -1;
            }
        }
        if (strcmp(argv[2], "off") == 0) {
            extern QSPI_HandleTypeDef hqspi;
            extern void MX_QUADSPI_Init(void);
            puts("Restart qspi");
            HAL_QSPI_DeInit(&hqspi);
            MX_QUADSPI_Init();
            puts("Done");
            return 0;
        }
        if (strcmp(argv[2], "test") == 0) {
            testQPSIMemMap();
            return 0;
        }
        printf("Unknown action: %s\n%s on|off|test\n", argv[1], argv[0]);
        return 0;
    }

    if (strcmp(argv[1], "freq") == 0) {
        int freq;
        if (argc != 3)
            goto usage;
        if (sscanf(argv[2], "%i", &freq) == 1) {
            RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
            PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_FDCAN | RCC_PERIPHCLK_USART1 |
                                                       RCC_PERIPHCLK_UART8 | RCC_PERIPHCLK_SDMMC | RCC_PERIPHCLK_USB |
                                                       RCC_PERIPHCLK_QSPI;
            PeriphClkInitStruct.PLL2.PLL2M = 4;
            PeriphClkInitStruct.PLL2.PLL2N = freq;
            PeriphClkInitStruct.PLL2.PLL2P = 2;
            PeriphClkInitStruct.PLL2.PLL2Q = 2;
            PeriphClkInitStruct.PLL2.PLL2R = 2;
            PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_1;
            PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOWIDE;
            PeriphClkInitStruct.PLL2.PLL2FRACN = 0;
            PeriphClkInitStruct.QspiClockSelection = RCC_QSPICLKSOURCE_PLL2;
            PeriphClkInitStruct.SdmmcClockSelection = RCC_SDMMCCLKSOURCE_PLL;
            PeriphClkInitStruct.FdcanClockSelection = RCC_FDCANCLKSOURCE_PLL;
            PeriphClkInitStruct.Usart234578ClockSelection = RCC_USART234578CLKSOURCE_D2PCLK1;
            PeriphClkInitStruct.Usart16ClockSelection = RCC_USART16CLKSOURCE_D2PCLK2;
            PeriphClkInitStruct.UsbClockSelection = RCC_USBCLKSOURCE_PLL;
            if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
                Error_Handler();
            }
            qspi_inited = false;
            return 0;
        }
        goto usage;
    }

    sfud_flash *flash = sfud_get_device(SFUD_W25_DEVICE_INDEX);

    if (!qspi_inited) {
        if (sfud_init() == SFUD_SUCCESS) {
            printf("qspi init OK\r\n");
            /* enable qspi fast read mode, set four data lines width */
            sfud_printRet("fast_read_enable", sfud_qspi_fast_read_enable(flash, 4));
            qspi_inited = true;
        } else
            printf("qspi init fail\r\n");
    }

    if (strcmp(argv[1], "demo") == 0) {
        sfud_demo(0, sizeof(sfud_demo_test_buf), sfud_demo_test_buf);
        return 0;
    }

    if (strcmp(argv[1], "read") == 0) {
        int offset, size;
        if (argc < 4)
            goto usage;
        if (sscanf(argv[2], "%i", &offset) == 1) {
            if (sscanf(argv[3], "%i", &size) == 1) {
                uint8_t *buffer = malloc(size);
                if (!buffer) {
                    perror("malloc buffer");
                    return -1;
                }
                sfud_printRet("read", sfud_read(flash, offset, size, buffer));
                for (int i = 0; i < size; i++) {
                    if ((i % 16) == 0) {
                        printf("\n%04X ", i);
                    }
                    printf("%02X ", buffer[i]);
                }
                printf("\n");
                free(buffer);
                return 0;
            }
        }
    }

    if (strcmp(argv[1], "write") == 0) {
        if (argc < 4)
            goto usage;
        int offset;
        if (sscanf(argv[2], "%i", &offset) != 1)
            goto usage;
        int bufSize = argc - 3;
        if (bufSize < 1)
            goto usage;
        uint8_t *buf = malloc(bufSize);
        if (!buf) {
            perror("malloc buffer");
            return -1;
        }
        for (int i = 3; i < argc; i++) {
            int data;
            if (sscanf(argv[i], "%i", &data) != 1) {
                printf("Error read %s\r\n", argv[i]);
                goto usage;
            }
            buf[i] = data;
        }
        sfud_printRet("write", sfud_write(flash, offset, bufSize, buf));
        free(buf);
        return 0;
    }

    if (strcmp(argv[1], "erase") == 0) {
        if (argc < 4)
            goto usage;
        int offset, size;
        if (sscanf(argv[2], "%i", &offset) != 1)
            goto usage;
        if (sscanf(argv[3], "%i", &size) != 1)
            goto usage;
        sfud_printRet("erase", sfud_erase(flash, offset, size));
        return 0;
    }

usage:
    printf("Usage: %s <command>\r\nThe <command> must be:\r\n"
           "  freq <MHz>                         Set the QSPI frequency\r\n"
           "  read <offset> <size>               Hexdump from offset, size bytes\r\n"
           "  write <offset> <byte0> ... <byteN> Write bytes from offset\r\n"
           "  erase <offset>                     Erase 4K at <offset>\r\n"
           "  demo                               Start demo on first 1024 bytes\r\n",
           argv[0]);
    return -1;
}

static CMDFUNC(cmd_usb)
{
    if (argc > 1) {
        if (strcmp(argv[1], "recv") == 0) {
            while (readKey() == -1) {
                uint8_t c;
                if (CDC_getByte(&c))
                    printf("RECV 0x%02X [%c]\r\n", c, c);
            }
            return 0;
        }
        if (strcmp(argv[1], "send") == 0) {
            extern USBD_HandleTypeDef hUsbDeviceFS;
            USBD_CDC_HandleTypeDef *cdc = (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;
            ;
            for (int i = 2; i < argc; i++) {
                int c;
                if (sscanf(argv[i], "%i", &c) == 1) {
                    while (cdc->TxState == 1)
                        ;
                    CDC_Transmit_FS((uint8_t *)&c, 1);
                } else {
                    while (cdc->TxState == 1)
                        ;
                    CDC_Transmit_FS((uint8_t *)argv[i], strlen(argv[i]));
                }
            }
            return 0;
        }
    }
    printf("usage: %s recv|send\r\n", argv[0]);
    return -1;
}

static CMDFUNC(cmd_eth)
{
    if (argc < 2)
        goto usage;

    if (strcmp(argv[1], "phyrd") == 0) {
        if (argc < 3)
            goto usage;

        int reg;
        if (sscanf(argv[2], "%i", &reg) != 1) {
            printf("cannot decode register number %s\r\n", argv[2]);
            goto usage;
        }

        uint32_t val;
        HAL_ETH_ReadPHYRegister(&heth, 0, reg, &val);
        printf("reg[%02X] = 0x%08X\r\n", reg, (unsigned int)val);
        for (int bit = 0; bit < 16; bit++)
            printf("   bit[%d] = %d\r\n", bit, (int)((val >> bit) & 1));
        return 0;
    }

    if (strcmp(argv[1], "phywr") == 0) {
        if (argc < 4)
            goto usage;

        int reg;
        if (sscanf(argv[2], "%i", &reg) != 1) {
            printf("cannot decode register number %s\r\n", argv[2]);
            goto usage;
        }

        int val;
        if (sscanf(argv[3], "%i", &val) != 1) {
            printf("cannot decode register number %s\r\n", argv[3]);
            goto usage;
        }

        HAL_ETH_WritePHYRegister(&heth, 0, reg, (uint32_t)val);

        return 0;
    }

    if (strcmp(argv[1], "rawtx") == 0) {
    }

    if (strcmp(argv[1], "rawrx") == 0) {
        while (!HAL_ETH_IsRxDataAvailable(&heth)) {
            if (readKey())
                return -1;
        }
        printf("Data arrived\r\n");
        uint32_t len;
        HAL_ETH_GetRxDataLength(&heth, &len);
        if (len == 0) {
            printf("Data length is zero\r\n");
            return -1;
        }
        return 0;
    }

usage:
    printf("usage: %s <command>\r\n"
           " where <command> is one of:\r\n"
           "   phyrd  <reg>        Read PHY register\r\n"
           "   phywr  <reg> <val>  Write PHY register\r\n"
           "   rawtx...            Send raw packet (TODO)\r\n"
           "   rawrx...            Receive raw packet (TODO)\r\n",
           argv[0]);
    return -1;
}

static CMDFUNC(cmd_485)
{
    if (argc > 1) {
        if (strcmp(argv[1], "send") == 0) {
            HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, 1);
            for (int i = 2; i < argc; i++) {
                int c;
                if (sscanf(argv[i], "%i", &c) == 1) {
                    HAL_UART_Transmit(&uart485, (uint8_t *)&c, 1, 0);
                } else {
                    printf("cannot transmit %s\r\n", argv[i]);
                }
            }
            HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, 0);
            return 0;
        } else if (strcmp(argv[1], "recv") == 0) {
            HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, 0);
            while (readKey() == -1) {
                uint8_t c;
                if (HAL_UART_Receive(&uart485, &c, 1, 0) == HAL_OK)
                    printf("RECV: %c [%d, 0x%02X]\r\n", c, c, c);
            }
            return 0;
        }
    }
    printf("USAGE: %s recv|send [data0 data1 data2...]\r\n", argv[0]);
    return -1;
}

static CMDFUNC(cmd_can)
{
    // TODO Implement me
    return 0;
}

int mrl_execute(int argc, const char *const *argv)
{
    for (int i = 0; i < (sizeof(commands) / sizeof(*commands)); i++)
        if (strcmp(commands[i].cmd, argv[0]) == 0)
            return retcode = commands[i].func(argc, argv);
    printf("Unknown commando: \"%s\"\r\n", argv[0]);
    return -1;
}
