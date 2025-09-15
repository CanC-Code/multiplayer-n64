/* md5.c - RFC1321 style MD5 implementation
 * Public domain
 */

#include "md5.h"
#include <string.h> /* memcpy, memset */

#define T_MASK ((md5_word_t)~0)
#define T1  0xd76aa478
#define T2  0xe8c7b756
#define T3  0x242070db
#define T4  0xc1bdceee
#define T5  0xf57c0faf
#define T6  0x4787c62a
#define T7  0xa8304613
#define T8  0xfd469501
#define T9  0x698098d8
#define T10 0x8b44f7af
#define T11 0xffff5bb1
#define T12 0x895cd7be
#define T13 0x6b901122
#define T14 0xfd987193
#define T15 0xa679438e
#define T16 0x49b40821
#define T17 0xf61e2562
#define T18 0xc040b340
#define T19 0x265e5a51
#define T20 0xe9b6c7aa
#define T21 0xd62f105d
#define T22 0x02441453
#define T23 0xd8a1e681
#define T24 0xe7d3fbc8
#define T25 0x21e1cde6
#define T26 0xc33707d6
#define T27 0xf4d50d87
#define T28 0x455a14ed
#define T29 0xa9e3e905
#define T30 0xfcefa3f8
#define T31 0x676f02d9
#define T32 0x8d2a4c8a
#define T33 0xfffa3942
#define T34 0x8771f681
#define T35 0x6d9d6122
#define T36 0xfde5380c
#define T37 0xa4beea44
#define T38 0x4bdecfa9
#define T39 0xf6bb4b60
#define T40 0xbebfbc70
#define T41 0x289b7ec6
#define T42 0xeaa127fa
#define T43 0xd4ef3085
#define T44 0x04881d05
#define T45 0xd9d4d039
#define T46 0xe6db99e5
#define T47 0x1fa27cf8
#define T48 0xc4ac5665
#define T49 0xf4292244
#define T50 0x432aff97
#define T51 0xab9423a7
#define T52 0xfc93a039
#define T53 0x655b59c3
#define T54 0x8f0ccc92
#define T55 0xffeff47d
#define T56 0x85845dd1
#define T57 0x6fa87e4f
#define T58 0xfe2ce6e0
#define T59 0xa3014314
#define T60 0x4e0811a1
#define T61 0xf7537e82
#define T62 0xbd3af235
#define T63 0x2ad7d2bb
#define T64 0xeb86d391

#define F(x, y, z) ((x & y) | (~x & z))
#define G(x, y, z) ((x & z) | (y & ~z))
#define H(x, y, z) (x ^ y ^ z)
#define I(x, y, z) (y ^ (x | ~z))

#define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

#define STEP(f, a, b, c, d, x, t, s) \
    a += f(b, c, d) + x + t; \
    a = ROTATE_LEFT(a, s); \
    a += b;

