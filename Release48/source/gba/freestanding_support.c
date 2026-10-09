#include <stddef.h>
#include <stdint.h>
void __aeabi_memclr(void* p, size_t n) { uint8_t* q=(uint8_t*)p; while(n--) *q++=0; }
void __aeabi_memclr4(void* p, size_t n) { __aeabi_memclr(p,n); }
void __aeabi_memclr8(void* p, size_t n) { __aeabi_memclr(p,n); }
void __aeabi_memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d=(uint8_t*)dst; const uint8_t* s=(const uint8_t*)src;
    while(n--) *d++=*s++;
}
void __aeabi_memcpy4(void* dst, const void* src, size_t n) { __aeabi_memcpy(dst,src,n); }
