You are looking at the ReadMe file for MightEMacs, an Emacs-style text editor
which runs on Unix and Linux platforms.  See file *Install.txt* for build and
installation instructions.  A C99 C compiler is required if you are not
installing on a Linux or macOS platform, or do not want to use the included
binaries.

History and Project Goals
-------------------------
MightEMacs is designed to be a fast and full-featured text editor.  Goals of the
project are to create an Emacs text editor that will do the following:

 1. Provide the ability to edit code quickly and easily with few keystrokes.
 2. Use key bindings that are well designed and intuitive.
 3. Be as easy as possible to learn.
 4. Be robust and powerful enough to perform sophisticated editing and
    automation tasks, provide a high level of extensibility, and yet not be
    overly complex.

MightEMacs is focused on editing files well and being easily extensible, but
also being an editor that is not daunting -- one that can be learned fairly
quickly by the average programmer.  The tradeoff is that it lacks some features
(and complexity) of other editors.  It uses a Text User Interface with no
special text formatting.  It is not an IDE and doesn't try to be.  It does
however, provide most of the things you would expect from an Emacs editor, like
the ability to create multiple buffers, windows, and screens, global and buffer
modes, a kill ring, search ring, keyboard macros, sophisticated navigation,
searching, and editing commands, multiple working directories, language-specific
auto-formatting, fence matching, extensibility, a built-in help system, and
more.

It also supports a C-like scripting language that is very powerful and fairly
easy to learn (assuming you already have programming experience).  The
distribution package includes several scripts as well that perform various
tasks, such as finding a file that contains a particular C function or Ruby
method definition and opening it at that location, naming and storing keyboard
macros in a file, or grepping for files and performing a global search and
replace on them.

See the `memacs(1)` man page and on-line help (via `ESC ?`) for further
information.

Distribution
------------
The current distribution of MightEMacs may be obtained at
`https://github.com/italia389/MightEMacs.git`.

Contact and Feedback
--------------------
User feedback is welcomed and encouraged.  If you have the time and interest,
please contact Rick Marinelli <italian389@yahoo.com> with your questions,
comments, bug reports, likes, dislikes, or whatever you feel is worth
mentioning.  Questions and feature requests are welcomed as well.  You may also
post questions or comments on the MightEMacs discussion forum on Reddit at
`http://reddit.com/r/memacs`.

Notes
-----
This distribution of MightEMacs is version 9.0.2.   64-bit binaries are included
for GNU/Linux variants (built on a CentOS vers. 6 machine) and macOS vers. 10.10
and later on Intel.  The sources should compile on other platforms as well;
however, this has not been tested and there may be some (hopefully minor) issues
which will need to be resolved.  If you are compiling on a platform other than
one of those listed above and encounter any problems, please contact the author
with the details.

Note that a portion of a library (ProLib) is included which contains routines
that are used by the editor.  The most important ones are in datum.c.  These
functions manage "Datum" objects, which are objects that contain data of various
types, including strings that can be of any length and built on the fly.  It
also includes array.c and hash.c, which contain routines for managing arrays and
hash tables, and getswitch.c, which contains a function for parsing and
validating command line switches.  Prolib contains these and other functions
that can be used in any C program.  The library is independent of MightEMacs,
but included with it for convenience.

Credits
-------
MightEMacs (c) Copyright 2018 Richard W. Marinelli was originally based on the
MicroEMACS 3.12 text editor (c) Copyright 1993 Daniel M. Lawrence, et al., but
it has been extensively rewritten and enhanced.  Consequently, little or none of
the original code remains.

MightEMacs is written by Rick Marinelli <italian389@yahoo.com>.

MicroEMACS was written by Dan Lawrence with contributions by Dana Hoggatt
and C. Smith.

See License.txt for the MightEMacs license.
