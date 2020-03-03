#include "biquad.hpp"

static inline double flush_denormal(double d) {
    return (fabs(d) < DBL_MIN) ? 0.0f : d;
}

Biquad::Biquad() {
    set_normalized_coefficients(1, 0, 0, 1, 0, 0);
    reset();
    samplerate = 44100.0;
    frequency = 350.0;
    q = 1.0;
    gain = 0.0;
    filter_type = BiquadFilterType::PEAKING;
    update_params();
    set_channels(1);
}

Biquad::~Biquad() = default;

double Biquad::process(const double source, uint chan_ix) {
    FilterHistory fh = history.at(chan_ix);
    double x = source;
    double x1 = fh.x1;
    double x2 = fh.x2;
    double y1 = fh.y1;
    double y2 = fh.y2;

    double y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;

    fh.x2 = flush_denormal(x1);
    fh.x1 = flush_denormal(x);
    fh.y2 = flush_denormal(y1);
    fh.y1 = flush_denormal(y);

    return y;
}

void Biquad::set_frequency(double f) {
    frequency = f;
    update_params();
}

void Biquad::set_q(double Q) {
    q = Q;
    update_params();
}

void Biquad::set_gain(double g) {
    gain = g;
    update_params();
}

void Biquad::set_filter_type(BiquadFilterType t) {
    filter_type = t;
    update_params();
}

void Biquad::set_zero_pole_pairs(std::complex<double> zero, std::complex<double> pole) {
    double zb0 = 1;
    double zb1 = -2 * zero.real();

    double zero_mag = abs(zero);
    double zb2 = zero_mag * zero_mag;

    double za1 = -2 * pole.real();

    double pole_mag = abs(pole);
    double za2 = pole_mag * pole_mag;

    set_normalized_coefficients(zb0, zb1, zb2, 1, za1, za2);
}

// void Biquad::reset() {
//     for (FilterHistory<double> h : histories_double) {
//         h.x1 = h.x2 = h.y1 = h.y2 = 0;
//     }

//     for (FilterHistory<float> h : histories_float) {
//         h.x1 = h.x2 = h.y1 = h.y2 = 0;
//     }
// }

void Biquad::set_normalized_coefficients(double nb0, double nb1, double nb2, double na0, double na1, double na2) {
    double a0inv = 1.0 / na0;

    b0 = nb0 * a0inv;
    b1 = nb1 * a0inv;
    b2 = nb2 * a0inv;
    a1 = na1 * a0inv;
    a2 = na2 * a0inv;
}

void Biquad::update_params() {
    switch (filter_type) {
        case ALLPASS:
            set_allpass_params();
            break;
        case BANDPASS:
            set_bandpass_params();
            break;
        case HIGHPASS:
            set_highpass_params();
            break;
        case HIGHSHELF:
            set_highshelf_params();
            break;
        case LOWPASS:
            set_lowpass_params();
            break;
        case LOWSHELF:
            set_lowpass_params();
            break;
        case NOTCH:
            set_notch_params();
            break;
        case PEAKING:
            set_peaking_params();
            break;
        default:
            break;
    }
}

void Biquad::set_allpass_params() {
    double n_frequency = std::max(0.0, std::min(frequency / (samplerate / 2.0), 1.0));
    q = std::max(0.0, q);

    if (n_frequency > 0 && n_frequency < 1) {
        if (q > 0) {
            double omega = M_PI * n_frequency;
            double alpha = sin(omega) / (2 * q);
            double k = cos(omega);

            double nb0 = 1 - alpha;
            double nb1 = -2 * k;
            double nb2 = 1 + alpha;
            double na0 = 1 + alpha;
            double na1 = -2 * k;
            double na2 = 1 - alpha;

            set_normalized_coefficients(nb0, nb1, nb2, na0, na1, na2);
        } else {
            set_normalized_coefficients(-1, 0, 0, 1, 0, 0);
        }
    } else {
        set_normalized_coefficients(1, 0, 0, 1, 0, 0);
    }
}

