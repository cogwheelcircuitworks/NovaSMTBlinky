
#define NDM_INTERCMD_DELAY_MS 5
#define NDM_HALF_BIT_PERIOD_US 150
#define NDM_DEMO_DURATION_MS 5000

class NovaDotMatrixDriver {
  public:
    uint8_t clk_pin,data_pin;
    void Setup(void);
    void Write(uint8_t );
    void WriteBuf(uint8_t *, uint8_t ); 
};

