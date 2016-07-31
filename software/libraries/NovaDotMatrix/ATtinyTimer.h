#if 1
#ifndef ATtinyTimer_h
#define ATtinyTimer_h

// The ATtinyTimer class

#define ATT_FAST_FLAG_BIT 0b10000000
// some of these were moved out of the class to speed things up
extern volatile uint8_t ATtinyTimerFastFlags;
extern volatile uint8_t ATtinyTimerFiveHundredHzCtr ; 
extern uint8_t ATtinyTimerFiveHundredHzDiv ; // ISR resets Ctr to Div when Flag is cleared
#define ATT_FIVE_HUNDRED_HZ_SHORTDIV 1
#define ATT_FIVE_HUNDRED_HZ_DIV 2


class ATtinyTimer
{
  public:
    void Setup(void); 
    void Loop(void); 
    volatile bool triggered; // indicates if function has been called

    // indicates a certain amount of time has passed...

    uint8_t OneHundredHzCtr ;
    uint8_t OneHundredHzDiv ;
    volatile bool OneHundredHzFlag;
#define ATT_ONEHUNDRED_HZ_DIV 11

    uint8_t FiftyHzCtr ;
    uint8_t FiftyHzDiv ;
    volatile bool FiftyHzFlag;
#define ATT_FIFTY_HZ_DIV 1

    uint8_t TenHzCtr ;
    uint8_t TenHzDiv ;
    volatile uint8_t TenHzFlags;
#define ATT_TENHZDIV        10

    uint8_t OneHzCtr;
    uint8_t OneHzDiv;
    volatile bool OneHzFlag;
    volatile uint8_t OneHzFlags;
#define ATT_ONEHZDIV        100

}; 

#endif // ATtinyTimer_h

#define DISABLE_TIMER_IRUPS TIMSK &= ~_BV(TOIE1)
#define ENABLE_TIMER_IRUPS  TIMSK |= _BV(TOIE1)

#endif
