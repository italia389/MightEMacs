# (c) Copyright 2026 Richard W. Marinelli
#
# This work is licensed under the GNU General Public License (GPLv3).  To view a copy of this license, see the
# "License.txt" file included with this distribution or visit http://www.gnu.org/licenses/gpl-3.0.en.html.
#
# shLib.ms		Ver. 10.0.0
#	MightEMacs library for shell script editing.

##### Commands and Functions #####

# Wrap "if false; then" and "fi" around a block of lines.
command shWrapIf0(0) {description: 'Wrap ~#uif false; then~U and ~ufi~U around [-]n lines (default 1).'}

	$0 => langWrapPrep false, '', 'false; then', 'endif'
endroutine

# Wrap "if true; then" and "fi" around a block of lines.
command shWrapIf1(0) {description: 'Wrap ~#uif true; then~U and ~ufi~U around [-]n lines (default 1).'}
	$0 => langWrapPrep false, '', 'true; then', 'endif'
endroutine

# Wrap "if false; then" and "else" around a block of lines, duplicate them, and add "fi".
command shWrapIfElse(0) {description: 'Wrap ~#uif false; then~U and ~uelse~U around [-]n lines (default 1), duplicate them,\
 and add ~ufi~U.'}
	$0 => langWrapIfElse ''
endroutine

# Go to other end of block if current line contains a block begin/end symbol, such as "if".  Set mark '`' to current position,
# mark '^' to top line of block, mark ':' to "else" line (if any), and mark '$' to bottom line.
command shGotoBlkBound(0) {description: "Go to other end of block if current line contains a block begin/end symbol (such as\
 \"if\").  Set mark ~u`~U to current position, mark ~u^~U to top line of block, mark ~u:~U to \"else\" line (if any), and mark\
 ~u$~U to bottom line.\n\n\
Returns: true if successful, otherwise false."}
	$0 => langGotoBlkBound $langParams['Shell']
endroutine

# Done.  Return array of key bindings for "public" commands and array of "subroutine search" parameters (see comments at
# beginning of lang.ms for details).  The latter array is nil -- not used.
[["shGotoBlkBound\tC-c C-g", "shWrapIf0\tESC 0", "shWrapIf1\tESC 1", "shWrapIfElse\tESC 2"],\
 nil, nil, nil]
