/*----------------------------------------------------------------------------*
 *  Copyright (c) 2009,2010   Southeastern Universities Research Association, *
 *                            Thomas Jefferson National Accelerator Facility  *
 *                                                                            *
 *    This software was developed under a United States Government license    *
 *    described in the NOTICE file included as part of this distribution.     *
 *                                                                            *
 *    Author:  Bryan Moffit                                                   *
 *             moffit@jlab.org                   Jefferson Lab, MS-12B3       *
 *             Phone: (757) 269-5660             12000 Jefferson Ave.         *
 *             Fax:   (757) 269-5800             Newport News, VA 23606       *
 *                                                                            *
 *----------------------------------------------------------------------------*
 *
 * Description:
 *     JLab extra routines to compliment the GEFanuc API
 *
 * SVN: $Rev$
 *
 *----------------------------------------------------------------------------*/

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/mman.h>
#include "jvme.h"
#include "jlabgef.h"
#include "gef/gefcmn_vme_tempe.h"
#include "gef/gefcmn_osa.h"
#include "gef/gefcmn_vme_framework.h"
#include "gef/gefcmn_errno.h"

/**
   Maximum window size for A32 addressing
*/
#define A32_MAX_WINDOW_MAP_SIZE   0x0f000000

/** \name Userspace window pointers
    Global pointers to the Userspace windows
    \{ */
void *a16_window=NULL;
void *a24_window=NULL;
void *a32_window=NULL;
void *a32blt_window=NULL;
void *a32slave_window=NULL;
void *crcsr_window=NULL;
/*  \} */
/** \name Physical Memory Base of the Slave Window
     Physical Memory Base of the Slave Window
     \{ */
int a32slave_physmembase=0;
/*  \} */

/** \name VME address handles for GEF library
    \{ */
static GEF_VME_MASTER_HDL a16_hdl, a24_hdl, a32_hdl, a32blt_hdl;
static GEF_VME_MASTER_HDL crcsr_hdl;
static GEF_VME_SLAVE_HDL a32slave_hdl;
static GEF_MAP_HDL a16map_hdl, a24map_hdl, a32map_hdl, a32bltmap_hdl, a32slavemap_hdl;
static GEF_MAP_HDL crcsrmap_hdl;
/* \} */

/** \name Default VME window widths
    \{ */
unsigned int a32_window_width        = 0x00010000;
unsigned int a32blt_window_width     = 0x0a000000;
unsigned int a24_window_width        = 0x01000000;
unsigned int a16_window_width        = 0x00010000;
unsigned int crcsr_window_width        = 0x01000000;
/*  \} */

/** Handler for Tempe Register mmap */
static struct _GEF_VME_MAP_MASTER *mapHdl;

/** Flag for turning off or on driver verbosity
    \see vmeSetQuietFlag()
*/
extern unsigned int vmeQuietFlag;

/** Mutex for locking/unlocking Tempe driver access */
pthread_mutex_t tsi_mutex = PTHREAD_MUTEX_INITIALIZER;

/** \name Default VME window settings (GE-VME API structures)
    \{ */
GEF_VME_ADDR addr_A16 =
  {
    .upper = 0x00000000,
    .lower = 0x00000000,
    .addr_space = GEF_VME_ADDR_SPACE_A16,
    .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
    .transfer_mode = GEF_VME_TRANSFER_MODE_MBLT,
    .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
    .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
    .broadcast_id = 0,
    .flags = 0
  };

GEF_VME_ADDR addr_A24 =
  {
    .upper = 0x00000000,
    .lower = 0x00000000,
    .addr_space = GEF_VME_ADDR_SPACE_A24,
    .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
    .transfer_mode = GEF_VME_TRANSFER_MODE_MBLT,
    .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
    .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
    .broadcast_id = 0,
    .flags = 0
  };

GEF_VME_ADDR addr_A32 =
  {
    .upper = 0x00000000,
    .lower = 0x08000000,
    .addr_space = GEF_VME_ADDR_SPACE_A32,
    .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
    .transfer_mode = GEF_VME_TRANSFER_MODE_SCT,
    .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
    .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
    .broadcast_id = 0,
    .flags = 0
  };

GEF_VME_ADDR addr_A32blt =
  {
    .upper = 0x00000000,
    .lower = 0x08000000,
    .addr_space = GEF_VME_ADDR_SPACE_A32,
    .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
    .transfer_mode = GEF_VME_TRANSFER_MODE_MBLT,
    .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
    .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
    .broadcast_id = 0,
    .flags = 0
  };

GEF_VME_ADDR addr_A32slave =
  {
    .upper = 0x00000000,
    .lower = 0x08000000,
    .addr_space = GEF_VME_ADDR_SPACE_A32,
    .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
    .transfer_mode = GEF_VME_TRANSFER_MODE_2eSST,
    .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
    .vme_2esst_rate = GEF_VME_2ESST_RATE_320,
    .broadcast_id = 0,
    .flags = 0
  };

GEF_VME_ADDR addr_CRCSR =
  {
    .upper = 0x00000000,
    .lower = 0x00000000,
    .addr_space = GEF_VME_ADDR_SPACE_CRCSR,
    .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
    .transfer_mode = GEF_VME_TRANSFER_MODE_MBLT,
    .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
    .vme_2esst_rate = GEF_VME_2ESST_RATE_160,
    .broadcast_id = 0,
    .flags = 0
  };

/*  \} */

/* didOpen=0(1) when the default windows have not (have been) opened */
static int didOpen=0;

/* Prototypes of routines not included in the gefaunc headers */
GEF_STATUS GEF_STD_CALL
gefVmeReadReg (GEF_VME_BUS_HDL  bus_hdl,
               GEF_UINT32       offset,
               GEF_VME_DWIDTH   width,
               void            *buffer);

GEF_STATUS GEF_STD_CALL
gefVmeWriteReg (GEF_VME_BUS_HDL  bus_hdl,
		GEF_UINT32       offset,
		GEF_VME_DWIDTH   width,
		void            *buffer);

/*!
  Open a VME window for the A16 address space, and mmap it into userspace

  \see jlabgefCloseA16()

  @param *a16p Where to return base address of userspace map
  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefOpenA16(void *a16p)
{

  GEF_STATUS status;

  LOCK_TSI;

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_A16,
				    a16_window_width, &a16_hdl);
  if (status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeCreateMasterWindow", status);
    UNLOCK_TSI;
    return -1;
  }

  /* Now map the VME A16 Window into a local address */
  status = gefVmeMapMasterWindow(a16_hdl, 0, a16_window_width, &a16map_hdl, a16p);
  if(status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeMapMasterWindow", status);
    gefVmeReleaseMasterWindow(a16_hdl);
    a16p = NULL;
    UNLOCK_TSI;
    return -1;
  }

  UNLOCK_TSI;

  return 0;
}

