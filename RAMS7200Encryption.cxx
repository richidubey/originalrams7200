#include "RAMS7200Encryption.hxx"

void cookey(const unsigned int *raw1, unsigned int *keyout)
{
    unsigned int *cook;
    const unsigned int *raw0;
    unsigned int dough[32];
    int i;

    cook = dough;
    for(i=0; i < 16; i++, raw1++)
    {
        raw0 = raw1++;
        *cook    = (*raw0 & 0x00fc0000L) << 6;
        *cook   |= (*raw0 & 0x00000fc0L) << 10;
        *cook   |= (*raw1 & 0x00fc0000L) >> 10;
        *cook++ |= (*raw1 & 0x00000fc0L) >> 6;
        *cook    = (*raw0 & 0x0003f000L) << 12;
        *cook   |= (*raw0 & 0x0000003fL) << 16;
        *cook   |= (*raw1 & 0x0003f000L) >> 4;
        *cook++ |= (*raw1 & 0x0000003fL);
    }

    XMEMCPY(keyout, dough, sizeof(dough));
}
void deskey(const unsigned char *key, short edf, unsigned int *keyout)
{
    unsigned int i, j, l, m, n, kn[32];
    unsigned char pc1m[56], pcr[56];

    for (j=0; j < 56; j++) {
        l = (unsigned int)pc1[j];
        m = l & 7;
        pc1m[j] = (unsigned char)((key[l >> 3U] & bytebit[m]) == bytebit[m] ? 1 : 0);
    }

    for (i=0; i < 16; i++) {
        if (edf == DE1) {
           m = (15 - i) << 1;
        } else {
           m = i << 1;
        }
        n = m + 1;
        kn[m] = kn[n] = 0L;
        for (j=0; j < 28; j++) {
            l = j + (unsigned int)totrot[i];
            if (l < 28) {
               pcr[j] = pc1m[l];
            } else {
               pcr[j] = pc1m[l - 28];
            }
        }
        for (/*j = 28*/; j < 56; j++) {
            l = j + (unsigned int)totrot[i];
            if (l < 56) {
               pcr[j] = pc1m[l];
            } else {
               pcr[j] = pc1m[l - 28];
            }
        }
        for (j=0; j < 24; j++)  {
            if ((int)pcr[(int)pc2[j]] != 0) {
               kn[m] |= bigbyte[j];
            }
            if ((int)pcr[(int)pc2[j+24]] != 0) {
               kn[n] |= bigbyte[j];
            }
        }
    }

    cookey(kn, keyout);
}

#if (ARGTYPE == 0)
void crypt_argchk(const char *v, const char *s, int d)
{
 fprintf(stderr, "LTC_ARGCHK '%s' failure on line %d of file %s\n",
         v, d, s);
 //TODO: Put fprintf to file here.
}
#endif


int des_setup(const unsigned char *key, int keylen, int num_rounds, symmetric_key *skey)
{
    LTC_ARGCHK(key != NULL);
    LTC_ARGCHK(skey != NULL);

    if (num_rounds != 0 && num_rounds != 16) {
        return CRYPT_INVALID_ROUNDS;
    }

    if (keylen != 8) {
        return CRYPT_INVALID_KEYSIZE;
    }

    deskey(key, EN0, skey->des.ek);
    deskey(key, DE1, skey->des.dk);

    return CRYPT_OK;
}



void desfunc(unsigned int *block, const unsigned int *keys)
{
    unsigned int work, right, leftt;
    int cur_round;

    leftt = block[0];
    right = block[1];
   {
      long long unsigned int tmp;
      tmp = des_ip[0][LTC_BYTE(leftt, 0)] ^
            des_ip[1][LTC_BYTE(leftt, 1)] ^
            des_ip[2][LTC_BYTE(leftt, 2)] ^
            des_ip[3][LTC_BYTE(leftt, 3)] ^
            des_ip[4][LTC_BYTE(right, 0)] ^
            des_ip[5][LTC_BYTE(right, 1)] ^
            des_ip[6][LTC_BYTE(right, 2)] ^
            des_ip[7][LTC_BYTE(right, 3)];
      leftt = (unsigned int)(tmp >> 32);
      right = (unsigned int)(tmp & 0xFFFFFFFFUL);
   }

    for (cur_round = 0; cur_round < 8; cur_round++) {
        work  = RORc(right, 4) ^ *keys++;
        leftt ^= SP7[work        & 0x3fL]
              ^  SP5[(work >>  8) & 0x3fL]
              ^  SP3[(work >> 16) & 0x3fL]
              ^  SP1[(work >> 24) & 0x3fL];
        work  = right ^ *keys++;
        leftt ^= SP8[ work        & 0x3fL]
              ^  SP6[(work >>  8) & 0x3fL]
              ^  SP4[(work >> 16) & 0x3fL]
              ^  SP2[(work >> 24) & 0x3fL];

        work = RORc(leftt, 4) ^ *keys++;
        right ^= SP7[ work        & 0x3fL]
              ^  SP5[(work >>  8) & 0x3fL]
              ^  SP3[(work >> 16) & 0x3fL]
              ^  SP1[(work >> 24) & 0x3fL];
        work  = leftt ^ *keys++;
        right ^= SP8[ work        & 0x3fL]
              ^  SP6[(work >>  8) & 0x3fL]
              ^  SP4[(work >> 16) & 0x3fL]
              ^  SP2[(work >> 24) & 0x3fL];
    }

   {
      long long unsigned int tmp;
      tmp = des_fp[0][LTC_BYTE(leftt, 0)] ^
            des_fp[1][LTC_BYTE(leftt, 1)] ^
            des_fp[2][LTC_BYTE(leftt, 2)] ^
            des_fp[3][LTC_BYTE(leftt, 3)] ^
            des_fp[4][LTC_BYTE(right, 0)] ^
            des_fp[5][LTC_BYTE(right, 1)] ^
            des_fp[6][LTC_BYTE(right, 2)] ^
            des_fp[7][LTC_BYTE(right, 3)];
      leftt = (unsigned int)(tmp >> 32);
      right = (unsigned int)(tmp & 0xFFFFFFFFUL);
   }
    block[0] = right;
    block[1] = leftt;
}


                 
int des_ecb_encrypt(const unsigned char *pt, unsigned char *ct, const symmetric_key *skey)
{
    unsigned int work[2];
    LTC_ARGCHK(pt   != NULL);
    LTC_ARGCHK(ct   != NULL);
    LTC_ARGCHK(skey != NULL);
    LOAD32H(work[0], pt+0);
    LOAD32H(work[1], pt+4);
    desfunc(work, skey->des.ek);
    STORE32H(work[0],ct+0);
    STORE32H(work[1],ct+4);
    return CRYPT_OK;
}

int des_ecb_decrypt(const unsigned char *ct, unsigned char *pt, const symmetric_key *skey)
{
    unsigned int work[2];
    LTC_ARGCHK(pt   != NULL);
    LTC_ARGCHK(ct   != NULL);
    LTC_ARGCHK(skey != NULL);
    LOAD32H(work[0], ct+0);
    LOAD32H(work[1], ct+4);
    desfunc(work, skey->des.dk);
    STORE32H(work[0],pt+0);
    STORE32H(work[1],pt+4);
    return CRYPT_OK;
}

void des_done(symmetric_key *skey)
{
  LTC_UNUSED_PARAM(skey);
}

const char *error_to_string(int err)
{
   if (err < 0 || err >= (int)(sizeof(err_2_str)/sizeof(err_2_str[0]))) {
      return "Invalid error code.";
   }
   return err_2_str[err];
}