#ifndef __SHA256_ACCEL_HPP__
#define __SHA256_ACCEL_HPP__

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

#include <stdint.h>
#include <cstring>
#include <cstdio>
#include <limits>
#include <boost/random.hpp>

class SHA256 {
public:
    typedef uint8_t Digest __attribute__((__vector_size__(32)));
    typedef uint8_t Data __attribute__((__vector_size__(64)));
    typedef uint32_t State __attribute__((__vector_size__(32)));

    uint64_t total;
    bool finalized;
    State state;

    SHA256() {
    }

    void start();
    bool update(Data input, unsigned len);
    Digest digest();
};

#endif // __SHA256_ACCEL_HPP__
