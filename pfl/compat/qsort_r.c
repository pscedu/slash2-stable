/* An alternative to qsort, with an identical interface.
   This file is part of the GNU C Library.
   Copyright (C) 1992-2014 Free Software Foundation, Inc.
   Written by Mike Haertel, September 1988.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */

#include <alloca.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define __alloca alloca
#define __sysconf sysconf
# define __set_errno(val) (errno = (val))

typedef int (*__compar_d_fn_t) (const void *, const void *, void *);

struct msort_param
{
  size_t s;
  size_t var;
  __compar_d_fn_t cmp;
  void *arg;
  char *t;
};

#define SWAP(a, b, size)						      \
  do									      \
    {									      \
      size_t __size = (size);						      \
      char *__a = (a), *__b = (b);					      \
      do								      \
	{								      \
	  char __tmp = *__a;						      \
	  *__a++ = *__b;						      \
	  *__b++ = __tmp;						      \
	} while (--__size > 0);						      \
    } while (0)

#define MAX_THRESH 4

typedef struct
  {
    char *lo;
    char *hi;
  } stack_node;

#define STACK_SIZE	(CHAR_BIT * sizeof(size_t))
#define PUSH(low, high)	((void) ((top->lo = (low)), (top->hi = (high)), ++top))
#define	POP(low, high)	((void) (--top, (low = top->lo), (high = top->hi)))
#define	STACK_NOT_EMPTY	(stack < top)

void
_quicksort (void *const pbase, size_t total_elems, size_t size,
	    __compar_d_fn_t cmp, void *arg)
{
  char *base_ptr = (char *) pbase;

  const size_t max_thresh = MAX_THRESH * size;

  if (total_elems == 0)
    /* Avoid lossage with unsigned arithmetic below.  */
    return;

  if (total_elems > MAX_THRESH)
    {
      char *lo = base_ptr;
      char *hi = &lo[size * (total_elems - 1)];
      stack_node stack[STACK_SIZE];
      stack_node *top = stack;

      PUSH (NULL, NULL);

      while (STACK_NOT_EMPTY)
        {
          char *left_ptr;
          char *right_ptr;

	  /* Select median value from among LO, MID, and HI. Rearrange
	     LO and HI so the three values are sorted. This lowers the
	     probability of picking a pathological pivot value and
	     skips a comparison for both the LEFT_PTR and RIGHT_PTR in
	     the while loops. */

	  char *mid = lo + size * ((hi - lo) / size >> 1);

	  if ((*cmp) ((void *) mid, (void *) lo, arg) < 0)
	    SWAP (mid, lo, size);
	  if ((*cmp) ((void *) hi, (void *) mid, arg) < 0)
	    SWAP (mid, hi, size);
	  else
	    goto jump_over;
	  if ((*cmp) ((void *) mid, (void *) lo, arg) < 0)
	    SWAP (mid, lo, size);
	jump_over:;

	  left_ptr  = lo + size;
	  right_ptr = hi - size;

	  /* Here's the famous ``collapse the walls'' section of quicksort.
	     Gotta like those tight inner loops!  They are the main reason
	     that this algorithm runs much faster than others. */
	  do
	    {
	      while ((*cmp) ((void *) left_ptr, (void *) mid, arg) < 0)
		left_ptr += size;

	      while ((*cmp) ((void *) mid, (void *) right_ptr, arg) < 0)
		right_ptr -= size;

	      if (left_ptr < right_ptr)
		{
		  SWAP (left_ptr, right_ptr, size);
		  if (mid == left_ptr)
		    mid = right_ptr;
		  else if (mid == right_ptr)
		    mid = left_ptr;
		  left_ptr += size;
		  right_ptr -= size;
		}
	      else if (left_ptr == right_ptr)
		{
		  left_ptr += size;
		  right_ptr -= size;
		  break;
		}
	    }
	  while (left_ptr <= right_ptr);

          /* Set up pointers for next iteration.  First determine whether
             left and right partitions are below the threshold size.  If so,
             ignore one or both.  Otherwise, push the larger partition's
             bounds on the stack and continue sorting the smaller one. */

          if ((size_t) (right_ptr - lo) <= max_thresh)
            {
              if ((size_t) (hi - left_ptr) <= max_thresh)
		/* Ignore both small partitions. */
                POP (lo, hi);
              else
		/* Ignore small left partition. */
                lo = left_ptr;
            }
          else if ((size_t) (hi - left_ptr) <= max_thresh)
	    /* Ignore small right partition. */
            hi = right_ptr;
          else if ((right_ptr - lo) > (hi - left_ptr))
            {
	      /* Push larger left partition indices. */
              PUSH (lo, right_ptr);
              lo = left_ptr;
            }
          else
            {
	      /* Push larger right partition indices. */
              PUSH (left_ptr, hi);
              hi = right_ptr;
            }
        }
    }

  /* Once the BASE_PTR array is partially sorted by quicksort the rest
     is completely sorted using insertion sort, since this is efficient
     for partitions below MAX_THRESH size. BASE_PTR points to the beginning
     of the array to sort, and END_PTR points at the very last element in
     the array (*not* one beyond it!). */

#define min(x, y) ((x) < (y) ? (x) : (y))

  {
    char *const end_ptr = &base_ptr[size * (total_elems - 1)];
    char *tmp_ptr = base_ptr;
    char *thresh = min(end_ptr, base_ptr + max_thresh);
    char *run_ptr;

    /* Find smallest element in first threshold and place it at the
       array's beginning.  This is the smallest array element,
       and the operation speeds up insertion sort's inner loop. */

    for (run_ptr = tmp_ptr + size; run_ptr <= thresh; run_ptr += size)
      if ((*cmp) ((void *) run_ptr, (void *) tmp_ptr, arg) < 0)
        tmp_ptr = run_ptr;

    if (tmp_ptr != base_ptr)
      SWAP (tmp_ptr, base_ptr, size);

    /* Insertion sort, running from left-hand-side up to right-hand-side.  */

    run_ptr = base_ptr + size;
    while ((run_ptr += size) <= end_ptr)
      {
	tmp_ptr = run_ptr - size;
	while ((*cmp) ((void *) run_ptr, (void *) tmp_ptr, arg) < 0)
	  tmp_ptr -= size;

	tmp_ptr += size;
        if (tmp_ptr != run_ptr)
          {
            char *trav;

	    trav = run_ptr + size;
	    while (--trav >= run_ptr)
              {
                char c = *trav;
                char *hi, *lo;

                for (hi = lo = trav; (lo -= size) >= tmp_ptr; hi = lo)
                  *hi = *lo;
                *hi = c;
              }
          }
      }
  }
}

