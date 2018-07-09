/*****************************************************************
 * 
 * tsprimary_list.c - "Primary" Readout list routines for tsprimary
 *
 * Usage:
 *
 *    #include "tsprimary_list.c"
 *
 *  then define the following routines:
 * 
 *    void rocDownload(); 
 *    void rocPrestart(); 
 *    void rocGo(); 
 *    void rocEnd(); 
 *    void rocTrigger();
 *
 * SVN: $Rev$
 *
 */


#define ROL_NAME__ "TSPRIMARY"
#ifndef MAX_EVENT_LENGTH
/* Check if an older definition is used */
#ifdef MAX_SIZE_EVENTS
#define MAX_EVENT_LENGTH MAX_SIZE_EVENTS
#else
#define MAX_EVENT_LENGTH 10240
#endif /* MAX_SIZE_EVENTS */
#endif /* MAX_EVENT_LENGTH */
#ifndef MAX_EVENT_POOL
/* Check if an older definition is used */
#ifdef MAX_NUM_EVENTS
#define MAX_EVENT_POOL MAX_NUM_EVENTS
#else
#define MAX_EVENT_POOL   400
#endif /* MAX_NUM_EVENTS */
#endif /* MAX_EVENT_POOL */
/* POLLING_MODE */
#define POLLING___
#define POLLING_MODE
/* INIT_NAME should be defined by roc_### (maybe at compilation time - check Makefile-rol) */
#ifndef INIT_NAME
#warn "INIT_NAME undefined. Setting to tsprimary_list__init"
#define INIT_NAME tsprimary_list__init
#endif
#include <stdio.h>
#include <rol.h>
#include "jvme.h"
#include <TSPRIMARY_source.h>
#include "tsLib.h"
extern int bigendian_out;

extern int tsDoAck;
extern int tsNeedAck;
/*! Buffer node pointer */
extern DMANODE *the_event;
/*! Data pointer */
extern unsigned int *dma_dabufp;


#define ISR_INTLOCK INTLOCK
#define ISR_INTUNLOCK INTUNLOCK

pthread_mutex_t ack_mutex=PTHREAD_MUTEX_INITIALIZER;
#define ACKLOCK {				\
    if(pthread_mutex_lock(&ack_mutex)<0)	\
      perror("pthread_mutex_lock");		\
}
#define ACKUNLOCK {				\
    if(pthread_mutex_unlock(&ack_mutex)<0)	\
      perror("pthread_mutex_unlock");		\
}
pthread_cond_t ack_cv = PTHREAD_COND_INITIALIZER;
#define ACKWAIT {					\
    if(pthread_cond_wait(&ack_cv, &ack_mutex)<0)	\
      perror("pthread_cond_wait");			\
}
#define ACKSIGNAL {					\
    if(pthread_cond_signal(&ack_cv)<0)			\
      perror("pthread_cond_signal");			\
  }
int ack_runend=0;

/* ROC Function prototypes defined by the user */
void rocDownload();
void rocPrestart();
void rocGo();
void rocEnd();
void rocTrigger();
void rocCleanup();
int  getOutQueueCount();
int  getInQueueCount();
void doAck();

/* Asynchronous (to tsprimary rol) trigger routine, connects to rocTrigger */
void asyncTrigger();

/* Input and Output Partitions for VME Readout */
DMA_MEM_ID vmeIN, vmeOUT;


static void __download()
{
  int status;

  daLogMsg("INFO","Readout list compiled %s", DAYTIME);
#ifdef POLLING___
  rol->poll = 1;
#endif
  *(rol->async_roc) = 0; /* Normal ROC */

  bigendian_out=1;

  pthread_mutex_init(&ack_mutex, NULL);
  pthread_cond_init(&ack_cv,NULL);
 
  /* Open the default VME windows */
  vmeOpenDefaultWindows();

  /* Initialize memory partition library */
  dmaPartInit();

  /* Setup Buffer memory to store events */
  dmaPFreeAll();
  vmeIN  = dmaPCreate("vmeIN",MAX_EVENT_LENGTH,MAX_EVENT_POOL,0);
  vmeOUT = dmaPCreate("vmeOUT",0,0,0);

  /* Reinitialize the Buffer memory */
  dmaPReInitAll();
  dmaPStatsAll();

  /* Initialize VME Interrupt interface - use defaults */
  tsInit(TS_ADDR,TS_READOUT,0);

  /* Execute User defined download */
  rocDownload();

  daLogMsg("INFO","Download Executed");

  /* FIXME: Not sure if these belong at end of download, or at the beginning of prestart */
  tsClockReset();
  taskDelay(2);
  tsTrigLinkReset();
  taskDelay(2);

} /*end download */     

