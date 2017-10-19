#include "CRC16.h"
#define BITMASK(X) (1L << (X))
#define MASK32 0xFFFFFFFFL

CRC16::CRC16()
{
	cm_poly  = 0x1021;	// this is the CCITT16 Poly
	cm_width = 16;
	cm_init  = 0xFFFF;
	cm_refin = true;
	cm_refot = true;
	cm_xorot = 0xFFFFL;
}
/******************************************************************************/
unsigned long CRC16::reflect (unsigned long v, byte  b)
/* Returns the value v with the bottom b [0,32] bits reflected. */
/* Example: reflect(0x3e23L,3) == 0x3e26                        */
{
 byte   i;
 unsigned long  t = v;
 for (i=0; i<b; i++)
 {
  if (t & 1L)
     v |=  BITMASK((b-1)-i);
  else
     v &= ~BITMASK((b-1)-i);
  t >>=1;
 }
 return v;
}

/******************************************************************************/
unsigned long  CRC16::widmask ()
/* Returns a longword whose value is (2^p_cm->cm_width)-1.     */
/* The trick is to do this portably (e.g. without doing <<32). */
{
 return (((1L<<(cm_width-1))-1L)<<1)|1L;
}

/******************************************************************************/

void CRC16::ini ()
{
	cm_reg = cm_init;
}

/******************************************************************************/

void CRC16::nxt (byte  ch)
{
 byte   i;
 unsigned long  uch  = (unsigned long ) ch;
 unsigned long  topbit = BITMASK(cm_width-1);

 if (cm_refin) uch = reflect(uch, 8);
 cm_reg ^= (uch << (cm_width-8));
 for (i=0; i<8; i++)
 {
  if (cm_reg & topbit)
     cm_reg = (cm_reg << 1) ^ cm_poly;
  else
     cm_reg <<= 1;
  cm_reg &= widmask();
 }
}

/******************************************************************************/

void CRC16::blk (byte *blk_adr, int blk_len)
{
  while (blk_len--) nxt(*blk_adr++);
}

void CRC16::appnd(byte *p_buffer, int blk_len)
{
  int i = 0;
  int	last_idx = blk_len - 2;
  ini();

  for(int i = 0; i < last_idx; i++)
  {
    nxt(p_buffer[i]);
  }

  int res = crc() & widmask();
  p_buffer[last_idx++] = res & 0x00FF;
  p_buffer[last_idx] = (res >> 8) & 0x00FF;
}

bool CRC16::chk(byte *p_buffer, int blk_len)
{
  int i = 0;
  int	last_idx = blk_len - 2;
  ini();

  for(int i = 0; i < last_idx; i++)
  {
    nxt(p_buffer[i]);
  }

  int res =  crc();
  byte  b1 = res;
  byte b2 = (res >> 8); 
  return  ((last_idx >= 2 ) && (p_buffer[last_idx] == b1) && (p_buffer[last_idx + 1] == b2));
}


/******************************************************************************/

unsigned long  CRC16::crc ()
{
 if (cm_refot)
    return (cm_xorot ^ reflect(cm_reg, cm_width)) & widmask();
 else
    return (cm_xorot ^ cm_reg) & widmask();
}
