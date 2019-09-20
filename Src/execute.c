#include <execute.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#include <main.h>
#include <bsp_driver_sd.h>
#include <ff.h>
#include <fatfs.h>

#define CMDFUNC(name) int name(int argc, const char *const *argv)

static CMDFUNC(cmd_gpio);
static CMDFUNC(cmd_help);
static CMDFUNC(cmd_sdinfo);
static CMDFUNC(cmd_sdls);

struct {
   const char *cmd;
   int (*func)(int, const char * const *);
} commands[] = {
   { "gpio", cmd_gpio },
   { "help", cmd_help },
   { "sdinfo", cmd_sdinfo },
   { "sdls", cmd_sdls },
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
                  // TODO: Decode string for same format as bellow read info
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
   printf("usage: %s P[A..K][0..15] (0|1)\r\n", argv[0]);
   return 0;
}

static CMDFUNC(cmd_help)
{
   uint32_t freq = HAL_RCC_GetSysClockFreq();
   int freq_mhz = freq / ((int) 1e6);
   int freq_frac = freq % ((int) 1e6);
   printf(
      "CPU Cortex-M7 running at %d.%d MHz\r\n"
      "Availables commands:\r\n", freq_mhz, freq_frac);
   for (int i=0; i< (sizeof(commands)/sizeof(*commands)); i++)
      printf("  %s\r\n", commands[i].cmd);
   return 0;
}

static CMDFUNC(cmd_sdinfo)
{
   if (BSP_SD_Init() == MSD_OK) {
      HAL_SD_CardInfoTypeDef info;
      BSP_SD_GetCardInfo(&info);
      printf("CardType:     %"PRIu32 "\r\n", info.CardType);
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

int mrl_execute(int argc, const char * const * argv)
{
   for (int i=0; i< (sizeof(commands)/sizeof(*commands)); i++)
      if (strcmp(commands[i].cmd, argv[0]) == 0)
         return commands[i].func(argc, argv);
   printf("Unknown commando: \"%s\"\r\n", argv[0]);
   return 0;
}
