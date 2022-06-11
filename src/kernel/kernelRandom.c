//
//  Visopsys
//  Copyright (C) 1998-2003 J. Andrew McLaughlin
// 
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of the GNU General Public License as published by the Free
//  Software Foundation; either version 2 of the License, or (at your option)
//  any later version.
// 
//  This program is distributed in the hope that it will be useful, but
//  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
//  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
//  for more details.
//  
//  You should have received a copy of the GNU General Public License along
//  with this program; if not, write to the Free Software Foundation, Inc.,
//  59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
//
//  kernelRandom.c
//

// These functions are used to enable random number generation using
// the Visopsys kernel.

#include "kernelRandom.h"
#include "kernelSysTimerFunctions.h"
#include "kernelRtcFunctions.h"
#include "kernelLog.h"


static volatile unsigned kernelRandomSeed;


static unsigned random(unsigned seed)
{
  // This routine will return a pseudo-random number based on the seed
  // supplied.  I made up the algorithm and it's not too terriffic.  It
  // should be replaced with a commonly accepted one.
  
  int count, mod;
  unsigned otherOperand = 0;
  unsigned oldSeed = 0;

  // Save the old random seed temporarily
  oldSeed = seed;

  // We do a loop 10 times to perform semi-random operations on the random
  // seed.  We'll "mod" the seed to semi-randomly pick an operation to 
  // perform on it, for which we will use the result of a divide operation
  // to get the other operand.

  for (count = 0; count < 10; count ++)
    {
      otherOperand = (seed / 3) * (kernelSysTimerRead() * 13);
      otherOperand ^= kernelSysTimerRead();
      
      while (otherOperand == 0)
	{
	  // Oops.  Find something reasonable
	  otherOperand = kernelRtcReadSeconds() + (seed / 3);
	}

      mod = (seed % 3);

      if (mod == 0)
	// We ADD the other operand to the seed 
	seed = (seed + otherOperand);
      else if (mod == 1)
	// We SUBTRACT the other operand from the seed
	seed = (seed - otherOperand);
      else
	// We MULTIPLY the seed by the other operand
	seed = (seed * otherOperand);

      while (seed == 0)
	{
	  // We can't have that at all.
	  seed = (oldSeed + 97);
	  // Make sure we do at least one extra iteration
	  count --;
	}
    }

  return (seed);
}


/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////
//
//  Below here, the functions are exported for external use
//
/////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////


int kernelRandomInitialize(void)
{
  // This initialize function should be called during kernel initialization
  // BEFORE memory has been initialized.  This is because these 
  // functions will utilize the fact that the data in a given memory 
  // location will be more-or-less random after power-up.  

  int status = 0;
  int count;

  // This isn't great.  It will be better after the kernel can save its
  // random seed from the last session, and use it to help further 
  // the randomization.

  while (kernelRandomSeed == 0)
    {
      // We will do this loop the same number of times as the current
      // month * 10
      for (count = (kernelRtcReadMonth() * 10); count > 0; count--)
	{
	  // OR the seed with the number of seconds the current seconds
	  kernelRandomSeed = kernelRtcReadSeconds();

	  // We will left-shift the seed by the minutes / 2
	  kernelRandomSeed <<= (kernelRtcReadMinutes() / 2);

	  // We will OR the seed with the current hour
	  kernelRandomSeed |= kernelRtcReadHours();

	  // Finally, we will XOR the seed with the current timer value
	  kernelRandomSeed ^= kernelSysTimerRead();
	}
    }

  kernelLog("The kernel's random seed is: %u", kernelRandomSeed);

  return (status);
}


unsigned kernelRandomUnformatted(void)
{
  // This is for getting an unformatted random number using the kernel's
  // random seed.
  kernelRandomSeed = random(kernelRandomSeed);
  return (kernelRandomSeed);
}


unsigned kernelRandomFormatted(unsigned start, unsigned end)
{
  // This function will return a random number between "start" and "end"
  // using the kernel's random seed.
  kernelRandomSeed = random(kernelRandomSeed);
  return ((kernelRandomSeed % ((end - start) + 1)) + start);
}


unsigned kernelRandomSeededUnformatted(unsigned seed)
{
  // This is for getting an unformatted random number using the user's
  // random seed.
  return (random(seed));
}


unsigned kernelRandomSeededFormatted(unsigned seed, unsigned start,
				     unsigned end)
{
  // This function will return a random number between "start" and "end"
  // using the user's random seed.
  return ((random(seed) % ((end - start) + 1)) + start);
}
