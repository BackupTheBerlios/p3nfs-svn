#include <stdio.h>
#include "cnv.h"

#if defined(__SVR4) || defined(__GLIBC__) || defined(__FreeBSD__)
#include <string.h>
#include <stdlib.h>
#endif
#ifdef __NeXT__
#include <string.h>
#include <objc/hashtable.h>
#define strdup NXCopyStringBuffer
#endif
#define HASHSIZE 99
#include "nfs_prot.h"
#include "mp.h"

static int nextinode = 6;
static p_inode *numtab[HASHSIZE];
static p_inode *namtab[HASHSIZE];

/*
 * Verrry simple hash :-)
 */
static unsigned hash(str)
  char *str;
{
  unsigned i = 0, hashval = 3*HASHSIZE/4;

  while(*str)
    {
      i = *str++;
      hashval = (hashval << (i & 7)) + i;
    }

  return hashval % HASHSIZE;
}

/* Get struct with inode */
p_inode *
get_num(i)
  int i;
{
  p_inode *ptr;

  for(ptr = numtab[i % HASHSIZE]; ptr; ptr = ptr->nextnum)
    if(i == ptr->inode)
      break;
  if(!ptr)
    {
      printf("Inode %d not found (aborting)\n", i);
      abort();
    }
  return ptr;
}

static p_inode *
newinode(name, inode)
  char *name;
  int inode;
{
  p_inode *ptr;
  int idx = hash(name);

  ptr = (p_inode *)malloc(sizeof(*ptr));
  ptr->name = (char *)strdup(name);
  ptr->inode = inode;

/* insert into both hashtabs */
  ptr->nextnam = namtab[idx];
  namtab[idx] = ptr;

  ptr->nextnum = numtab[inode % HASHSIZE];
  numtab[inode % HASHSIZE] = ptr;
  return ptr;
}

/* Get/create struct with name */
p_inode *
get_nam(name)
  char *name;
{
  p_inode *ptr;
  int idx = hash(name);

  for(ptr = namtab[idx]; ptr; ptr = ptr->nextnam)
    if(!strcmp(name, ptr->name))
      break;
  if (!ptr)
    ptr = newinode(name, nextinode++);
  if (debug > 1)
    printf("\tget_nam(%s) returns %08x->inode = %d\n",
           name, (unsigned int)ptr, ptr->inode);
  return ptr;
}

void
inode2fh(inode, fh)
  int inode;
  char *fh;
{
  bzero(fh, NFS_FHSIZE);
  bcopy((char *)&inode, fh, sizeof(inode));
  if(debug > 2)
    {
      int i;
      printf("\tFH2I: ");
      for(i = 0; i < sizeof(mynfs_fh); i++)
	printf("%02x", *fh++);
      printf("\n");
    }
}

int
fh2inode(fh)
  char *fh;
{
  int inode;

  bcopy(fh, (char *)&inode, sizeof(inode));
  if(debug > 2)
    {
      int i;
      printf("\tI2FH: ");
      for(i = 0; i < sizeof(mynfs_fh); i++)
	printf("%02x", *fh++);
      printf("\n");
    }
  return inode;
}



/* Rename: the inode must be conserved */
p_inode *
re_nam(old, new)
  char *old, *new;
{
  p_inode *nptr, *optr, **nampp, **numpp;
  int idx = hash(old);

  if (debug) printf("re_nam: %s->%s\n", old, new);
  for(nampp = &namtab[idx]; *nampp; nampp = &(*nampp)->nextnam)
    if(!strcmp(old, (*nampp)->name))
      break;
  if(!*nampp) return get_nam(new);

  optr = *nampp;
  if (debug) printf("re_nam: %d\n", optr->inode);
  *nampp = optr->nextnam;

  /* delete it from the other hashtab too */
  idx = optr->inode % HASHSIZE;
  for(numpp = &numtab[idx]; *numpp; numpp = &(*numpp)->nextnum)
    if(optr == (*numpp))
      break;
  if(!*numpp)
    {
      printf("Entry in one hashtab only (aborting)\n");
      abort();
    }
  *numpp = optr->nextnum;

  nptr = newinode(new, optr->inode);
  if (debug) printf("re_nam: new entry created\n");
  free(optr->name);
  free(optr);

  return nptr;
}

/* Cache routines */
struct cache *
search_cache(root, inode)
  struct cache *root;
  unsigned inode;
{
  struct cache *cp;

  for(cp = root; cp; cp = cp->next)
    if(cp->inode == inode)
      return cp;
  return 0;
}

struct cache *
add_cache(root, inode, fp)
  struct cache **root;
  unsigned inode;
  fattr *fp;
{
  struct cache *cp;
  cp = (struct cache *)malloc(sizeof(*cp));
  cp->inode = inode;
  cp->attr = *fp;
  cp->dcache = 0;
  cp->actual_size = fp->size;
  cp->next = *root;
  *root = cp;
  return cp;
}

