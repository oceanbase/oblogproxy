/**
 * Copyright (c) 2025 OceanBase
 * OceanBase Migration Service LogProxy is licensed under Mulan PubL v2.
 * You can use this software according to the terms and conditions of the Mulan PubL v2.
 * You may obtain a copy of Mulan PubL v2 at:
 *          http://license.coscl.org.cn/MulanPubL-2.0
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
 * EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
 * MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PubL v2 for more details.
 */
#include "binlog_dtoa.h"
#include "float.h"
#include <cassert>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace oceanbase::binlog {

#define BINLOG_ALIGN(A,L)	(((A) + (L) - 1) & ~((L) - 1))
#define SIZEOF_CHARP  8
#define ALIGN_SIZE(A)	MY_ALIGN((A),sizeof(double))

#ifndef TRUE
#define TRUE		(1)	
#define FALSE		(0)	
#endif

#define BINLOG_MAX(a, b)    ((a) > (b) ? (a) : (b))
#define DTOA_OVERFLOW 9999
#define DTOA_BUF_MAX_SIZE (460 * sizeof(void *))
#define Scale_Bit 0x10
#define n_bigtens 5

typedef int32_t Long;
typedef uint32_t ULong;
typedef int64_t LLong;
typedef uint64_t ULLong;

typedef union { double d; ULong L[2]; } U;

#if defined(WORDS_BIGENDIAN) || (defined(__FLOAT_WORD_ORDER) &&        \
                                 (__FLOAT_WORD_ORDER == __BIG_ENDIAN))
#define word0(x) (x)->L[0]
#define word1(x) (x)->L[1]
#else
#define word0(x) (x)->L[1]
#define word1(x) (x)->L[0]
#endif

#define dval(x) (x)->d

#define Exp_shift  20
#define Exp_shift1 20
#define Exp_msk1    0x100000
#define Exp_mask  0x7ff00000
#define P 53
#define Bias 1023
#define Emin (-1022)
#define Exp_1  0x3ff00000
#define Exp_11 0x3ff00000
#define Ebits 11
#define Frac_mask  0xfffff
#define Frac_mask1 0xfffff
#define Ten_pmax 22
#define Bletch 0x10
#define Bndry_mask  0xfffff
#define Bndry_mask1 0xfffff
#define LSB 1
#define Sign_bit 0x80000000
#define Log2P 1
#define Tiny1 1
#define Quick_max 14
#define Int_max 14

#ifndef Flt_Rounds
#ifdef FLT_ROUNDS
#define Flt_Rounds FLT_ROUNDS
#else
#define Flt_Rounds 1
#endif
#endif 

#ifdef Honor_FLT_ROUNDS
#define Rounding rounding
#undef Check_FLT_ROUNDS
#define Check_FLT_ROUNDS
#else
#define Rounding Flt_Rounds
#endif

#define rounded_product(a,b) a*= b
#define rounded_quotient(a,b) a/= b

#define Big0 (Frac_mask1 | Exp_msk1*(DBL_MAX_EXP+Bias-1))
#define Big1 0xffffffff
#define FFFFFFFF 0xffffffffUL


#define Kmax 15

#define copy_bigint(x,y) memcpy((char *)&x->sign, (char *)&y->sign,   \
                          2*sizeof(int) + y->wds*sizeof(ULong))


typedef struct _bigint
{
  union {
    ULong *x;              
    struct _bigint *next;   
  } p;
  int k;                   
  int maxwds;             
  int sign;               
  int wds;                
} Bigint;

typedef struct BinlogStackAllocator
{
  char *begin;
  char *free;
  char *end;
  Bigint *freelist[Kmax+1];
} BinlogStackAllocator;


static Bigint *alloc_bigint(int k, BinlogStackAllocator *alloc)
{
  Bigint *rv;
  assert(k <= Kmax);
  if (k <= Kmax &&  alloc->freelist[k]) {
    rv = alloc->freelist[k];
    alloc->freelist[k] = rv->p.next;
  } else {
    int x, len;
    x = 1 << k;
    len = BINLOG_ALIGN(sizeof(Bigint) + x * sizeof(ULong), SIZEOF_CHARP);
    if (alloc->free + len <= alloc->end) {
      rv = (Bigint*) alloc->free;
      alloc->free += len;
    } else {
      rv = (Bigint*) malloc(len);
    }
    rv->k = k;
    rv->maxwds = x;
  }
  rv->sign = rv->wds = 0;
  rv->p.x = (ULong*) (rv + 1);
  return rv;
}

