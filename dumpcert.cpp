#include <stdio.h>

#include "tqsl.h"
#include "sign.h"
extern int debugLevel;
int main(int argc,char *argv[])
{
  if (argc != 2)
    return(-1);

  TqslCert 	cert;

  readCert(argv[1],&cert);
  dumpCert(&cert,stdout);
  return(0);

}