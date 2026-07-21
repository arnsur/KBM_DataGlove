#include <cstdint>

typedef struct DataMessage {
    uint8_t hand_id;
    int8_t mouseX;
    int8_t mouseY;
    int8_t scrollTicks;
    bool leftClick;
    bool rightClick;
    bool middleClick;
    bool mouseFwd;
    bool mouseBack;
    char keysPressed[6];
} DataMessage;

struct FingerProfile {
    int bottomValMain;
    int homeValMain;

    int bottomValAlt;
    int homeValAlt;
    
    
    float altColVal;

    int rowSensor;
    int colSensor;
};

enum ConnectionStatus {CONNECTED, DISCONNECTED, SEARCHING, UNKNOWN};

enum BendZone { NUM_ROW, TOP_ROW, HOME_ROW, BOTTOM_ROW };