static void free_bigint(Bigint *v, BinlogStackAllocator *alloc)
{
  if (v != NULL) {
    char *gptr= (char*) v;                       
    if (gptr < alloc->begin || gptr >= alloc->end) {
      free(gptr);
    } else if (v->k <= Kmax) {
      
      v->p.next= alloc->freelist[v->k];
      alloc->freelist[v->k]= v;
    }
  }
}

static Bigint *mult_and_add(Bigint *b, int m, int a, BinlogStackAllocator *alloc)
{
  int i, wds;
  ULong *x;
  ULLong carry, y;
  Bigint *b1;

  wds= b->wds;
  x= b->p.x;
  i= 0;
  carry= a;
  do
  {
    y= *x * (ULLong)m + carry;
    carry= y >> 32;
    *x++= (ULong)(y & FFFFFFFF);
  }
  while (++i < wds);
  if (carry)
  {
    if (wds >= b->maxwds)
    {
      b1= alloc_bigint(b->k+1, alloc);
      copy_bigint(b1, b);
      free_bigint(b, alloc);
      b= b1;
    }
    b->p.x[wds++]= (ULong) carry;
    b->wds= wds;
  }
  return b;
}

static Bigint *string2bigint(const char *s, int nd0, int nd, ULong y9, BinlogStackAllocator *alloc)
{
  Bigint *b;
  int i, k;
  Long x, y;

  x= (nd + 8) / 9;
  for (k= 0, y= 1; x > y; y <<= 1, k++) ;
  b= alloc_bigint(k, alloc);
  b->p.x[0]= y9;
  b->wds= 1;

  i= 9;
  if (9 < nd0)
  {
    s+= 9;
    do
      b= mult_and_add(b, 10, *s++ - '0', alloc);
    while (++i < nd0);
    s++;                                        
  }
  else
    s+= 10;
  for(; i < nd; i++)
    b= mult_and_add(b, 10, *s++ - '0', alloc);
  return b;
}

static int hi0bits(register ULong x)
{
  register int k= 0;

  if (!(x & 0xffff0000))
  {
    k= 16;
    x<<= 16;
  }
  if (!(x & 0xff000000))
  {
    k+= 8;
    x<<= 8;
  }
  if (!(x & 0xf0000000))
  {
    k+= 4;
    x<<= 4;
  }
  if (!(x & 0xc0000000))
  {
    k+= 2;
    x<<= 2;
  }
  if (!(x & 0x80000000))
  {
    k++;
    if (!(x & 0x40000000))
      return 32;
  }
  return k;
}

static int lo0bits(ULong *y)
{
  register int k;
  register ULong x= *y;

  if (x & 7)
  {
    if (x & 1)
      return 0;
    if (x & 2)
    {
      *y= x >> 1;
      return 1;
    }
    *y= x >> 2;
    return 2;
  }
  k= 0;
  if (!(x & 0xffff))
  {
    k= 16;
    x>>= 16;
  }
  if (!(x & 0xff))
  {
    k+= 8;
    x>>= 8;
  }
  if (!(x & 0xf))
  {
    k+= 4;
    x>>= 4;
  }
  if (!(x & 0x3))
  {
    k+= 2;
    x>>= 2;
  }
  if (!(x & 1))
  {
    k++;
    x>>= 1;
    if (!x)
      return 32;
  }
  *y= x;
  return k;
}

static Bigint *integer2bigint(int i, BinlogStackAllocator *alloc)
{
  Bigint *b;
  b= alloc_bigint(1, alloc);
  b->p.x[0]= i;
  b->wds= 1;
  return b;
}

static Bigint *bigint_mul_bigint(Bigint *a, Bigint *b, BinlogStackAllocator *alloc)
{
  Bigint *c;
  int k, wa, wb, wc;
  ULong *x, *xa, *xae, *xb, *xbe, *xc, *xc0;
  ULong y;
  ULLong carry, z;

  if (a->wds < b->wds)
  {
    c= a;
    a= b;
    b= c;
  }
  k= a->k;
  wa= a->wds;
  wb= b->wds;
  wc= wa + wb;
  if (wc > a->maxwds)
    k++;
  c= alloc_bigint(k, alloc);
  for (x= c->p.x, xa= x + wc; x < xa; x++)
    *x= 0;
  xa= a->p.x;
  xae= xa + wa;
  xb= b->p.x;
  xbe= xb + wb;
  xc0= c->p.x;
  for (; xb < xbe; xc0++)
  {
    if ((y= *xb++))
    {
      x= xa;
      xc= xc0;
      carry= 0;
      do
      {
        z= *x++ * (ULLong)y + *xc + carry;
        carry= z >> 32;
        *xc++= (ULong) (z & FFFFFFFF);
      }
      while (x < xae);
      *xc= (ULong) carry;
    }
  }
  for (xc0= c->p.x, xc= xc0 + wc; wc > 0 && !*--xc; --wc) ;
  c->wds= wc;
  return c;
}

