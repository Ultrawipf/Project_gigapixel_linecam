#ifndef PCFIOQT_H
#define PCFIOQT_H
#include "pcf8574.h"
#include <QTimer>
#include <chrono>
#include <memory>

class PcfIoQt : public QObject, public PCFCallbackHandler{
    Q_OBJECT
public:
    PcfIoQt();
    ~PcfIoQt();
    void pcfCallback() override;

    const std::chrono::milliseconds debounceMs{50};

    void setMask(uint8_t mask);
    uint8_t signalMask = 0xff;

signals:
    void buttonPressed(uint8_t data);
    void buttonReleased(uint8_t data);

private:
    std::unique_ptr<PCF8574> pcf;
    //long lastTime = 0;
    uint8_t lastVal = 0;
    std::chrono::time_point<std::chrono::system_clock> lastTime;
};

#endif // PCFIOQT_H
