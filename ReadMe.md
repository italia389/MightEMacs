You are looking at the ReadMe file for MightEMacs, an Emacs-style text editor which runs on Unix and Linux platforms.  See file
*Install.txt* for build and installation instructions.  A C99 C compiler is required if you are not installing on a Linux or
macOS platform, or do not want to use the included binaries.

History and Project Goals
-------------------------
MightEMacs is designed to be a fast and full-featured text editor.  Goals of the project are to create an Emacs-style text
editor that will do the following:

 1. Provide the ability to edit code quickly and easily with few keystrokes.
 2. Use key bindings that are well designed and intuitive.
 3. Be as easy as possible to learn.
 4. Be robust and powerful enough to perform sophisticated editing and automation tasks, provide a high level of extensibility,
    and yet not be overly complex.

MightEMacs is focused on editing files well and being easily extensible, but also being an editor that is not daunting -- one
that can be learned fairly quickly by the average programmer.  The tradeoff is that it lacks some features (and complexity) of
other editors.  It uses a Text User Interface and is run from a terminal window.  It is not an IDE and doesn't try to be.  It is
however, very fast, robust, and provides most of the things you would expect from an Emacs editor, like the ability to create
multiple buffers, windows, and screens, built-in and user-defined modes of different types, a kill ring, search ring, keyboard
macros, sophisticated navigation, searching, and editing commands, multiple working directories, language-specific
auto-formatting, fence matching, extensibility, a built-in help system, and more.

It also supports a C-like scripting language that is very powerful and fairly easy to learn (assuming you already have
programming experience).  The distribution package includes several scripts as well that perform various tasks, such as finding
a file that contains a particular function or method definition and opening it at that location, naming and storing keyboard
macros in a file, wrapping a pair of fence characters or quotes around one or more words in the text, or grepping for files and
performing a global search and replace on them.

See the `memacs(1)` man page and on-line help (via `ESC ?`) for further information.

Examples
--------
Following are some examples of MightEMacs' features and how it can be used to edit code.  The cursor (point) is shown as (^)
and keys that are typed are shown in <b>bold</b>.  Control keys are shown as `C-X`, where `X` is usually a letter.  Note
that the `C-_` (control + underscore) key, which is used often, can be entered without holding down the shift key (control +
dash), which makes it much easier to type.

