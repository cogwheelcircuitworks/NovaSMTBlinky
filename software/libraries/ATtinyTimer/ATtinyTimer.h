#if 1
#ifndef ATtinyTimer_h
#define ATtinyTimer_h

// The ATtinyTimer class

class ATtinyTimer
{
  public:
    void Setup(); // the setup function
    volatile bool triggered; // indicates if function has been called

    // indicates a certain amount of time has passed...
    volatile bool TwoHundredHzFlag; 
    volatile bool OneHundredHzFlag;
    volatile bool TenHzFlag;
    volatile bool OneHzFlag;


    unsigned char TwoHundredHzCtr ;
#define ATT_TWOHUNDREDHZDIV 5
    unsigned char OneHundredHzCtr ;
#define ATT_ONEHUNDREDHZDIV 10
    unsigned char TenHzCtr ;
#define ATT_TENHZDIV        10
    unsigned char OneHzCtr;
#define ATT_ONEHZDIV        100

}; 

#endif // ATtinyTimer_h

#endif