/*!
  Open a VME window for the A24 address space, and mmap it into userspace

  \see jlabgefCloseA24()

  @param *a24p Where to return base address of userspace map
  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefOpenA24(void *a24p)
{

  GEF_STATUS status;

  LOCK_TSI;

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_A24, a24_window_width, &a24_hdl);
  if (status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeCreateMasterWindow", status);
    UNLOCK_TSI;
    return -1;
  }

  /* Now map the VME A24 Window into a local address */
  status = gefVmeMapMasterWindow(a24_hdl, 0, a24_window_width, &a24map_hdl, a24p);
  if(status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeMapMasterWindow", status);
    gefVmeReleaseMasterWindow(a24_hdl);
    a24p = NULL;
    UNLOCK_TSI;
    return -1;
  }

  UNLOCK_TSI;

  return 0;
}

/*!
  Open a VME window for the A32 address space, and mmap it into userspace

  \see jlabgefCloseA32()

  @param base  VME Base address of the VME window
  @param size  Size of the VME window
  @param *a32p Where to return base address of userspace map
  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefOpenA32(unsigned int base, unsigned int size, void *a32p)
{

  GEF_STATUS status;

  addr_A32.lower = base;

  LOCK_TSI;

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_A32, size, &a32_hdl);
  if (status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeCreateMasterWindow", status);
    UNLOCK_TSI;
    return -1;
  }

  if((size == 0)||(size>A32_MAX_WINDOW_MAP_SIZE))
    {
      printf("%s: WARN: Invalid Window map size specified = 0x%x. Not Mapped.\n",
	     __func__,size);
      a32p = NULL;
      UNLOCK_TSI;
      return 0;
    }

  /* Now map the VME A32 Window into a local address */
  status = gefVmeMapMasterWindow(a32_hdl, 0, size, &a32map_hdl, a32p);
  if(status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeMapMasterWindow", status);
    gefVmeReleaseMasterWindow(a32_hdl);
    a32p = NULL;
    UNLOCK_TSI;
    return -1;
  }

  UNLOCK_TSI;

  return 0;
}

int
jlabgefSetA32BltWindowWidth(unsigned int size)
{
  a32blt_window_width = size;
  if(size>A32_MAX_WINDOW_MAP_SIZE)
    {
      printf("%s: WARN: size (0x%x) larger than Max Map size.\n",
	     __func__,size);
    }
  return OK;
}

/*!
  Open a VME window for the A32 address space, and mmap it into userspace
  This window is meant to be used to setup block transfers.

  \see jlabgefCloseA32Blt()

  @param base  VME Base address of the VME window
  @param size  Size of the VME window
  @param *a32p Where to return base address of userspace map
  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefOpenA32Blt(unsigned int base, unsigned int size, void *a32p)
{

  GEF_STATUS status;

  addr_A32blt.lower = base;

  LOCK_TSI;

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_A32blt, size, &a32blt_hdl);
  if (status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeCreateMasterWindow", status);
    UNLOCK_TSI;
    return -1;
  }

  if((size == 0)||(size>A32_MAX_WINDOW_MAP_SIZE))
    {
      printf("%s: WARN: Invalid Window map size specified = 0x%x. Set to Max (0x%x)\n",
	     __func__,size,A32_MAX_WINDOW_MAP_SIZE);
      size = A32_MAX_WINDOW_MAP_SIZE;
    }

  /* Now map the VME A32 Window into a local address */
  status = gefVmeMapMasterWindow(a32blt_hdl, 0, size, &a32bltmap_hdl, a32p);
  if(status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeMapMasterWindow", status);
    gefVmeReleaseMasterWindow(a32blt_hdl);
    a32p = NULL;
    UNLOCK_TSI;
    return -1;
  }

  UNLOCK_TSI;

  return 0;
}

/*!
  Open a VME window for the CRCSR address space, and mmap it into userspace

  \see jlabgefCloseCRCSR()

  @param *acrcsrp Where to return base address of userspace map
  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefOpenCRCSR(void *crcsrp)
{

  GEF_STATUS status;

  LOCK_TSI;

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_CRCSR, crcsr_window_width, &crcsr_hdl);
  if (status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeCreateMasterWindow", status);
    UNLOCK_TSI;
    return -1;
  }

  /* Now map the VME CRCSR Window into a local address */
  status = gefVmeMapMasterWindow(crcsr_hdl, 0, crcsr_window_width, &crcsrmap_hdl, crcsrp);
  if(status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeMapMasterWindow", status);
    gefVmeReleaseMasterWindow(crcsr_hdl);
    crcsrp = NULL;
    UNLOCK_TSI;
    return -1;
  }

  UNLOCK_TSI;

  return 0;
}

/*!
  Open a Slave VME window for the A32 address space.

  \see jlabgefCloseA32Slave()

  @param base  VME Base address of the VME window
  @param size  Size of the VME window
  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefOpenSlaveA32(unsigned int base, unsigned int size)
{
  GEF_STATUS status;
  UINT32 itat;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  addr_A32slave.lower = base;

  LOCK_TSI;

  status = gefVmeCreateSlaveWindow(vmeHdl, &addr_A32slave, size, &a32slave_hdl);
  if (status != GEF_STATUS_SUCCESS) {jlabgefPrintGefError((char *)__func__, "gefVmeCreateSlaveWindow", status);
    UNLOCK_TSI;
    return -1;
  }

  /* Now map the VME A32 Slave Window into a local address */
  status = gefVmeMapSlaveWindow(a32slave_hdl, 0, size, &a32slavemap_hdl,
				(GEF_MAP_PTR*)&a32slave_window);
  if(status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeMapSlaveWindow", status);
    gefVmeRemoveSlaveWindow(a32slave_hdl);
    a32slave_window = NULL;
    UNLOCK_TSI;
    return -1;
  }

#ifdef DEBUGSLAVE
  struct _GEF_VME_MAP_SLAVE *tryHdl = (struct _GEF_VME_MAP_SLAVE *)&a32slavemap_hdl;

  printf("%s: phys_addr = 0x%08x\n",
	 __func__,(tryHdl->phys_addr));
#endif

  /* Set bits in ITAT0, to turn all all types of block transfers.
     Assume that window 0 was the one setup, for the moment */

  UINT32 itsal, itofl, iteal, phys_addr;
  itsal = pTempe->lcsr.inboundTranslation[0].itsal; itsal = LSWAP(itsal);
  itofl = pTempe->lcsr.inboundTranslation[0].itofl; itofl = LSWAP(itofl);
  iteal = pTempe->lcsr.inboundTranslation[0].iteal; iteal = LSWAP(iteal);

  phys_addr = itsal+itofl;
  a32slave_physmembase = phys_addr;

#ifdef DEBUGSLAVE
  printf("%s:\n  ITSAL = 0x%08x\n  ITOFL = 0x%08x\n  ITEAL = 0x%08x\n",
	 __func__,itsal,itofl,iteal);

  printf("  phys_addr = 0x%08x\n",phys_addr);
