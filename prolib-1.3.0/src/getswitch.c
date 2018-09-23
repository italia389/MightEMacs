// ProLib (c) Copyright 2017 Richard W. Marinelli
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

//#define GSDebugFile		stderr
#ifdef GSDebugFile
extern FILE *GSDebugFile;
#endif
// Get switch of form -sw1 val1 -sw2 -n +n...
//
// Arguments:
//	argcp		Pointer to count of switch strings in argvp array (typically &argc from main() function).
//	argvp		Indirect pointer to array of argument strings (typically &argp from main() function).
//	swtabp		Indirect pointer to switch table, which contains a descriptor for each valid switch.
//	resultp		Pointer to result record, containing primary switch name and its value, if any.
//
// Notes:
//  1. Caller sets flags to zero in the last descriptor of *swtabp to indicate end of table.
//  2. Each invocation of this routine returns one switch.  Routine sets *swtabp to NULL after first call to process subsequent
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
int getswitch(int *argcp,char ***argvp,SwitchDescriptor **swtabp,SwitchResult *resultp) {
	static char myname[] = "getswitch";
	char *arg,*argsw = NULL,*tabsw,**tabswp,*str0,*str1;
	int switchIndex = 0;
	ushort argType;
	enum {expectOptionalArg,expectRequiredArg,expectSwitch} stateExpected = expectSwitch;
	enum {foundArg,foundNoArg,foundNullArg,foundSwitch} stateFound;
	SwitchDescriptor *swdp;
	char *digits = "0123456789";
	static struct {
		SwitchDescriptor *swdp;		// Pointer to first descriptor in switch table.
		Hash *hp;			// Hash of switches expected and found (for duplicate checking).
		} state;


#ifdef GSDebugFile
fprintf(GSDebugFile,"%s(): argcp %.8X (%d), argvp %.8X, *argvp %.8X (%s)...\n",myname,
 (uint) argcp,*argcp,(uint) argvp,(uint) *argvp,**argvp);
#endif
	// Intialize variables and validate switch table if first call.
	if(*swtabp != NULL) {
#ifdef GSDebugFile
fputs("getswitch(): Initializing...\n",GSDebugFile);
#endif
		if(hnew(&state.hp,19) != 0)
			return -1;
		for(state.swdp = swdp = *swtabp; swdp->flags != 0; ++swdp) {
			if(swdp->flags & SF_NumericSwitch) {

				// Numeric switch.  Check if duplicate.
				tabsw = (swdp->flags & SF_PlusType) ? NSPlusKey : NSMinusKey;
				if(hsearch(state.hp,tabsw) != NULL)
					return emsgf(-1,"%s(): Multiple numeric (%c) switch descriptors found",myname,
					 (swdp->flags & SF_PlusType) ? '+' : '-');
				if(hcreate(state.hp,tabsw,NULL,NULL) != 0)
					return -1;
				}
			else {
				// Standard switch.  Check if argument type specified.
				if((swdp->flags & SF_ArgMask) == 0)
					return emsgf(-1,"%s(): Argument type not specified for -%s switch",myname,*swdp->namep);

				// Check switch name plus any aliases for duplicates.
				for(tabswp = swdp->namep; (tabsw = *tabswp) != NULL; ++tabswp) {
					if(hsearch(state.hp,tabsw) != NULL)
						return emsgf(-1,"%s(): Multiple -%s switch descriptors found",myname,tabsw);
					if(hcreate(state.hp,tabsw,NULL,NULL) != 0)
						return -1;
					}
				}
			}

		// Switch table is valid.  Clear hash table and table pointer.
		hclear(state.hp);
		*swtabp = NULL;
		}
	else if(state.swdp == NULL)
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
fprintf(GSDebugFile,"### Parsing arg '%s' (argc %d)...\n",arg,*argcp); fflush(GSDebugFile);
#endif
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
						for(swdp = state.swdp; swdp->flags != 0; ++swdp)
							if((swdp->flags & (SF_NumericSwitch | SF_PlusType)) ==
							 (SF_NumericSwitch | SF_PlusType)) {
								if(strpspn(arg + 2,digits) == NULL) {
									stateFound = foundSwitch;
									goto FoundDone;
									}
								goto BadNumericSwitch;
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
		// Compare expected and found states.
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
				fprintf(GSDebugFile,"*** Checking switch table for '%s'...\n",argsw);
fflush(GSDebugFile);
#endif
				// Check table for a match, skipping any numeric switches.
				for(swdp = state.swdp; swdp->flags != 0; ++swdp)
					if(swdp->namep != NULL) {

						// Check switch name plus any aliases.
						for(tabswp = swdp->namep; *tabswp != NULL; ++tabswp) {
							if(strcmp(argsw,*tabswp) == 0) {
								tabsw = *swdp->namep;
								goto MatchFound;
								}
							 }
						}

#ifdef GSDebugFile
fprintf(GSDebugFile,"### No switch found matching argsw '%s'.\n",argsw); fflush(GSDebugFile);
#endif
				// No match found.  Check if valid numeric switch (-n type).
				for(swdp = state.swdp; swdp->flags != 0; ++swdp)
					if((swdp->flags & (SF_NumericSwitch | SF_PlusType)) == SF_NumericSwitch) {
						if(strpspn(argsw,digits) == NULL) {
							tabsw = NSMinusKey;
							goto MatchFound;
							}
						if(isdigit(*argsw))
							goto BadNumericSwitch;
						break;
						}

				// No numeric switch in switch table or wrong type.
				goto UnknownSwitch;
MatchFound:
#ifdef GSDebugFile
	fprintf(GSDebugFile,"### Match found, checking hash table for key '%s'...\n",tabsw); fflush(GSDebugFile);
#endif
				// Found match; check for duplicate.
				if(!(swdp->flags & SF_AllowRepeat) && hsearch(state.hp,tabsw) != NULL)
					goto DupNotAllowed;
#ifdef GSDebugFile
	fputs("### Not a dup, adding to hash...\n",GSDebugFile); fflush(GSDebugFile);
#endif
				// Not a duplicate or repeat allowed; save it.
				if(hcreate(state.hp,tabsw,NULL,NULL) != 0)
					return -1;
				--*argcp;
				++*argvp;
				switchIndex = swdp - state.swdp + 1;

#ifdef GSDebugFile
	fprintf(GSDebugFile,"### Checking switch type (switchIndex %d)...\n",switchIndex); fflush(GSDebugFile);
#endif
				// Numeric switch?
				if(swdp->flags & SF_NumericSwitch)
					goto NumericSwitchReturn;

				// Simple switch?
				if((argType = swdp->flags & SF_ArgMask) == SF_NoArg)
					goto NoArgReturn;

				// Keyword switch.
				stateExpected = (argType == SF_OptionalArg) ? expectOptionalArg : expectRequiredArg;
				break;
			case foundNullArg:

				// Null argument found.
				if(stateExpected == expectSwitch)
					goto NoSwitchesLeft;

				// Validate argument.
				if(!(swdp->flags & SF_AllowNullArg))
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
				if(swdp->flags & SF_NumericArg) {
					str0 = arg;
					if(*str0 == '-' || *str0 == '+') {
						if(!(swdp->flags & SF_AllowSign))
							goto MustBeUnsigned;
						++str0;
						}
					if((str1 = strpspn(str0,digits)) == str0)
						goto MustBeNumeric;
					if(str1) {
						if(*str1 != '.')
							goto MustBeNumeric;
						if(!(swdp->flags & SF_AllowDecimal))
							goto MustBeInteger;
						if(strpspn(str1 + 1,digits))
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
	for(swdp = state.swdp; swdp->flags != 0; ++swdp) {
		if(swdp->flags & SF_RequiredSwitch) {
			tabsw = !(swdp->flags & SF_NumericSwitch) ? swdp->namep[0] : (swdp->flags & SF_PlusType) ? NSPlusKey :
			 NSMinusKey;
			if(hsearch(state.hp,tabsw) == NULL)
				return (swdp->flags & SF_NumericSwitch) ? emsgf(-1,"Numeric (%c) switch required",
				 (swdp->flags & SF_PlusType) ? '+' : '-') : emsgf(-1,"-%s switch required",tabsw);
			}
		}

	// Scan completed.  Delete hash, reset static pointer, and return result.
	hfree(state.hp);
	state.swdp = NULL;
	return 0;

	// Switch found.
NumericSwitchReturn:
#ifdef GSDebugFile
fprintf(GSDebugFile,"### NumSwRtn: arg '%s'\n",arg); fflush(GSDebugFile);
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
fprintf(GSDebugFile,"### RETURNING name '%s', value '%s', index %d...\n",resultp->name,resultp->value,switchIndex); fflush(GSDebugFile);
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
	return (swdp->flags & SF_NumericSwitch) ? emsgf(-1,"Duplicate numeric switch, %s",arg) :
	 emsgf(-1,"Duplicate switch, -%s",argsw);
ValueRequired:
	return emsgf(-1,"-%s switch requires a value",argsw);
NullNotAllowed:
	return emsgf(-1,"-%s switch value cannot be null",argsw);
	}
