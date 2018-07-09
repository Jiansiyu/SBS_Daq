/*----------------------------------------------------------------------------*
 *  Copyright (c) 2009        Southeastern Universities Research Association, *
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
 *     Header for JLab DMA routines to compliment the GEFanuc API
 *
 *----------------------------------------------------------------------------*/

#ifndef __JLABGEFDMA__
#define __JLABGEFDMA__

int  jlabgefDmaConfig(unsigned int addrType, unsigned int dataType, unsigned int sstMode);
int  jlabgefDmaSend(unsigned long locAdrs, unsigned int vmeAdrs, int size);
int  jlabgefDmaSendPhys(unsigned long physAdrs, unsigned int vmeAdrs, int size);

int  jlabgefGetBerrStatus();
int  jlabgefDmaDone();
int  jlabgefDmaAllocLLBuffer();
int  jlabgefDmaFreeLLBuffer();
int  jlabgefDmaSetupLL(unsigned long locAddrBase,unsigned int *vmeAddr,
		       unsigned int *dmaSize,unsigned int numt);
int  jlabgefDmaSendLL();
unsigned long jlabgefDmaLocalToPhysAdrs(unsigned long locAdrs);
unsigned int jlabgefDmaLocalToVmeAdrs(unsigned long locAdrs);

int  jlabgefAllocDMATrash(int size);
int  jlabgefFreeDMATrash();
int  jlabgefDmaFlush(unsigned int vmeaddr);

void jlabgefReadDMARegs();

#endif /* __JLABGEFDMA__ */