static ULong powers5[]=
{
  625UL,

  390625UL,

  2264035265UL, 35UL,

  2242703233UL, 762134875UL,  1262UL,

  3211403009UL, 1849224548UL, 3668416493UL, 3913284084UL, 1593091UL,

  781532673UL,  64985353UL,   253049085UL,  594863151UL,  3553621484UL,
  3288652808UL, 3167596762UL, 2788392729UL, 3911132675UL, 590UL,

  2553183233UL, 3201533787UL, 3638140786UL, 303378311UL, 1809731782UL,
  3477761648UL, 3583367183UL, 649228654UL, 2915460784UL, 487929380UL,
  1011012442UL, 1677677582UL, 3428152256UL, 1710878487UL, 1438394610UL,
  2161952759UL, 4100910556UL, 1608314830UL, 349175UL
};

static Bigint p5_a[]=
{
  { { powers5 }, 1, 1, 0, 1 },
  { { powers5 + 1 }, 1, 1, 0, 1 },
  { { powers5 + 2 }, 1, 2, 0, 2 },
  { { powers5 + 4 }, 2, 3, 0, 3 },
  { { powers5 + 7 }, 3, 5, 0, 5 },
  { { powers5 + 12 }, 4, 10, 0, 10 },
  { { powers5 + 22 }, 5, 19, 0, 19 }
};

#define P5A_MAX (sizeof(p5_a)/sizeof(*p5_a) - 1)

static Bigint *pow5mult(Bigint *b, int k, BinlogStackAllocator *alloc)
{
  Bigint *b1, *p5, *p51=NULL;
  int i;
  static int p05[3]= { 5, 25, 125 };
  bool overflow= FALSE;

  if ((i= k & 3))
    b= mult_and_add(b, p05[i-1], 0, alloc);

  if (!(k>>= 2))
    return b;
  p5= p5_a;
  for (;;)
  {
    if (k & 1)
    {
      b1= bigint_mul_bigint(b, p5, alloc);
      free_bigint(b, alloc);
      b= b1;
    }
    if (!(k>>= 1))
      break;
    if (overflow)
    {
      p51= bigint_mul_bigint(p5, p5, alloc);
      free_bigint(p5, alloc);
      p5= p51;
    }
    else if (p5 < p5_a + P5A_MAX)
      ++p5;
    else if (p5 == p5_a + P5A_MAX)
    {
      p5= bigint_mul_bigint(p5, p5, alloc);
      overflow= TRUE;
    }
  }
  if (p51)
    free_bigint(p51, alloc);
  return b;
}

static Bigint *left_shift(Bigint *b, int k, BinlogStackAllocator *alloc)
{
  int i, k1, n, n1;
  Bigint *b1;
  ULong *x, *x1, *xe, z;

  n= k >> 5;
  k1= b->k;
  n1= n + b->wds + 1;
  for (i= b->maxwds; n1 > i; i<<= 1)
    k1++;
  b1= alloc_bigint(k1, alloc);
  x1= b1->p.x;
  for (i= 0; i < n; i++)
    *x1++= 0;
  x= b->p.x;
  xe= x + b->wds;
  if (k&= 0x1f)
  {
    k1= 32 - k;
    z= 0;
    do
    {
      *x1++= *x << k | z;
      z= *x++ >> k1;
    }
    while (x < xe);
    if ((*x1= z))
      ++n1;
  }
  else
    do
      *x1++= *x++;
    while (x < xe);
  b1->wds= n1 - 1;
  free_bigint(b, alloc);
  return b1;
}

static int bigint_cmp(Bigint *a, Bigint *b)
{
  ULong *xa, *xa0, *xb, *xb0;
  int i, j;

  i= a->wds;
  j= b->wds;
  if (i-= j)
    return i;
  xa0= a->p.x;
  xa= xa0 + j;
  xb0= b->p.x;
  xb= xb0 + j;
  for (;;)
  {
    if (*--xa != *--xb)
      return *xa < *xb ? -1 : 1;
    if (xa <= xa0)
      break;
  }
  return 0;
}

