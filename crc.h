/* CRC_TSIZE is the size of the CRC lookup table used by the CRC32 algorithms. */
#define	CRC_TSIZE 256

/* These are the polynomials used by the CRC32 algorithms. */
#define	CRC32_POLYNOMIAL 0xEDB88320UL
#define	CRC32_INIT       0xFFFFFFFFUL

/* Function prototypes for CRC functions */
u32 CalculateCRC32Checksum(unsigned char *dwStart, u32 dwSize);