#endif

  itat = pTempe->lcsr.inboundTranslation[0].itat; itat = LSWAP(itat);

  if(itat & TSI148_LCSR_ITAT_EN)
    {
      itat |= (TSI148_LCSR_ITAT_2eSSTB | TSI148_LCSR_ITAT_2eSST | TSI148_LCSR_ITAT_2eVME |
	       TSI148_LCSR_ITAT_MBLT | TSI148_LCSR_ITAT_BLT);
      itat |= (TSI148_LCSR_ITAT_SUPR | TSI148_LCSR_ITAT_NPRIV |
	       TSI148_LCSR_ITAT_PGM | TSI148_LCSR_ITAT_DATA);
      pTempe->lcsr.inboundTranslation[0].itat = LSWAP(itat);
    }
  else
    {
      printf("%s: ERROR: Inbound window attributes not modified.  Unexpected window number.\n",
	     __func__);
    }

  UNLOCK_TSI;
  return status;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenA16()

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefCloseA16()
{

  GEF_STATUS status;

  LOCK_TSI;

  /* Unmap A16 address space */
  status = gefVmeUnmapMasterWindow(a16map_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeUnmapMasterWindow", status);
      UNLOCK_TSI;
      return -1;
    }


  /* Release the A16 Master window */
  status = gefVmeReleaseMasterWindow(a16_hdl);
  if (status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeReleaseMasterWindow", status);
      UNLOCK_TSI;
      return -1;
    }

  a16_window=NULL;

  UNLOCK_TSI;

  return 0;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenA24()

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefCloseA24()
{

  GEF_STATUS status;

  LOCK_TSI;

  /* Unmap A24 address space */
  status = gefVmeUnmapMasterWindow(a24map_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeUnmapMasterWindow", status);
      UNLOCK_TSI;
      return -1;
    }

  /* Release the A24 Master window */
  status = gefVmeReleaseMasterWindow(a24_hdl);
  if (status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeReleaseMasterWindow", status);
      UNLOCK_TSI;
      return -1;
    }


  a24_window=NULL;

  UNLOCK_TSI;

  return 0;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenA32()

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefCloseA32()
{

  GEF_STATUS status;

  LOCK_TSI;

  /* Unmap A32 address space */
  status = gefVmeUnmapMasterWindow(a32map_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeUnmapMasterWindow", status);
      UNLOCK_TSI;
      return -1;
    }

  if(a32_window!=NULL)
    {
      /* Release the A32 Master window */
      status = gefVmeReleaseMasterWindow(a32_hdl);
      if (status != GEF_STATUS_SUCCESS)
	{
	  jlabgefPrintGefError((char *)__func__, "gefVmeReleaseMasterWindow", status);
	  UNLOCK_TSI;
	  return -1;
	}

      a32_window=NULL;
    }

  UNLOCK_TSI;

  return 0;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenA32Blt()

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefCloseA32Blt()
{

  GEF_STATUS status;

  LOCK_TSI;

  /* Unmap A32 address space */
  status = gefVmeUnmapMasterWindow(a32bltmap_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeUnmapMasterWindow", status);
      UNLOCK_TSI;
      return -1;
    }

  if(a32blt_window!=NULL)
    {
      /* Release the A32 Master window */
      status = gefVmeReleaseMasterWindow(a32blt_hdl);
      if (status != GEF_STATUS_SUCCESS)
	{
	  jlabgefPrintGefError((char *)__func__, "gefVmeReleaseMasterWindow", status);
	  UNLOCK_TSI;
	  return -1;
	}

      a32blt_window=NULL;
    }

  UNLOCK_TSI;

  return 0;
}

/*!
  Routine to unmmap and close the VME window opened with jlabgefOpenCRCSR()

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefCloseCRCSR()
{

  GEF_STATUS status;

  LOCK_TSI;

  /* Unmap CRCSR address space */
  status = gefVmeUnmapMasterWindow(crcsrmap_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeUnmapMasterWindow", status);
      UNLOCK_TSI;
      return -1;
    }


  /* Release the A24 Master window */
  status = gefVmeReleaseMasterWindow(crcsr_hdl);
  if (status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeReleaseMasterWindow", status);
      UNLOCK_TSI;
      return -1;
    }


  crcsr_window=NULL;

  UNLOCK_TSI;

  return 0;
}

/*!
  Routine to close the slate VME window opened with jlabgefOpenSlaveA32()

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefCloseA32Slave()
{

  GEF_STATUS status;

  LOCK_TSI;

  /* Unmap A32 address space */
  status = gefVmeUnmapSlaveWindow(a32slavemap_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeUnmapSlaveWindow", status);
      UNLOCK_TSI;
      return -1;
    }


  /* Release the A32 Master window */
  status = gefVmeRemoveSlaveWindow(a32slave_hdl);
  if (status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeRemoveSlaveWindow", status);
      UNLOCK_TSI;
      return -1;
    }


  a32slave_window=NULL;

  UNLOCK_TSI;

  return 0;
}

/*!
  Routine to initialize the GE-VME API
  - opens default VME Windows and maps them into Userspace
  - maps VME Bridge Registers into Userspace
  - disables interrupts on VME Bus Errors
  - creates a shared mutex for interrupt/trigger locking
  - calls vmeBusCreateLockShm()

  \see jlabgefCloseDefaultWindows();

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefOpenDefaultWindows()
{
  GEF_STATUS status;

  if(didOpen)
    return 1;

  LOCK_TSI;
  vmeHdl = NULL;

  status = gefVmeOpen(&vmeHdl);
  if (status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeOpen", status);
      vmeHdl = NULL;
      UNLOCK_TSI;
      return -1;
    }

  /* Turn off the Debug Flags, by default */
  gefVmeSetDebugFlags(vmeHdl,0x0);

  UNLOCK_TSI;

  /* Open CRCSR window */
  if(crcsr_window==NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening CRCSR Window ");
      status = jlabgefOpenCRCSR(&crcsr_window);
    }
  if(status==GEF_STATUS_SUCCESS)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_CRCSR.lower,
	       (unsigned long)crcsr_window,
	       crcsr_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
    }

  /* Open A32 windows */
  if(a32_window==NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening A32 Window   ");
      status = jlabgefOpenA32(0xfb000000,a32_window_width,&a32_window);
    }
  if(status==GEF_STATUS_SUCCESS)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_A32.lower,
	       (unsigned long)a32_window,
	       a32_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
    }

  if(a32blt_window==NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening A32Blt Window");
      status = jlabgefOpenA32Blt(0x08000000,a32blt_window_width,&a32blt_window);
    }
  if(status==GEF_STATUS_SUCCESS)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_A32blt.lower,
	       (unsigned long)a32blt_window,
	       a32blt_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
    }

  /* Open A24 window */
  if(a24_window==NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening A24 Window   ");
      status = jlabgefOpenA24(&a24_window);
    }
  if(status==GEF_STATUS_SUCCESS)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_A24.lower,
	       (unsigned long)a24_window,
	       a24_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
    }

  /* Open A16 window */
  if(a16_window==NULL)
    {
      if(!vmeQuietFlag)
	printf("Opening A16 Window   ");
      status = jlabgefOpenA16(&a16_window);
    }
  if(status==GEF_STATUS_SUCCESS)
    {
      if(!vmeQuietFlag)
	printf("  VME (LOCAL) 0x%08x (0x%lx), width 0x%08x\n",
	       addr_A16.lower,
	       (unsigned long)a16_window,
	       a16_window_width);
    }
  else
    {
      if(!vmeQuietFlag)
	printf("... Open Failed!\n");
    }

  jlabgefMapTsi();

  /* By JLAB default... disable IRQ on BERR */
  status = jlabgefDisableBERRIrq(1);
  if(status != GEF_STATUS_SUCCESS)
    {
      printf("%s: ERROR Disabling IRQ on BERR",__func__);
    }

  /* Create Shared Mutex */
  vmeBusCreateLockShm();

  didOpen=1;

  return status;
}

