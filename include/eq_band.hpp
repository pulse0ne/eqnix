#ifndef EQ_BAND_HPP
#define EQ_BAND_HPP

#include <glib/gtypes.h>
#include <vector>
#include <mutex>

enum BandType {
    PEAKING, HIGH_SHELF, LOW_SHELF
};

class EQBand {
public:
    bool enabled;

    void set_freq(float new_freq);
    void set_bw(float new_bw);
    void set_gain(float new_gain);
    void set_band_type(BandType new_bt);
    
    float get_freq() { return freq; }
    float get_bw() { return bw; }
    float get_gain() { return gain; }
    BandType get_band_type() { return bandtype; }
    
    std::vector<float> get_frequency_response(const std::vector<const float> frequencies);
private:
    BandType bandtype;
    float freq, bw, gain;

    float rate = 44100;

    double b1, b2;     // IIR coefficients for outputs
    double a0, a1, a2; // IIR coefficients for inputs

    std::mutex lock;

    void update_coefficients();
};

#endif // EQ_BAND_HPP
