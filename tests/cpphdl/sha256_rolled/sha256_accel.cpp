#include "sha256_accel.hpp"

/* This implementation of SHA256 derived from:
 * http://www.spale.com/download/scrypt/scrypt1.0/sha256.c
 *
 * The original copyright follows and continues to apply to this file
 * and this file only, not the remainder of software in this project.
 */

/*
 *  Copyright (C) 2001-2003  Christophe Devine
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307
 *  USA
 */

void SHA256::start() {
    total = 0;
    finalized = false;

    state[0] = 0x6A09E667;
    state[1] = 0xBB67AE85;
    state[2] = 0x3C6EF372;
    state[3] = 0xA54FF53A;
    state[4] = 0x510E527F;
    state[5] = 0x9B05688C;
    state[6] = 0x1F83D9AB;
    state[7] = 0x5BE0CD19;
}

// DBL_INT_ADD treats two unsigned ints a and b as one 64-bit integer and adds c to it
#define DBL_INT_ADD(a,b,c) if (a > 0xffffffff - (c)) ++b; a += c;
#define ROTLEFT(a,b) (((a) << (b)) | ((a) >> (32-(b))))
#define ROTRIGHT(a,b) (((a) >> (b)) | ((a) << (32-(b))))

#define CH(x,y,z) (((x) & (y)) ^ (~(x) & (z)))
#define MAJ(x,y,z) (((x) & (y)) ^ ((x) & (z)) ^ ((y) & (z)))
#define EP0(x) (ROTRIGHT(x,2) ^ ROTRIGHT(x,13) ^ ROTRIGHT(x,22))
#define EP1(x) (ROTRIGHT(x,6) ^ ROTRIGHT(x,11) ^ ROTRIGHT(x,25))
#define SIG0(x) (ROTRIGHT(x,7) ^ ROTRIGHT(x,18) ^ ((x) >> 3))
#define SIG1(x) (ROTRIGHT(x,17) ^ ROTRIGHT(x,19) ^ ((x) >> 10))

bool SHA256::update(Data data, unsigned len)  {
    if (finalized || len > 64)
        return false;

    const uint32_t k __attribute__((__vector_size__(256))) = {
       0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
       0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
       0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
       0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
       0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
       0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
       0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
       0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
       0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
       0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
       0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
       0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
       0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
       0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
       0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
       0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };

    uint32_t a,b,c,d,e,f,g,h,i,j,t1,t2;
    uint32_t m __attribute__((__vector_size__(256)));

    if (len < 64) {
        data[len] = 0x80;
        finalized = true;
    }
    for (unsigned i=(len+1); i<64; i++) {
        data[i] = 0;
    }

    total += len;

    State s = state;

    for (i=0,j=0; i < 16; ++i, j += 4)
        m[i] = (data[j] << 24) | (data[j+1] << 16) | (data[j+2] << 8) | (data[j+3]);
    for ( ; i < 64; ++i)
        m[i] = SIG1(m[i-2]) + m[i-7] + SIG0(m[i-15]) + m[i-16];

    a = s[0];
    b = s[1];
    c = s[2];
    d = s[3];
    e = s[4];
    f = s[5];
    g = s[6];
    h = s[7];

    for (i = 0; i < 64; ++i) {
        t1 = h + EP1(e) + CH(e,f,g) + k[i] + m[i];
        t2 = EP0(a) + MAJ(a,b,c);
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    State newState = {
        s[0] + a, s[1] + b, s[2] + c, s[3] + d,
        s[4] + e, s[5] + f, s[6] + g, s[7] + h
    };
    this->state = newState;

    return true;
}

#define GET_UINT32(n,b,i)                       \
{                                               \
    (n) = ( (uint32_t) (b)[(i)    ] << 24 )       \
        | ( (uint32_t) (b)[(i) + 1] << 16 )       \
        | ( (uint32_t) (b)[(i) + 2] <<  8 )       \
        | ( (uint32_t) (b)[(i) + 3]       );      \
}

#define PUT_UINT32(n,b,i)                       \
{                                               \
    (b)[(i)    ] = (uint8_t) ( (n) >> 24 );       \
    (b)[(i) + 1] = (uint8_t) ( (n) >> 16 );       \
    (b)[(i) + 2] = (uint8_t) ( (n) >>  8 );       \
    (b)[(i) + 3] = (uint8_t) ( (n)       );       \
}

SHA256::Digest SHA256::digest() {
    SHA256::Data sha256_padding = { 
     0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    uint32_t high, low;
    uint64_t total = this->total >> 3;
    uint8_t msglen[8];

    high = total & 0xFFFFFFFF;
    low  = (total >> 32);

    PUT_UINT32( high, msglen, 0 );
    PUT_UINT32( low,  msglen, 4 );

    if (!finalized) {
        finalized = false;
        update(sha256_padding, 64);
    }

    Data len_end = {
        msglen[0], msglen[1], msglen[2], msglen[3],
        msglen[4], msglen[5], msglen[6], msglen[7],
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    update( len_end, 64 );

    Digest digest = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    PUT_UINT32( state[0], digest,  0 );
    PUT_UINT32( state[1], digest,  4 );
    PUT_UINT32( state[2], digest,  8 );
    PUT_UINT32( state[3], digest, 12 );
    PUT_UINT32( state[4], digest, 16 );
    PUT_UINT32( state[5], digest, 20 );
    PUT_UINT32( state[6], digest, 24 );
    PUT_UINT32( state[7], digest, 28 );
    return digest;
}

