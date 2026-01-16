// Core/Src/sd_diskio_dma.c

#include "ff_gen_drv.h"
#include "diskio.h"
#include "stm32f4xx_hal.h"

extern SD_HandleTypeDef hsd;

static volatile uint8_t sdio_rx_done = 0;
static volatile uint8_t sdio_tx_done = 0;
static volatile uint8_t sdio_err     = 0;

static void wait_transfer(void)
{
  while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) { }
}

/* HAL callbacks (HAL 내부 __weak 을 오버라이드) */
void HAL_SD_RxCpltCallback(SD_HandleTypeDef *phsd)
{
  (void)phsd;
  sdio_rx_done = 1;
}

void HAL_SD_TxCpltCallback(SD_HandleTypeDef *phsd)
{
  (void)phsd;
  sdio_tx_done = 1;
}

void HAL_SD_ErrorCallback(SD_HandleTypeDef *phsd)
{
  (void)phsd;
  sdio_err = 1;
  sdio_rx_done = 1;
  sdio_tx_done = 1;
}

static DSTATUS SD_initialize(BYTE lun)
{
  (void)lun;


  if (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER)
    return STA_NOINIT;
  return 0;
}

static DSTATUS SD_status(BYTE lun)
{
  (void)lun;
  return 0;
}

static DRESULT SD_read(BYTE lun, BYTE *buff, DWORD sector, UINT count)
{
  (void)lun;

  sdio_rx_done = 0;
  sdio_err     = 0;
  uint32_t t0 = HAL_GetTick();


  if (HAL_SD_ReadBlocks_DMA(&hsd, (uint8_t*)buff, (uint32_t)sector, (uint32_t)count) != HAL_OK)
    return RES_ERROR;

  while (!sdio_rx_done) { if (HAL_GetTick() - t0 > 2000) return RES_ERROR;  }
  wait_transfer();

  if (sdio_err) return RES_ERROR;
  if (HAL_SD_GetError(&hsd) != HAL_SD_ERROR_NONE) return RES_ERROR;

  return RES_OK;
}

#if _USE_WRITE == 1
static DRESULT SD_write(BYTE lun, const BYTE *buff, DWORD sector, UINT count)
{
  (void)lun;

  sdio_tx_done = 0;
  sdio_err     = 0;

  if (HAL_SD_WriteBlocks_DMA(&hsd, (uint8_t*)buff, (uint32_t)sector, (uint32_t)count) != HAL_OK)
    return RES_ERROR;

  while (!sdio_tx_done) { }
  wait_transfer();

  if (sdio_err) return RES_ERROR;
  if (HAL_SD_GetError(&hsd) != HAL_SD_ERROR_NONE) return RES_ERROR;

  return RES_OK;
}
#endif

#if _USE_IOCTL == 1
static DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  (void)lun;

  switch (cmd)
  {
    case CTRL_SYNC:
      return RES_OK;

    case GET_SECTOR_SIZE:
      *(WORD*)buff = 512;
      return RES_OK;

    case GET_SECTOR_COUNT:
    {
      HAL_SD_CardInfoTypeDef info;
      HAL_SD_GetCardInfo(&hsd, &info);
      *(DWORD*)buff = (DWORD)info.LogBlockNbr;
      return RES_OK;
    }

    case GET_BLOCK_SIZE:
      *(DWORD*)buff = 1;
      return RES_OK;

    default:
      return RES_PARERR;
  }
}
#endif

/* fatfs.c 가 참조하는 그 심볼 */
const Diskio_drvTypeDef SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
#if _USE_WRITE == 1
  SD_write,
#endif
#if _USE_IOCTL == 1
  SD_ioctl,
#endif
};
