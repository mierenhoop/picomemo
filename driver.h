/**
 * Copyright 2026 mierenhoop
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef OMEMO_DRIVER_H_
#define OMEMO_DRIVER_H_

#include "omemo.h"

int omemoDriverHmac(const omemoKey k, const uint8_t *in, size_t ilen, uint8_t out[static 32]);
int omemoDriverAesEncrypt(omemoKey k, size_t n, uint8_t iv[static 16], const uint8_t *s, uint8_t *d);
int omemoDriverAesDecrypt(omemoKey k, size_t n, uint8_t iv[static 16], const uint8_t *s, uint8_t *d);
int omemoDriverHkdf(const uint8_t *salt, size_t saltn, const uint8_t *key, size_t keyn, const uint8_t *info, size_t infon, uint8_t *out, size_t outn);
int omemoDriverGcmEncrypt(uint8_t *d, const uint8_t key[static 16], size_t n, const uint8_t iv[static 12], uint8_t tag[static 16], const uint8_t *s);
int omemoDriverGcmDecrypt(uint8_t *d, const uint8_t key[static 16], size_t n, const uint8_t iv[static 12], const uint8_t *tag, size_t tagn, const uint8_t *s);
int omemoDriverCompare(const void *a, const void *b, size_t n);

void omemoDriverEdSignMod(omemoCurveSignature sig, omemoKey pub, omemoKey prv, uint8_t *msg, size_t msgn);
bool omemoDriverEdVerify(omemoCurveSignature sig, omemoKey pub, uint8_t *msg, size_t msgn);
void omemoDriverEdSeedToPubPrv(omemoKey pub, omemoKey prv, omemoKey seed);
void omemoDriverEdPubToCvPub(omemoKey cv, omemoKey ed);
void omemoDriverCvPrvToEdPub(omemoKey pub, omemoKey prv);
void omemoDriverCvPubToEdPub(omemoKey ed, omemoKey cv);
void omemoDriverCvPrvToPub(omemoKey pub, omemoKey prv);
int  omemoDriverX25519(omemoKey out, omemoKey prv, omemoKey pub);

#endif