/*!
  Routine to cleanup what was initialized by jlabgefOpenDefaultWindows()

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefCloseDefaultWindows()
{
  GEF_STATUS status;
  if(didOpen==0) return 1;

  /* Kill Shared Mutex - Here we just unmap it (don't remove the shm file) */
  vmeBusKillLockShm(0);


  jlabgefUnmapTsi();

  /* Close A16 window */
  if(!vmeQuietFlag)
    printf("Closing A16 Window\n");
  jlabgefCloseA16();

  /* Close A24 window */
  if(!vmeQuietFlag)
    printf("Closing A24 Window\n");
  jlabgefCloseA24();

  /* Close A32 windows */
  if(!vmeQuietFlag)
    printf("Closing A32Blt Window\n");
  jlabgefCloseA32Blt();

  if(!vmeQuietFlag)
    printf("Closing A32 Window\n");
  jlabgefCloseA32();

  /* Close CRCSR window */
  if(!vmeQuietFlag)
    printf("Closing CRCSR Window\n");
  jlabgefCloseCRCSR();

  /* Close the VME Device file */
  if(!vmeQuietFlag)
    printf("Calling gefVmeClose\n");

  LOCK_TSI;
  status = gefVmeClose(vmeHdl);
  if (status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeClose", status);
    }
  UNLOCK_TSI;

  didOpen=0;

  return status;
}

/*!
  Read a 32bit word from a Tempe Chip register.

  \see gefvme_vme_tempe.h

  @param offset Register offset from Tempe Base Address.  A list of
  defines for offsets from register names is found in the header file:
  gefvme_vme_tempe.h

  @return 32bit value at the requested register, if successful.  -1,
  otherwise.

*/
GEF_UINT32
jlabgefReadRegister(GEF_UINT32 offset)
{
  GEF_STATUS status;
  GEF_UINT32 temp;

  status = gefVmeReadReg(vmeHdl,offset,GEF_VME_DWIDTH_D32,&temp);
  if (status != GEF_STATUS_SUCCESS) {
    jlabgefPrintGefError((char *)__func__, "gefVmeReadReg", status);
    return -1;
  } else {
    return temp;
  }

}

/*!
  Write a 32bit word to a Tempe Chip register.

  \see gefvme_vme_tempe.h

  @param offset Register offset from Tempe Base Address.  A list of
  defines for offsets from register names is found in the header file:
  gefvme_vme_tempe.h
  @param buffer 32bit word to be written to offset

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefWriteRegister(GEF_UINT32 offset, GEF_UINT32 buffer)
{
  GEF_STATUS status;

  status = gefVmeWriteReg(vmeHdl,offset,GEF_VME_DWIDTH_D32,&buffer);
  if (status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeWriteReg", status);
    }

  return status;

}

/*!
  Assert SYSRESET on the VMEbus.

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
 */
GEF_STATUS
jlabgefSysReset()
{
  GEF_STATUS status;
  UINT32 read;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  read = pTempe->lcsr.vctrl; read = LSWAP(read);

#ifdef DEBUG_SYSRESET
  printf("VCTRL = 0x%08x SRESET = 0x%x\n",
	 read,
	 read & TEMPE_VCTRL_SRESET);
#endif /* DEBUG_SYSRESET */

  status = gefVmeSysReset(vmeHdl);
  if (status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeSysReset", status);
    }

  /* Wait 200us... SYSRESET should clear by then */
  usleep(200);
  read = pTempe->lcsr.vctrl; read = LSWAP(read);

  if(read & TEMPE_VCTRL_SRESET)
    {
      /* Not cleared... clear it */
      pTempe->lcsr.vctrl = LSWAP(read & ~TEMPE_VCTRL_SRESET);
    }
  UNLOCK_TSI;

  return status;
}

/*!
  Routine to query the status of interrupts on VME Bus Error

  @return 1 if enabled, 0 if disabled, -1 if error.
*/
int
jlabgefBERRIrqStatus()
{
  GEF_UINT32 tmpCtl;
  unsigned int inten_enabled=0, inteo_enabled=0;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  tmpCtl = pTempe->lcsr.inten; tmpCtl = LSWAP(tmpCtl);
  UNLOCK_TSI;
  if(tmpCtl == -1)
    {
      printf("%s: ERROR TEMPE_INTEN read failed.", __func__);
      return ERROR;
    }

  /* Check if BERR IRQ is enabled in INTEN */
  if(tmpCtl&BIT_INT_VME_ERR)
    inten_enabled=1;
  else
    inten_enabled=0;

  LOCK_TSI;
  tmpCtl = pTempe->lcsr.inteo; tmpCtl = LSWAP(tmpCtl);
  UNLOCK_TSI;
  if(tmpCtl == -1)
    {
      printf("%s: ERROR: TEMPE_INTEO read failed.", __func__);
      return ERROR;
    }

  /* Check if BERR IRQ is enabled in INTEO */
  if(tmpCtl&BIT_INT_VME_ERR)
    inteo_enabled=1;
  else
    inteo_enabled=0;

  if(inten_enabled==inteo_enabled)
    {
      return inten_enabled;
    }
  else
    {
      printf("%s: ERROR: TEMPE_INTEN != TEMPE_INTEO (%d != %d)",
	     __func__, inten_enabled, inteo_enabled);
      return ERROR;
    }

}