static void
msort_with_tmp (const struct msort_param *p, void *b, size_t n)
{
  char *b1, *b2;
  size_t n1, n2;

  if (n <= 1)
    return;

  n1 = n / 2;
  n2 = n - n1;
  b1 = b;
  b2 = (char *) b + (n1 * p->s);

  msort_with_tmp (p, b1, n1);
  msort_with_tmp (p, b2, n2);

  char *tmp = p->t;
  const size_t s = p->s;
  __compar_d_fn_t cmp = p->cmp;
  void *arg = p->arg;
  switch (p->var)
    {
    case 0:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      *(uint32_t *) tmp = *(uint32_t *) b1;
	      b1 += sizeof (uint32_t);
	      --n1;
	    }
	  else
	    {
	      *(uint32_t *) tmp = *(uint32_t *) b2;
	      b2 += sizeof (uint32_t);
	      --n2;
	    }
	  tmp += sizeof (uint32_t);
	}
      break;
    case 1:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      *(uint64_t *) tmp = *(uint64_t *) b1;
	      b1 += sizeof (uint64_t);
	      --n1;
	    }
	  else
	    {
	      *(uint64_t *) tmp = *(uint64_t *) b2;
	      b2 += sizeof (uint64_t);
	      --n2;
	    }
	  tmp += sizeof (uint64_t);
	}
      break;
    case 2:
      while (n1 > 0 && n2 > 0)
	{
	  unsigned long *tmpl = (unsigned long *) tmp;
	  unsigned long *bl;

	  tmp += s;
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      bl = (unsigned long *) b1;
	      b1 += s;
	      --n1;
	    }
	  else
	    {
	      bl = (unsigned long *) b2;
	      b2 += s;
	      --n2;
	    }
	  while (tmpl < (unsigned long *) tmp)
	    *tmpl++ = *bl++;
	}
      break;
    case 3:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (*(const void **) b1, *(const void **) b2, arg) <= 0)
	    {
	      *(void **) tmp = *(void **) b1;
	      b1 += sizeof (void *);
	      --n1;
	    }
	  else
	    {
	      *(void **) tmp = *(void **) b2;
	      b2 += sizeof (void *);
	      --n2;
	    }
	  tmp += sizeof (void *);
	}
      break;
    default:
      while (n1 > 0 && n2 > 0)
	{
	  if ((*cmp) (b1, b2, arg) <= 0)
	    {
	      memcpy (tmp, b1, s);
	      tmp += s;
	      b1 += s;
	      --n1;
	    }
	  else
	    {
	      memcpy (tmp, b2, s);
	      tmp += s;
	      b2 += s;
	      --n2;
	    }
	}
      break;
    }

  if (n1 > 0)
    memcpy (tmp, b1, n1 * s);
  memcpy (b, p->t, (n - n2) * s);
}

