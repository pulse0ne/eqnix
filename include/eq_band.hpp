#ifndef EQ_BAND_HPP
#define EQ_BAND_HPP

#include <glib.h>
#include <vector>
#include <mutex>

enum BandType {
    PEAKING, HIGH_SHELF, LOW_SHELF
};

class EQBand {
public:
    bool enabled;

    void set_freq(double new_freq);
    void set_bw(double new_bw);
    void set_gain(double new_gain);
    void set_band_type(BandType new_bt);
    
    double get_freq() { return freq; }
    double get_bw() { return bw; }
    double get_gain() { return gain; }
    BandType get_band_type() { return bandtype; }

    std::vector<double> get_frequency_response(std::vector<double> frequencies);
private:
    BandType bandtype = BandType::PEAKING;
    double freq, bw, gain;

    double rate = 44100.0;

    double b1, b2;     // IIR coefficients for outputs
    double a0, a1, a2; // IIR coefficients for inputs

    std::mutex lock;

    void update_coefficients();
    void setup_peaking();
    void setup_highshelf();
    void setup_lowshelf();
    double calculate_bw();
};

#endif // EQ_BAND_HPP
