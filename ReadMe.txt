You are looking at the ReadMe file for MightEMacs, an Emacs text editor which
runs on Unix and Linux platforms.  See file Install.txt for build and
installation instructions.  A C99 C compiler is required.

Notes
-----
The current MightEMacs distribution can be downloaded from
https://github.com/italia389/MightEMacs.git.  This distribution is version
8.0.0, which is the first public release of the editor.   Binaries are included
for Red Hat Linux (RHEL 5.5 and later) and OS X (Intel 10.6 and later).  The
sources should compile on other platforms as well; however, this has not been
tested and there are bound to be some (hopefully minor) issues which will need
to be resolved.

If you are compiling on a platform other than Red Hat Linux or OS X and
encounter any problems, please contact me with the details.  I would like to
have a clean port of MightEMacs for all major Unix and Linux platforms at a
minimum.

Note that a library (geeklib) is included which contains a few routines that are
used by the editor.  The most important ones are in valobj.c.  These functions
manage "Value" objects, which are strings that can be of any length and built on
the fly.  Geeklib contains those and other generic functions that can be used in
any C program.  The library is independent of MightEMacs, but included with it
for convenience.

MightEMacs has been in private development for a couple of years.  I have not
felt that it was ready for public release until now.  My goals were to:

    * Make the editor very robust and bug free.
    * Develop a simple but powerful scripting language that is easy to learn,
      allows customization of as many aspects of the editor as possible, and
      finalize it (or least get it to a point where there shouldn't be major
      syntax changes in the future).
    * Add sufficient features and capabilities to make editing files (and code
      in particular) fast and easy.

I believe I have accomplished these goals so I am now making it available to
anyone else who is interested.  I would definitely like to get user feedback.
If you have the time, please let me know what you think of it, good or bad, or
whatever you feel is worth mentioning.  Questions and feature requests are
welcome as well.  If you think something is illogical or lame, say so, but
please provide good arguments to support your opinion.  I will keep an open
mind.  Personally, I think MightEMacs is quite good and it has definitely made
me much more productive.  It is also a pleasure to use.  I hope you will feel
the same way.

Collaboration
-------------
As I mentioned previously, MightEMacs has been in private development thus far.
I am not interested in collaborating with others just yet, but definitely would
like feedback.  Future collaboration will depend on whether it attains a
"critical mass" of users who are genuinely interested in the editor and really
want to use it.  We'll see how that goes.  I do plan to continue development in
any case.

Credits
-------
MightEMacs (c) Copyright 2015 Richard W. Marinelli is based on code from
MicroEMACS 3.12 (c) Copyright 1988, 1989, 1990, 1991, 1992, 1993 Daniel M.
Lawrence, which was based on code by Dave G. Conroy, Steve Wilhite, and George
Jones.

MightEMacs is written by Rick Marinelli <italian389@yahoo.com>.

MicroEMACS was written by Dan Lawrence with contributions by Dana Hoggatt
and C. Smith.

MicroEMACS 3.12 can be copied and distributed freely for any non-commercial
purpose, but can be incorporated into commercial software only with the
permission of the current author.

See License.txt for the MightEMacs license.
