#include "daisy_seed.h"
#include "stm32h7xx_hal.h"
#include "sys/system.h"
#include "stm32h7xx_hal_dma.h"
#include "stm32h7xx_ll_tim.h"
#include "stm32h7xx_ll_bus.h"
#include "n64common.h"
#include "daisydrive64.h"
#include "flashram.h"
#include "diskio.h"
#include "ff_gen_drv.h"
#include "menu.h"
#include "sounds.h"

void LoadRom(const char* Name, uint32_t Offset);
void ContinueRomLoad(bool LoadSave);
uint32_t *MenuBase = (uint32_t*)(FlashRamStorage + 32770);

using namespace daisy;
SdmmcHandler::Config sd_cfg;
extern SdmmcHandler   sd;
extern uint32_t CurrentRomSaveType;

void InitMenuFunctions(void)
{
    // Setup menu interrupt priority, needs to be below the SysTick priority otherwise SD operations it will cause deadlocks.
    HAL_NVIC_SetPriority(EXTI3_IRQn, 15, 1);
    NVIC_SetVector(EXTI3_IRQn, (uint32_t)&EXTI3_IRQHandler);
    HAL_NVIC_EnableIRQ(EXTI3_IRQn);

    // Enable software interrupts.
    EXTI->IMR1 |= DAISY_MENU_INTERRUPT;
    EXTI->EMR1 |= DAISY_MENU_INTERRUPT;
}

extern const Diskio_drvTypeDef SD_Driver;
extern Disk_drvTypeDef disk;
void EnableMenu(void) {

    sd_cfg.Defaults();
    sd_cfg.speed = SD_SPEED;
    sd.Init(sd_cfg);

    // Mount SD Card
    disk.drv[0] = &SD_Driver;
    DSTATUS Status = disk_initialize(0);
    if (RES_OK != Status) {
        PlayInternalAudio(sdfailedmp3, sizeof(sdfailedmp3));
        BlinkAndDie(500, 100);
    }
}

