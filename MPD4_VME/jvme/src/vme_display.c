/*
 * File:
 *    vme_display.c
 *
 * Description:
 *    Command line program to display registers from the vmeBUS.
 *    Much code here shamelessly borrowed from vxWorks
 *
 *    We'll probably release this as binary only.
 *
 * SVN: $Rev: 501 $
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "jvme.h"

#define MAX_BYTES_PER_LINE       16
#define EOS           '\0'    /* C string terminator */

unsigned int A16MemOffset = 0x0;

void
usage()
{
  printf("Usage: ");
  printf("vme_display [OPTION]... [VMEADDRESS]... [WIDTH]\n");
  printf("Access and display memory from VMEADDRESS with data WIDTH\n");
  printf("\n");
  printf("If WIDTH (in bytes) is not specified, 2 will be assumed\n"); 
  printf("Valid values of WIDTH are 2, 4, and 8 bytes.");
  printf("\n");
  printf("OPTIONs:\n");
  printf("  -a ADDRSPACE                Specify Address Space.  Valid values are\n");
  printf("                                a16, a24, and a32.  If Address space is\n");
  printf("                                not specified, it will be inferred from\n");
  printf("                                VMEADDRESS.\n");
  printf("  -n NUNITS                   Number of Units of WIDTH to read.  If not\n");
  printf("                                specified, 128 (0x80) will be used\n");
  printf("\n\n");
}

void vmeDisplay(volatile void *adrs, int nunits, int width, unsigned int offset);

int
main(int argc, char *argv[]) 
{
  int iarg=1;
  int stat;
  int nunits=0;
  unsigned int userAddr=0;
  unsigned int userWidth=0;
  unsigned int laddr;
  unsigned short amcode=0;
  unsigned int memOffset;
  char *addrString;

  /* Evaluate the command line arguments */
  while (iarg < argc) 
    {
      if (strcmp(argv[iarg],"-a") == 0)  /* Address space specified */
	{
	  if (iarg < argc-1) 
	    {
	      addrString = argv[++iarg];
	      if(strcasecmp(addrString,"a16") == 0)
		amcode = 0x29;
	      else if(strcasecmp(addrString,"a24") == 0)
		amcode = 0x39;
	      else if(strcasecmp(addrString,"a32") == 0)
		amcode = 0x09;
	      else if(strcasecmp(addrString,"crcsr") == 0)
		amcode = 0x2f;
	      else
		{
		  printf("  Invalid address space specified (%s)\n",addrString);
		  usage();
		  return -1;
		}
	      ++iarg;
	      continue;
	    }
	}
      if (strcmp(argv[iarg],"-n") == 0)  /* Number of words specified */
	{
	  if (iarg < argc-1) 
	    {
	      nunits = atoi(argv[++iarg]);
	      ++iarg;
	      continue;
	    }
	}
      /* otherwise, assume that arguments are <addr> <width> */
      if (iarg == argc-2) 
	{
	  userAddr = (unsigned int) strtoll(argv[iarg],NULL,16)&0xffffffff;
	  iarg++;
	  userWidth = atoi(argv[iarg]);
	  break;
	}
      else if (iarg == argc-1)
	{
	  userAddr = strtol(argv[iarg],NULL,16);
	  userAddr = (unsigned int) strtoll(argv[iarg],NULL,16)&0xffffffff;
	  userWidth = 2;
	  break;
	}
      else
	{
	  usage();
	  return -1;
	}
    }

  if(userAddr==0)
    {
      usage();
      return -1;
    }

  if((userWidth != 2) && (userWidth != 4) && (userWidth != 8) )
    {
      printf("  Invalid data width (%d)\n",userWidth);
      usage();
      return -1;
    }

  if(amcode==0)
    {
      /* Determine the address space from the userAddr */
      if(userAddr<0x10000)
	{
	  amcode = 0x29;
	}
      else if (userAddr<0x01000000)
	{
	  amcode = 0x39;
	}
      else
	{
	  amcode = 0x09;
	}
    }

  /* Initialize the VME bridge... quietly */
  vmeSetQuietFlag(1);
  stat = vmeOpenDefaultWindows();
  if(stat != OK)
    {
      printf("Failed to open default VME windows\n");
      goto CLOSE;
    }
  
  /* Determine the local address */
  stat = vmeBusToLocalAdrs(amcode,(char *)userAddr,(char **)&laddr);
  if (stat != 0) 
    {
      printf("ERROR: Error in vmeBusToLocalAdrs(0x%x,0x%x,&laddr) res=%d \n",
	     amcode,userAddr,stat);
      goto CLOSE;
    } 
  memOffset = laddr - userAddr;

  /* Display the requested memory */
  printf("\n");
  vmeDisplay((void *)laddr, nunits, userWidth, memOffset);

  /* Clear any Execptions, if they occurred */
  vmeClearException(0);

 CLOSE:
    
  printf("\n");
  vmeCloseDefaultWindows();

  return 0;
}

