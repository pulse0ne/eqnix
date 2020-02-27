#include "eq_band.hpp"

std::vector<float> EQBand::get_frequency_response(const std::vector<const float> frequencies) {
    std::vector<float> fr(frequencies.size());
    return fr;    
}

void EQBand::update_coefficients() {
    
}

void EQBand::set_band_type(BandType new_bt) {
    lock.lock();
    bandtype = new_bt;
    update_coefficients();
    lock.unlock();
}

void EQBand::set_freq(float new_freq) {
    lock.lock();
    freq = new_freq;
    update_coefficients();
    lock.unlock();
}

void EQBand::set_bw(float new_bw) {
    lock.lock();
    bw = new_bw;
    update_coefficients();
    lock.unlock();
}

void EQBand::set_gain(float new_gain) {
    lock.lock();
    gain = new_gain;
    update_coefficients();
    lock.unlock();
}