#define SWAP_WORDS(x) ((((x) & 0xFFFF) << 16) | (((x) & 0xFFFF0000) >> 16))
BYTE testTemp[512] __attribute__((aligned(16)));
inline void HandleExecute(void)
{
    uint32_t operation = SWAP_WORDS(MenuBase[REG_EXECUTE_FUNCTION]);
    switch(operation) {
        case GET_FW_VERSION:
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_DMA_BUSY;
            MenuBase[REG_FUNCTION_PARAMETER] = SWAP_WORDS(0x00000666);
        break;
        case ENABLE_MENU_FUNCTIONS:
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            EnableMenu();
        break;
        case DISABLE_MENU_FUNCTIONS:
        break;
        case SD_CARD_READ_SECTOR:
        {
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_ERROR;
            uint32_t temp;
            MenuBase[REG_DMA_RAM_ADDR] = SWAP_WORDS(MenuBase[REG_DMA_RAM_ADDR]);
            DRESULT result = disk_read(0, testTemp, MenuBase[REG_DMA_RAM_ADDR], 1);
            uint32_t* Ptr = &MenuBase[REG_DMA_DATA];
            for (int i = 0; i < 128; i += 1) {
                temp = __bswap32(((uint32_t*)testTemp)[i]);
                *Ptr = SWAP_WORDS(temp);
                Ptr += 1;
            }

            if (result != F_OK) {
                MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_ERROR;
            }
        }
        break;
        case SD_CARD_WRITE_SECTOR:
        {
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            MenuBase[REG_STATUS] &= ~DAISY_STATUS_BIT_SD_ERROR;
            uint32_t* Ptr = &MenuBase[REG_DMA_DATA];
            uint32_t* OutPtr = (uint32_t*)testTemp;
            for (int i = 0; i < 128; i += 1) {
                OutPtr[i] = Ptr[i];
                OutPtr[i] = SWAP_WORDS(OutPtr[i]);
                OutPtr[i] = __bswap32(OutPtr[i]);
            }

            MenuBase[REG_DMA_RAM_ADDR] = SWAP_WORDS(MenuBase[REG_DMA_RAM_ADDR]);
            DRESULT result = disk_write(0, testTemp, MenuBase[REG_DMA_RAM_ADDR], 1);
            if (result != F_OK) {
                MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_ERROR;
            }
        }
        break;
        case UPLOAD_ROM:
        {
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            uint32_t* Ptr = &MenuBase[REG_DMA_DATA];
            for (int i = 0; i < 128; i += 1) {
                *Ptr = SWAP_WORDS(*Ptr);
                *Ptr = __bswap32(*Ptr);
                Ptr += 1;
            }

            char *namePtr = (char*)&MenuBase[REG_DMA_DATA];
            LoadRom((namePtr + 1), 0);
            // Setup the save game.
            if ((CurrentRomSaveType == EEPROM_4K) || (CurrentRomSaveType == EEPROM_16K)) {
                SI_Reset();
                EEPROMType = CurrentRomSaveType;
                SI_Enable();

            } else {
                SI_Reset();
            }

            // Allow the rom to continue loading in the background.
            MenuBase[REG_STATUS] &= ~(DAISY_STATUS_BIT_DMA_BUSY | DAISY_STATUS_BIT_SD_BUSY);
            ContinueRomLoad(true);
        }
        break;
        case UPLOAD_ROM_EX:
        {
            MenuBase[REG_STATUS] |= DAISY_STATUS_BIT_SD_BUSY;
            // Read file count.
            uint32_t FileCount = 0;
            FileCount = SWAP_WORDS(MenuBase[REG_DMA_DATA]);
            uint32_t ReadOffset = sizeof(FileCount);
            for (uint32_t i = 0; i < FileCount; i += 1) {
                // Read offset
                uint32_t RomOffset = 0;
                RomOffset = SWAP_WORDS(MenuBase[REG_DMA_DATA + (ReadOffset / 4)]);
                ReadOffset += sizeof(RomOffset);
                // Read string length.
                uint32_t RomPathLength = 0;
                RomPathLength = SWAP_WORDS(MenuBase[REG_DMA_DATA + (ReadOffset / 4)]);
                ReadOffset += sizeof(RomPathLength);
                if ((RomPathLength & 3) != 0) {
                    BlinkAndDie(111, 222);
                }

                // Read string, all incoming strings need to be 4 byte aligned.
                uint32_t* Ptr = &MenuBase[REG_DMA_DATA + (ReadOffset / 4)];

                // Byteswap the incoming string.
                // TODO: Why is this needed...?
                for (uint32_t i = 0; i < (RomPathLength / 4); i += 1) {
                    *Ptr = SWAP_WORDS(*Ptr);
                    *Ptr = __bswap32(*Ptr);
                    Ptr += 1;
                }

                char *namePtr = (((char*)&MenuBase[REG_DMA_DATA + (ReadOffset / 4)]));
                ReadOffset += RomPathLength;
                LoadRom((namePtr + 1), RomOffset);
                if ((FileCount - 1) != i) {
                    ContinueRomLoad(false);
                }
            }

            // Setup the save game.
            if ((CurrentRomSaveType == EEPROM_4K) || (CurrentRomSaveType == EEPROM_16K)) {
                SI_Reset();
                EEPROMType = CurrentRomSaveType;
                SI_Enable();

            } else {
                SI_Reset();
            }

            // Allow the rom to continue loading in the background.
            MenuBase[REG_STATUS] &= ~(DAISY_STATUS_BIT_DMA_BUSY | DAISY_STATUS_BIT_SD_BUSY);
            ContinueRomLoad(true);
        }
        break;
        case SET_SAVE_TYPE:
            MenuBase[REG_FUNCTION_PARAMETER] = SWAP_WORDS(MenuBase[REG_FUNCTION_PARAMETER]);
            switch (MenuBase[REG_FUNCTION_PARAMETER]) {
                case SAVE_TYPE_OFF:
                case SAVE_TYPE_EEP4k:
                    CurrentRomSaveType = EEPROM_4K;
                    break;
                case SAVE_TYPE_EEP16k:
                    CurrentRomSaveType = EEPROM_16K;
                    break;
                case SAVE_TYPE_SRAM:
                case SAVE_TYPE_SRAM96:
                    CurrentRomSaveType = SAVE_SRAM;
                    break;
                case SAVE_TYPE_FLASH:
                    CurrentRomSaveType = SAVE_FLASH_1M;
                    break;
                default:
                    CurrentRomSaveType = EEPROM_4K;
                    break;
            };
        break;
        default:
        while(1) {}
        break;
    }

    MenuBase[REG_STATUS] &= ~(DAISY_STATUS_BIT_DMA_BUSY | DAISY_STATUS_BIT_SD_BUSY);
}

// These functions and extern needs to be defined somewhere else.
// The software menu interrupt doesn't make sense to be in here either as it has become a generic interrupt for 
// non time critical operations.
void SaveEEPRom(const char* Name);
void SaveFlashRam(const char* Name);

//volatile uint32_t xHandler = 0;
//uint32_t xSave[20];
extern "C"
ITCM_FUNCTION
void EXTI3_IRQHandler(void)
{
    EXTI->PR1 = DAISY_MENU_INTERRUPT;
    if (Running == false) {
        return;
    }
    //xSave[xHandler] = MenuBase[REG_EXECUTE_FUNCTION];
    //xHandler += 1;

    // Only allow OS64* roms to have menu access.
    // Menu roms are not allowed to have save functions.
    if (CURRENT_ROMNAME_STARTS_WITH_OS64) {
        HandleExecute();

    } else if (SaveFileDirty != false) {
        uint32_t SaveFence = gSaveFence;

        // Stall untill N64 finished the bulk of the write.
        do {
            SaveFence = gSaveFence;
            EXTI->PR1 = DAISY_MENU_INTERRUPT;
            System::Delay(100);
        } while (SaveFence != gSaveFence);

        // Commit changes to SD card.
        if (CurrentRomSaveType == EEPROM_16K || CurrentRomSaveType == EEPROM_4K) {
            SaveEEPRom(CurrentRomName);
        } else {
            SaveFlashRam(CurrentRomName);
        }

        SaveFileDirty = false;
    }
}