static Bigint *bigint_diff(Bigint *a, Bigint *b, BinlogStackAllocator *alloc)
{
  Bigint *c;
  int i, wa, wb;
  ULong *xa, *xae, *xb, *xbe, *xc;
  ULLong borrow, y;

  i= bigint_cmp(a,b);
  if (!i)
  {
    c= alloc_bigint(0, alloc);
    c->wds= 1;
    c->p.x[0]= 0;
    return c;
  }
  if (i < 0)
  {
    c= a;
    a= b;
    b= c;
    i= 1;
  }
  else
    i= 0;
  c= alloc_bigint(a->k, alloc);
  c->sign= i;
  wa= a->wds;
  xa= a->p.x;
  xae= xa + wa;
  wb= b->wds;
  xb= b->p.x;
  xbe= xb + wb;
  xc= c->p.x;
  borrow= 0;
  do
  {
    y= (ULLong)*xa++ - *xb++ - borrow;
    borrow= y >> 32 & (ULong)1;
    *xc++= (ULong) (y & FFFFFFFF);
  }
  while (xb < xbe);
  while (xa < xae)
  {
    y= *xa++ - borrow;
    borrow= y >> 32 & (ULong)1;
    *xc++= (ULong) (y & FFFFFFFF);
  }
  while (!*--xc)
    wa--;
  c->wds= wa;
  return c;
}

static double ulp(U *x)
{
  register Long L;
  U u;

  L= (word0(x) & Exp_mask) - (P - 1)*Exp_msk1;
  word0(&u) = L;
  word1(&u) = 0;
  return dval(&u);
}

static double b2d(Bigint *a, int *e)
{
  ULong *xa, *xa0, w, y, z;
  int k;
  U d;
#define d0 word0(&d)
#define d1 word1(&d)

  xa0= a->p.x;
  xa= xa0 + a->wds;
  y= *--xa;
  k= hi0bits(y);
  *e= 32 - k;
  if (k < Ebits)
  {
    d0= Exp_1 | y >> (Ebits - k);
    w= xa > xa0 ? *--xa : 0;
    d1= y << ((32-Ebits) + k) | w >> (Ebits - k);
    goto ret_d;
  }
  z= xa > xa0 ? *--xa : 0;
  if (k-= Ebits)
  {
    d0= Exp_1 | y << k | z >> (32 - k);
    y= xa > xa0 ? *--xa : 0;
    d1= z << k | y >> (32 - k);
  }
  else
  {
    d0= Exp_1 | y;
    d1= z;
  }
 ret_d:
#undef d0
#undef d1
  return dval(&d);
}

static Bigint *d2b(U *d, int *e, int *bits, BinlogStackAllocator *alloc)
{
  Bigint *b;
  int de, k;
  ULong *x, y, z;
  int i;
#define d0 word0(d)
#define d1 word1(d)

  b= alloc_bigint(1, alloc);
  x= b->p.x;

  z= d0 & Frac_mask;
  d0 &= 0x7fffffff;      
  if ((de= (int)(d0 >> Exp_shift)))
    z|= Exp_msk1;
  if ((y= d1))
  {
    if ((k= lo0bits(&y)))
    {
      x[0]= y | z << (32 - k);
      if (k >= 0 && k <32) {
        z >>= k;
      } else {
      }
    }
    else
      x[0]= y;
    i= b->wds= (x[1]= z) ? 2 : 1;
  }
  else
  {
    k= lo0bits(&z);
    x[0]= z;
    i= b->wds= 1;
    k+= 32;
  }
  if (de)
  {
    *e= de - Bias - (P-1) + k;
    *bits= P - k;
  }
  else
  {
    *e= de - Bias - (P-1) + 1 + k;
    *bits= 32*i - hi0bits(x[i-1]);
  }
  return b;
#undef d0
#undef d1
}

static double ratio(Bigint *a, Bigint *b)
{
  U da, db;
  int k, ka, kb;

  dval(&da)= b2d(a, &ka);
  dval(&db)= b2d(b, &kb);
  k= ka - kb + 32*(a->wds - b->wds);
  if (k > 0)
    word0(&da)+= k*Exp_msk1;
  else
  {
    k= -k;
    word0(&db)+= k*Exp_msk1;
  }
  return dval(&da) / dval(&db);
}

static const double tens[] =
{
  1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9,
  1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
  1e20, 1e21, 1e22
};

static const double bigtens[]= { 1e16, 1e32, 1e64, 1e128, 1e256 };
static const double tinytens[]=
{ 1e-16, 1e-32, 1e-64, 1e-128,
  9007199254740992.*9007199254740992.e-256 
};

