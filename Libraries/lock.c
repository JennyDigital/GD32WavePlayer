/** Code locking to level 1
  *
  */

  #include "main.h"

FLASH_OBProgramInitTypeDef pOBInit = {0};

void TOOLS_RDPLevel1_Lock( void )
{
 if( HAL_FLASH_Unlock() == HAL_OK )
  {
    HAL_FLASHEx_OBGetConfig( &pOBInit );

    if( pOBInit.RDPLevel == OB_RDP_LEVEL_0 )
    {
      HAL_FLASH_OB_Unlock();
      pOBInit.RDPLevel    = OB_RDP_LEVEL_1;
      pOBInit.OptionType  =  OPTIONBYTE_RDP;
      HAL_FLASHEx_OBProgram( &pOBInit );
      HAL_FLASH_OB_Lock();
      HAL_FLASH_OB_Launch();
    }
  }
  HAL_FLASH_OB_Lock();
}