/*!
  Routine to disable interrupts on VME Bus Error

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefDisableBERRIrq(int pflag)
{
  GEF_UINT32 tmpCtl;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  tmpCtl = pTempe->lcsr.inten; tmpCtl = LSWAP(tmpCtl);
  UNLOCK_TSI;

  if(tmpCtl == -1)
    {
      printf("%s: ERROR: (TEMPE_INTEN) read failed.",
	     __func__);
      return ERROR;
    }

  tmpCtl &= ~BIT_INT_VME_ERR;

  LOCK_TSI;
  pTempe->lcsr.inten = LSWAP(tmpCtl);
  UNLOCK_TSI;

  LOCK_TSI;
  pTempe->lcsr.inteo = LSWAP(tmpCtl);
  UNLOCK_TSI;

  if(!vmeQuietFlag)
    if(pflag==1)
      printf("VME Bus Error IRQ Disabled\n");

  return GEF_STATUS_SUCCESS;
}

/*!
  Routine to enable interrupts on VME Bus Error

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefEnableBERRIrq(int pflag)
{
  GEF_UINT32 tmpCtl;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  LOCK_TSI;
  tmpCtl = pTempe->lcsr.inten; tmpCtl = LSWAP(tmpCtl);
  UNLOCK_TSI;

  if(tmpCtl == -1)
    {
      printf("%s: (TEMPE_INTEN) read failed.", __func__);
      return -1;
    }

  tmpCtl  |= BIT_INT_VME_ERR;

  LOCK_TSI;
  pTempe->lcsr.inten = LSWAP(tmpCtl);
  UNLOCK_TSI;

  LOCK_TSI;
  pTempe->lcsr.inteo = LSWAP(tmpCtl);
  UNLOCK_TSI;

  if(!vmeQuietFlag)
    if(pflag==1)
      printf("VME Bus Error IRQ Enabled\n");

  return GEF_STATUS_SUCCESS;
}

/*!
  Mmap the Tempe register space to userspace.

  \see jlabgefUnmapTsi()

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefMapTsi()
{
  GEF_STATUS status;
  GEF_UINT32 offset=0x0;
  unsigned int pci_base = 0x0;
  unsigned int driver_deviveni = 0x0;
  unsigned short devi=0, veni=0;
  unsigned int map_deviveni = 0x0;

  char *virtual_addr;
  mapHdl = (struct _GEF_VME_MAP_MASTER *) malloc (sizeof (struct _GEF_VME_MAP_MASTER));

  if (NULL == mapHdl)
    {
      return(GEF_VME_SET_ERROR(GEF_STATUS_NO_MEM));
    }

  memset(mapHdl, 0, sizeof(struct _GEF_VME_MAP_MASTER));

  /* Obtain the base from the PCI-VME Bridge itself */
  pci_base = jlabgefReadRegister(TEMPE_PCI_MBARL) & 0xfffff000;
  mapHdl->phys_addr = pci_base;

  mapHdl->virt_addr = 0;
  GEF_UINT32 size = 0x7ffff;

  status = gefOsaMap (vmeHdl->osa_intf_hdl,
		      &(mapHdl->osa_map_hdl),
		      &virtual_addr,
		      ((GEF_UINT32)(mapHdl->phys_addr)+offset),
		      size);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefOsaMap", status);
      pTempe = NULL;
      return status;
    }

  pTempe = (tsi148_t *)virtual_addr;

  /* Quick check of the map, compared to a read through the kernel driver */
  LOCK_TSI;
  driver_deviveni = jlabgefReadRegister(TEMPE_DEVI_VENI);
  veni = pTempe->gcsr.veni;
  devi = pTempe->gcsr.devi;
  map_deviveni = SSWAP(veni) | (SSWAP(devi))<<16;
  UNLOCK_TSI;

  if(driver_deviveni != map_deviveni)
    {
      printf("%s: ERROR: kernel map inconsistent with userspace map\n",
	     __func__);
      printf("\tdriver_deviveni\t= 0x%x\n\tmap_deviveni\t= 0x%x\n",
	     driver_deviveni,map_deviveni);
      pTempe = NULL;
    }
  else
    {
      if(!vmeQuietFlag)
	printf("%s: Tempe Chip Userspace map successful\n",
	       __func__);
    }

  return status;
}

/*!
  Free up the mmap of the Tempe register space to userspace.

  @return GEF_STATUS_SUCCESS, if successful.  Error status from GEF_STATUS_ENUM, otherwise.
*/
GEF_STATUS
jlabgefUnmapTsi()
{
  GEF_STATUS status;

  LOCK_TSI;
  status = gefOsaUnMap(mapHdl->osa_map_hdl);
  if(status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefOsaUnMap", status);
    }

  free(mapHdl);
  UNLOCK_TSI;

  return status;
}

/*!
  Routine to clear any VME Exception that is currently flagged on the VME Bridge Chip

  @param pflag
  - 1 to turn on verbosity
  - 0 to disable verbosity.
*/
void
jlabgefClearException(int pflag)
{
  /* check the VME Exception Attribute... clear it, and put out a warning */
  volatile unsigned int vmeExceptionAttribute=0;
  unsigned int veal=0;

  LOCK_TSI;
  vmeExceptionAttribute = pTempe->lcsr.veat;
  vmeExceptionAttribute = LSWAP(vmeExceptionAttribute);

  if(vmeExceptionAttribute & TEMPE_VEAT_VES)
    {
      if(pflag==1)
	{
	  veal = pTempe->lcsr.veal; veal = LSWAP(veal);
	  printf("%s: Clearing VME Exception (0x%x) at VME address 0x%x\n",
		 __func__,
		 vmeExceptionAttribute, veal);
	}
      pTempe->lcsr.veat = LSWAP(TEMPE_VEAT_VESCL);
    }
  UNLOCK_TSI;

}

/*!
  Clear any Bus Error exceptions, if they exist
*/
void
jlabgefClearBERR()
{
  volatile unsigned int vmeExceptionAttribute=0;
  unsigned int veal;

  /* check the VME Exception Attribute... clear it, and put out a warning */
  LOCK_TSI;
  vmeExceptionAttribute = pTempe->lcsr.veat;
  vmeExceptionAttribute = LSWAP(vmeExceptionAttribute);

  if( (vmeExceptionAttribute & TEMPE_VEAT_VES) &&
      ((vmeExceptionAttribute & TEMPE_VEAT_BERR) ||
       (vmeExceptionAttribute & TEMPE_VEAT_2eST)) )
    {
      veal = pTempe->lcsr.veal; veal = LSWAP(veal);
      printf("%s: Clearing VME BERR/2eST (0x%x) at VME address 0x%x\n",
	     __func__,
	     vmeExceptionAttribute,
	     veal);
      pTempe->lcsr.veat = LSWAP(TEMPE_VEAT_VESCL);
    }
  UNLOCK_TSI;

}

/*!
  Routine to probe a VME Address for a VME Bus Error

  @param *addr address to be probed
  @param size  size (1, 2, or 4) of address to read (in bytes)
  @param *retVal where to return value

  @return 0, if successful. -1, otherwise.
*/
int
jlabgefMemProbe(char* addr, int size, char *retVal)
{
  volatile unsigned int vmeExceptionAttribute=0;
  unsigned int lval; unsigned short sval; char cval;
  int irqOnBerr=0;
  unsigned int veal;

  /* If IRQ on BERR is enabled, disable it... enable it again at the end */
  irqOnBerr = jlabgefBERRIrqStatus();
  if(irqOnBerr==1)
    {
      jlabgefDisableBERRIrq(0);
    }

  /* Clear the Exception register, before trying to read */
  jlabgefClearException(0);

  /* Perform a test read */
  switch(size)
    {
    case 4:
      memcpy(&lval,(void *)addr,sizeof(unsigned int));
      lval = LSWAP((unsigned int)lval);
      memcpy(retVal,&lval,sizeof(unsigned int));
      break;
    case 2:
      memcpy(&sval,(void *)addr,sizeof(unsigned short));
      sval = SSWAP((unsigned short)sval);
      memcpy(retVal,&sval,sizeof(unsigned short));
      break;
    case 1:
      memcpy(&cval,(void *)addr,sizeof(char));
      memcpy(retVal,&cval,sizeof(char));
      break;
    default:
      printf("%s: ERROR: Invalid size %d",__func__,size);
      return ERROR;
    }

  /* Check the Exception register for a VME Bus Error */
  LOCK_TSI;
  vmeExceptionAttribute = pTempe->lcsr.veat;
  vmeExceptionAttribute = LSWAP(vmeExceptionAttribute);

  if( (vmeExceptionAttribute & TEMPE_VEAT_VES) &&
      (vmeExceptionAttribute & TEMPE_VEAT_BERR) )
    {
      /* Clear the exception register */
      if(!vmeQuietFlag)
	{
	  veal = pTempe->lcsr.veal; veal = LSWAP(veal);
	  printf("%s: Clearing VME BERR/2eST (0x%x) at VME address 0x%x\n",
		 __func__,
		 vmeExceptionAttribute,veal);
	}
      pTempe->lcsr.veat = LSWAP(TEMPE_VEAT_VESCL);
      UNLOCK_TSI;
      return ERROR;
    }
  UNLOCK_TSI;

  if(irqOnBerr==1)
    {
      jlabgefEnableBERRIrq(0);
    }

  return OK;

}