struct dcache *
add_dcache(cp, offset, len, data)
  struct cache *cp;
  unsigned offset, len;
  unsigned char *data;
{
  struct dcache *dcp;
  dcp = (struct dcache *)malloc(sizeof(*dcp));
  dcp->towrite = 1;
  dcp->offset = offset;
  dcp->data = 0;
  dcp->len = len;
  if(len)
    {
      dcp->data = (unsigned char *)malloc(len);
      bcopy(data, dcp->data, len);
    }
  dcp->next = cp->dcache;
  cp->dcache = dcp;
  return dcp;
}

void
clean_dcache(cp)
  struct cache *cp;
{
  struct dcache *dcp, *dcpn;
  for (dcp = cp->dcache; dcp ; dcp = dcpn)
    {
      dcpn = dcp->next;
      if(dcp->len)
	free(dcp->data);
      free(dcp);
    }
  cp->dcache = 0;
}

struct dcache *
search_dcache(cp, off, len)
  struct cache *cp;
  unsigned int off, len;
{
  struct dcache *dcp;
  for (dcp = cp->dcache; dcp; dcp = dcp->next)
    if (dcp->offset == off && dcp->len >= len)
      return dcp;
  return 0;
}

void
rem_cache(root, inode)
  struct cache **root;
  unsigned inode;
{
  struct cache *cp, **cpp;

  for(cpp = root; (cp = *cpp); cpp = &cp->next)
    if (cp->inode == inode)
      break;
  if (!cp)
    return;
  *cpp = cp->next;
  clean_dcache(cp);
  free(cp);
}

void
clean_cache(root)
  struct cache **root;
{
  struct cache *cp, *cpn;

  for(cp= *root; cp; cp = cpn)
    {
      cpn = cp->next;
      clean_dcache(cp);
      free(cp);
    }
  *root = 0;
}

/*
  All data (i.e. filenames) is stored ISO, so conversion is needed
  when communicating with the psion.
  Exception: the psion5, which uses ISO.
 */
char *
cp2iso(str)
  char *str;
{
  static char lbuf[1024];
  unsigned char *s, *p;

  if ( series5 ) return strcpy( lbuf, str );

  p = (unsigned char *)lbuf;
  for(s = (unsigned char *)str; *s; s++)
    if(*s > 128)
      *p++ = tbl_cp2iso[*s-128];
    else
      *p++ = *s;

  *p = 0;
  return lbuf;
}

char *
iso2cp(str)
  char *str;
{
  static char lbuf[1024];
  unsigned char *s, *p;

  if ( series5 ) return strcpy( lbuf, str );

  p = (unsigned char *)lbuf;
  for(s = (unsigned char *)str; *s; s++)
    if(*s > 128)
      *p++ = tbl_iso2cp[*s-128];
    else
      *p++ = *s;

  *p = 0;
  return lbuf;
}

/*
 * MS-DOS type 8.3 conversion. Not needed for the S5.
 */
static char *
check_ext(file)
  char *file;
{
  static char namebuf[13];
  char *s, *p, *sp;
  int l, ll;

  if ( series5 ) return file;

  p = namebuf;

  /* convert filename to a DOS (puke) conforming one */
  s = (char *)index(file, '.');
  if(s)
    *s = 0;
  strncpy(p, file, 8);
  p[8] = 0;
  if(s)
    *s++ = '.';
  p += strlen(p);
  *p++ = '.';
  *p = 0;

  if(s)
    {
      l = 0;
      if( (sp = (char *)rindex(s, '.')) ) /* Another dot found */
        {
 	  *sp++ = 0;
	  l = (int)strlen(sp);
	  l = l > 3 ? 3 : l;
	}
      ll = 3 - l;
      /* now copy ll chars */
      for(;*s && ll; s++)
      	{
	  if (*s == '.')
	    continue;
	  *p++ = *s;
	  ll--;
	}
      *p = 0;
      if (sp)
      	{
          strncpy(p, sp, l);
	  p[l] = 0;
	}
    }
  return namebuf;
}

char *
build_path(dir, file)
  char *dir, *file;
{
  static char namebuf[300];

  if(!strcmp(dir, ""))
    strcpy(namebuf, file);
/* FIXME (rom::)*/
  else if(!strcmp(dir, "rom::"))
    sprintf(namebuf, "%s%s", dir, check_ext(file));
  else
    sprintf(namebuf, "%s\\%s", dir, check_ext(file));

  return namebuf;
}

int
getpinode(inode)
  p_inode *inode;
{
  char *p;
  int i;

  if(inode->inode == root_fattr.fileid) /* Root inode */
    i = root_fattr.fileid - 1;			/* RUDI !!! */
  else if(!(p = (char *)rindex(inode->name, '\\'))) /* device inode */
    i = root_fattr.fileid;
  else
    {
      char lbuf[1024];
      *p = 0; strcpy(lbuf, inode->name); *p = '\\';
      i = get_nam(lbuf)->inode;
    }
  return i;
}

/* FIXME (rom::)*/
char *
dirname(dir)
  char *dir;
{
  static char namebuf[300];

  if(!strcmp(dir, "rom::"))
    return dir;
  sprintf(namebuf, "%s\\", dir);
  return namebuf;
}

/* FIXME rom:: */
char *
filname(dir)
  char *dir;
{
  char *p;
  if(!strncmp(dir, "rom::", 5))
    return dir+5;
  if ((p = (char *)rindex(dir, '\\')))
    return p+1;
  else return dir;
}
