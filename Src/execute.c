#include <execute.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <main.h>

#define CMDFUNC(name) int name(int argc, const char *const *argv)

static CMDFUNC(cmd_gpio);
static CMDFUNC(cmd_help);

struct {
   const char *cmd;
   int (*func)(int, const char * const *);
} commands[] = {
   { "gpio", cmd_gpio },
   { "help", cmd_help },
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
               // write port
               if (sscanf(argv[2], "%d", &val) == 1) {
                  printf("Wrinting %d to %s\r\n", val, argv[1]);
                  HAL_GPIO_WritePin(portPtr, 1 << pin, val);
                  return 0;
               }
            } else {
               // read port
               val = HAL_GPIO_ReadPin(portPtr, 1 << pin);
               printf("%s = %d\r\n", argv[1], val);
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

int mrl_execute(int argc, const char * const * argv)
{
   for (int i=0; i< (sizeof(commands)/sizeof(*commands)); i++)
      if (strcmp(commands[i].cmd, argv[0]) == 0)
         return commands[i].func(argc, argv);
   return 0;
}