static double binlog_strtod_int(const char *s00, char **se, int *error, char *buf, size_t buf_size)
{
  int scale;
  int bb2, bb5, bbe, bd2, bd5, bbbits, bs2, c = 0, dsign,
     e, e1, esign, i, j, k, nd, nd0, nf, nz, nz0, sign;
  const char *s, *s0, *s1, *end = *se;
  double aadj, aadj1;
  U aadj2, adj, rv, rv0;
  Long L;
  ULong y, z;
  Bigint *bb, *bb1, *bd, *bd0, *bs, *delta;
#ifdef SET_INEXACT
  int inexact, oldinexact;
#endif
#ifdef Honor_FLT_ROUNDS
  int rounding;
#endif
  BinlogStackAllocator alloc;

  *error= 0;

  alloc.begin= alloc.free= buf;
  alloc.end= buf + buf_size;
  memset(alloc.freelist, 0, sizeof(alloc.freelist));

  sign= nz0= nz= 0;
  dval(&rv)= 0.;
  for (s= s00; s < end; s++)
    switch (*s) {
    case '-':
      sign= 1;
    case '+':
      s++;
      goto break2;
    case '\t':
    case '\n':
    case '\v':
    case '\f':
    case '\r':
    case ' ':
      continue;
    default:
      goto break2;
    }
 break2:
  if (s >= end)
    goto ret0;

  if (*s == '0')
  {
    nz0= 1;
    while (++s < end && *s == '0') ;
    if (s >= end)
      goto ret;
  }
  s0= s;
  y= z= 0;
  for (nd= nf= 0; s < end && (c= *s) >= '0' && c <= '9'; nd++, s++)
    if (nd < 9)
      y= 10*y + c - '0';
    else if (nd < 16)
      z= 10*z + c - '0';
  nd0= nd;
  if (s < end && c == '.')
  {
    s++;
    if(s < end) {
      c = *s;
    }
    if (!nd)
    {
      for (; s < end; ++s)
      {
        c= *s;
        if (c != '0')
          break;
        nz++;
      }
      if (s < end && c > '0' && c <= '9')
      {
        s0= s;
        nf+= nz;
        nz= 0;
      }
      else
        goto dig_done;
    }
    for (; s < end; ++s)
    {
      c= *s;
      if (c < '0' || c > '9')
        break;
      if (nd < 2 * DBL_DIG)
      {
        nz++;
        if (c-= '0')
        {
          nf+= nz;
          for (i= 1; i < nz; i++)
            if (nd++ < 9)
              y*= 10;
            else if (nd <= DBL_DIG + 1)
              z*= 10;
          if (nd++ < 9)
            y= 10*y + c;
          else if (nd <= DBL_DIG + 1)
            z= 10*z + c;
          nz= 0;
        }
      }
    }
  }
 dig_done:
  e= 0;
  if (s < end && (c == 'e' || c == 'E'))
  {
    if (!nd && !nz && !nz0)
      goto ret0;
    s00= s;
    esign= 0;
    if (++s < end)
      switch (c= *s) {
      case '-':
        esign= 1;
        if (++s < end)
          c = *s;
        break;
      case '+':
        if (++s < end)
          c = *s;
        break;
      }
    if (s < end && c >= '0' && c <= '9')
    {
      while (s < end && c == '0') {
        s++;
        if(s < end)
          c= *s;
      }
      if (s < end && c > '0' && c <= '9') {
        L= c - '0';
        s1= s;
        while (++s < end && (c= *s) >= '0' && c <= '9')
          L= 10*L + c - '0';
        if (s - s1 > 8 || L > 19999)
          e= 19999; 
        else
          e= (int)L;
        if (esign)
          e= -e;
      }
      else
        e= 0;
    }
    else
      s= s00;
  }
  if (!nd)
  {
    if (!nz && !nz0)
    {
 ret0:
      s= s00;
      sign= 0;
    }
    goto ret;
  }
  e1= e -= nf;
  if (!nd0)
    nd0= nd;
  k= nd < DBL_DIG + 1 ? nd : DBL_DIG + 1;
  dval(&rv)= y;
  if (k > 9)
  {
#ifdef SET_INEXACT
    if (k > DBL_DIG)
      oldinexact = get_inexact();
#endif
    dval(&rv)= tens[k - 9] * dval(&rv) + z;
  }
  bd0= 0;
  if (nd <= DBL_DIG
#ifndef Honor_FLT_ROUNDS
    && Flt_Rounds == 1
#endif
      )
  {
    if (!e)
      goto ret;
    if (e > 0)
    {
      if (e <= Ten_pmax)
      {
#ifdef Honor_FLT_ROUNDS
        if (sign)
        {
          rv.d= -rv.d;
          sign= 0;
        }
#endif
        rounded_product(dval(&rv), tens[e]);
        goto ret;
      }
      i= DBL_DIG - nd;
      if (e <= Ten_pmax + i)
      {
#ifdef Honor_FLT_ROUNDS
        if (sign)
        {
          rv.d= -rv.d;
          sign= 0;
        }
#endif
        e-= i;
        dval(&rv)*= tens[i];
        rounded_product(dval(&rv), tens[e]);
        goto ret;
      }
    }
#ifndef Inaccurate_Divide
    else if (e >= -Ten_pmax)
    {
#ifdef Honor_FLT_ROUNDS
      if (sign)
      {
        rv.d= -rv.d;
        sign= 0;
      }
#endif
      rounded_quotient(dval(&rv), tens[-e]);
      goto ret;
    }
#endif
  }
  e1+= nd - k;

#ifdef SET_INEXACT
  inexact= 1;
  if (k <= DBL_DIG)
    oldinexact= get_inexact();
#endif
  scale= 0;
#ifdef Honor_FLT_ROUNDS
  if ((rounding= Flt_Rounds) >= 2)
  {
    if (sign)
      rounding= rounding == 2 ? 0 : 2;
    else
      if (rounding != 2)
        rounding= 0;
  }
#endif


  if (e1 > 0)
  {
    if ((i= e1 & 15))
      dval(&rv)*= tens[i];
    if (e1&= ~15)
    {
      if (e1 > DBL_MAX_10_EXP)
      {
 ovfl:
        *error= EOVERFLOW;
        
#ifdef Honor_FLT_ROUNDS
        switch (rounding)
        {
        case 0:
        case 3:
          word0(&rv)= Big0;
          word1(&rv)= Big1;
          break;
        default:
          word0(&rv)= Exp_mask;
          word1(&rv)= 0;
        }
#else 
        word0(&rv)= Exp_mask;
        word1(&rv)= 0;
#endif 
#ifdef SET_INEXACT
        dval(&rv0)= 1e300;
        dval(&rv0)*= dval(&rv0);
#endif
        if (bd0)
          goto retfree;
        goto ret;
      }
      e1>>= 4;
      for(j= 0; e1 > 1; j++, e1>>= 1)
        if (e1 & 1)
          dval(&rv)*= bigtens[j];
      word0(&rv)-= P*Exp_msk1;
      dval(&rv)*= bigtens[j];
      if ((z= word0(&rv) & Exp_mask) > Exp_msk1 * (DBL_MAX_EXP + Bias - P))
        goto ovfl;
      if (z > Exp_msk1 * (DBL_MAX_EXP + Bias - 1 - P))
      {
        word0(&rv)= Big0;
        word1(&rv)= Big1;
      }
      else
        word0(&rv)+= P*Exp_msk1;
    }
  }
  else if (e1 < 0)
  {
    e1= -e1;
    if ((i= e1 & 15))
      dval(&rv)/= tens[i];
    if ((e1>>= 4))
    {
      if (e1 >= 1 << n_bigtens)
        goto undfl;
      if (e1 & Scale_Bit)
        scale= 2 * P;
      for(j= 0; e1 > 0; j++, e1>>= 1)
        if (e1 & 1)
          dval(&rv)*= tinytens[j];
      if (scale && (j = 2 * P + 1 - ((word0(&rv) & Exp_mask) >> Exp_shift)) > 0)
      {
        if (j >= 32)
        {
          word1(&rv)= 0;
          if (j >= 53)
            word0(&rv)= (P + 2) * Exp_msk1;
          else
            word0(&rv)&= 0xffffffff << (j - 32);
        }
        else
          word1(&rv)&= 0xffffffff << j;
      }
      if (!dval(&rv))
      {
 undfl:
          dval(&rv)= 0.;
          if (bd0)
            goto retfree;
          goto ret;
      }
    }
  }

  bd0= string2bigint(s0, nd0, nd, y, &alloc);

  for(;;)
  {
    bd= alloc_bigint(bd0->k, &alloc);
    copy_bigint(bd, bd0);
    bb= d2b(&rv, &bbe, &bbbits, &alloc);
    bs= integer2bigint(1, &alloc);

    if (e >= 0)
    {
      bb2= bb5= 0;
      bd2= bd5= e;
    }
    else
    {
      bb2= bb5= -e;
      bd2= bd5= 0;
    }
    if (bbe >= 0)
      bb2+= bbe;
    else
      bd2-= bbe;
    bs2= bb2;
#ifdef Honor_FLT_ROUNDS
    if (rounding != 1)
      bs2++;
#endif
    j= bbe - scale;
    i= j + bbbits - 1;  
    if (i < Emin)  
      j+= P - Emin;
    else
      j= P + 1 - bbbits;
    bb2+= j;
    bd2+= j;
    bd2+= scale;
    i= bb2 < bd2 ? bb2 : bd2;
    if (i > bs2)
      i= bs2;
    if (i > 0)
    {
      bb2-= i;
      bd2-= i;
      bs2-= i;
    }
    if (bb5 > 0)
    {
      bs= pow5mult(bs, bb5, &alloc);
      bb1= bigint_mul_bigint(bs, bb, &alloc);
      free_bigint(bb, &alloc);
      bb= bb1;
    }
    if (bb2 > 0)
      bb= left_shift(bb, bb2, &alloc);
    if (bd5 > 0)
      bd= pow5mult(bd, bd5, &alloc);
    if (bd2 > 0)
      bd= left_shift(bd, bd2, &alloc);
    if (bs2 > 0)
      bs= left_shift(bs, bs2, &alloc);
    delta= bigint_diff(bb, bd, &alloc);
    dsign= delta->sign;
    delta->sign= 0;
    i= bigint_cmp(delta, bs);
#ifdef Honor_FLT_ROUNDS
    if (rounding != 1)
    {
      if (i < 0)
      {
        if (!delta->p.x[0] && delta->wds <= 1)
        {
#ifdef SET_INEXACT
          inexact= 0;
#endif
          break;
        }
        if (rounding)
        {
          if (dsign)
          {
            adj.d= 1.;
            goto apply_adj;
          }
        }
        else if (!dsign)
        {
          adj.d= -1.;
          if (!word1(&rv) && !(word0(&rv) & Frac_mask))
          {
            y= word0(&rv) & Exp_mask;
            if (!scale || y > 2*P*Exp_msk1)
            {
              delta= left_shift(delta, Log2P, &alloc);
              if (bigint_cmp(delta, bs) <= 0)
              adj.d= -0.5;
            }
          }
 apply_adj:
          if (scale && (y= word0(&rv) & Exp_mask) <= 2 * P * Exp_msk1)
            word0(&adj)+= (2 * P + 1) * Exp_msk1 - y;
          dval(&rv)+= adj.d * ulp(&rv);
        }
        break;
      }
      adj.d= ratio(delta, bs);
      if (adj.d < 1.)
        adj.d= 1.;
      if (adj.d <= 0x7ffffffe)
      {
        
        y= adj.d;
        if (y != adj.d)
        {
          if (!((rounding >> 1) ^ dsign))
            y++;
          adj.d= y;
        }
      }
      if (scale && (y= word0(&rv) & Exp_mask) <= 2 * P * Exp_msk1)
        word0(&adj)+= (2 * P + 1) * Exp_msk1 - y;
      adj.d*= ulp(&rv);
      if (dsign)
        dval(&rv)+= adj.d;
      else
        dval(&rv)-= adj.d;
      goto cont;
    }
#endif 

    if (i < 0)
    {
      if (dsign || word1(&rv) || word0(&rv) & Bndry_mask ||
          (word0(&rv) & Exp_mask) <= (2 * P + 1) * Exp_msk1)
      {
#ifdef SET_INEXACT
        if (!delta->x[0] && delta->wds <= 1)
          inexact= 0;
#endif
        break;
      }
      if (!delta->p.x[0] && delta->wds <= 1)
      {
#ifdef SET_INEXACT
        inexact= 0;
#endif
        break;
      }
      delta= left_shift(delta, Log2P, &alloc);
      if (bigint_cmp(delta, bs) > 0)
        goto drop_down;
      break;
    }
    if (i == 0)
    {
      if (dsign)
      {
        if ((word0(&rv) & Bndry_mask1) == Bndry_mask1 &&
            word1(&rv) ==
            ((scale && (y = word0(&rv) & Exp_mask) <= 2 * P * Exp_msk1) ?
             (0xffffffff & (0xffffffff << (2*P+1-(y>>Exp_shift)))) :
             0xffffffff))
        {
          word0(&rv)= (word0(&rv) & Exp_mask) + Exp_msk1;
          word1(&rv) = 0;
          dsign = 0;
          break;
        }
      }
      else if (!(word0(&rv) & Bndry_mask) && !word1(&rv))
      {
 drop_down:
        if (scale)
        {
          L= word0(&rv) & Exp_mask;
          if (L <= (2 *P + 1) * Exp_msk1)
          {
            if (L > (P + 2) * Exp_msk1)
              break;
            goto undfl;
          }
        }
        L= (word0(&rv) & Exp_mask) - Exp_msk1;
        word0(&rv)= L | Bndry_mask1;
        word1(&rv)= 0xffffffff;
        break;
      }
      if (!(word1(&rv) & LSB))
        break;
      if (dsign)
        dval(&rv)+= ulp(&rv);
      else
      {
        dval(&rv)-= ulp(&rv);
        if (!dval(&rv))
          goto undfl;
      }
      dsign= 1 - dsign;
      break;
    }
    if ((aadj= ratio(delta, bs)) <= 2.)
    {
      if (dsign)
        aadj= aadj1= 1.;
      else if (word1(&rv) || word0(&rv) & Bndry_mask)
      {
        if (word1(&rv) == Tiny1 && !word0(&rv))
          goto undfl;
        aadj= 1.;
        aadj1= -1.;
      }
      else
      {
        if (aadj < 2. / FLT_RADIX)
          aadj= 1. / FLT_RADIX;
        else
          aadj*= 0.5;
        aadj1= -aadj;
      }
    }
    else
    {
      aadj*= 0.5;
      aadj1= dsign ? aadj : -aadj;
#ifdef Check_FLT_ROUNDS
      switch (Rounding)
      {
      case 2:
        aadj1-= 0.5;
        break;
      case 0:
      case 3:
        aadj1+= 0.5;
      }
#else
      if (Flt_Rounds == 0)
        aadj1+= 0.5;
#endif
    }
    y= word0(&rv) & Exp_mask;

    if (y == Exp_msk1 * (DBL_MAX_EXP + Bias - 1))
    {
      dval(&rv0)= dval(&rv);
      word0(&rv)-= P * Exp_msk1;
      adj.d= aadj1 * ulp(&rv);
      dval(&rv)+= adj.d;
      if ((word0(&rv) & Exp_mask) >= Exp_msk1 * (DBL_MAX_EXP + Bias - P))
      {
        if (word0(&rv0) == Big0 && word1(&rv0) == Big1)
          goto ovfl;
        word0(&rv)= Big0;
        word1(&rv)= Big1;
        goto cont;
      }
      else
        word0(&rv)+= P * Exp_msk1;
    }
    else
    {
      if (scale && y <= 2 * P * Exp_msk1)
      {
        if (aadj <= 0x7fffffff)
        {
          if ((z= (ULong) aadj) <= 0)
            z= 1;
          aadj= z;
          aadj1= dsign ? aadj : -aadj;
        }
        dval(&aadj2) = aadj1;
        word0(&aadj2)+= (2 * P + 1) * Exp_msk1 - y;
        aadj1= dval(&aadj2);
        adj.d= aadj1 * ulp(&rv);
        dval(&rv)+= adj.d;
        if (rv.d == 0.)
          goto undfl;
      }
      else
      {
        adj.d= aadj1 * ulp(&rv);
        dval(&rv)+= adj.d;
      }
    }
    z= word0(&rv) & Exp_mask;
#ifndef SET_INEXACT
    if (!scale)
      if (y == z)
      {
        L= (Long)aadj;
        aadj-= L;
        if (dsign || word1(&rv) || word0(&rv) & Bndry_mask)
        {
          if (aadj < .4999999 || aadj > .5000001)
            break;
        }
        else if (aadj < .4999999 / FLT_RADIX)
          break;
      }
#endif
 cont:
    free_bigint(bb, &alloc);
    free_bigint(bd, &alloc);
    free_bigint(bs, &alloc);
    free_bigint(delta, &alloc);
  }
#ifdef SET_INEXACT
  if (inexact)
  {
    if (!oldinexact)
    {
      word0(&rv0)= Exp_1 + (70 << Exp_shift);
      word1(&rv0)= 0;
      dval(&rv0)+= 1.;
    }
  }
  else if (!oldinexact)
    clear_inexact();
#endif
  if (scale)
  {
    word0(&rv0)= Exp_1 - 2 * P * Exp_msk1;
    word1(&rv0)= 0;
    dval(&rv)*= dval(&rv0);
  }
#ifdef SET_INEXACT
  if (inexact && !(word0(&rv) & Exp_mask))
  {
    
    dval(&rv0)= 1e-300;
    dval(&rv0)*= dval(&rv0);
  }
#endif
 retfree:
  free_bigint(bb, &alloc);
  free_bigint(bd, &alloc);
  free_bigint(bs, &alloc);
  free_bigint(bd0, &alloc);
  free_bigint(delta, &alloc);
 ret:
  *se= (char *)s;
  return sign ? -dval(&rv) : dval(&rv);
}

double BinlogStrotd::convert(const char *str, char **end, int *error)
{
  char buf[DTOA_BUF_MAX_SIZE];
  double res = 0.0;
  if (!(end != NULL && ((str != NULL && *end != NULL) ||
                              (str == NULL && *end == NULL)) &&
              error != NULL)) {
    return 0.0;
  }
  res = binlog_strtod_int(str, end, error, buf, sizeof(buf));
  return (*error == 0) ? res : (res < 0 ? -DBL_MAX : DBL_MAX);
}


#undef P
#undef Rounding

}  // namespace oceanbase::binlog