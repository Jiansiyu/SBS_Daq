/*
 * File:
 *    sspReadoutTest.c
 *
 * Description:
 *    Test Readout of ssp
 *
 *
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "jvme.h"
#include "tiLib.h"
#include "sdLib.h"
#include "fadcLib.h"
#include "sspLib.h"

extern unsigned long tiA32Base;

int main()
{
  int stat=0, iFlag=0, dCnt=0, idata=0;
  DMA_MEM_ID vmeIN,vmeOUT;
  extern DMANODE *the_event;
  extern unsigned int *dma_dabufp;
  DMANODE *outEvent;

  tiA32Base = 0x08000000;	

  vmeSetQuietFlag(1);

  stat = vmeOpenDefaultWindows();

  if(stat!=OK)
    goto CLOSE;

  dmaPFreeAll();
  vmeIN  = dmaPCreate("vmeIN",10240,1,0);
  vmeOUT = dmaPCreate("vmeOUT",0,0,0);
    
  dmaPStatsAll();

  dmaPReInitAll();


  iFlag = SSP_INIT_SKIP_FIRMWARE_CHECK | 0xFFFF0000;

  sspInit(3<<19,1<<19,18,iFlag);
  extern int nSSP;
  int issp=0;
  sspMpdFiberReset(0);
  sspMpdFiberLinkReset(0,0xffffffff);

  for(issp=0; issp<nSSP; issp++)
    {
      sspCheckAddresses(sspSlot(issp));
      sspMpdDisable(sspSlot(issp), 0xffffffff);
      sspMpdEnable(sspSlot(issp), 0x1);
      
      sspEnableBusError(sspSlot(issp));
      sspSetBlockLevel(sspSlot(issp),1);
    }
  sspSoftReset(0);
  
  sspGStatus(0);
  sspMpdPrintStatus(0);

/*   goto CLOSE; */

  tiInit(0,0,TI_INIT_SKIP_FIRMWARE_CHECK);
  sdInit(0);

  sdSetActiveVmeSlots(sspSlotMask());
/*   sleep(1); */
  int timeout=0;
  while((sspBReady(0)!=1) && (timeout<100))
    {
      timeout++;
    }

  if(timeout>=100)
    {
      printf("TIMEOUT!\n");
      goto CLOSE;
    }

  if(1)
    {
      GETEVENT(vmeIN,1);

      vmeDmaConfig(2,5,1);
      dCnt = sspReadBlock(0, dma_dabufp, 1024>>2,1);
      if(dCnt<=0)
	{
	  printf("No data or error.  dCnt = %d\n",dCnt);
	}
      else
	{
	  dma_dabufp += dCnt;
	}

      PUTEVENT(vmeOUT);

      outEvent = dmaPGetItem(vmeOUT);
  
      dCnt = outEvent->length;
  
      printf("  dCnt = %d\n",dCnt);
      for(idata=0;idata<dCnt;idata++)
	{
	  faDataDecode(LSWAP(outEvent->data[idata]));
/* 	  if((idata%5)==0) printf("\n\t"); */
/* 	  printf("  0x%08x ",(unsigned int)LSWAP(outEvent->data[idata])); */
	}
      printf("\n\n");

      dmaPFreeItem(outEvent);
    }
  else
    {
      printf(" No data ready\n");
    }

 CLOSE:
  dmaPFreeAll();

  vmeOpenDefaultWindows();
	
  return 0;
}