static void __prestart()
{
  ACKLOCK;
  ack_runend=0;
  ACKUNLOCK;
  CTRIGINIT;
  *(rol->nevents) = 0;
  unsigned long jj, adc_id;
  daLogMsg("INFO","Entering Prestart");


  TSPRIMARY_INIT;
  CTRIGRSS(TSPRIMARY,1,usrtrig,usrtrig_done);
  CRTTYPE(1,TSPRIMARY,1);


  /* Execute User defined prestart */
  rocPrestart();

  /* Send a Sync Reset 
     - required by FADC250 after it is enabled */

  printf("%s: Sending sync\n",__FUNCTION__);
  taskDelay(2);
  tsSyncReset(1);
  taskDelay(2);

  /* Connect User Trigger Routine */
  tsIntConnect(TS_INT_VEC,asyncTrigger,0);

  daLogMsg("INFO","Prestart Executed");

  if (__the_event__) WRITE_EVENT_;
  *(rol->nevents) = 0;
  rol->recNb = 0;
} /*end prestart */     

static void __end()
{
  int ii, ievt, rem_count;
  int len, type, lock_key;
  DMANODE *outEvent;
  int oldnumber;
  int iwait=0;
  int blocksLeft=0;
  unsigned int blockstatus=0;
  int bready=0;

  tsDisableTriggerSource(1);
  
  printf("%s: Starting purge of TS blocks\n",__FUNCTION__);
  while(iwait<10000)
    {
      iwait++;
      if (getOutQueueCount()>0) 
	{
	  printf("Purging an event in vmeOUT (iwait = %d, count = %d)\n",iwait,
		 getOutQueueCount());
	  /* This wont work without a secondary readout list (will crash EB or hang the ROC) */
	  __poll();
	}
    }

  printf("%s: DONE with purge of TS blocks\n",__FUNCTION__);

  tsStatus(0);
  dmaPStatsAll();
  tsIntDisable();
  tsIntDisconnect();

  /* Execute User defined end */
  rocEnd();

  CDODISABLE(TSPRIMARY,1,0);
 
  dmaPStatsAll();
      
  daLogMsg("INFO","End Executed");

  if (__the_event__) WRITE_EVENT_;
} /* end end block */

static void __pause()
{
  CDODISABLE(TSPRIMARY,1,0);
  daLogMsg("INFO","Pause Executed");
  
  if (__the_event__) WRITE_EVENT_;
} /*end pause */

static void __go()
{
  daLogMsg("INFO","Entering Go");
  ACKLOCK;
  ack_runend=0;
  ACKUNLOCK;

  CDOENABLE(TSPRIMARY,1,1);
  rocGo();
  
  tsIntEnable(1); 
  
  if (__the_event__) WRITE_EVENT_;
}

