#ifndef EXPOSURESPINBOX_H
#define EXPOSURESPINBOX_H

#include <QDoubleSpinBox>
#include <algorithm>
#include <iterator>

// Exposure spinbox for the touchscreen: the arrows step through a list of exposures instead of
// fixed increments, so long exposures and back to base take a few taps. Typed values stay exact.
// The minimum is the base exposure (shown as "base")
class ExposureSpinBox : public QDoubleSpinBox
{
public:
    ExposureSpinBox(QWidget* parent = nullptr) : QDoubleSpinBox(parent){
        setAccelerated(true);
    }

    void stepBy(int steps) override{
        static constexpr double ladderMs[] = {3,5,7,10,15,20,30,50,70,100,150,200,300,500,700,1000};
        constexpr double eps = 0.01;
        double v = value();
        for(; steps > 0; steps--){
            const double* next = std::upper_bound(std::begin(ladderMs),std::end(ladderMs),v + eps);
            v = next != std::end(ladderMs) ? *next : maximum();
        }
        for(; steps < 0; steps++){
            const double* next = std::lower_bound(std::begin(ladderMs),std::end(ladderMs),v - eps);
            v = next != std::begin(ladderMs) ? *(next - 1) : minimum();
        }
        setValue(std::clamp(v,minimum(),maximum()));
    }
};

#endif // EXPOSURESPINBOX_H
