// (c) Copyright 2016 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// prime.c		Routine for finding and returning a prime number.

#include "pldef.h"
#include <math.h>

// Find first prime number which is greater than or equal to n and return it, or 0 if UINT_MAX is reached.  Algorithm used is
// trial division, which is fast for a 32-bit unsigned integer.
uint prime(uint n) {
	uint i;
	uint nMax;

	if(n <= 2)
		return 2;
	nMax = round(sqrt(n));		// Use round() instead of trunc() in case an exact root is less than the integral
					// value; for example, sqrt(121) = 10.999999999.
	if((n & 1) == 0)
		++n;
	for(;;) {
		// Is n divisible by an odd number less than or equal to its square root?
		for(i = 3; i <= nMax; i += 2) {
			if(n % i == 0)

				// Yes, n is not prime.
				goto nextn;
			}

		// n is prime.  Return it.
		return n;
nextn:
		if(n == UINT_MAX)
			break;
		n += 2;
		}

	// Not found.
	return 0;
	}
