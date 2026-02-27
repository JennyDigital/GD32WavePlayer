#ifndef _DALBY_MULTI_H
#define _DALBY_MULTI_H

typedef enum {
    OPT_Chime,
    OPT_MindTheDoor,
    OPT_DoorsOpeningClosing,
    OPT_GroundFloor = 8,
    OPT_FirstFloor,
    OPT_SecondFloor,
    OPT_ThirdFloor,
    OPT_TopFloor,
    OPT_LiftOutOfService = 15
  } OptionSelTypeDef;

void              ChimeLoop ( void );
OptionSelTypeDef  GetOption ( void );

#endif // _DALBY_MULTI_H