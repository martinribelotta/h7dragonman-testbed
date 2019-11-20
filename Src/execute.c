#include <execute.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>

#include <main.h>
#include <bsp_driver_sd.h>
#include <ff.h>
#include <fatfs.h>
#include <sfud.h>

#define CMDFUNC(name) int name(int argc, const char *const *argv)

static CMDFUNC(cmd_gpio);
static CMDFUNC(cmd_help);
static CMDFUNC(cmd_sdinfo);
static CMDFUNC(cmd_sdls);
static CMDFUNC(cmd_qspi);
static CMDFUNC(cmd_485);

struct {
   const char *cmd;
   int (*func)(int, const char * const *);
} commands[] = {
   { "gpio", cmd_gpio },
   { "help", cmd_help },
   { "sdinfo", cmd_sdinfo },
   { "sdls", cmd_sdls },
   { "qspi", cmd_qspi },
   { "485", cmd_485 },
};

static CMDFUNC(cmd_gpio)
{
   if (argc >= 2) {
      char port;
      int pin;
      int val;
      if (sscanf(argv[1], "P%c%d", &port, &pin) == 2) {
         if (port >= 'A' && port <= 'K' && pin >= 0 && pin <= 15) {
            static GPIO_TypeDef * const portMap[] = {
               GPIOA, GPIOB, GPIOC, GPIOD, GPIOE, GPIOF,
               GPIOG, GPIOH, GPIOI, GPIOJ, GPIOK
            };
            GPIO_TypeDef *portPtr = portMap[port - 'A'];
            if (argc >= 3) {
               if (sscanf(argv[2], "%d", &val) == 1) {
                  // write port value
                  printf("Wrinting %d to %s\r\n", val, argv[1]);
                  HAL_GPIO_WritePin(portPtr, 1 << pin, val);
                  return 0;
               } else {
                  static const struct { const char *name; uint32_t mode; } modemap[] = {
                     { "out", GPIO_MODE_OUTPUT_PP },
                     { "in", GPIO_MODE_INPUT },
                     { "outoc", GPIO_MODE_OUTPUT_OD },
                     { "analog", GPIO_MODE_ANALOG },
                  };
                  // TODO: Decode string for same format as bellow read info
                  for (int i=0; i<(sizeof(modemap)/sizeof(*modemap)); i++) {
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
               static const char *const mode_text[] =
                  { "Input", "Output", "Alternate", "Analog" };
               val = HAL_GPIO_ReadPin(portPtr, 1 << pin);
               int mode = (portPtr->MODER >> (pin * 2)) & 0x3;
               printf("%s = %d (%s", argv[1], val, mode_text[mode]);
               if (mode == 1) {
                  static const char * const speed_text[] =
                     { "low", "medium", "high", "very high" };
                  int speed = (portPtr->OSPEEDR >> (pin * 2)) & 0x3;
                  if ((portPtr->OTYPER >> pin) & 1)
                     printf(" open drain");
                  printf(" %s speed", speed_text[speed]);
               } else if (mode == 2) {
                  int hl = (pin >> 3) & 1;
                  int alt = (portPtr->AFR[hl] >> (pin & 0x7)) & 0xF;
                  printf(" AF%d", alt);
               }
               static const char *const pull_text[] =
                  { "", "pull up", "pull down", "" };
               int pull = (portPtr->PUPDR >> (pin * 2)) & 0x03;
               if (pull > 0)
                  printf(" %s", pull_text[pull]);
               printf(")\r\n");
               return 0;
            }
         }
      }
   }
   printf("usage: %s P[A..K][0..15] (0|1|in|out|analog|outoc)\r\n", argv[0]);
   return 0;
}

static CMDFUNC(cmd_help)
{
   uint32_t freq = HAL_RCC_GetSysClockFreq();
   int freq_mhz = freq / ((int) 1e6);
   int freq_frac = freq % ((int) 1e6);
   printf(
      "Build at %s %s with " __VERSION__
      "CPU Cortex-M7 running at %d.%d MHz\r\n"
      "Availables commands:\r\n",
      __DATE__, __TIME__,
      freq_mhz, freq_frac);
   for (int i=0; i< (sizeof(commands)/sizeof(*commands)); i++)
      printf("  %s\r\n", commands[i].cmd);
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
      printf("CardVersion:  %"PRIu32 "\r\n", info.CardVersion);
      printf("Class:        %"PRIu32 "\r\n", info.Class);
      printf("RelCardAdd:   %"PRIu32 "\r\n", info.RelCardAdd);
      printf("BlockNbr:     %"PRIu32 "\r\n", info.BlockNbr);
      printf("BlockSize:    %"PRIu32 "\r\n", info.BlockSize);
      printf("LogBlockNbr:  %"PRIu32 "\r\n", info.LogBlockNbr);
      printf("LogBlockSize: %"PRIu32 "\r\n", info.LogBlockSize);
      printf("CardSpeed:    %"PRIu32 "\r\n", info.CardSpeed);
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
    for (i = 0; i < size; i++)
    {
        data[i] = i;
    }
    /* erase test */
    result = sfud_erase(flash, addr, size);
    if (result == SFUD_SUCCESS)
    {
        printf("Erase the %s flash data finish. Start from 0x%08X, size is %zu.\r\n", flash->name, (unsigned int) addr, size);
    }
    else
    {
        printf("Erase the %s flash data failed.\r\n", flash->name);
        return;
    }
    /* write test */
    result = sfud_write(flash, addr, size, data);
    if (result == SFUD_SUCCESS)
    {
        printf("Write the %s flash data finish. Start from 0x%08X, size is %zu.\r\n", flash->name, (unsigned int) addr, size);
    }
    else
    {
        printf("Write the %s flash data failed.\r\n", flash->name);
        return;
    }
    /* read test */
    result = sfud_read(flash, addr, size, data);
    if (result == SFUD_SUCCESS)
    {
        printf("Read the %s flash data success. Start from 0x%08X, size is %zu. The data is:\r\n", flash->name, (unsigned int) addr, size);
        printf("Offset (h) 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F\r\n");
        for (i = 0; i < size; i++)
        {
            if (i % 16 == 0)
            {
                printf("[%08X] ", (unsigned int) (addr + i));
            }
            printf("%02X ", data[i]);
            if (((i + 1) % 16 == 0) || i == size - 1)
            {
                printf("\r\n");
            }
        }
        printf("\r\n");
    }
    else
    {
        printf("Read the %s flash data failed.\r\n", flash->name);
    }
    /* data check */
    for (i = 0; i < size; i++)
    {
        if (data[i] != i % 256)
        {
            printf("Read and check write data has an error. Write the %s flash data failed.\r\n", flash->name);
            break;
        }
    }
    if (i == size)
    {
        printf("The %s flash test is success.\r\n", flash->name);
    }
}

#define SFUD_DEMO_TEST_BUFFER_SIZE                     1024
static uint8_t sfud_demo_test_buf[SFUD_DEMO_TEST_BUFFER_SIZE];

static CMDFUNC(cmd_qspi)
{
   if (sfud_init() == SFUD_SUCCESS)
   {
      printf("qspi init OK\r\n");
      /* enable qspi fast read mode, set four data lines width */
      sfud_qspi_fast_read_enable(sfud_get_device(SFUD_W25_DEVICE_INDEX), 4);
      sfud_demo(0, sizeof(sfud_demo_test_buf), sfud_demo_test_buf);
   } else
      printf("qspi init fail\r\n");
   return 0;
}

extern UART_HandleTypeDef huart8;
extern UART_HandleTypeDef huart1;

#define uart485 huart8

static int readKey() {
   uint8_t c;
   if (HAL_UART_Receive(&huart1, &c, 1, 0) == HAL_OK)
      return c;
   return -1;
}

static void rs485_usage(const char *argv0)
{
   printf("USAGE: %s recv|send [data0 data1 data2...]\r\n", argv0);
}

static CMDFUNC(cmd_485)
{
   if (argc <= 1) {
      rs485_usage(argv[0]);
      return -1;
   }
   if (strcmp(argv[1], "send") == 0) {
      HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, 1);
      for (int i=2; i<argc; i++) {
         int c;
         if (sscanf(argv[i], "%i", &c) == 1) {
            HAL_UART_Transmit(&uart485, (uint8_t*) &c, 1, 0);
         } else {
            printf("cannot transmit %s\r\n", argv[i]);
         }
      }
      HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, 0);
   } else if (strcmp(argv[1], "recv") == 0) {
      HAL_GPIO_WritePin(RS485_DE_GPIO_Port, RS485_DE_Pin, 0);
      while (readKey() == -1) {
         uint8_t c;
         if (HAL_UART_Receive(&uart485, &c, 1, 0) == HAL_OK)
            printf("RECV: %c [%d, 0x%02X]\r\n", c, c, c);
      }
   } else
      rs485_usage(argv[0]);
   return 0;
}

int mrl_execute(int argc, const char * const * argv)
{
   for (int i=0; i< (sizeof(commands)/sizeof(*commands)); i++)
      if (strcmp(commands[i].cmd, argv[0]) == 0)
         return commands[i].func(argc, argv);
   printf("Unknown commando: \"%s\"\r\n", argv[0]);
   return 0;
}
