/*
 * File:
 *    sspLibTest.c
 *
 * Description:
 *    Test ssp library
 *
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "sspLib.h"

int main()
{
  int stat;
  int iFlag=0;
  int i=0;
	
  vmeSetQuietFlag(1);

  stat = vmeOpenDefaultWindows();

  if(stat!=OK)
    goto CLOSE;

  iFlag = 0xFFFF0000 | SSP_INIT_SKIP_FIRMWARE_CHECK;

  vmeBusLock();

  sspInit(3<<19,1<<19,18,iFlag);
  sspCheckAddresses(0);

  sspMpdFiberReset(0);
  sspMpdFiberLinkReset(0,0xffffffff);

  sspMpdDisable(sspSlot(0), 0xffffffff);
  sspMpdEnable(sspSlot(0), 0x1);

  sspSoftReset(0);

  sspGStatus(0);

  
  taskDelay(2);
  for(i=0; i<1; i++)
    {
      if(sspMpdReadRegs(0,0)==ERROR)
	{
	  sspSoftReset(0);
	}
    }

/*   sspGStatus(0); */
/*   sspMpdPrintStatus(0); */

  vmeBusUnlock();

 CLOSE:
  vmeOpenDefaultWindows();
	
  return 0;
}