/*! Maximum allowed GE-VME callback handles */
#define MAX_CB_HDL 25
/*! Array of GE-VME callback handles linked to their associated interrupt level */
static GEF_CALLBACK_HDL gefCB_hdl[MAX_CB_HDL] = {
  0,0,0,0,0,
  0,0,0,0,0,
  0,0,0,0,0,
  0,0,0,0,0,
  0,0,0,0,0
};

/*!
  Routine to connect a routine to a VME Bus Interrupt

  @param vector  interrupt vector to attach to
  @param level   VME Bus interrupt level
  @param routine routine to be called
  @param arg     argument to be passed to the routine

  @return 0, if successful. -1, otherwise.
*/
int
jlabgefIntConnect(unsigned int vector,
		  unsigned int level,
		  VOIDFUNCPTR routine,
		  unsigned int arg)
{
  GEF_STATUS status;

  if(level>=MAX_CB_HDL)
    {
      printf("%s: ERROR: Interrupt level %d not supported\n",
	     __func__, level);
      return ERROR;
    }

  if(gefCB_hdl[level]!=0)
    {
      printf("%s: ERROR: Interrupt already connected for level %d\n",
	     __func__, level);
      return ERROR;
    }

  status = gefVmeAttachCallback (vmeHdl, level, vector,
				 routine,(void *)&arg, &gefCB_hdl[level]);

  if (status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeAttachCallback", status);
      return ERROR;
    }

  return OK;
}

/*!
  Routine to release the routine attached with vmeIntConnect()

  @param level  VME Bus Interrupt level

  @return 0, if successful. -1, otherwise.
*/
int
jlabgefIntDisconnect(unsigned int level)
{
  GEF_STATUS status;

  if(level>=MAX_CB_HDL)
    {
      printf("%s: ERROR: Interrupt level %d not supported\n",
	     __func__, level);
      return ERROR;
    }

  if(gefCB_hdl[level]==0)
    {
      printf("%s: WARN: Interrupt not connected for level %d\n",
	     __func__, level);
      return OK;
    }

  status = gefVmeReleaseCallback(gefCB_hdl[level]);
  if (status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeReleaseCallback", status);
      return ERROR;
    }

  gefCB_hdl[level] = 0;

  return OK;
}

/*!
  Routine to convert a a VME Bus address to a Userspace Address

  @param vmeAdrsSpace Bus address space in whihc vmeBusAdrs resides
  @param *vmeBusAdrs  Bus address to convert
  @param **pPciAdrs   Where to return Userspace address

  @return 0, if successful. -1, otherwise.
*/
int
jlabgefVmeBusToLocalAdrs(int vmeAdrsSpace,
			 char *vmeBusAdrs,
			 char **pPciAdrs)
{
  unsigned int vmeSpaceMask;
  unsigned int vmeAdrToConvert;
  unsigned int base;
  unsigned int limit;
  unsigned int trans;
  unsigned int busAdrs;
  int adrConverted=0;
  char *pciBusAdrs = 0;

  switch (vmeAdrsSpace)
    {
    case GEF_VME_ADDR_MOD_A32SP:
    case GEF_VME_ADDR_MOD_A32SD:
    case GEF_VME_ADDR_MOD_A32UP:
    case GEF_VME_ADDR_MOD_A32UD:

      /* See if the window is A32 enabled */

      if (a32_window!=NULL || a32blt_window!=NULL)
	{
	  vmeSpaceMask = 0xffffffff;
	  vmeAdrToConvert = (unsigned long)vmeBusAdrs;
	  base = addr_A32.lower;
	  limit = a32_window_width + addr_A32.lower;
	  trans = 0;

	  if ( ((base + trans) <= vmeAdrToConvert) &&
	       ((limit + trans) >= vmeAdrToConvert) )
	    {
	      busAdrs = vmeAdrToConvert - base;
	      pciBusAdrs = (char *)((unsigned long)busAdrs + (unsigned long)a32_window);
	      adrConverted = 1;
	      break;
	    }
	  else
	    {
	      base = addr_A32blt.lower;
	      limit = a32blt_window_width + addr_A32blt.lower;
	      trans = 0;
	      if ( ((base + trans) <= vmeAdrToConvert) &&
		   ((limit + trans) >= vmeAdrToConvert) )
		{
		  busAdrs = vmeAdrToConvert - base;
		  pciBusAdrs = (char *)((unsigned long)busAdrs + (unsigned long)a32blt_window);
		  adrConverted = 1;
		  break;
		}

	    }

	  break;
	}

    case GEF_VME_ADDR_MOD_A24SP:
    case GEF_VME_ADDR_MOD_A24SD:
    case GEF_VME_ADDR_MOD_A24UP:
    case GEF_VME_ADDR_MOD_A24UD:

      /* See if the window is A24 enabled */

      if (a24_window!=NULL)
	{
	  vmeSpaceMask = 0x00ffffff;
	  vmeAdrToConvert = (unsigned long)vmeBusAdrs & vmeSpaceMask;
	  base = addr_A24.lower;
	  limit = a24_window_width + addr_A24.lower;
	  trans = 0;
	  if ( ((base + trans) <= vmeAdrToConvert) &&
	       ((limit + trans) >= vmeAdrToConvert) )
	    {
	      busAdrs = vmeAdrToConvert - base;
	      pciBusAdrs = (char *)((unsigned long)busAdrs + (unsigned long)a24_window);
	      adrConverted = 1;
	      break;
	    }

	  break;
	}

    case GEF_VME_ADDR_MOD_A16S:
    case GEF_VME_ADDR_MOD_A16U:

      /* See if the window is A24 enabled */

      if (a16_window!=NULL)
	{
	  vmeSpaceMask = 0x0000ffff;
	  vmeAdrToConvert = (unsigned long)vmeBusAdrs & vmeSpaceMask;
	  base = addr_A16.lower;
	  limit = a16_window_width + addr_A16.lower;
	  trans = 0;
	  if ( ((base + trans) <= vmeAdrToConvert) &&
	       ((limit + trans) >= vmeAdrToConvert) )
	    {
	      busAdrs = vmeAdrToConvert - base;
	      pciBusAdrs = (char *)((unsigned long)busAdrs + (unsigned long)a16_window);
	      adrConverted = 1;
	      break;
	    }

	  break;
	}

    case GEF_VME_ADDR_MOD_CR_CSR:

      /* See if the window is CRCSR enabled */

      if (crcsr_window!=NULL)
	{
	  vmeSpaceMask = 0x00ffffff;
	  vmeAdrToConvert = (unsigned long)vmeBusAdrs & vmeSpaceMask;
	  base = addr_CRCSR.lower;
	  limit = crcsr_window_width + addr_CRCSR.lower;
	  trans = 0;
	  if ( ((base + trans) <= vmeAdrToConvert) &&
	       ((limit + trans) >= vmeAdrToConvert) )
	    {
	      busAdrs = vmeAdrToConvert - base;
	      pciBusAdrs = (char *)((unsigned long)busAdrs + (unsigned long)crcsr_window);
	      adrConverted = 1;
	      break;
	    }

	  break;
	}

    default:
      return (ERROR);		/* invalid address space */
    }

  if (adrConverted == 1)
    {
      *pPciAdrs = pciBusAdrs;
      return (OK);
    }
    return (ERROR);

}