1. Duplicate a block of lines in JavaScript:

    <pre>
    switch(grade) {
        case 'A':
            "Good job"
            break(^)
    </pre>

    <pre>
    <b>C-_ 2</b> (n == -2)
    <b>ESC d</b> (duplicate current line + prior two)
    </pre>
    Yields:

    <pre>
    switch(grade) {
        case 'A':
            "Good job"
            break
        (^)case 'A':
            "Good job"
            break
    </pre>

2. Copy and paste in shell script:

    <pre>
    if [ $x -lt 0 ]; then
        echo 'Error: x cannot be negative' 1>&2
    (^)
    </pre>

    <pre>
    <b>C-SPC</b> (set mark)
    <b>C-p C-p</b> (move up two lines)
    <b>C-c C-l</b> (copy current line to kill ring)
    <b>C-x C-x</b> (swap point and mark)
    <b>C-u C-u</b> (n == 0)
    <b>C-y</b> (yank line that was copied without moving point)
    <b>el</b> (enter more text)
    </pre>
    Yields:

    <pre>
    if [ $x -lt 0 ]; then
        echo 'Error: x cannot be negative' 1>&2
    el(^)if [ $x -lt 0 ]; then
    </pre>

3. Cut and paste in C++:

    <pre>
    do {
        process(i);
        } while(++i < 5);(^)
    </pre>

    <pre>
    <b>ESC C-a</b> (move to beginning of text: "}")
    <b>C-f</b> (forward character)
    <b>C-d</b> (delete character at point: " ")
    <b>C-h C-k</b> (kill to end of current line)
    <b>C-_ C-_</b> (n == -2)
    <b>ESC C-a</b> (move to beginning of text two lines above)
    <b>C-d C-d C-y</b> (delete "do" and yank "while" clause)
    <b>DEL</b> (delete previous character: ";")
    </pre>
    Yields:

    <pre>
    while(++i < 5)(^) {
        process(i);
        }
    </pre>

4. Search forward and backward in Python:

    <pre>
    &#35; Print function.(^)
    def printme(str):
        "This prints a string passed into this function"
        print str
        return
    &nbsp;
    &#35; Call function.
    printme("Jack and Jill...")
    printme("went up the hill.")
    </pre>

    <pre>
    <b>C-u</b> (n == 2)
    <b>C-s print ESC</b> (search forward for "print" two times)
    <b>C-u 3</b> (n == 3)
    <b>C-]</b> (find next occurrence three more times)
    <b>C-r Call ESC</b> (search backward for "Call" once)
    </pre>
    Yields:

    <pre>
    &#35; Print function.
    def printme(str):
        "This prints a string passed into this function"
        print str
        return
    &nbsp;
    &#35; (^)Call function.
    printme("Jack and Jill...")
    printme("went up the hill.")
    </pre>

5. Replace a string pattern in Ruby:

    <pre>
    (^)files.each do |fileInfo|
        fileName = fileInfo[1]
        if fileInfo[0].nil?
            fileInfo[0] = fileName
        end
    end
    </pre>

    <pre>
    <b>C-u</b> (n == 2)
    <b>ESC r fileName ESC</b> (replace "fileName"...)
    <b>pathname ESC</b> (with "pathname" two times)
    </pre>
    Yields:

    <pre>
    files.each do |fileInfo|
        pathname = fileInfo[1]
        if fileInfo[0].nil?
            fileInfo[0] = pathname(^)
        end
    end
    </pre>

6. Query replace a regular expression pattern in C:

    <pre>
    if(flags & OpEval)(^)
        mset(mkp,NULL);
    else if(n >= 0)
        mset(mkp,wkbuf);
    else
        // mset(a,b) where a is a char buffer.
        mset(NULL,wkbuf);
    </pre>

    <pre>
    <b>ESC q</b> (query replace...)
    <b>(mset\()([^,]+),([^)]+):r ESC</b> ("from" regular expression...)
    <b>\1\3,\2 ESC</b> ("to" replacement pattern)
    <b>SPC SPC n SPC</b> (replace first two occurrences, skip third, do last)
    </pre>
    Yields (reversed <b>mset()</b> arguments):

    <pre>
    if(flags & OpEval)
        mset(NULL,mkp);
    else if(n >= 0)
        mset(wkbuf,mkp);
    else
        // mset(a,b) where a is a char buffer.
        mset(wkbuf,NULL(^));
    </pre>

7. Rewrap comment block in Java:

    <pre>
    // MightEMacs can rewrap
        // comment blocks composed of contiguous single-line
    // comments.  Indentation of first comment line is
    // used for whole block.  Also, the
    // prefixes (for example, "//" or "#") can be
            // conf(^)igured.
    </pre>

    <pre>
    <b>C-SPC</b> (set mark)
    <b>C-r wrap ESC</b> (search backward for "wrap" -- first line)
    <b>C-u 0</b> (n == 0)
    <b>ESC .</b> (rewrap lines in region)
    </pre>
    Yields:

    <pre>
    // MightEMacs can rewrap comment blocks composed of contiguous
    // single-line comments.  Indentation of first comment line is
    // used for whole block.  Also, the prefixes (for example, "//"
    // or "#") can be configured.(^)
    </pre>

8. Add numbers to a block of lines in Swift:

    <pre>
    /* Create array. */
    var attr:[String] = [
        "Name",
        "Street",
        "City",
        "State",
        "Home phone",
        "Cell phone"](^)
    </pre>

    <pre>
    <b>C-_ 5</b> (n == -5)
    <b>ESC #</b> (enumerate lines)
    <b>0 RTN 1 RTN /* %u %/ ESC</b> (enter parameters)
    </pre>
    Yields:

    <pre>
    /* Create array. */
    var attr:[String] = [
    /* 0 */    "Name",
    /* 1 */    "Street",
    /* 2 */    "City",
    /* 3 */    "State",
    /* 4 */    "Home phone",
    /* 5 */    "Cell phone"]
    (^)
    </pre>

9. Mark and swap positions in HTML:

    <pre>
    &lt;head&gt;
        &lt;title&gt;Example(^)&lt;/title&gt;
        Header.
    &lt;/head&gt;
    &lt;body&gt;
        &lt;h3&gt;Mark and Swap&lt;/h3&gt;
        Marks, which are visible characters like X or /, can be set
        at various places in a buffer and returned to later.  The
        default mark "." is also used to define a region.
    &lt;/body&gt;
    </pre>

    <pre>
    <b>C-SPC</b> (set mark "." -- default)
    <b>C-u ESC C-e</b> (move forward to end of second word: "Header")
    <b>C-u C-SPC x</b> (set mark "x")
    <b>C-u 7 C-e</b> (move down to end of seventh line)
    </pre>
    Yields:

    <pre>
    &lt;head&gt;
        &lt;title&gt;Example&lt;/title&gt;
        Header.
    &lt;/head&gt;
    &lt;body&gt;
        &lt;h3&gt;Mark and Swap&lt;/h3&gt;
        Marks, which are visible characters like X or /, can be set
        at various places in a buffer and returned to later.  The
        default mark "." is also used to define a region.(^)
    &lt;/body&gt;
    </pre>

    <pre>
    <b>C-x C-x</b> (swap point with default mark "." -- now at "Example")
    <b>C-x C-x</b> (swap again -- back at "region.")
    <b>C-u C-x C-x x</b> (swap with mark "x" -- now at "Header")
    <b>C-u C-x C-x x</b> (swap again -- back at "region.")
    <b>ESC SPC x</b> (go to mark "x")
    </pre>
    Yields:

    <pre>
    &lt;head&gt;
        &lt;title&gt;Example&lt;/title&gt;
        Header(^).
    &lt;/head&gt;
    &lt;body&gt;
        &lt;h3&gt;Mark and Swap&lt;/h3&gt;
        Marks, which are visible characters like X or /, can be set
        at various places in a buffer and returned to later.  The
        default mark "." is also used to define a region.
    &lt;/body&gt;
    </pre>

10. Multi-file query replace in C:

    <pre>
    <b>C-x C-g</b> (grep files...)
    <b>*.c</b> (enter filename template...)
    <b>mset( ESC</b> (enter search pattern)
    </pre>
    Yields (on message line):

    <pre>
    [10 files found: buffer.c, edit.c, ..., region.c, search.c]
    </pre>

    <pre>
    <b>C-_ ESC C-q</b> (query replace on files found...)
    <b>ESC</b> (accept "mset(" search pattern entered previously...)
    <b>markSet( ESC</b> (enter replacement pattern)
    <b>SPC SPC !</b> (do replacement twice, then do rest unprompted)
    (repeat on 9 remaining files)
    </pre>
    Yields (in all files):

    <pre>
    if(flags & OpEval)
        markSet(NULL,mkp);
    else if(n >= 0)
        markSet(wkbuf,mkp);
    else
        // markSet(a,b) where a is a char buffer.
        markSet((^)wkbuf,NULL);
    </pre>

11. Move text and change case of letters in PHP:

    <pre>
    $found = FALSE;
    if($r == 0)
        echo "No addresses found";
    else {
        echo "Complete address list";
        $found = TRUE;
        }(^)
    </pre>

    <pre>
    <b>C-_ C-_ C-a</b> (move up two lines to beginning of line)
    <b>ESC f</b> (move forward one word)
    <b>C-u 3 ESC C-t</b> (change next three words to title case)
    <b>C-p C-b</b> (move up one line and backward one character: "{")
    <b>C-h }</b> (kill fenced region: "{" through "}")
    <b>DEL</b> (delete previous character: " ")
    <b>C-p C-h C-l</b> (move to previous line and kill it)
    </pre>
    Yields:

    <pre>
    $found = FALSE;
    if($r == 0)
    (^)else
    </pre>

    <pre>
    <b>C-b</b> (move backward one character to end of previous line)
    <b>SPC C-_ C-y</b> (insert a space and yank -1th entry from kill ring)
    <b>C-u 3 C-a</b> (move to beginning of third line down)
    <b>C-y</b> (yank 0th entry from kill ring)
    <b>C-p C-x u</b> (move to previous line and convert it to upper case)
    <b>C-p</b> (move to beginning of previous line...)
    <b>ESC C-l</b> (and convert first word to lower case)
    </pre>
    Yields:

    <pre>
    $found = FALSE;
    if($r == 0) {
        echo "Complete Address List";
        $found = TRUE;
        }
    else
        echo(^) "NO ADDRESSES FOUND";
    </pre>

12. Other things you can do:

    <pre>
    <b>ESC C-\</b> (trim white space from end of all lines in buffer)
    <b>C-_ 4 TAB</b> (set tabbing style to four spaces)
    <b>C-u ESC )</b> (indent all lines in region two tab stops)
    <b>C-u 5 C-x C-e 4</b> (change 4-space tabs to hard tabs in next five lines)
    <b>C-x ( ... C-x ) C-u 10 C-x e</b> (record keystrokes, then play back 10 times)
    <b>C-x v sort RTN</b> (filter buffer through "sort" command and replace with result)
    <b>C-x ` date RTN</b> (execute "date" command and insert its output at point)
    <b>ESC t</b> (truncate buffer -- delete all text from point to end of buffer)
    <b>ESC u</b> (undelete -- restore most recently-deleted text)
    </pre>


Distribution
------------
The current distribution of MightEMacs may be obtained at `https://github.com/italia389/MightEMacs.git`.

Contact and Feedback
--------------------
User feedback is welcomed and encouraged.  If you have the time and interest, please contact Rick Marinelli
<italian389@yahoo.com> with your questions, comments, bug reports, likes, dislikes, or whatever you feel is worth mentioning.
Questions and feature requests are welcomed as well.  You may also post questions or comments on the MightEMacs discussion forum
on Reddit at `http://reddit.com/r/memacs`.

Notes
-----
This distribution of MightEMacs is version 9.2.0.  Installer packages containing 64-bit binaries are included for Linux
platforms and macOS vers. 10.10 and later.  The sources can be compiled as well if desired; however, the build process has not
been tested on other Unix platforms and there may be some (hopefully minor) issues which will need to be resolved.  If you are
compiling the sources and encounter any problems, please contact the author with the details.

Note that a portion of a library (ProLib) is included which contains routines that are used by the editor.  The most important
ones are in datum.c.  These functions manage "Datum" objects, which are objects that contain data of various types, including
strings that can be of any length and built on the fly.  It also includes array.c and hash.c, which contain routines for
managing arrays and hash tables, and getswitch.c, which contains a function for parsing and validating command line switches.
Prolib contains these and other functions that can be used in any C program.  The library is independent of MightEMacs, but
included with it for convenience.

Credits
-------
MightEMacs (c) Copyright 2019 Richard W. Marinelli was written by Rick Marinelli <italian389@yahoo.com>.

See License.txt for the MightEMacs license.
