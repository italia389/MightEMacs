// GeekLib (c) Copyright 2015 Richard W. Marinelli
//
// This work is licensed under the Creative Commons Attribution-NonCommercial 4.0 International License.
// To view a copy of this license, see the "License.txt" file included with this distribution or visit
// http://creativecommons.org/licenses/by-nc/4.0/legalcode.
//
// geeklib.h - Header file for geek library.

#ifndef geeklib_h
#define geeklib_h

#include "geekdef.h"

#define VDebug		0		// Include debugging code for value objects.
#define VTest		0		// Use "caller-tracking" code and sanity checks for value objects.
#define VGet		0		// Include "get" value object routines (experimental).

// Expression value definitions, for creating and managing strings and integers allocated from heap space.
typedef struct substr {
	struct substr *ss_nextp;	// Link to next item in list.
	char ss_text[1];		// Substring (on heap).
	} SubStr;

typedef struct value {
	struct value *v_nextp;		// Link to next item in list.
	ushort v_type;			// Type of value.
	char *v_strp;			// Current string value if VALMINI or VALSTR; otherwise, NULL.
	union {				// Current value.
		long v_int;		// Integer.
		char v_str[sizeof(void *)];	// Self-contained mini-string.
		char *v_solop;		// Solo string (on heap).
		SubStr *v_sheadp;	// Head of string list.
		} u;
	} Value;

#define VALNIL		0x0001		// Nil value.
#define VALINT		0x0002		// Integer type.
#define VALMINI		0x0004		// Mini string.
#define VALSTR		0x0008		// Solo string.
#define VALSLIST	0x0010		// (Multi-item) string list.

#define VALSMASK	(VALMINI | VALSTR)	// Standard string types.

#if VTest
#define VALCHUNK	32		// Maximum size of string list chunks/pieces.  *TEST VALUE*
#else
#define VALCHUNK	128		// Maximum size of string list chunks/pieces.
#endif

typedef struct {
	Value *sl_vp;			// Value pointer.
	char *sl_strp;			// Next byte in current piece.
#if VGet
	char *sl_strpz;			// Last byte in current piece (for get).
	SubStr *sl_ssp;			// Next string piece (for get).
#endif
	char sl_buf[VALCHUNK];		// Work buffer (for put).
	} StrList;

// External variables.
extern Value *vgarbp;			// Head of list of temporary Value records ("garbage collection").

// External function declarations.
extern char *chlit(int c,bool wrap);
extern char *stplcpy(char *destp,char *srcp,size_t size);
extern char *strfit(char *destp,size_t maxlen,char *srcp,size_t srclen);
extern int vclose(StrList *slp);
extern int vcpy(Value *destp,Value *srcp);
#if VDebug
extern void vdelete(Value *vp,char *callerp);
#else
extern void vdelete(Value *vp);
#endif
#if 0
extern void vdelist(Value *vp);
#endif
#if VDebug
extern void vdump(Value *vp,char *label);
#endif
extern bool vempty(StrList *slp);
extern void vgarbpop(Value *vp);
#if VGet
extern int vgetc(StrList *slp);
#endif
#if VDebug
extern void vinit(Value *vp,char *callerp);
#else
extern void vinit(Value *vp);
#endif
extern bool visnil(Value *vp);
extern bool visnull(Value *vp);
#if VTest
extern int vnew(Value **vpp,bool perm,char *callerp,char *vnamep);
#else
extern int vnew(Value **vpp,bool perm);
#endif
extern int vnewstr(Value **vpp,char *strp);
extern void vnil(Value *vp);
extern void vnull(Value *vp);
#if VGet
extern int vopen(StrList *slp,Value *vp,bool put);
#elif VTest
extern int vopen(StrList *slp,Value *vp,bool append,char *callerp,char *vnamep);
#else
extern int vopen(StrList *slp,Value *vp,bool append);
#endif
extern int vputc(int c,StrList *slp);
extern int vputf(StrList *slp,char *fmtp,...);
extern int vputfs(char *strp,size_t len,StrList *slp);
extern int vputs(char *strp,StrList *slp);
extern int vputv(Value *vp,StrList *slp);
extern int vsalloc(Value *vp,size_t len);
extern void vsetchr(int c,Value *vp);
extern int vsetfstr(char *strp,size_t len,Value *vp);
extern void vsethstr(char *strp,Value *vp);
extern void vsetint(long i,Value *vp);
extern int vsetstr(char *strp,Value *vp);
extern int vshquote(Value *destp,char *srcp);
extern int vstrlit(StrList *destp,char *srcp,size_t len);
extern int vunputc(StrList *slp);
extern Value *vxfer(Value *destp,Value *srcp);
#endif
