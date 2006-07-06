int
docrc16_1(crc, c)
  int crc;
  unsigned char c;
{
  int acc, i;

  acc = crc;
  acc ^= c << 8;
  for(i = 0; i < 8; i++)
    if(acc & 0x8000)
      {
	acc <<= 1;
	acc ^= 0x1021;
      }
    else
      {
	acc <<= 1;
      }
  return acc & 0xffff;
}

int
docrc16(ptr, len)
  unsigned char *ptr;
  int len;
{
  int acc, i;

  acc = 0;
  while(len--)
    {
      acc ^= *ptr++ << 8;
      for(i = 0; i < 8; i++)
        if(acc & 0x8000)
	  {
	    acc <<= 1;
	    acc ^= 0x1021;
	  }
	else
	  {
	    acc <<= 1;
	  }
    }
  return acc & 0xffff;
}
