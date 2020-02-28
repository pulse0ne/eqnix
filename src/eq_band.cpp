#include "eq_band.hpp"
#include <cmath>
#include <complex>

static inline double calculate_omega(double freq, double rate) {
    double r = freq / rate;
    return r >= 0.5 ? M_PI : freq <= 0.0 ? 0.0 : 2.0 * M_PI * r;
}

double EQBand::calculate_bw() {
    double calculated_bw = 0.0;

    if (bw / rate >= 0.5) {
        // if bandwidth == 0.5, set the bandwidth to a slightly smaller value
        calculated_bw = M_PI - 0.00000001;
    } else if (bw <= 0.0) {
        // if bandwidth == 0 this band won't change anything
        a0 = 1.0;
        a1 = 0.0;
        a2 = 0.0;
        b1 = 0.0;
        b2 = 0.0;
    } else {
        calculated_bw = 2.0 * M_PI * (bw / rate);
    }
    return calculated_bw;
}

void EQBand::setup_peaking() {
    double scaled_gain, omega, calculated_bw;
    double alpha, alpha1, alpha2, b0;

    scaled_gain = pow(10.0, gain / 40.0);
    omega = calculate_omega(freq, rate);
    calculated_bw = calculate_bw();
    if (calculated_bw == 0.0) return;

    alpha = tan(calculated_bw / 2.0);
    alpha1 = alpha * scaled_gain;
    alpha2 = alpha / scaled_gain;

    b0 = (1.0 + alpha2);

    a0 = (1.0 + alpha1) / b0;
    a1 = (-2.0 * cos(omega)) / b0;
    a2 = (1.0 - alpha1) / b0;
    b1 = (2.0 * cos(omega)) / b0;
    b2 = -(1.0 - alpha2) / b0;
}

void EQBand::setup_highshelf() {
    double scaled_gain, omega, calculated_bw;
    double alpha, delta, b0;
    double egp, egm;

    scaled_gain = pow(10.0, gain / 40.0);
    omega = calculate_omega(freq, rate);
    calculated_bw = calculate_bw();
    if (calculated_bw == 0.0) return;
    
    egm = scaled_gain - 1.0;
    egp = scaled_gain + 1.0;
    alpha = tan(calculated_bw / 2.0);
    delta = 2.0 * sqrt(scaled_gain) * alpha;
    
    b0 = egp - egm * cos(omega) + delta;

    a0 = ((egp + egm * cos(omega) + delta) * scaled_gain) / b0;
    a1 = ((egm + egp * cos(omega)) * -2.0 * scaled_gain) / b0;
    a2 = ((egp + egm * cos(omega) - delta) * scaled_gain) / b0;
    b1 = ((egm - egp * cos(omega)) * -2.0) / b0;
    b2 = -((egp - egm * cos(omega) - delta)) / b0;
}

void EQBand::setup_lowshelf() {
    double scaled_gain, omega, calculated_bw;
    double alpha, delta, b0;
    double egp, egm;

    scaled_gain = pow(10.0, gain / 40.0);
    omega = calculate_omega(freq, rate);
    calculated_bw = calculate_bw();
    if (calculated_bw == 0.0) return;

    egm = gain - 1.0;
    egp = gain + 1.0;
    alpha = tan(calculated_bw / 2.0);
    delta = 2.0 * sqrt(scaled_gain) * alpha;

    b0 = egp + egm * cos(omega) + delta;

    a0 = ((egp - egm * cos(omega) + delta) * scaled_gain) / b0;
    a1 = ((egm - egp * cos(omega)) * 2.0 * scaled_gain) / b0;
    a2 = ((egp - egm * cos(omega) - delta) * scaled_gain) / b0;
    b1 = ((egm + egp * cos(omega)) * 2.0) / b0;
    b2 = -((egp + egm * cos(omega) - delta)) / b0;
}

void EQBand::update_coefficients() {
    if (bandtype == BandType::PEAKING) {
        setup_peaking();
    } else if (bandtype == BandType::HIGH_SHELF) {
        setup_highshelf();
    } else {
        setup_lowshelf();
    }
}

void EQBand::set_band_type(BandType new_bt) {
    lock.lock();
    bandtype = new_bt;
    update_coefficients();
    lock.unlock();
}

void EQBand::set_freq(double new_freq) {
    lock.lock();
    freq = new_freq;
    update_coefficients();
    lock.unlock();
}

void EQBand::set_bw(double new_bw) {
    lock.lock();
    bw = new_bw;
    update_coefficients();
    lock.unlock();
}

void EQBand::set_gain(double new_gain) {
    lock.lock();
    gain = new_gain;
    update_coefficients();
    lock.unlock();
}

std::vector<double> EQBand::get_frequency_response(std::vector<double> frequencies) {
    std::vector<double> fr(frequencies.size());
    /**
     *         b0 + b1*(z^-1)+b2*(z^-2)
     * H(z) = --------------------------
     *         a0 + a1*(z^-1)+a2*(z^-2)
     */
    double scaled_gain = pow(10.0, gain / 40.0);
    double omega = calculate_omega(freq, rate);
    double calculated_bw = calculate_bw();
    double b0;

    if (bandtype == BandType::PEAKING) {
        b0 = 1.0 + (tan(calculated_bw / 2.0) / scaled_gain);
    } else if (bandtype == BandType::HIGH_SHELF) {
        double egm = scaled_gain - 1.0;
        double egp = scaled_gain + 1.0;
        double alpha = tan(calculated_bw / 2.0);
        double delta = 2.0 * sqrt(scaled_gain) * alpha;
        
        b0 = egp - egm * cos(omega) + delta;
    } else {
        double egm = gain - 1.0;
        double egp = gain + 1.0;
        double alpha = tan(calculated_bw / 2.0);
        double delta = 2.0 * sqrt(scaled_gain) * alpha;

        b0 = egp + egm * cos(omega) + delta;
    }

    for (auto i = 0; i < frequencies.size(); i++) {
        double o = -M_PI * (frequencies[i] / (rate / 2.0));
        std::complex<double> z = std::complex<double>(cos(o), sin(o));
        std::complex<double> n = b0 + (b1 + b2 * z) * z;
        std::complex<double> d = std::complex<double>(1, 0) + (a1 + a2 * z) * z;
        std::complex<double> r = n / d;
        // fr.push_back(abs(r));
        fr[i] = abs(r);
    }

    return fr;    
}
