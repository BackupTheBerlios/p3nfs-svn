/*
  Twoway conversion between ISO8859-1 and cp850
  Only the upper 128 characters are affected
  Rudolf Koenig
 */

unsigned char tbl_iso2cp[128] = 
{
  0x9f, 0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb9, 0xba,
  0xbb, 0xbc, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4,
  0xc5, 0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce,
  0xd5, 0xd9, 0xda, 0xdb, 0xdc, 0xdf, 0xf2, 0xfe,
  0xff, 0xad, 0xbd, 0x9c, 0xcf, 0xbe, 0xdd, 0xf5,
  0xf9, 0xb8, 0xa6, 0xae, 0xaa, 0xf0, 0xa9, 0xee,
  0xf8, 0xf1, 0xfd, 0xfc, 0xef, 0xe6, 0xf4, 0xfa,
  0xf7, 0xfb, 0xa7, 0xaf, 0xac, 0xab, 0xf3, 0xa8,
  0xb7, 0xb5, 0xb6, 0xc7, 0x8e, 0x8f, 0x92, 0x80,
  0xd4, 0x90, 0xd2, 0xd3, 0xde, 0xd6, 0xd7, 0xd8,
  0xd1, 0xa5, 0xe3, 0xe0, 0xe2, 0xe5, 0x99, 0x9e,
  0x9d, 0xeb, 0xe9, 0xea, 0x9a, 0xed, 0xe7, 0xe1,
  0x85, 0xa0, 0x83, 0xc6, 0x84, 0x86, 0x91, 0x87,
  0x8a, 0x82, 0x88, 0x89, 0x8d, 0xa1, 0x8c, 0x8b,
  0xd0, 0xa4, 0x95, 0xa2, 0x93, 0xe4, 0x94, 0xf6,
  0x9b, 0x97, 0xa3, 0x96, 0x81, 0xec, 0xe8, 0x98,
};

unsigned char tbl_cp2iso[128] = 
{
  0xc7, 0xfc, 0xe9, 0xe2, 0xe4, 0xe0, 0xe5, 0xe7,
  0xea, 0xeb, 0xe8, 0xef, 0xee, 0xec, 0xc4, 0xc5,
  0xc9, 0xe6, 0xc6, 0xf4, 0xf6, 0xf2, 0xfb, 0xf9,
  0xff, 0xd6, 0xdc, 0xf8, 0xa3, 0xd8, 0xd7, 0x80,
  0xe1, 0xed, 0xf3, 0xfa, 0xf1, 0xd1, 0xaa, 0xba,
  0xbf, 0xae, 0xac, 0xbd, 0xbc, 0xa1, 0xab, 0xbb,
  0x81, 0x82, 0x83, 0x84, 0x85, 0xc1, 0xc2, 0xc0,
  0xa9, 0x86, 0x87, 0x88, 0x89, 0xa2, 0xa5, 0x8a,
  0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0xe3, 0xc3,
  0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0xa4,
  0xf0, 0xd0, 0xca, 0xcb, 0xc8, 0x98, 0xcd, 0xce,
  0xcf, 0x99, 0x9a, 0x9b, 0x9c, 0xa6, 0xcc, 0x9d,
  0xd3, 0xdf, 0xd4, 0xd2, 0xf5, 0xd5, 0xb5, 0xde,
  0xfe, 0xda, 0xdb, 0xd9, 0xfd, 0xdd, 0xaf, 0xb4,
  0xad, 0xb1, 0x9e, 0xbe, 0xb6, 0xa7, 0xf7, 0xb8,
  0xb0, 0xa8, 0xb7, 0xb9, 0xb3, 0xb2, 0x9f, 0xa0
};