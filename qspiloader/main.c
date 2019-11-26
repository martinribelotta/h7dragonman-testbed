#include <stm32h7xx_hal.h>
#include <sfud.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

extern void initialise_monitor_handles(void);

QSPI_HandleTypeDef hqspi;
GPIO_InitTypeDef GPIO_InitStruct;

__attribute__((noreturn)) void hostExit(int retcode) {
   (void) retcode;
   while(1)
      __asm__ volatile("bkpt #1");
}

void Error_Handler() {
   hostExit(-1);
}

void SystemClock_Config(void) {
   RCC_OscInitTypeDef RCC_OscInitStruct = {0};
   RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
   RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

   HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

   __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

   while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

   __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);

   RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
   RCC_OscInitStruct.HSEState = RCC_HSE_ON;
   RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
   RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
   RCC_OscInitStruct.PLL.PLLM = 4;
   RCC_OscInitStruct.PLL.PLLN = 480;
   RCC_OscInitStruct.PLL.PLLP = 2;
   RCC_OscInitStruct.PLL.PLLQ = 20;
   RCC_OscInitStruct.PLL.PLLR = 2;
   RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_1;
   RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
   RCC_OscInitStruct.PLL.PLLFRACN = 0;
   if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
      Error_Handler();

   RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                           |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                           |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
   RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
   RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
   RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
   RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
   RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
   RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
   RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

   if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
      Error_Handler();
   PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_QSPI;
   PeriphClkInitStruct.PLL2.PLL2M = 4;
   PeriphClkInitStruct.PLL2.PLL2N = 50;  // QSPI clock in MHz
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
   if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
      Error_Handler();
}

typedef union param_t param;
union param_t {
    int pdInt;
    void *pdPtr;
    char *pdChrPtr;
};

typedef enum {
  SEMIHOSTING_EnterSVC = 0x17,
  SEMIHOSTING_ReportException = 0x18,
  SEMIHOSTING_SYS_CLOSE = 0x02,
  SEMIHOSTING_SYS_CLOCK = 0x10,
  SEMIHOSTING_SYS_ELAPSED = 0x30,
  SEMIHOSTING_SYS_ERRNO = 0x13,
  SEMIHOSTING_SYS_FLEN = 0x0C,
  SEMIHOSTING_SYS_GET_CMDLINE = 0x15,
  SEMIHOSTING_SYS_HEAPINFO = 0x16,
  SEMIHOSTING_SYS_ISERROR = 0x08,
  SEMIHOSTING_SYS_ISTTY = 0x09,
  SEMIHOSTING_SYS_OPEN = 0x01,
  SEMIHOSTING_SYS_READ = 0x06,
  SEMIHOSTING_SYS_READC = 0x07,
  SEMIHOSTING_SYS_REMOVE = 0x0E,
  SEMIHOSTING_SYS_RENAME = 0x0F,
  SEMIHOSTING_SYS_SEEK = 0x0A,
  SEMIHOSTING_SYS_SYSTEM = 0x12,
  SEMIHOSTING_SYS_TICKFREQ = 0x31,
  SEMIHOSTING_SYS_TIME = 0x11,
  SEMIHOSTING_SYS_TMPNAM = 0x0D,
  SEMIHOSTING_SYS_WRITE = 0x05,
  SEMIHOSTING_SYS_WRITEC = 0x03,
  SEMIHOSTING_SYS_WRITE0 = 0x04,
} HostSysCall;

static inline int
__attribute__ ((always_inline))
call_host (int reason, void* arg) {
  int value;
  asm volatile (

      " mov r0, %[rsn]  \n"
      " mov r1, %[arg]  \n"
      " bkpt %[swi]\n"
      " mov %[val], r0"

      : [val] "=r" (value) /* Outputs */
      : [rsn] "r" (reason), [arg] "r" (arg), [swi] "i" (0xAB) /* Inputs */
      : "r0", "r1", "r2", "r3", "ip", "lr", "memory", "cc"
      // Clobbers r0 and r1, and lr if in supervisor mode
  );

  // Accordingly to page 13-77 of ARM DUI 0040D other registers
  // can also be clobbered. Some memory positions may also be
  // changed by a system call, so they should not be kept in
  // registers. Note: we are assuming the manual is right and
  // Angel is respecting the APCS.
  return value;
}

int hostGetCommandLine(char *buf, size_t len) {
   struct {
      char *buf;
      uint32_t len;
   } params = { buf, len };
   return call_host(SEMIHOSTING_SYS_GET_CMDLINE, &params);
}

