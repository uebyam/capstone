#include "dh.h"

// to[0] = base
// to[1] == base ** 2   % mod
// to[2] == base ** 4   % mod
// to[3] == base ** 8   % mod
// to[4] == base ** 16  % mod
// ...
void dh_store_sqrmod_arr(uint16_t *to, uint16_t len, uint16_t _base, uint16_t _mod) {
    if (!len) return;
    if (!to) return;
    uint32_t base = _base, mod = _mod;
    if (base > mod) base %= mod;
    to[0] = (uint16_t)base;

    for (uint16_t i = 1; i < len; i++) {
        base *= base;
        if (base > mod) base %= mod;
        to[i] = base;
    }
}

// pass in a sqrmod array length 16, or at least length log2(pow)
uint16_t dh_modpow_witharr(uint16_t *arr, uint16_t pow, uint16_t _mod) {
    uint32_t mod = _mod;
    uint32_t result = 1;

    int ind = 0;
    while (pow) {
        if (pow & 1) {
            result *= arr[ind];
            result %= mod;
        }
        ind++;
        pow >>= 1;
    }

    return result;
}

uint16_t dh_modpow(uint16_t _base, uint16_t pow, uint16_t _mod) {
    uint32_t base = _base;
    uint32_t mod = _mod;
    if (base > mod) base %= mod;
    uint32_t result = 1;

    int ind = 0;
    while (pow) {
        if (ind >= 1) {
            base *= base;
        } else ind++;

        if (pow & 1) {
            if (base > mod) base %= mod;
            result *= base;
            result %= mod;
        } else if (base > 65535) base %= mod;
        pow >>= 1;
    }

    return result;
}

void dh_compute_public_key(uint16_t *dest, uint16_t *src, uint16_t len, uint16_t mod, uint16_t gen) {
    for (uint16_t i = 0; i < len; i++) {
        uint16_t tmp = dh_modpow(gen, src[i], mod);
        dest[i] = tmp;
    }
}

void dh_compute_shared_secret(uint16_t *dest, uint16_t *sec, uint16_t *pub, uint16_t len, uint16_t mod) {
    for (uint16_t i = 0; i < len; i++) {
        uint16_t tmp = dh_modpow(pub[i], sec[i], mod);
        dest[i] = tmp;
    }
}