/*!
  Routine to convert a Userspace Address to a VME Bus address

  @param localAdrs  Local (userspace) address to convert
  @param *vmeAdrs   Where to return VME address
  @param *amCode    Where to return address modifier

  @return 0, if successful. -1, otherwise.
*/
int
jlabgefLocalToVmeAdrs(unsigned long localAdrs, unsigned int *vmeAdrs,
		      unsigned short *amCode)
{
  /* Go through each window to see where the localAdrs falls */
  if((localAdrs>=(unsigned long)a32_window) &&
     (localAdrs<((unsigned long)(a32_window+a32_window_width))))
    {
      *vmeAdrs = localAdrs - (unsigned long)a32_window + addr_A32.lower;
      *amCode = 0x09;
      return OK;
    }

  if((localAdrs>=(unsigned long)a32blt_window) &&
     (localAdrs<((unsigned long)(a32blt_window+a32blt_window_width))))
    {
      *vmeAdrs = localAdrs - (unsigned long)a32blt_window + addr_A32blt.lower;
      *amCode = 0x09;
      return OK;
    }

  if((localAdrs>=(unsigned long)a24_window) &&
     (localAdrs<((unsigned long)(a24_window+a24_window_width))))
    {
      *vmeAdrs = localAdrs - (unsigned long)a24_window + addr_A24.lower;
      *amCode = 0x39;
      return OK;
    }

  if((localAdrs>=(unsigned long)a16_window) &&
     (localAdrs<((unsigned long)(a16_window+a16_window_width))))
    {
      *vmeAdrs = localAdrs - (unsigned long)a16_window + addr_A16.lower;
      *amCode = 0x29;
      return OK;
    }

  if((localAdrs>=(unsigned long)crcsr_window) &&
     (localAdrs<((unsigned long)(crcsr_window+crcsr_window_width))))
    {
      *vmeAdrs = localAdrs - (unsigned long)crcsr_window + addr_CRCSR.lower;
      *amCode = 0x2F;
      return OK;
    }

  printf("%s: ERROR: VME address not found from 0x%lx\n",
	 __func__,localAdrs);

  *amCode=0xFFFF;
  *vmeAdrs=0xFFFFFFFF;
  return ERROR;
}

/*!
  Routine to change the address modifier of the A24 Outbound VME Window.
  The A24 Window must be opened, prior to calling this routine.

  @param addr_mod Address modifier to be used.  If 0, the default (0x39) will be used.

  @return 0, if successful. -1, otherwise
*/

int
jlabgefSetA24AM(int addr_mod)
{
  int iwin, done=0;
  unsigned int amode, otat, AM_OTAT, enabled;

  if(pTempe == NULL)
    {
      printf("%s: ERROR: No MAP to VME bridge\n", __func__);
      return ERROR;
    }

  if(a24_window==NULL)
    {
      printf("%s: ERROR: A24 Window is not open.  Unable to change AM.\n",
	     __func__);
      return -1;
    }

  if( (addr_mod<0x10 || addr_mod>0x1F) && (addr_mod !=0) )
    {
      printf("%s: ERROR: Invalid AM code (0x%x).  Must be 0x10 - 0x1F).",
	     __func__,addr_mod);
    }

  if(addr_mod == 0)
    addr_mod = 0x39;

  /* Translate the AM code to the bits required in the OTAT reg */
  if(addr_mod == 0x39)
    {
      amode = TSI148_LCSR_OTAT_AMODE_A24;
    }
  else
    {
      /* lower 4 bits */
      amode = (1<<3) | ((addr_mod & 0xF)>>2);
      /* bit 4 and 5 */
      amode |= ( (addr_mod&0X3)<<4 );
    }

  /* Loop through the windows and find the one we want (A24) */
  LOCK_TSI;
  for(iwin=0; iwin<8; iwin++)
    {
      otat = pTempe->lcsr.outboundTranslation[iwin].otat;
      otat = LSWAP(otat);

      enabled = otat & TSI148_LCSR_OTAT_EN;
      AM_OTAT = otat & TSI148_LCSR_OTAT_AMODE_M;

      if(enabled)
	{
	  if( (AM_OTAT == TSI148_LCSR_OTAT_AMODE_A24) ||
	      ((AM_OTAT & (2<<2)) == (2<<2) ) )
	    {
	      pTempe->lcsr.outboundTranslation[iwin].otat =
		LSWAP( (otat &
			~(TSI148_LCSR_OTAT_SUP | TSI148_LCSR_OTAT_PGM | TSI148_LCSR_OTAT_AMODE_M) )
		       | amode);
	      done=1;
	    }
	}

    }
  UNLOCK_TSI;

  if(done==0)
    {
      printf("%s: ERROR: Unable to find an open A24 window.\n",
	     __func__);
      return -1;
    }

  if(!vmeQuietFlag)
    printf("%s: INFO: A24 Window AM changed to 0x%x\n",
	   __func__, addr_mod);

  return 0;
}

/*!
  Routine to enable/disable debug flags set in the VME Bridge Kernel Driver

  @param flags GE-VME API dependent flags to toggle specific debug levels and messages

  @return 0, if successful. -1, otherwise.
*/
int
jlabgefSetDebugFlags(int flags)
{
  GEF_STATUS status;

  status = gefVmeSetDebugFlags(vmeHdl,flags);

  if(status != GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeSetDebugFlags", status);
      return ERROR;
    }

  return OK;
}

static GEF_VME_MASTER_HDL tempHdl;

