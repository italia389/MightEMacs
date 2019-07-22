// ProLib (c) Copyright 2019 Richard W. Marinelli
//
// This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
// "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
//
// getswitch.c		Routine for processing command-line switches.

#include "os.h"
#include "plexcep.h"
#include "pllib.h"
#include "plstring.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "plgetswitch.h"
#include "plhash.h"

#define ssptr(hrec)		((SwitchState *) hrec->value->u.d_blob.b_mem)
//#define GSDebugFile		stderr
#ifdef GSDebugFile
extern FILE *GSDebugFile;
#endif
// Get switch of form -sw1 val1 -sw2 -n +n...
//
// Arguments:
//	argcp		Pointer to count of switch strings in argvp array (typically &argc from main() function).
//	argvp		Indirect pointer to array of argument strings (typically &argv from main() function).
//	swp		Indirect pointer to switch table, which contains a descriptor for each valid switch.
//	resultp		Pointer to result record, containing primary switch name and its value, if any.
//
// Notes:
//  1. Caller sets flags to zero in the last descriptor of *swp to indicate end of table.
//  2. Each invocation of this routine returns one switch.  Routine sets *swp to NULL after first call to process subsequent
//     switches.
//  3. If a switch is found, values in *resultp are set as appropriate and a 1-relative index of switch in the switch table is
//     returned.  If a numeric switch is found, "name" is set to NULL and "value" is set to the switch that was found, including
//     the leading '-' or '+' character.
//  4. All switch names in the switch table must be unique -- duplicates not allowed.  Also, a maximum of one -n switch
//     descriptor (SF_NumericSwitch flag set and SF_PlusType not set) and one +n switch descriptor (both SF_NumericSwitch and
//     SF_PlusType flags set) may be specified.
//  5. If any of the following occurs, switch scanning stops, *argcp is set to the number of arguments remaining, *argvp is set
//     to the argument following the last switch found, and zero is returned:
//		a. *argcp arguments have been processed;
//		b. **argvp is a NULL pointer;
//		c. a "--" switch or "-" argument is found;
//		d. a string is found which does not begin with '-' (or '+' if a switch descriptor of type SF_PlusType exists
//		   in the switch table).
//  6. If a switch requires an argument and an argument is found which begins with '--', the string following the first '-' is
//     returned as the argument.
//  7. If an error occurs, an exception message is set and -1 is returned.
int getswitch(int *argcp,char ***argvp,Switch **swp,SwitchResult *resultp) {
	static char myname[] = "getswitch";
	char *arg,*argsw = NULL,*tabsw,**tabswp,*str0,*str1;
	int switchIndex = 0;
	ushort argType;
	enum {expectOptionalArg,expectRequiredArg,expectSwitch} stateExpected = expectSwitch;
	enum {foundArg,foundNoArg,foundNullArg,foundSwitch} stateFound;
	Switch *sw;
	char *digits = "0123456789";
	Hash *htab;
	HashRec *hrec;
	SwitchState *ss;
	static struct {
		Switch *sw;			// Pointer to first descriptor in switch table.
		Hash *htab;			// Hash of switches expected and found (for duplicate checking).
		} state;


#ifdef GSDebugFile
	fprintf(GSDebugFile,"%s(): argcp %.8X (%d), argvp %.8X, *argvp %.8X (%s)...\n",myname,
	 (uint) argcp,*argcp,(uint) argvp,(uint) *argvp,**argvp);
#endif
	// Intialize variables and validate switch table if first call.
	if(*swp != NULL) {
#ifdef GSDebugFile
		fputs("getswitch(): Initializing...\n",GSDebugFile);
#endif
		if((state.htab = hnew(19,0.0,0.0)) == NULL)
			return -1;
		for(state.sw = sw = *swp; sw->flags != 0; ++sw) {

			// Create one SwitchState object that the primary switch and all its aliases point to.
			if((ss = (SwitchState *) malloc(sizeof(SwitchState))) == NULL) {
				plexcep.flags |= ExcepMem;
				return emsge(-1);
				}
			ss->sw = sw;
			ss->foundCount = 0;

			// Check switch type.
			if(sw->flags & SF_NumericSwitch) {

				// Numeric switch.  Check if duplicate.
				tabsw = (sw->flags & SF_PlusType) ? NSPlusKey : NSMinusKey;
				if(hsearch(state.htab,tabsw) != NULL)
					return emsgf(-1,"%s(): Multiple numeric (%c) switch descriptors found",myname,
					 (sw->flags & SF_PlusType) ? '+' : '-');
				if((hrec = hset(state.htab,tabsw,NULL,false)) == NULL)
					return -1;
				dsetblobref((void *) ss,0,hrec->value);
				}
			else {
				// Standard switch.  Check if argument type specified.
				if((sw->flags & SF_ArgMask) == 0)
					return emsgf(-1,"%s(): Argument type not specified for -%s switch",myname,*sw->namev);

				// Check switch name plus any aliases for duplicates.
				for(tabswp = sw->namev; (tabsw = *tabswp) != NULL; ++tabswp) {
					if(hsearch(state.htab,tabsw) != NULL)
						return emsgf(-1,"%s(): Multiple -%s switch descriptors found",myname,tabsw);
					if((hrec = hset(state.htab,tabsw,NULL,false)) == NULL)
						return -1;
					dsetblobref((void *) ss,0,hrec->value);
					}
				}
			}

		// Switch table is valid.  Clear table pointer.
		*swp = NULL;
		}
	else if(state.sw == NULL)
		return emsgf(-1,"%s(): Switch table not specified",myname);

	// Loop through argument list and parse a switch.
	for(;;) {
		// Set 'found' state.
		if(*argcp == 0 || (arg = **argvp) == NULL) {
			stateFound = foundNoArg;
#ifdef GSDebugFile
			arg = NULL;
			fprintf(GSDebugFile,"### foundNoArg: argc %d, argv NULL\n",*argcp);
#endif
			}
		else {
#ifdef GSDebugFile
			fprintf(GSDebugFile,"### Parsing arg '%s' (argc %d)...\n",arg,*argcp);
			fflush(GSDebugFile);
#endif
			ss = NULL;
			switch(arg[0]) {
				case '\0':
					stateFound = foundNullArg;
					break;
				case '-':
					switch(arg[1]) {
						case '-':
							if(arg[2] == '\0') {
								stateFound = foundNoArg;
								--*argcp;
								++*argvp;
								}
							else {
								stateFound = foundArg;
								++arg;
								}
							break;
						case '\0':
							stateFound = foundArg;
							break;
						default:
							stateFound = foundSwitch;
						}
					break;
				case '+':
					if(isdigit(arg[1])) {
						htab = state.htab;
						while((hrec = heach(&htab)) != NULL) {
							sw = (ss = ssptr(hrec))->sw;
							if((sw->flags & (SF_NumericSwitch | SF_PlusType)) ==
							 (SF_NumericSwitch | SF_PlusType)) {
								if(*strpspn(arg + 2,digits) == '\0') {
									stateFound = foundSwitch;
									goto FoundDone;
									}
								goto BadNumericSwitch;
								}
							}
						}
					// else fall through.
				default:
					stateFound = foundArg;
				}
			}
FoundDone:
#ifdef GSDebugFile
		fprintf(GSDebugFile,"*** Found arg '%s', stateFound %d, argCount %d, argListNext '%s'.\n",arg,stateFound,
		 *argcp,*argcp == 0 ? "NULL" : *(*argvp + 1));
		fflush(GSDebugFile);
#endif
		// Compare expected and found states.  If found state is foundSwitch and ss is NULL, need to search for switch
		// in hash table.
		switch(stateFound) {
			case foundNoArg:

				// No arguments left.
				switch(stateExpected) {
					case expectOptionalArg:
						goto NoArgReturn;
					case expectRequiredArg:
						goto ValueRequired;
					default:	// expectSwitch
						resultp->name = resultp->value = NULL;
						goto NoSwitchesLeft;
					}
			case foundSwitch:

				// Switch found.
				switch(stateExpected) {
					case expectOptionalArg:
						goto NoArgReturn;
					case expectRequiredArg:
						goto ValueRequired;
					default:	// expectSwitch
						argsw = arg + 1;
						if(*arg == '+') {
							tabsw = NSPlusKey;
							goto MatchFound;
							}
					}
#ifdef GSDebugFile
				fprintf(GSDebugFile,"*** Checking hash table (size %lu, entries %lu) for '%s'...\n",
				 state.htab->hashSize,state.htab->recCount,argsw);
				fflush(GSDebugFile);
#endif
				// Check hash table for a match.
				if(ss != NULL)
					goto SetName;
				if((hrec = hsearch(state.htab,argsw)) != NULL) {
					ss = ssptr(hrec);
SetName:
					tabsw = *ss->sw->namev;
					goto MatchFound;
					}
#ifdef GSDebugFile
				fprintf(GSDebugFile,"### No switch found matching argsw '%s'.\n",argsw);
				fflush(GSDebugFile);
#endif
				// No match found.  Check if valid numeric switch (-n type).
				htab = state.htab;
				while((hrec = heach(&htab)) != NULL) {
					sw = (ss = ssptr(hrec))->sw;
					if((sw->flags & (SF_NumericSwitch | SF_PlusType)) == SF_NumericSwitch) {
						if(*strpspn(argsw,digits) == '\0') {
							tabsw = NSMinusKey;
							goto MatchFound;
							}
						if(isdigit(*argsw))
							goto BadNumericSwitch;
						break;
						}
					}

				// No numeric switch in switch table or wrong type.
				goto UnknownSwitch;
MatchFound:
#ifdef GSDebugFile
				fprintf(GSDebugFile,"### Match found, checking if -%s is a duplicate...\n",tabsw);
				fflush(GSDebugFile);
#endif
				// Found match; check for duplicate (ss and tabsw are set).
				sw = ss->sw;
				if(++ss->foundCount > 1 && !(sw->flags & SF_AllowRepeat))
					goto DupNotAllowed;
#ifdef GSDebugFile
				fputs("### Not a duplicate, updating argument pointers...\n",GSDebugFile);
				fflush(GSDebugFile);
#endif
				// Not a duplicate or repeat allowed; update argument pointers.
				--*argcp;
				++*argvp;
				switchIndex = sw - state.sw + 1;

#ifdef GSDebugFile
				fprintf(GSDebugFile,"### Checking switch type (switchIndex %d)...\n",switchIndex);
				fflush(GSDebugFile);
#endif
				// Numeric switch?
				if(sw->flags & SF_NumericSwitch)
					goto NumericSwitchReturn;

				// Simple switch?
				if((argType = sw->flags & SF_ArgMask) == SF_NoArg)
					goto NoArgReturn;

				// Keyword switch.
				stateExpected = (argType == SF_OptionalArg) ? expectOptionalArg : expectRequiredArg;
				break;
			case foundNullArg:

				// Null argument found.
				if(stateExpected == expectSwitch)
					goto NoSwitchesLeft;

				// Validate argument.
				if(!(sw->flags & SF_AllowNullArg))
					goto NullNotAllowed;

				// Return keyword switch and argument.
				--*argcp;
				++*argvp;
				goto ArgReturn;
			case foundArg:

				// Non-null argument found.
				if(stateExpected == expectSwitch)
					goto NoSwitchesLeft;

				// Validate argument.
				if(sw->flags & SF_NumericArg) {
					str0 = arg;
					if(*str0 == '-' || *str0 == '+') {
						if(!(sw->flags & SF_AllowSign))
							goto MustBeUnsigned;
						++str0;
						}
					if((str1 = strpspn(str0,digits)) == str0)
						goto MustBeNumeric;
					if(*str1 != '\0') {
						if(*str1 != '.')
							goto MustBeNumeric;
						if(!(sw->flags & SF_AllowDecimal))
							goto MustBeInteger;
						if(*strpspn(str1 + 1,digits) != '\0')
							goto MustBeNumeric;
						}
					}

				// Return keyword switch and argument.
				--*argcp;
				++*argvp;
				goto ArgReturn;
			}
		}

NoSwitchesLeft:

	// No switch arguments left; check for required switch(es).
	htab = state.htab;
	while((hrec = heach(&htab)) != NULL) {
		sw = (ss = ssptr(hrec))->sw;
		if((sw->flags & SF_RequiredSwitch) && ss->foundCount == 0)
			return (sw->flags & SF_NumericSwitch) ? emsgf(-1,"Numeric (%c) switch required",
			 (sw->flags & SF_PlusType) ? '+' : '-') : emsgf(-1,"-%s switch required",sw->namev[0]);
		}

	// Scan completed.  Delete SwitchState objects from hash, delete hash, reset static pointer, and return result.
	for(sw = state.sw; sw->flags != 0; ++sw) {
		hrec = hsearch(state.htab,!(sw->flags & SF_NumericSwitch) ? sw->namev[0] : (sw->flags & SF_PlusType) ?
		 NSPlusKey : NSMinusKey);
		free((void *) ssptr(hrec));
		}
	hfree(state.htab);
	state.sw = NULL;
	return 0;

	// Switch found.
NumericSwitchReturn:
#ifdef GSDebugFile
	fprintf(GSDebugFile,"### NumSwRtn: arg '%s'\n",arg);
	fflush(GSDebugFile);
#endif
	resultp->value = arg;
	argsw = NULL;
	goto SwitchFound;
NoArgReturn:
	resultp->value = NULL;
	goto SwitchFound;
ArgReturn:
	resultp->value = arg;
SwitchFound:
	resultp->name = argsw;
#ifdef GSDebugFile
	fprintf(GSDebugFile,"### RETURNING name '%s', value '%s', index %d...\n",resultp->name,resultp->value,switchIndex);
	fflush(GSDebugFile);
#endif
	return switchIndex;

	// Exception return.
MustBeNumeric:
	str0 = "numeric";
	goto MustBe;
MustBeUnsigned:
	str0 = "unsigned";
	goto MustBe;
MustBeInteger:
	str0 = "an integer";
MustBe:
	return emsgf(-1,"-%s switch value '%s' must be %s",argsw,arg,str0);
UnknownSwitch:
	return emsgf(-1,"Unknown switch, -%s",argsw);
BadNumericSwitch:
	return emsgf(-1,"Invalid numeric switch, %s",arg);
DupNotAllowed:
	return (sw->flags & SF_NumericSwitch) ? emsgf(-1,"Duplicate numeric switch, %s",arg) :
	 emsgf(-1,"Duplicate switch, -%s",argsw);
ValueRequired:
	return emsgf(-1,"-%s switch requires a value",argsw);
NullNotAllowed:
	return emsgf(-1,"-%s switch value cannot be null",argsw);
	}
