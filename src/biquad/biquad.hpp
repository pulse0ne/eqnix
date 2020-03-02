#ifndef BIQUAD_HPP
#define BIQUAD_HPP

#include <gst/gst.h>
#include <complex>

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

class Biquad {
public:
    Biquad();
    ~Biquad();

    void set_frequency(double f);
    void set_q(double Q);
    void set_gain(double g);
    void set_filter_type(BiquadFilterType t);

    void set_samplerate(double rate);

    double get_frequency() { return frequency; }
    double get_q() { return q; }
    double get_gain() { return gain; }
    BiquadFilterType get_filter_type() { return filter_type; }

    void set_zero_pole_pairs(std::complex<double> zero, std::complex<double> pole);
    void reset();

    void get_frequency_response(int num_freq, const double* freqs, double* mag_res, double* phase_res);

    double process_double(const double source);
    float process_float(const float source);

private:
    // properties
    double frequency;
    double q;
    double gain;
    BiquadFilterType filter_type;

    // samplerate
    double samplerate = 44100.0;

    // filter params
    double b0;
    double b1;
    double b2;
    double a1;
    double a2;

    // history
    double x1;
    double x2;
    double y1;
    double y2;

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