static int
jlabgefOpenTmpVmeHdl(int amcode, unsigned int addr)
{
  int rval=OK;
  GEF_STATUS status;
  GEF_VME_ADDR addr_struct =
    {
      .upper = 0x00000000,
      .lower = addr,
      .addr_space = GEF_VME_ADDR_SPACE_A32,
      .addr_mode = GEF_VME_ADDR_MODE_USER | GEF_VME_ADDR_MODE_DATA,
      .transfer_mode = GEF_VME_TRANSFER_MODE_SCT,
      .transfer_max_dwidth = GEF_VME_TRANSFER_MAX_DWIDTH_32,
      .vme_2esst_rate = 0,
      .broadcast_id = 0,
      .flags = 0
    };

  switch(amcode)
    {
    case GEF_VME_ADDR_MOD_A32SP:
    case GEF_VME_ADDR_MOD_A32SD:
    case GEF_VME_ADDR_MOD_A32UP:
    case GEF_VME_ADDR_MOD_A32UD:
      {
	addr_struct.addr_space = GEF_VME_ADDR_SPACE_A32;
	break;
      }
    case GEF_VME_ADDR_MOD_A24SP:
    case GEF_VME_ADDR_MOD_A24SD:
    case GEF_VME_ADDR_MOD_A24UP:
    case GEF_VME_ADDR_MOD_A24UD:
      {
	addr_struct.addr_space = GEF_VME_ADDR_SPACE_A24;
	break;
      }

    case GEF_VME_ADDR_MOD_A16S:
    case GEF_VME_ADDR_MOD_A16U:
      {
	addr_struct.addr_space = GEF_VME_ADDR_SPACE_A16;
	break;
      }

    case GEF_VME_ADDR_MOD_CR_CSR:
      {
	addr_struct.addr_space = GEF_VME_ADDR_SPACE_CRCSR;
	break;
      }

    default:
      {
	rval = ERROR;
      }
    }

  status = gefVmeCreateMasterWindow(vmeHdl, &addr_struct, 0x10000, &tempHdl);
  if(status!=GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeCreateMasterWindow", status);
      rval = ERROR;
    }

  return rval;
}

static int
jlabgefCloseTmpVmeHdl()
{
  GEF_STATUS status;

  status = gefVmeReleaseMasterWindow(tempHdl);
  if (status != GEF_STATUS_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeReleaseMasterWindow", status);
      return ERROR;
    }

  return OK;
}

unsigned int
jlabgefVmeRead32(int amcode, unsigned int addr)
{
  unsigned int rval=0, base=0, offset=0;
  GEF_STATUS stat=0;

  base   = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_TSI;

  if(jlabgefOpenTmpVmeHdl(amcode, base)!=OK)
    {
      printf("%s: ERROR: Unable to read from register\n",
	     __func__);
      UNLOCK_TSI;
      return ERROR;
    }


  stat = gefVmeRead32(tempHdl, offset, 1, &rval);
  if(stat!=GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeRead32", stat);
      jlabgefCloseTmpVmeHdl();
      UNLOCK_TSI;
      return ERROR;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_TSI;

  return LSWAP(rval);
}

unsigned short
jlabgefVmeRead16(int amcode, unsigned int addr)
{
  unsigned short rval=0;
  unsigned int base=0, offset=0;
  GEF_STATUS stat=0;

  base   = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_TSI;

  if(jlabgefOpenTmpVmeHdl(amcode, base)!=OK)
    {
      printf("%s: ERROR: Unable to read from register\n",
	     __func__);
      UNLOCK_TSI;
      return ERROR;
    }


  stat = gefVmeRead16(tempHdl, offset, 1, &rval);
  if(stat!=GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeRead16", stat);
      UNLOCK_TSI;
      return ERROR;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_TSI;

  return SSWAP(rval);
}

unsigned char
jlabgefVmeRead8(int amcode, unsigned int addr)
{
  unsigned char rval=0;
  unsigned int base=0, offset=0;
  GEF_STATUS stat=0;

  base   = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_TSI;

  if(jlabgefOpenTmpVmeHdl(amcode, base)!=OK)
    {
      printf("%s: ERROR: Unable to read from register\n",
	     __func__);
      UNLOCK_TSI;
      return ERROR;
    }


  stat = gefVmeRead8(tempHdl, offset, 1, &rval);
  if(stat!=GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeRead8", stat);
      UNLOCK_TSI;
      return ERROR;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_TSI;

  return rval;
}

void
jlabgefVmeWrite32(int amcode, unsigned int addr, unsigned int wval)
{
  unsigned int base=0, offset=0;
  GEF_STATUS stat=0;

  base   = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_TSI;

  if(jlabgefOpenTmpVmeHdl(amcode, base)!=OK)
    {
      printf("%s: ERROR: Unable to write to register\n",
	     __func__);
      UNLOCK_TSI;
      return;
    }

  wval = LSWAP(wval);

  stat = gefVmeWrite32(tempHdl, offset, 1, &wval);
  if(stat!=GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeWrite32", stat);
      UNLOCK_TSI;
      return;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_TSI;
}

void
jlabgefVmeWrite16(int amcode, unsigned int addr, unsigned short wval)
{
  unsigned int base=0, offset=0;
  GEF_STATUS stat=0;

  base   = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_TSI;

  if(jlabgefOpenTmpVmeHdl(amcode, base)!=OK)
    {
      printf("%s: ERROR: Unable to write to register\n",
	     __func__);
      UNLOCK_TSI;
      return;
    }

  wval = SSWAP(wval);

  stat = gefVmeWrite16(tempHdl, offset, 1, &wval);
  if(stat!=GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeWrite16", stat);
      UNLOCK_TSI;
      return;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_TSI;
}

void
jlabgefVmeWrite8(int amcode, unsigned int addr, unsigned char wval)
{
  unsigned int base=0, offset=0;
  GEF_STATUS stat=0;

  base   = addr & 0xFFFF0000;
  offset = addr & 0x0000FFFF;

  LOCK_TSI;

  if(jlabgefOpenTmpVmeHdl(amcode, base)!=OK)
    {
      printf("%s: ERROR: Unable to write to register\n",
	     __func__);
      UNLOCK_TSI;
      return;
    }


  stat = gefVmeWrite8(tempHdl, offset, 1, &wval);
  if(stat!=GEF_SUCCESS)
    {
      jlabgefPrintGefError((char *)__func__, "gefVmeWrite8", stat);
      UNLOCK_TSI;
      return;
    }

  jlabgefCloseTmpVmeHdl();
  UNLOCK_TSI;
}

void
jlabgefPrintGefError(char *calling_func, char *gef_func, GEF_STATUS status)
{
  GEF_UINT32 errno;
  char message[256][GEF_STATUS_LAST] =
    {
      "Success",
      "Not Supported",
      "Out Of Memory",
      "Invalid Address",
      "Read Error",
      "Write Error",
      "Device Not Initialized",
      "No Such Device",
      "Driver Error",
      "Interrupted",
      "Timed Out",
      "Event In Use",
      "Thread Creation Failed",
      "Callback Not Attached",
      "Device In Use",
      "Operation Not Allowed",
      "Bad Parameter 1",
      "Bad Parameter 2",
      "Bad Parameter 3",
      "Bad Parameter 4",
      "Bad Parameter 5",
      "Bad Parameter 6",
      "Bad Parameter 7",
      "Bad Parameter 8",
      "Bad Parameter 9",
      "Bad Parameter 10",
      "Bad Parameter 11",
      "Invalid Error Code",
      "No Event Pending",
      "Event Release",
      "CPCI Offset Not Aligned",
    };

  errno = GEF_GET_ERROR(status);

  printf("\n%s: ERROR\n\t %s returned: %s (%d)\n\n",
	 calling_func,
	 gef_func,
	 message[errno],
	 errno);

}
