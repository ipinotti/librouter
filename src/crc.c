/* Function		
 * ========
 * This module calculates a checksum on a region using the CRC32
 * polynomial and returns the result to
 * the caller.  If the CRC is calculated correctly, the returned value
 * will be zero.
 */

#include "typedefs.h"
#include "crc.h" /* Checksum and CRC definitions */

/*
 * Function:	CalculateCRC32Checksum
 *
 * This function calculates a 32-bit CRC for the supplied range, returning
 * the checksum (which should be zero).  The core calculation executes the
 * following C code to calculate the CRC:
 *
 * while (wSize--)
 * {
 *   wIndex = (UCHAR) (*pByte++ ^ dwCRC) ;
 *   dwCRC >>= 8 ;
 *   dwCRC ^= dwCRCTable[wIndex] ;
 * }
 */

u32 CalculateCRC32Checksum(unsigned char *dwStart, /* Start of region to calculate CRC */
                           u32 dwSize)	           /* Size of region to calculate CRC */
{
  u32 dwCRCTable[CRC_TSIZE] ;  /* CRC32 lookup table */
  u16 wIndex ;	               /* Index into the CRC table */
  unsigned char *pByte;        /* Pointer into the CRC buffer */
  u32 wSize;                   /* Size of the buffer being calculated */
  u32 dwCRC;                   /* Calculated CRC */


  /* First build the CRC32 lookup table */
  for (wIndex = 0; wIndex < CRC_TSIZE; wIndex++)   
  {
    dwCRC = wIndex ;
    for (wSize = 0; wSize < 8; wSize++)   
    {
      if (dwCRC & 1)   
      {
        dwCRC >>= 1 ;
        dwCRC ^= CRC32_POLYNOMIAL ;
      }
      else  dwCRC >>= 1 ;
    }

    /* Save the table entry */
    dwCRCTable[wIndex] = dwCRC ;
  }

  /* Pass thru the buffer and add the new data to the checksum */
  dwCRC = CRC32_INIT;

  /* Build a pointer to the start of the next calculation */
  pByte = dwStart;

  /* Compute the size of the buffer */
  wSize = dwSize;

  /* Calculate the CRC on the region */
  while (wSize--)   
  {
    wIndex = (unsigned char) (*pByte++ ^ dwCRC) ;
    dwCRC >>= 8 ;
    dwCRC ^= dwCRCTable[wIndex] ;
  }
  /* Return the computed CRC */
  return dwCRC ;
}
