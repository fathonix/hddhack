//Ripped from various pieces on the Intarweb

#include <stdint.h>
int memcmp(const void *cs_in, const void *ct_in, int n)
{
  int i;  
  const unsigned char * cs = (const unsigned char*) cs_in;
  const unsigned char * ct = (const unsigned char*) ct_in;

  for (i = 0; i < n; i++, cs++, ct++)
  {
    if (*cs < *ct)
    {
      return -1;
    }
    else if (*cs > *ct)
    {
      return 1;
    }
  }
  return 0;
}


int memcmp_32b(const void *cs_in, const void *ct_in, int n)
{
  int i;  
  const unsigned int * cs = (const unsigned int*) cs_in;
  const unsigned int * ct = (const unsigned int*) ct_in;

  for (i = 0; i < (n/4); i++, cs++, ct++)
  {
    if (*cs < *ct)
    {
      return -1;
    }
    else if (*cs > *ct)
    {
      return 1;
    }
  }
  return 0;
}


void* memcpy(void* dest, const void* src, int count) {
        char* dst8 = (char*)dest;
        char* src8 = (char*)src;

        while (count--) {
            *dst8++ = *src8++;
        }
        return dest;
    }