void vmeDisplay
(
 volatile void *adrs,	/* address to display (if 0, display next block */
 int nunits,		/* number of units to print (if 0, use default) */
 int width, 		/* width of displaying unit (1, 2, 4, 8) */
 unsigned int offset    /* Offset to remove from printf statement */ 
 )
{
  static int dNitems = 0x80;	/* default number of item to display */
  static int dWidth  = 2;	/* default width */
  static void *last_adrs = 0;	/* last location displayed */

  int item;		/* item counter displayed per line */
  char ascii [MAX_BYTES_PER_LINE + 1]; /* ascii buffer for displaying */
#ifdef SHOWASCII
  int ix;			/* temporary count */
  char *pByte;		/* byte pointer for filling ascii buffer */
#endif
  char *tmpByte;		/* temporary byte pointer */
  unsigned short *tmpShort;		/* temporary short word pointer */
  unsigned short printShort;
  unsigned long *tmpLong;		/* temporary long word pointer */
  unsigned long printLong;
  unsigned long printLongLong;

  ascii [MAX_BYTES_PER_LINE] = EOS;	/* put an EOS on the string */

  if (nunits == 0)
    nunits = dNitems;	/* no count specified: use default count */
  else
    dNitems = nunits;	/* change default count */

  if (width == 0)
    width = dWidth;
  else
    {			/* check for valid width */
      if (width != 1 && width != 2 && width != 4 && width != 8)
	width = 1;
      dWidth = width;
    }

  if (adrs == 0) 		/* no address specified: use last address */
    {
      adrs = last_adrs;
    }
  else
    {
      last_adrs = (void *)adrs;
    }
  /* round address down to appropriate boundary */

  last_adrs = (void *)((int) last_adrs & ~(width - 1));

  /* print leading spaces on first line */

  /*   bfill (ascii, 16, '.'); */
  memset (&ascii, '.', 16);

  printf ("%08x:  ", (int) (last_adrs - offset) & ~0xf);

  for (item = 0; item < ((int) last_adrs & 0xf) / width; item++)
    {
      printf ("%*s ", 2*width, " ");
      /*       bfill (&ascii[item * width], 2*width, ' '); */
      memset (&ascii[item * width], ' ', 2*width);
    }

  /* print out all the words */

  while (nunits-- > 0)
    {
      if (item == MAX_BYTES_PER_LINE/width)
	{
	  /* end of line: */
	   /*   print out ascii format values and address of next line */
#ifdef SHOWASCII
	  printf ("  *%16s*\n%08x:  ", ascii, (int) (last_adrs - offset));
#else
	  printf ("\n%08x:  ",(int) (last_adrs - offset));
#endif
	  /* 	  bfill (ascii, MAX_BYTES_PER_LINE, '.'); /\* clear out ascii buffer *\/ */
	  memset (ascii, '.', MAX_BYTES_PER_LINE); /* clear out ascii buffer */
	  item = 0;				/* reset word count */
	}

      switch (width)			/* display in appropriate format */
	{
	case 1:
	  tmpByte = (char *)last_adrs;
	  printf ("%02x", *tmpByte);
	  break;
	case 2:
	  tmpShort = (unsigned short *)last_adrs;
	  printShort = *tmpShort;
	  printf ("%04x", SSWAP(printShort));
	  break;
	case 4:
	  tmpLong = (unsigned long *)last_adrs;
	  printLong = *tmpLong;
	  printf ("%08lx", LSWAP(printLong));
	  break;
	case 8:
	  tmpLong = (unsigned long *)last_adrs;
	  printLong = *tmpLong;
	  printLongLong = *(tmpLong+1);
	  printf ("%08lx%08lx", LSWAP(printLongLong), LSWAP(printLong));
	  break;
	default:
	  tmpByte = (char *)last_adrs;
	  printf ("%02x", *tmpByte);
	  break;
	}

      printf (" ");	/* space between words */

#ifdef SHOWASCII
      /* set ascii buffer */
      pByte = (char *) last_adrs;
      for (ix = 0; ix < width; ix ++)
	{
	  if (*pByte == ' ' || (isascii (*pByte) && isprint (*pByte)))
	    {
	      ascii[item*width + ix] = *pByte;
	    }
	  pByte ++;
	}
#endif

      last_adrs = (void *)((int)last_adrs + width);
      item++;
    }

  /* print remainder of last line */
  for (; item < MAX_BYTES_PER_LINE/width; item++)
    printf ("%*s ", 2*width, " ");

#ifdef SHOWASCII
  printf ("  *%16s*\n", ascii);	/* print out ascii format values */
#else
  printf("\n");
#endif

}