struct fadc_data_struct fadc_data;
void 
faDataDecode(unsigned int data)
{
  int i_print = 1;
  static unsigned int type_last = 15;	/* initialize to type FILLER WORD */
  static unsigned int time_last = 0;
  int idata=0;

  if( data & 0x80000000 )		/* data type defining word */
    {
      fadc_data.new_type = 1;
      fadc_data.type = (data & 0x78000000) >> 27;
    }
  else
    {
      fadc_data.new_type = 0;
      fadc_data.type = type_last;
    }
        
  switch( fadc_data.type )
    {
    case 0:		/* BLOCK HEADER */
      if( fadc_data.new_type )
	{
	  fadc_data.slot_id_hd = ((data) & 0x7C00000) >> 22;
	  fadc_data.modID      = (data & 0x3C0000)>>18;
	  fadc_data.blk_num    = (data & 0x3FF00) >> 8;
	  fadc_data.n_evts     = (data & 0xFF);
	  if( i_print ) 
	    printf("%8X - BLOCK HEADER - slot = %d  modID = %d   n_evts = %d   n_blk = %d\n",
		   data, fadc_data.slot_id_hd, 
		   fadc_data.modID, fadc_data.n_evts, fadc_data.blk_num);
	}
      else
	{
	  fadc_data.PL  = (data & 0x1FFC0000) >> 18;
	  fadc_data.NSB = (data & 0x0003FE00) >> 9;
	  fadc_data.NSA = (data & 0x000001FF) >> 0;

	  printf("%8X - BLOCK HEADER 2 - PL = %d  NSB = %d  NSA = %d\n",
		 data, 
		 fadc_data.PL,
		 fadc_data.NSB,
		 fadc_data.NSA);
	}
      break;

    case 1:		/* BLOCK TRAILER */
      fadc_data.slot_id_tr = (data & 0x7C00000) >> 22;
      fadc_data.n_words = (data & 0x3FFFFF);
      if( i_print ) 
	printf("%8X - BLOCK TRAILER - slot = %d   n_words = %d\n",
	       data, fadc_data.slot_id_tr, fadc_data.n_words);
      break;

    case 2:		/* EVENT HEADER */
      fadc_data.slot_id_evh = (data & 0x7C00000) >> 22;
      fadc_data.evt_num_1 = (data & 0x3FFFFF);
      if( i_print ) 
	printf("%8X - EVENT HEADER 1 - slot = %d   evt_num = %d\n", data, 
	       fadc_data.slot_id_evh, fadc_data.evt_num_1);
      break;

    case 3:		/* TRIGGER TIME */
      if( fadc_data.new_type )
	{
	  fadc_data.time_1 = (data & 0x7FFFFFF);
	  if( i_print ) 
	    printf("%8X - TRIGGER TIME 1 - time = %08x\n", data, fadc_data.time_1);
	  fadc_data.time_now = 1;
	  time_last = 1;
	}    
      else
	{
	  if( time_last == 1 )
	    {
	      fadc_data.time_2 = (data & 0xFFFFFF);
	      if( i_print ) 
		printf("%8X - TRIGGER TIME 2 - time = %08x\n", data, fadc_data.time_2);
	      fadc_data.time_now = 2;
	    }    
	  else if( time_last == 2 )
	    {
	      fadc_data.time_3 = (data & 0xFFFFFF);
	      if( i_print ) 
		printf("%8X - TRIGGER TIME 3 - time = %08x\n", data, fadc_data.time_3);
	      fadc_data.time_now = 3;
	    }    
	  else if( time_last == 3 )
	    {
	      fadc_data.time_4 = (data & 0xFFFFFF);
	      if( i_print ) 
		printf("%8X - TRIGGER TIME 4 - time = %08x\n", data, fadc_data.time_4);
	      fadc_data.time_now = 4;
	    }    
	  else
	    if( i_print ) 
	      printf("%8X - TRIGGER TIME - (ERROR)\n", data);
	                
	  time_last = fadc_data.time_now;
	}    
      break;

    case 4:		/* WINDOW RAW DATA */
      if( fadc_data.new_type )
	{
	  fadc_data.chan = (data & 0x7800000) >> 23;
	  fadc_data.width = (data & 0xFFF);
	  if( i_print ) 
	    printf("%8X - WINDOW RAW DATA - chan = %d   nsamples = %d\n", 
		   data, fadc_data.chan, fadc_data.width);
	}    
      else
	{
	  fadc_data.valid_1 = 1;
	  fadc_data.valid_2 = 1;
	  fadc_data.adc_1 = (data & 0x1FFF0000) >> 16;
	  if( data & 0x20000000 )
	    fadc_data.valid_1 = 0;
	  fadc_data.adc_2 = (data & 0x1FFF);
	  if( data & 0x2000 )
	    fadc_data.valid_2 = 0;
	  if( i_print ) 
	    printf("%8X - RAW SAMPLES - valid = %d  adc = %4d   valid = %d  adc = %4d\n", 
		   data, fadc_data.valid_1, fadc_data.adc_1, 
		   fadc_data.valid_2, fadc_data.adc_2);
	}    
      break;
 
    case 5:		/* UNDEFINED TYPE */
      if( i_print ) 
	printf("%8X - UNDEFINED TYPE = %d\n", data, fadc_data.type);
      break;

    case 6:		/* PULSE RAW DATA */
      if( fadc_data.new_type )
	{
	  fadc_data.chan = (data & 0x7800000) >> 23;
	  fadc_data.pulse_num = (data & 0x600000) >> 21;
	  fadc_data.thres_bin = (data & 0x3FF);
	  if( i_print ) 
	    printf("%8X - PULSE RAW DATA - chan = %d   pulse # = %d   threshold bin = %d\n", 
		   data, fadc_data.chan, fadc_data.pulse_num, fadc_data.thres_bin);
	}    
      else
	{
	  fadc_data.valid_1 = 1;
	  fadc_data.valid_2 = 1;
	  fadc_data.adc_1 = (data & 0x1FFF0000) >> 16;
	  if( data & 0x20000000 )
	    fadc_data.valid_1 = 0;
	  fadc_data.adc_2 = (data & 0x1FFF);
	  if( data & 0x2000 )
	    fadc_data.valid_2 = 0;
	  if( i_print ) 
	    printf("%8X - PULSE RAW SAMPLES - valid = %d  adc = %d   valid = %d  adc = %d\n", 
		   data, fadc_data.valid_1, fadc_data.adc_1, 
		   fadc_data.valid_2, fadc_data.adc_2);
	}    
      break;

    case 7:		/* PULSE INTEGRAL */
      fadc_data.chan = (data & 0x7800000) >> 23;
      fadc_data.pulse_num = (data & 0x600000) >> 21;
      fadc_data.quality = (data & 0x180000) >> 19;
      fadc_data.integral = (data & 0x7FFFF);
      if( i_print ) 
	printf("%8X - PULSE INTEGRAL - chan = %d   pulse # = %d   quality = %d   integral = %d\n", 
	       data, fadc_data.chan, fadc_data.pulse_num, 
	       fadc_data.quality, fadc_data.integral);
      break;
 
    case 8:		/* PULSE TIME */
      fadc_data.chan = (data & 0x7800000) >> 23;
      fadc_data.pulse_num = (data & 0x600000) >> 21;
      fadc_data.quality = (data & 0x180000) >> 19;
      fadc_data.time = (data & 0xFFFF);
      if( i_print ) 
	printf("%8X - PULSE TIME - chan = %d   pulse # = %d   quality = %d   time = %d\n", 
	       data, fadc_data.chan, fadc_data.pulse_num, 
	       fadc_data.quality, fadc_data.time);
      break;

    case 9:		/* UNDEFINED TYPE */
      if( i_print ) 
	printf("%8X - UNDEFINED TYPE = %d\n", data, fadc_data.type);
      break;

    case 10:		/* UNDEFINED TYPE */
      if( i_print ) 
	printf("%8X - UNDEFINED TYPE = %d\n", data, fadc_data.type);
      break;

    case 11:		/* UNDEFINED TYPE */
      if( i_print ) 
	printf("%8X - UNDEFINED TYPE = %d\n", data, fadc_data.type);
      break;

    case 12:		/* SCALER HEADER */
      if( fadc_data.new_type )
	{
	  fadc_data.scaler_data_words = (data & 0x3F);
	  if( i_print ) 
	    printf("%8X - SCALER HEADER - data words = %d\n", data, fadc_data.scaler_data_words);
	}
      else
	{
	  for(idata=0; idata<fadc_data.scaler_data_words; idata++)
	    {
	      if( i_print ) 
		printf("%8X - SCALER DATA - word = %2d  counter = %d\n", 
		       data, idata, data);
	    }
	}
      break;
 
    case 13:		/* END OF EVENT */
      if( i_print ) 
	printf("%8X - END OF EVENT = %d\n", data, fadc_data.type);
      break;

    case 14:		/* DATA NOT VALID (no data available) */
      if( i_print ) 
	printf("%8X - DATA NOT VALID = %d\n", data, fadc_data.type);
      break;

    case 15:		/* FILLER WORD */
      if( i_print ) 
	printf("%8X - FILLER WORD = %d\n", data, fadc_data.type);
      break;
    }
	
  type_last = fadc_data.type;	/* save type of current data word */
		   
}        
