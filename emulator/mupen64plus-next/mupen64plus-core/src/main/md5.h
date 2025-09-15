/* md5.h - RFC1321 style MD5 implementation
 * Public domain
 */

#ifndef MD5_H
#define MD5_H

#include <stddef.h> /* for size_t */

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char md5_byte_t;   /* 8-bit */
typedef unsigned int  md5_word_t;   /* 32-bit */

/* MD5 state structure */
typedef struct md5_state_s {
    md5_word_t count[2];  /* message length in bits, lsw first */
    md5_word_t abcd[4];   /* digest buffer */
    md5_byte_t buf[64];   /* data block being processed */
} md5_state_t;

/* API */
void md5_init(md5_state_t *pms);
void md5_append(md5_state_t *pms, const md5_byte_t *data, size_t nbytes);
void md5_finish(md5_state_t *pms, md5_byte_t digest[16]);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* MD5_H */