void
qsort_r (void *b, size_t n, size_t s, __compar_d_fn_t cmp, void *arg)
{
  size_t size = n * s;
  char *tmp = NULL;
  struct msort_param p;

  /* For large object sizes use indirect sorting.  */
  if (s > 32)
    size = 2 * n * sizeof (void *) + s;

  if (size < 1024)
    /* The temporary array is small, so put it on the stack.  */
    p.t = __alloca (size);
  else
    {
      /* We should avoid allocating too much memory since this might
	 have to be backed up by swap space.  */
      static long int phys_pages;
      static int pagesize;

      if (pagesize == 0)
	{
	  phys_pages = __sysconf (_SC_PHYS_PAGES);

	  if (phys_pages == -1)
	    /* Error while determining the memory size.  So let's
	       assume there is enough memory.  Otherwise the
	       implementer should provide a complete implementation of
	       the `sysconf' function.  */
	    phys_pages = (long int) (~0ul >> 1);

	  /* The following determines that we will never use more than
	     a quarter of the physical memory.  */
	  phys_pages /= 4;

	  /* Make sure phys_pages is written to memory.  */
	  //atomic_write_barrier ();

	  pagesize = __sysconf (_SC_PAGESIZE);
	}

      /* Just a comment here.  We cannot compute
	   phys_pages * pagesize
	   and compare the needed amount of memory against this value.
	   The problem is that some systems might have more physical
	   memory then can be represented with a `size_t' value (when
	   measured in bytes.  */

      /* If the memory requirements are too high don't allocate memory.  */
      if (size / pagesize > (size_t) phys_pages)
	{
	  _quicksort (b, n, s, cmp, arg);
	  return;
	}

      /* It's somewhat large, so malloc it.  */
      int save = errno;
      tmp = malloc (size);
      __set_errno (save);
      if (tmp == NULL)
	{
	  /* Couldn't get space, so use the slower algorithm
	     that doesn't need a temporary array.  */
	  _quicksort (b, n, s, cmp, arg);
	  return;
	}
      p.t = tmp;
    }

  p.s = s;
  p.var = 4;
  p.cmp = cmp;
  p.arg = arg;

  if (s > 32)
    {
      /* Indirect sorting.  */
      char *ip = (char *) b;
      void **tp = (void **) (p.t + n * sizeof (void *));
      void **t = tp;
      void *tmp_storage = (void *) (tp + n);

      while ((void *) t < tmp_storage)
	{
	  *t++ = ip;
	  ip += s;
	}
      p.s = sizeof (void *);
      p.var = 3;
      msort_with_tmp (&p, p.t + n * sizeof (void *), n);

      /* tp[0] .. tp[n - 1] is now sorted, copy around entries of
	 the original array.  Knuth vol. 3 (2nd ed.) exercise 5.2-10.  */
      char *kp;
      size_t i;
      for (i = 0, ip = (char *) b; i < n; i++, ip += s)
	if ((kp = tp[i]) != ip)
	  {
	    size_t j = i;
	    char *jp = ip;
	    memcpy (tmp_storage, ip, s);

	    do
	      {
		size_t k = (kp - (char *) b) / s;
		tp[j] = jp;
		memcpy (jp, kp, s);
		j = k;
		jp = kp;
		kp = tp[k];
	      }
	    while (kp != ip);

	    tp[j] = jp;
	    memcpy (jp, tmp_storage, s);
	  }
    }
  else
    {
      if ((s & (sizeof (uint32_t) - 1)) == 0
	  && ((char *) b - (char *) 0) % __alignof__ (uint32_t) == 0)
	{
	  if (s == sizeof (uint32_t))
	    p.var = 0;
	  else if (s == sizeof (uint64_t)
		   && ((char *) b - (char *) 0) % __alignof__ (uint64_t) == 0)
	    p.var = 1;
	  else if ((s & (sizeof (unsigned long) - 1)) == 0
		   && ((char *) b - (char *) 0)
		      % __alignof__ (unsigned long) == 0)
	    p.var = 2;
	}
      msort_with_tmp (&p, b, n);
    }
  free (tmp);
}