static void md5_process(md5_state_t *pms, const md5_byte_t *data)
{
    md5_word_t a = pms->abcd[0], b = pms->abcd[1],
               c = pms->abcd[2], d = pms->abcd[3];
    md5_word_t x[16];
    int i;

    for (i = 0; i < 16; ++i) {
        x[i] = (md5_word_t)data[i*4] |
               ((md5_word_t)data[i*4+1] << 8) |
               ((md5_word_t)data[i*4+2] << 16) |
               ((md5_word_t)data[i*4+3] << 24);
    }

    /* Round 1 */
    STEP(F, a, b, c, d, x[0], T1, 7)
    STEP(F, d, a, b, c, x[1], T2, 12)
    STEP(F, c, d, a, b, x[2], T3, 17)
    STEP(F, b, c, d, a, x[3], T4, 22)
    STEP(F, a, b, c, d, x[4], T5, 7)
    STEP(F, d, a, b, c, x[5], T6, 12)
    STEP(F, c, d, a, b, x[6], T7, 17)
    STEP(F, b, c, d, a, x[7], T8, 22)
    STEP(F, a, b, c, d, x[8], T9, 7)
    STEP(F, d, a, b, c, x[9], T10, 12)
    STEP(F, c, d, a, b, x[10], T11, 17)
    STEP(F, b, c, d, a, x[11], T12, 22)
    STEP(F, a, b, c, d, x[12], T13, 7)
    STEP(F, d, a, b, c, x[13], T14, 12)
    STEP(F, c, d, a, b, x[14], T15, 17)
    STEP(F, b, c, d, a, x[15], T16, 22)

    /* Round 2 */
    STEP(G, a, b, c, d, x[1], T17, 5)
    STEP(G, d, a, b, c, x[6], T18, 9)
    STEP(G, c, d, a, b, x[11], T19, 14)
    STEP(G, b, c, d, a, x[0], T20, 20)
    STEP(G, a, b, c, d, x[5], T21, 5)
    STEP(G, d, a, b, c, x[10], T22, 9)
    STEP(G, c, d, a, b, x[15], T23, 14)
    STEP(G, b, c, d, a, x[4], T24, 20)
    STEP(G, a, b, c, d, x[9], T25, 5)
    STEP(G, d, a, b, c, x[14], T26, 9)
    STEP(G, c, d, a, b, x[3], T27, 14)
    STEP(G, b, c, d, a, x[8], T28, 20)
    STEP(G, a, b, c, d, x[13], T29, 5)
    STEP(G, d, a, b, c, x[2], T30, 9)
    STEP(G, c, d, a, b, x[7], T31, 14)
    STEP(G, b, c, d, a, x[12], T32, 20)

    /* Round 3 */
    STEP(H, a, b, c, d, x[5], T33, 4)
    STEP(H, d, a, b, c, x[8], T34, 11)
    STEP(H, c, d, a, b, x[11], T35, 16)
    STEP(H, b, c, d, a, x[14], T36, 23)
    STEP(H, a, b, c, d, x[1], T37, 4)
    STEP(H, d, a, b, c, x[4], T38, 11)
    STEP(H, c, d, a, b, x[7], T39, 16)
    STEP(H, b, c, d, a, x[10], T40, 23)
    STEP(H, a, b, c, d, x[13], T41, 4)
    STEP(H, d, a, b, c, x[0], T42, 11)
    STEP(H, c, d, a, b, x[3], T43, 16)
    STEP(H, b, c, d, a, x[6], T44, 23)
    STEP(H, a, b, c, d, x[9], T45, 4)
    STEP(H, d, a, b, c, x[12], T46, 11)
    STEP(H, c, d, a, b, x[15], T47, 16)
    STEP(H, b, c, d, a, x[2], T48, 23)

    /* Round 4 */
    STEP(I, a, b, c, d, x[0], T49, 6)
    STEP(I, d, a, b, c, x[7], T50, 10)
    STEP(I, c, d, a, b, x[14], T51, 15)
    STEP(I, b, c, d, a, x[5], T52, 21)
    STEP(I, a, b, c, d, x[12], T53, 6)
    STEP(I, d, a, b, c, x[3], T54, 10)
    STEP(I, c, d, a, b, x[10], T55, 15)
    STEP(I, b, c, d, a, x[1], T56, 21)
    STEP(I, a, b, c, d, x[8], T57, 6)
    STEP(I, d, a, b, c, x[15], T58, 10)
    STEP(I, c, d, a, b, x[6], T59, 15)
    STEP(I, b, c, d, a, x[13], T60, 21)
    STEP(I, a, b, c, d, x[4], T61, 6)
    STEP(I, d, a, b, c, x[11], T62, 10)
    STEP(I, c, d, a, b, x[2], T63, 15)
    STEP(I, b, c, d, a, x[9], T64, 21)

    pms->abcd[0] += a;
    pms->abcd[1] += b;
    pms->abcd[2] += c;
    pms->abcd[3] += d;
}

void md5_init(md5_state_t *pms)
{
    pms->count[0] = pms->count[1] = 0;
    pms->abcd[0] = 0x67452301;
    pms->abcd[1] = 0xefcdab89;
    pms->abcd[2] = 0x98badcfe;
    pms->abcd[3] = 0x10325476;
}

void md5_append(md5_state_t *pms, const md5_byte_t *data, size_t nbytes)
{
    const md5_byte_t *p = data;
    size_t left = nbytes;
    size_t offset = (pms->count[0] >> 3) & 63;

    if ((pms->count[0] += (md5_word_t)(nbytes << 3)) < (nbytes << 3))
        pms->count[1]++;
    pms->count[1] += (md5_word_t)(nbytes >> 29);

    if (offset) {
        size_t copy = (offset + nbytes > 64 ? 64 - offset : nbytes);
        memcpy(pms->buf + offset, p, copy);
        if (offset + copy < 64)
            return;
        p += copy;
        left -= copy;
        md5_process(pms, pms->buf);
    }

    for (; left >= 64; p += 64, left -= 64)
        md5_process(pms, p);

    if (left)
        memcpy(pms->buf, p, left);
}

void md5_finish(md5_state_t *pms, md5_byte_t digest[16])
{
    static const md5_byte_t pad[64] = { 0x80 };
    md5_byte_t data[8];
    int i;

    for (i = 0; i < 8; ++i)
        data[i] = (md5_byte_t)(pms->count[i >> 2] >> ((i & 3) * 8));

    md5_append(pms, pad, ((55 - ((pms->count[0] >> 3) & 63)) & 63) + 1);
    md5_append(pms, data, 8);

    for (i = 0; i < 16; ++i)
        digest[i] = (md5_byte_t)(pms->abcd[i >> 2] >> ((i & 3) * 8));
}

