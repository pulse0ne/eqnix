#ifndef BIQUAD_HPP
#define BIQUAD_HPP

#include <gst/gst.h>
#include <complex>
#include <vector>

typedef enum {
    ALLPASS = 0,
    BANDPASS,
    HIGHPASS,
    HIGHSHELF,
    LOWPASS,
    LOWSHELF,
    NOTCH,
    PEAKING
} BiquadFilterType;

struct FilterHistory {
    double x1, x2;
    double y1, y2;
};

class Biquad {
public:
    Biquad();
    virtual ~Biquad();

    void set_frequency(double f);
    void set_q(double Q);
    void set_gain(double g);
    void set_filter_type(BiquadFilterType t);

    void set_channels(uint num_channels);
    void set_samplerate(double rate);

    double get_frequency() { return frequency; }
    double get_q() { return q; }
    double get_gain() { return gain; }
    BiquadFilterType get_filter_type() { return filter_type; }

    void set_zero_pole_pairs(std::complex<double> zero, std::complex<double> pole);
    virtual void reset();

    void get_frequency_response(int num_freq, const double* freqs, double* mag_res, double* phase_res);
    double process(const double source, uint chan_ix);

private:
    // properties
    double frequency;
    double q;
    double gain;
    BiquadFilterType filter_type;

    // samplerate
    double samplerate = 44100.0;

    // channels
    uint channels = 1;

    // filter params
    double b0;
    double b1;
    double b2;
    double a1;
    double a2;

    // history
    std::vector<FilterHistory> history;

    void set_normalized_coefficients(double nb0, double nb1, double nb2, double na0, double na1, double na2);

    void update_params();

    void set_allpass_params();
    void set_bandpass_params();
    void set_highpass_params();
    void set_highshelf_params();
    void set_lowpass_params();
    void set_lowshelf_params();
    void set_notch_params();
    void set_peaking_params();

};

#endif // BIQUAD_HPP