void Biquad::set_bandpass_params() {
    double n_frequency = std::max(0.0, frequency / (samplerate / 2.0));
    q = std::max(0.0, q);

    if (n_frequency > 0 && n_frequency < 1) {
        double omega = M_PI * n_frequency;
        if (q > 0) {
            double alpha = sin(omega) / (2 * q);
            double k = cos(omega);

            double nb0 = alpha;
            double nb1 = 0;
            double nb2 = -alpha;
            double na0 = 1 + alpha;
            double na1 = -2 * k;
            double na2 = 1 - alpha;

            set_normalized_coefficients(nb0, nb1, nb2, na0, na1, na2);
        } else {
            set_normalized_coefficients(1, 0, 0, 1, 0, 0);
        }
    } else {
        set_normalized_coefficients(0, 0, 0, 1, 0, 0);
    }
}

void Biquad::set_highpass_params() {
    double n_frequency = std::max(0.0, std::min(frequency / (samplerate / 2.0), 1.0));

    if (n_frequency == 1) {
        set_normalized_coefficients(0, 0, 0, 1, 0, 0);
    } else if (n_frequency > 0) {
        q = std::max(0.0, q);
        double g = pow(10.0, 0.05 * q);
        double d = sqrt((4 - sqrt(16 - 16 / (g * g))) / 2);

        double theta = M_PI * n_frequency;
        double sn = 0.5 * d * sin(theta);
        double beta = 0.5 * (1 - sn) / (1 + sn);
        double gamma = (0.5 + beta) * cos(theta);
        double alpha = 0.25 * (0.5 + beta + gamma);

        double nb0 = 2 * alpha;
        double nb1 = 2 * -2 * alpha;
        double nb2 = 2 * alpha;
        double na1 = 2 * -gamma;
        double na2 = 2 * beta;

        set_normalized_coefficients(nb0, nb1, nb2, 1, na1, na2);
    } else {
        set_normalized_coefficients(1, 0, 0, 1, 0, 0);
    }
}

void Biquad::set_highshelf_params() {
    double n_frequency = std::max(0.0, std::min(frequency / (samplerate / 2.0), 1.0));
    double A = pow(10.0, gain / 40);

    if (n_frequency == 1) {
        set_normalized_coefficients(1, 0, 0, 1, 0, 0);
    } else if (n_frequency > 0) {
        double omega = M_PI * n_frequency;
        double S = 1;
        double alpha = 0.5 * sin(omega) * sqrt((A + 1 / A) * (1 / S - 1) + 2);
        double k = cos(omega);
        double k2 = 2 * sqrt(A) * alpha;
        double aPlusOne = A + 1;
        double aMinusOne = A - 1;

        double nb0 = A * (aPlusOne + aMinusOne * k + k2);
        double nb1 = -2 * A * (aMinusOne + aPlusOne * k);
        double nb2 = A * (aPlusOne + aMinusOne * k - k2);
        double na0 = aPlusOne - aMinusOne * k + k2;
        double na1 = 2 * (aMinusOne - aPlusOne * k);
        double na2 = aPlusOne - aMinusOne * k - k2;

        set_normalized_coefficients(nb0, nb1, nb2, na0, na1, na2);
    } else {
        set_normalized_coefficients(A * A, 0, 0, 1, 0, 0);
    }
}

void Biquad::set_lowpass_params() {
    double n_frequency = std::max(0.0, std::min(frequency / (samplerate / 2.0), 1.0));

    if (n_frequency == 1) {
        set_normalized_coefficients(1, 0, 0, 1, 0, 0);
    } else if (n_frequency > 0) {
        double n_q = std::max(0.0, q);
        double g = pow(10.0, 0.05 * n_q);
        double d = sqrt((4 - sqrt(16 - 16 / (g * g))) / 2);

        double theta = n_q * n_frequency;
        double sn = 0.5 * d * sin(theta);
        double beta = 0.5 * (1 - sn) / (1 + sn);
        double gamma = (0.5 + beta) * cos(theta);
        double alpha = 0.25 * (0.5 + beta - gamma);

        double nb0 = 2 * alpha;
        double nb1 = 2 * 2 * alpha;
        double nb2 = 2 * alpha;
        double na1 = 2 * -gamma;
        double na2 = 2 * beta;

        set_normalized_coefficients(nb0, nb1, nb2, 1, na1, na2);
    } else {
        set_normalized_coefficients(0, 0, 0, 1, 0, 0);
    }
}

