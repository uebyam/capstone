#ifndef DH_H
#define DH_H

#include <inttypes.h>

void dh_store_sqrmod_arr(uint16_t *dest, uint16_t len, uint16_t base, uint16_t mod);
uint16_t dh_modpow_witharr(uint16_t *arr, uint16_t pow, uint16_t mod);
uint16_t dh_modpow(uint16_t base, uint16_t pow, uint16_t mod);

void dh_compute_public_key(uint16_t *dest, uint16_t *src, uint16_t len, uint16_t mod, uint16_t gen);
void dh_compute_shared_secret(uint16_t *dest, uint16_t *sec, uint16_t *pub, uint16_t len, uint16_t mod);

#define DH_DEFAULT_GENERATOR 727
#define DH_DEFAULT_MOD 65521

#endif
