#ifndef _AES_H_
#define _AES_H_

//#include <stdint.h>
#include <QtGlobal>


// #define the macros below to 1/0 to enable/disable the mode of operation.
//
// CBC enables AES128 encryption in CBC-mode of operation and handles 0-padding.
// ECB enables the basic ECB 16-byte block algorithm. Both can be enabled simultaneously.

// The #ifndef-guard allows it to be configured before #include'ing or at compile time.
#ifndef CBC
  #define CBC 1
#endif

#ifndef ECB
  #define ECB 1
#endif



#if defined(ECB) && ECB

void AES128_ECB_encrypt(quint8* input, const quint8* key, quint8 *output);
void AES128_ECB_decrypt(quint8* input, const quint8* key, quint8 *output);

#endif // #if defined(ECB) && ECB


#if defined(CBC) && CBC

void AES128_CBC_encrypt_buffer(quint8* output, quint8* input, quint32 length, const quint8* key, const quint8* iv);
void AES128_CBC_decrypt_buffer(quint8* output, quint8* input, quint32 length, const quint8* key, const quint8* iv);

#endif // #if defined(CBC) && CBC



#endif //_AES_H_
