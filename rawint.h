
#ifndef __SWFHAX_RAWINT_H
#define __SWFHAX_RAWINT_H

#ifndef _BSD_SOURCE
#pragma warning You need to define _BSD_SOURCE
#define _BSD_SOURCE
#endif

#include <stdint.h>
#include <endian.h>

/**
* Reads  a little endian 8 bit value in the buffer, to an unsigned 8 bit int
*
* @param p the buffer
* @return the int
**/
static inline uint8_t __le_u8(const void *p) {
    return *((uint8_t*)(p));
}

/**
* Reads  a little endian 16 bit value in the buffer, to an unsigned 16 bit int
*
* @param p the buffer
* @return the int
**/
static inline uint16_t __le_u16(const void *p) {
    return *((uint16_t*)(p));
}

/**
* Reads  a little endian 8 bit value in the buffer, to a signed 8 bit int
*
* @param p the buffer
* @return the  int
**/ 
static inline int8_t __le_s8(const void *p) {
    return *((int8_t*)(p));
}

/**
* Reads  a little endian 8 bit value in the buffer, to a signed 16 bit int
*
* @param p the buffer
* @return the int
**/
static inline int16_t __le_s16(const void *p) {
    return *((int16_t*)(p));
}

/**
* Write a little endian 16 bit unsigned int into the buffer
*
* @param p the buffer
* @data 24 bit data to write
**/
static inline void __w_le_u16(void *p,const uint16_t val) {
    *((uint16_t*)(p)) = val;
}

/**
* Write a big endian 16 bit unsigned int into the buffer
*
* @param p the buffer
* @val 24 bit data to write
**/
static inline void __w_be_u16(void *p,const uint16_t val) {
    ((uint8_t*)(p))[0] = val >> 8UL;
    ((uint8_t*)(p))[1] = val;
}

/**
* Reads  a little endian 32 bit value in the buffer to a unsigned 24 bit int
*
* @param p the buffer
* @return the unsigned int
**/ 
static inline uint32_t __le_u32(const void *p) {
    return *((uint32_t*)(p));
}

/**
* Reads  a little endian 32 bit value in the buffer to a unsigned 32 bit int
*
* @param p the buffer
* @return the unsigned int
**/ 
static inline int32_t __le_s32(const void *p) {
    return *((int32_t*)(p));
}

/**
* Write a little  endian 32 bit unsigned int into the buffer
*
* @param p the buffer
* @val 24 bit data to write
**/ 
static inline void __w_le_u32(void *p,const uint32_t val) {
    *((uint32_t*)(p)) = val;
}

/**
* Write a big  endian 32 bit unsigned int into the buffer
*
* @param p the buffer
* @val 24 bit data to write
**/ 
static inline void __w_be_u32(void *p,const uint32_t val) {
    ((uint8_t*)(p))[0] = val >> 24UL;
    ((uint8_t*)(p))[1] = val >> 16UL;
    ((uint8_t*)(p))[2] = val >> 8UL;
    ((uint8_t*)(p))[3] = val;
}

/**
* Write a little  endian 64 bit unsigned int into the buffer
*
* @param p the buffer
* @data 24 bit data to write
**/ 
static inline uint64_t __le_u64(const void *p) {
    return *((const uint64_t*)(p));
}

/**
* Reads  a little endian 64 bit value in the buffer to a signed bit int
*
* @param p the buffer
* @return the unsigned int
**/ 
static inline int64_t __le_s64(const void *p) {
    return *((const int64_t*)(p));
}

/**
* Write a little  endian 64 bit unsigned int into the buffer
*
* @param p the buffer
* @val 64 bit data to write
**/ 
static inline void __w_le_u64(void *p,const uint64_t val) {
    *((uint64_t*)(p)) = val;
}

/**
* Reads  a big endian 16 bit value in the buffer to a unsigned 16 bit int
*
* @param p the buffer
* @return the unsigned int
**/ 
static inline uint16_t __be_u16(const void *p) {
    const unsigned char *c = (const unsigned char *)p;

    return
        (((uint16_t)c[0]) <<  8U) |
        (((uint16_t)c[1]) <<  0U);
}

