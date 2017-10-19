#ifndef CRC16_h
#define CRC16_h

#include <Arduino.h>
#include <inttypes.h>

class CRC16
{
private:
  unsigned long  widmask();
public:
  CRC16();
  byte   		cm_width;   /* Parameter: Width in bits [8,32].       */
  unsigned long cm_poly;    /* Parameter: The algorithm's polynomial. */
  unsigned long cm_init;    /* Parameter: Initial register value.     */
  byte 			cm_refin;   /* Parameter: Reflect input bytes?        */
  byte  		cm_refot;   /* Parameter: Reflect output CRC?         */
  unsigned long cm_xorot;   /* Parameter: XOR this to output CRC.     */
  unsigned long cm_reg;     /* Context: Context during execution.     */

  static unsigned long reflect(unsigned long v, byte  b);
  void ini();
  void nxt(byte  ch);
  void blk(byte *blk_adr, int blk_len);
  void appnd(byte *blk_adr, int blk_len);
  bool chk(byte *blk_adr, int blk_len);
  unsigned long  crc();
};

#endif
