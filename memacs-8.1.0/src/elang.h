// (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// elang.h	Language-specific definitions for MightEMacs.

// English is the only language currently supported.  However, others can be if english.h is translated to a new header
// file; for example, "spanish.h".
#if ENGLISH
#include "english.h"
#define LANGUAGE	"English"
//#elif SPANISH
//#include "spanish.h"
//#define LANGUAGE	"Spanish"
#else
#error 'ENGLISH macro not defined or not set to true'
#endif
