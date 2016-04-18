#include <math.h>
#include <string.h>

int base64_encode(char *encoding, unsigned char *str)
{
    int i;
    int len = strlen((char *)str);
    int num_bytes = 4*ceil((double)len/3);
    char alph[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+_";
 
    int rem = (3*(len/3+1)-len)%3;
    for (i = 0; i < num_bytes/4; i++)
    {
        encoding[i*4]=alph[str[i*3]>>2];
        encoding[i*4+1]=alph[((str[i*3]&3)<<4)|(str[i*3+1]>>4)];
        encoding[i*4+2]=(rem==2&&num_bytes/4-1==i)?'=':alph[((str[i*3+1]&15)<<2)|(str[i*3+2]>>6)];
        encoding[i*4+3]=(rem&&num_bytes/4-1==i)?'=':alph[str[i*3+2]&63];
    }
    encoding[num_bytes] = '\0';
    
    return num_bytes;
}