/**
* Reads  a big endian 16 bit value in the buffer to a signed 16 bit int
*
* @param p the buffer
* @return the unsigned int
**/ 
static inline int16_t __be_s16(const void *p) {
    return (int16_t)__be_u16(p);
}

/**
* Reads  a big endian 32 bit value in the buffer to a unsigned 32 bit int
*
* @param p the buffer
* @return the unsigned int
**/ 
static inline uint32_t __be_u32(const void *p) {
    const unsigned char *c = (const unsigned char *)p;

    return
        (((uint32_t)c[0]) << 24U) |
        (((uint32_t)c[1]) << 16U) |
        (((uint32_t)c[2]) <<  8U) |
        (((uint32_t)c[3]) <<  0U);
}

/**
* Reads  a big endian 32 bit value in the buffer to a signed 32 bit int
*
* @param p the buffer
* @return the unsigned int
**/
static inline int32_t __be_s32(const void *p) {
    return (int32_t)__be_u32(p);
}

/**
* Reads  a big endian 64 bit value in the buffer to an unsigned 64 bit int
*
* @param p the buffer
* @return the unsigned int
**/
static inline uint64_t __be_u64(const void *p) {
    const unsigned char *c = (const unsigned char *)p;

    return
        (((uint64_t)c[0]) << 56ULL) |
        (((uint64_t)c[1]) << 48ULL) |
        (((uint64_t)c[2]) << 40ULL) |
        (((uint64_t)c[3]) << 32ULL) |
        (((uint64_t)c[4]) << 24ULL) |
        (((uint64_t)c[5]) << 16ULL) |
        (((uint64_t)c[6]) <<  8ULL) |
        (((uint64_t)c[7]) <<  0ULL);
}

/**
* Reads  a big endian 64 bit value in the buffer to a signed 64 bit int
*
* @param p the buffer
* @return the unsigned int
**/
static inline int64_t __be_s64(const void *p) {
    return (int64_t)__be_u64(p);
}

/**
* Write a little endian 64 bit unsigned int into the buffer
*
* @param p the buffer
* @val 64 bit data to write
**/
static inline void __w_be_u64(void *p,const uint64_t val) {
    ((uint8_t*)(p))[0] = val >> 56ULL;
    ((uint8_t*)(p))[1] = val >> 48ULL;
    ((uint8_t*)(p))[2] = val >> 40ULL;
    ((uint8_t*)(p))[3] = val >> 32ULL;
    ((uint8_t*)(p))[4] = val >> 24UL;
    ((uint8_t*)(p))[5] = val >> 16UL;
    ((uint8_t*)(p))[6] = val >> 8UL;
    ((uint8_t*)(p))[7] = val;
}

/**
* Reads  a little endian value in the buffer to a signed 32 bit int
*
* @param p the buffer
* @return the unsigned int
**/
static inline uint16_t __be_to_he_16(uint16_t val) {
    return be16toh(val);
}

/**
* alias for htobe16 (host endian to big endian 16)
*
* @param p the buffer
* @return the unsigned int
**/
static inline uint16_t __he_to_be_16(uint16_t val) {
    return htobe16(val);
}

/**
* alias for be32toh (big  endian to host endian 32)
*
* @param p the buffer
* @return the unsigned int
**/
static inline uint32_t __be_to_he_32(uint32_t val) {
    return be32toh(val);
}

/**
* alias for htobe32 (host endian to big endian 32)
*
* @param p the buffer
* @return the unsigned int
**/
static inline uint32_t __he_to_be_32(uint32_t val) {
    return htobe32(val);
}

#endif /* __SWFHAX_RAWINT_H */
/* vim: set tabstop=4 */
/* vim: set softtabstop=4 */
/* vim: set shiftwidth=4 */
/* vim: set expandtab */