void usrtrig(unsigned long EVTYPE,unsigned long EVSOURCE)
{
  long EVENT_LENGTH;
  int ii, len, data, type=0, lock_key;
  int syncFlag=0, lateFail=0;
  unsigned int event_number=0;
  unsigned int currentWord=0;
  unsigned int shmEvent=0, shmData[MAX_EVENT_LENGTH>>2];
  DMANODE *outEvent;
  static int prev_len=0;

/*   ISR_INTLOCK; */
  outEvent = dmaPGetItem(vmeOUT);
  if(outEvent != NULL) 
    {
      len = outEvent->length;
      type = outEvent->type;
      event_number = outEvent->nevent;
/*       CEOPEN(type, BT_UI4); */
      CEOPEN(type, BT_BANK);

      shmEvent=0;
      if((event_number % 1000) == 0)
	shmEvent=1;


      if(rol->dabufp != NULL) 
	{
/* #define PRINTOUT */
#ifdef PRINTOUT
	  printf("Received %d triggers... \n",
		 event_number);
#endif
	  for(ii=0;ii<len;ii++) 
	    {
	      currentWord = LSWAP(outEvent->data[ii]);
	      *rol->dabufp++ = currentWord;
#ifdef PRINTOUT
	      if((ii%5)==0) printf("\n\t");
	      printf("  0x%08x ",(unsigned int)LSWAP(outEvent->data[ii]));
#endif
	      if(shmEvent)
		{
		  shmData[ii] = currentWord;
		}
	    }
	  if(shmEvent)
	    {
	    }
#ifdef PRINTOUT
	  printf("\n\n");
#endif
	  prev_len=len;
	}
      else 
	{
	  printf("tsprimary_list: ERROR rol->dabufp is NULL -- Event lost\n");
	}

      /* Execute the doAck routine after freeing up the buffer.
	 This allows for an Acknowledge to be sent back to the TS,
	 if it is needed */
/*       outEvent->part->free_cmd = *doAck; */

      CECLOSE;

      ACKLOCK;

      dmaPFreeItem(outEvent); /* doAck performed in here */

      if(tsNeedAck>0)
	{
	  tsNeedAck=0;
	  ACKSIGNAL;
	}
      ACKUNLOCK;
    }
  else
    {
      logMsg("Error: no Event in vmeOUT queue\n",0,0,0,0,0,0);
    }
  
 
} /*end trigger */

/*
 * doAck() 
 *   Routine to send a signal to the asyncTrigger thread, indicating that a
 *    buffer has been freed.
 *
 */

void doAck()
{
  ACKLOCK;
  if(tsNeedAck>0)
    {
      tsNeedAck=0;
      ACKSIGNAL;
    }
  ACKUNLOCK;

}

void asyncTrigger()
{
  int intCount=0;
  int length,size;
  unsigned int tirval;
  int clocksource=-1;

  intCount = tsGetIntCount();

  /* grap a buffer from the queue */
  GETEVENT(vmeIN,intCount);
  if(the_event->length!=0) 
    {
      printf("Interrupt Count = %d\t",intCount);
      printf("the_event->length = %d\n",the_event->length);
    }

  the_event->type = 1;
  
  /* Execute user defined Trigger Routine */
  rocTrigger();

  /* Put this event's buffer into the OUT queue. */
  ACKLOCK;
  PUTEVENT(vmeOUT);
  /* Check if the event length is larger than expected */
  length = (((int)(dma_dabufp) - (int)(&the_event->length))) - 4;
  size = the_event->part->size - sizeof(DMANODE);

  if(length>size) 
    {
      printf("rocLib: ERROR: Event length > Buffer size (%d > %d).  Event %d\n",
	     length,size,the_event->nevent);
    }
  if(dmaPEmpty(vmeIN))
    {

      printf("WARN: vmeIN out of event buffers (intCount = %d).\n",intCount);

      if(ack_runend==0 || tsBReady()>0)
	{
	  /* Set the NeedAck for Ack after a buffer is freed */
	  tsNeedAck = 1;
	  
	  /* Wait for the signal indicating that a buffer has been freed */
	  ACKWAIT;
	}
      else
	{
	  printf(" WHO CARES... ack_runend==1  tsBready = %d\n",tsBReady());
	}

    }

  ACKUNLOCK;


}

void usrtrig_done()
{
} /*end done */

void __done()
{
  poolEmpty = 0; /* global Done, Buffers have been freed */
} /*end done */

static void __status()
{
} /* end status */

int
getOutQueueCount()
{
  if(vmeOUT) 
    return(vmeOUT->list.c);
  else
    return(0);
}

int
getInQueueCount()
{
  if(vmeIN) 
    return(vmeIN->list.c);
  else
    return(0);
}

/* This routine is automatically executed just before the shared libary
   is unloaded.

   Clean up memory that was allocated 
*/
__attribute__((destructor)) void end (void)
{
  static int ended=0;

  if(ended==0)
    {
      printf("ROC Cleanup\n");
      
      rocCleanup();
      
      dmaPFreeAll();
      vmeCloseDefaultWindows();

      ended=1;
    }

}