void Biquad::set_lowshelf_params() {
    double n_frequency = std::max(0.0, std::min(frequency / (samplerate / 2.0), 1.0));
    double A = pow(10.0, gain / 40);

    if (n_frequency == 1) {
        set_normalized_coefficients(A * A, 0, 0, 1, 0, 0);
    } else if (n_frequency > 0) {
        double omega = M_PI * n_frequency;
        double S = 1; // filter slope (1 is max value)
        double alpha = 0.5 * sin(omega) * sqrt((A + 1 / A) * (1 / S - 1) + 2);
        double k = cos(omega);
        double k2 = 2 * sqrt(A) * alpha;
        double aPlusOne = A + 1;
        double aMinusOne = A - 1;

        double nb0 = A * (aPlusOne - aMinusOne * k + k2);
        double nb1 = 2 * A * (aMinusOne - aPlusOne * k);
        double nb2 = A * (aPlusOne - aMinusOne * k - k2);
        double na0 = aPlusOne + aMinusOne * k + k2;
        double na1 = -2 * (aMinusOne + aPlusOne * k);
        double na2 = aPlusOne + aMinusOne * k - k2;

        set_normalized_coefficients(nb0, nb1, nb2, na0, na1, na2);
    } else {
        set_normalized_coefficients(1, 0, 0, 1, 0, 0);
    }
}

void Biquad::set_notch_params() {
    double n_frequency = std::max(0.0, std::min(frequency / (samplerate / 2.0), 1.0));
    q = std::max(0.0, q);

    if (n_frequency > 0 && n_frequency < 1) {
        if (q > 0) {
            double omega = M_PI * n_frequency;
            double alpha = sin(omega) / (2 * q);
            double k = cos(omega);

            double nb0 = 1;
            double nb1 = -2 * k;
            double nb2 = 1;
            double na0 = 1 + alpha;
            double na1 = -2 * k;
            double na2 = 1 - alpha;

            set_normalized_coefficients(nb0, nb1, nb2, na0, na1, na2);
        } else {
            set_normalized_coefficients(0, 0, 0, 1, 0, 0);
        }
    } else {
        set_normalized_coefficients(1, 0, 0, 1, 0, 0);
    }
}

void Biquad::set_peaking_params() {
    double n_frequency = std::max(0.0, std::min(frequency / (samplerate / 2.0), 1.0));
    g_print("%f  -->  %f\n", frequency, n_frequency);
    double n_q = std::max(0.0, q);
    double A = pow(10.0, gain / 40.0);

    if (n_frequency > 0 && n_frequency < 1) {
        if (n_q > 0) {
            double omega = M_PI * n_frequency;
            double alpha = sin(omega) / (2 * n_q);
            double k = cos(omega);

            double nb0 = 1 + alpha * A;
            double nb1 = -2 * k;
            double nb2 = 1 - alpha * A;
            double na0 = 1 + alpha / A;
            double na1 = -2 * k;
            double na2 = 1 - alpha / A;

            set_normalized_coefficients(nb0, nb1, nb2, na0, na1, na2);
        } else {
            set_normalized_coefficients(A * A, 0, 0, 1, 0, 0);
        }
    } else {
        set_normalized_coefficients(1, 0, 0, 1, 0, 0);
    }
}

void Biquad::get_frequency_response(int num_freq, const double* freqs, double* mag_res, double* phase_res) {
    double nb0 = b0;
    double nb1 = b1;
    double nb2 = b2;
    double na1 = a1;
    double na2 = a2;

    for (int k = 0; k < num_freq; ++k) {
        g_print("fr:  %f   ", freqs[k]);
        double normalized_freq = std::min(freqs[k] / (samplerate / 2.0), 1.0);
        double omega = -M_PI * normalized_freq;
        std::complex<double> z = std::complex<double>(cos(omega), sin(omega));
        std::complex<double> numerator = nb0 + (nb1 + nb2 * z) * z;
        std::complex<double> denominator = std::complex<double>(1, 0) + (na1 + na2 * z) * z;
        g_print("%f\n", denominator);
        std::complex<double> response = numerator / denominator;
        mag_res[k] = static_cast<float>(abs(response));
        phase_res[k] = static_cast<float>(atan2(imag(response), real(response)));
    }
    for (auto i = 0; i < num_freq; ++i) {
        g_print("%f ", mag_res[i]);
    }
}

void Biquad::set_samplerate(double rate) {
    samplerate = rate;
    update_params();
}

void Biquad::set_channels(uint num_channels) {
    if (channels != num_channels) {
        channels = num_channels;
        // TODO: allocate
    }
}
