/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file    main.c
 * @brief   SDIO 4-bit + FATFS logger (DMA driver, no BSP)
 ******************************************************************************
 */
/* USER CODE END Header */

#include "main.h"
#include "fatfs.h"
#include "sdio.h"
#include <string.h>
#include <stdio.h>



FATFS fs;
FIL   fil;

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
 void MX_SDIO_SD_Init(void);

/* USER CODE BEGIN 0 */
static void log_line(const char *s)
{
  UINT bw;
  f_write(&fil, s, (UINT)strlen(s), &bw);
}
/* USER CODE END 0 */

int main(void)
{
  /* USER CODE BEGIN 1 */
  FRESULT fr;
  char buf[128];
  uint32_t seq = 0;
  /* USER CODE END 1 */

  HAL_Init();
  SystemClock_Config();

  MX_GPIO_Init();
  MX_SDIO_SD_Init();

  if (HAL_SD_Init(&hsd) != HAL_OK) Error_Handler();
  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK) Error_Handler();


  MX_FATFS_Init();          // FATFS_LinkDriver(&SD_Driver, SDPath)만 수행

  /* ================= SD + FATFS =================
     - SD 초기화(HAL_SD_Init / 4bit)는 "sd_diskio_dma.c"의 SD_Driver가 수행
     - main에서는 절대 HAL_SD_Init() / ConfigWideBusOperation() 호출하지 말 것
  */

  fr = f_mount(&fs, "", 1);                 // 여기서 disk_initialize() -> SD_initialize() 호출됨
  if (fr != FR_OK) Error_Handler();

  fr = f_open(&fil, "LOG.TXT", FA_OPEN_ALWAYS | FA_WRITE);
  if (fr != FR_OK) Error_Handler();

  f_lseek(&fil, f_size(&fil));              // append

  /* ================= LOG HEADER ================= */
  snprintf(buf, sizeof(buf),
           "\r\n# ==== SDIO LOG START (DMA) ====\r\n"
           "# mode=SDIO 4bit\r\n"
           "# format: t_ms,seq,sd_state,sd_err\r\n");

  log_line(buf);

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);

  /* ================= MAIN LOOP ================= */
  while (1)
  {
    uint32_t t = HAL_GetTick();
    uint32_t sd_state = HAL_SD_GetCardState(&hsd);
    uint32_t sd_err   = HAL_SD_GetError(&hsd);

    snprintf(buf, sizeof(buf),
             "%lu,%lu,%lu,0x%08lX\r\n",
             (unsigned long)t,
             (unsigned long)seq++,
             (unsigned long)sd_state,
             (unsigned long)sd_err);

    log_line(buf);

    fr = f_sync(&fil);                      // 안정성 우선
    if (fr != FR_OK) Error_Handler();

    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    HAL_Delay(1000);
  }
}

/* ================= CubeMX generated ================= */

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

  RCC_ClkInitStruct.ClockType =
      RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
      RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) Error_Handler();
}


static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
    HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    HAL_Delay(100);
  }
}