static char cmdLine[128];

void _init() {
   initialise_monitor_handles();
}

void NMI_Handler()        { hostExit(1); }
void HardFault_Handler()  { hostExit(2); }
void MemManage_Handler()  { hostExit(3); }
void BusFault_Handler()   { hostExit(4); }
void UsageFault_Handler() { hostExit(5); }
void SVC_Handler()        { hostExit(6); }
void DebugMon_Handler()   { hostExit(7); }
void PendSV_Handler()     { hostExit(8); }
void SysTick_Handler()    { HAL_IncTick(); }

static char sector[4 * 1024];

static const char *sfud_error_str[] = {
    "Not an error",
    "not found or not supported",
    "write error",
    "read error",
    "timeout error",
    "address is out of flash bound",
};

int main() {
   printf("Flash writer utility\n");

   HAL_Init();
   SystemClock_Config();

   __HAL_RCC_SYSCFG_CLK_ENABLE();
   __HAL_RCC_QSPI_CLK_ENABLE();
   __HAL_RCC_GPIOB_CLK_ENABLE();
   __HAL_RCC_GPIOE_CLK_ENABLE();
   __HAL_RCC_GPIOC_CLK_ENABLE();

   /**QUADSPI GPIO Configuration    
   PB2     ------> QUADSPI_CLK
   PE7     ------> QUADSPI_BK2_IO0
   PE8     ------> QUADSPI_BK2_IO1
   PE9     ------> QUADSPI_BK2_IO2
   PE10     ------> QUADSPI_BK2_IO3
   PC11     ------> QUADSPI_BK2_NCS 
   */
   GPIO_InitStruct.Pin = GPIO_PIN_2;
   GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
   GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
   HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

   GPIO_InitStruct.Pin = GPIO_PIN_7|GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10;
   GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
   GPIO_InitStruct.Pull = GPIO_NOPULL;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
   GPIO_InitStruct.Alternate = GPIO_AF10_QUADSPI;
   HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

   GPIO_InitStruct.Pin = GPIO_PIN_11;
   GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
   GPIO_InitStruct.Pull = GPIO_PULLUP;
   GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
   GPIO_InitStruct.Alternate = GPIO_AF9_QUADSPI;
   HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

   hqspi.Instance = QUADSPI;
   hqspi.Init.ClockPrescaler = 0;
   hqspi.Init.FifoThreshold = 1;
   hqspi.Init.SampleShifting = QSPI_SAMPLE_SHIFTING_NONE;
   hqspi.Init.FlashSize = 24;
   hqspi.Init.ChipSelectHighTime = QSPI_CS_HIGH_TIME_3_CYCLE;
   hqspi.Init.ClockMode = QSPI_CLOCK_MODE_0;
   hqspi.Init.FlashID = QSPI_FLASH_ID_2;
   hqspi.Init.DualFlash = QSPI_DUALFLASH_DISABLE;
   if (HAL_QSPI_Init(&hqspi) != HAL_OK)
      Error_Handler();

   if (sfud_init() == SFUD_SUCCESS) {
      printf("qspi init OK\r\n");
      /* enable qspi fast read mode, set four data lines width */
      sfud_qspi_fast_read_enable(sfud_get_device(SFUD_W25_DEVICE_INDEX), 4);
      // sfud_demo(0, sizeof(sfud_demo_test_buf), sfud_demo_test_buf);
   } else
      printf("qspi init fail\r\n");

   hostGetCommandLine(cmdLine, sizeof(cmdLine) - 1);
   printf("Cmd line: <<%s>>\n", cmdLine);

   FILE *f = fopen(cmdLine, "rb");
   if (f) {
      printf("Writing %s to flash\n[", cmdLine);
      const sfud_flash *flash = sfud_get_device(SFUD_W25_DEVICE_INDEX);
      uint32_t addr = 0;
      while (1) {
         size_t readed = fread(sector, sizeof(char), sizeof(sector), f);
         if (readed == 0 || readed == -1)
            break;
         sfud_err e = sfud_erase_write(flash, addr, readed, (const uint8_t *) sector);
         if (e != SFUD_SUCCESS) {
            printf("\nError writing qspi: %s\n", sfud_error_str[e]);
            hostExit(-2);
         } else {
            printf("#");
            fflush(stdout);
         }
         addr += readed;
      }
      fclose(f);
      printf("]\nDone\n");
   } else
      perror("open file");

   hostExit(0);
}
