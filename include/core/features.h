#ifndef _WIFI67_FEATURES_H_
#define _WIFI67_FEATURES_H_

struct wifi67_features {
    bool has_6ghz;
    bool has_5ghz;
    bool has_2ghz;
    bool has_160mhz;
    bool has_320mhz;
    bool has_mu_mimo;
    bool has_ofdma;
    bool has_4ss;
    bool has_8ss;
    bool has_16ss;
    bool has_advanced_coding;
    bool has_advanced_beamforming;
    bool has_advanced_security;
};

#endif /* _WIFI67_FEATURES_H_ */ 