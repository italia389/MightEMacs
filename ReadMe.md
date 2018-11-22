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
Following are some examples of MightEMacs' features and how it can be used to edit code.  The cursor (point) is shown as <span
style="color:red">(^)</span> and keys that are typed are shown in <span style="color:blue">blue</span>. Control keys are shown
as `C-X`, where `X` is usually a letter.  Note that `C-_` (control + underscore) can be typed without holding down the shift key
(control + dash).

1. Duplicate a block of lines in JavaScript:

    <pre>
    switch(grade) {
        case 'A':
            "Good job"
            break<span style="color:red">(^)</span>
    </pre>

    <pre>
    <code style="color:blue">C-_ 2</code> (n == -2)
    <code style="color:blue">ESC d</code> (duplicate current line + prior two)
    </pre>
    Yields:

    <pre>
    switch(grade) {
        case 'A':
            "Good job"
            break
        <span style="color:red">(^)</span>case 'A':
            "Good job"
            break
    </pre>

2. Copy and paste in shell script:

    <pre>
    if [ $x -lt 0 ]; then
        echo 'Error: x cannot be negative' 1>&2
    <span style="color:red">(^)</span>
    </pre>

    <pre>
    <code style="color:blue">C-SPC</code> (set mark)
    <code style="color:blue">C-p C-p</code> (move up two lines)
    <code style="color:blue">C-c C-l</code> (copy current line to kill ring)
    <code style="color:blue">C-x C-x</code> (swap point and mark)
    <code style="color:blue">C-u C-u</code> (n == 0)
    <code style="color:blue">C-y</code> (yank line that was copied without moving point)
    <code style="color:blue">el</code> (enter more text)
    </pre>
    Yields:

    <pre>
    if [ $x -lt 0 ]; then
        echo 'Error: x cannot be negative' 1>&2
    el<span style="color:red">(^)</span>if [ $x -lt 0 ]; then
    </pre>

3. Cut and paste in C++:

    <pre>
    do {
        process(i);
        } while(++i < 5);<span style="color:red">(^)</span>
    </pre>

    <pre>
    <code style="color:blue">ESC C-a</code> (move to beginning of text: "}")
    <code style="color:blue">C-f</code> (forward character)
    <code style="color:blue">C-d</code> (delete character at point: " ")
    <code style="color:blue">C-h C-k</code> (kill to end of current line)
    <code style="color:blue">C-_ C-_</code> (n == -2)
    <code style="color:blue">ESC C-a</code> (move to beginning of text two lines above)
    <code style="color:blue">C-d C-d C-y</code> (delete "do" and yank "while" clause)
    <code style="color:blue">DEL</code> (delete previous character: ";")
    </pre>
    Yields:

    <pre>
    while(++i < 5)<span style="color:red">(^)</span> {
        process(i);
        }
    </pre>

4. Search forward and backward in Python:

    <pre>
    &#35; Print function.<span style="color:red">(^)</span>
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
    <code style="color:blue">C-u</code> (n == 2)
    <code style="color:blue">C-s print ESC</code> (search forward for "print" two times)
    <code style="color:blue">C-u 3</code> (n == 3)
    <code style="color:blue">C-]</code> (find next occurrence three more times)
    <code style="color:blue">C-r Call ESC</code> (search backward for "Call" once)
    </pre>
    Yields:

    <pre>
    &#35; Print function.
    def printme(str):
        "This prints a string passed into this function"
        print str
        return
    &nbsp;
    &#35; <span style="color:red">(^)</span>Call function.
    printme("Jack and Jill...")
    printme("went up the hill.")
    </pre>

5. Replace a string pattern in Ruby:

    <pre>
    <span style="color:red">(^)</span>files.each do |fileInfo|
        fileName = fileInfo[1]
        if fileInfo[0].nil?
            fileInfo[0] = fileName
        end
    end
    </pre>

    <pre>
    <code style="color:blue">C-u</code> (n == 2)
    <code style="color:blue">ESC r fileName ESC</code> (replace "fileName"...)
    <code style="color:blue">pathname ESC</code> (with "pathname" two times)
    </pre>
    Yields:

    <pre>
    files.each do |fileInfo|
        pathname = fileInfo[1]
        if fileInfo[0].nil?
            fileInfo[0] = pathname<span style="color:red">(^)</span>
        end
    end
    </pre>

6. Query replace a regular expression pattern in C:

    <pre>
    if(flags & OpEval)<span style="color:red">(^)</span>
        mset(mkp,NULL);
    else if(n >= 0)
        mset(mkp,wkbuf);
    else
        // mset(a,b) where a is a char buffer.
        mset(NULL,wkbuf);
    </pre>

    <pre>
    <code style="color:blue">ESC q</code> (query replace...)
    <code style="color:blue">(mset\()([^,]+),([^)]+):r ESC</code> ("from" regular expression...)
    <code style="color:blue">\1\3,\2 ESC</code> ("to" replacement pattern)
    <code style="color:blue">SPC SPC n SPC</code> (replace first two occurrences, skip third, do last)
    </pre>
    Yields (reversed `mset()` arguments):

    <pre>
    if(flags & OpEval)
        mset(NULL,mkp);
    else if(n >= 0)
        mset(wkbuf,mkp);
    else
        // mset(a,b) where a is a char buffer.
        mset(wkbuf,NULL<span style="color:red">(^)</span>);
    </pre>

7. Rewrap comment block in Java:

    <pre>
    // MightEMacs can rewrap
        // comment blocks composed of contiguous single-line
    // comments.  Indentation of first comment line is
    // used for whole block.  Also, the
    // prefixes (for example, "//" or "#") can be
            // conf<span style="color:red">(^)</span>igured.
    </pre>

    <pre>
    <code style="color:blue">C-SPC</code> (set mark)
    <code style="color:blue">C-r wrap ESC</code> (search backward for "wrap" -- first line)
    <code style="color:blue">C-u 0</code> (n == 0)
    <code style="color:blue">ESC .</code> (rewrap lines in region)
    </pre>
    Yields:

    <pre>
    // MightEMacs can rewrap comment blocks composed of contiguous
    // single-line comments.  Indentation of first comment line is
    // used for whole block.  Also, the prefixes (for example, "//"
    // or "#") can be configured.<span style="color:red">(^)</span>
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
        "Cell phone"]<span style="color:red">(^)</span>
    </pre>

    <pre>
    <code style="color:blue">C-_ 5</code> (n == -5)
    <code style="color:blue">ESC #</code> (enumerate lines)
    <code style="color:blue">0 RTN 1 RTN /* %u %/ ESC</code> (enter parameters)
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
    <span style="color:red">(^)</span>
    </pre>

9. Mark and swap positions in HTML:

    <pre>
    &lt;head&gt;
        &lt;title&gt;Example<span style="color:red">(^)</span>&lt;/title&gt;
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
    <code style="color:blue">C-SPC</code> (set mark "." -- default)
    <code style="color:blue">C-u ESC C-e</code> (move forward to end of second word: "Header")
    <code style="color:blue">C-u C-SPC x</code> (set mark "x")
    <code style="color:blue">C-u 7 C-e</code> (move down to end of seventh line)
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
        default mark "." is also used to define a region.<span style="color:red">(^)</span>
    &lt;/body&gt;
    </pre>

    <pre>
    <code style="color:blue">C-x C-x</code> (swap point with default mark "." -- now at "Example")
    <code style="color:blue">C-x C-x</code> (swap again -- back at "region.")
    <code style="color:blue">C-u C-x C-x x</code> (swap with mark "x" -- now at "Header")
    <code style="color:blue">C-u C-x C-x x</code> (swap again -- back at "region.")
    <code style="color:blue">ESC SPC x</code> (go to mark "x")
    </pre>
    Yields:

    <pre>
    &lt;head&gt;
        &lt;title&gt;Example&lt;/title&gt;
        Header<span style="color:red">(^)</span>.
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
    <code style="color:blue">C-x C-g</code> (grep files...)
    <code style="color:blue">*.c</code> (enter filename template...)
    <code style="color:blue">mset( ESC</code> (enter search pattern)
    </pre>
    Yields (on message line):

    <pre>
    [10 files found: buffer.c, edit.c, ..., region.c, search.c]
    </pre>

    <pre>
    <code style="color:blue">C-_ ESC C-q</code> (query replace on files found...)
    <code style="color:blue">ESC</code> (accept "mset(" search pattern...)
    <code style="color:blue">markSet( ESC</code> (enter replacement pattern)
    <code style="color:blue">SPC SPC !</code> (do replacement twice, then do rest unprompted)
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
        markSet(<span style="color:red">(^)</span>wkbuf,NULL);
    </pre>

11. Move text and change case of letters in PHP:

    <pre>
    $found = FALSE;
    if($r == 0)
        echo "No addresses found";
    else {
        echo "Complete address list";
        $found = TRUE;
        }<span style="color:red">(^)</span>
    </pre>

    <pre>
    <code style="color:blue">C-_ C-_ C-a</code> (move up two lines to beginning of line)
    <code style="color:blue">C-u ESC f</code> (move forward two words)
    <code style="color:blue">C-u 3 ESC C-t</code> (change next three words to title case)
    <code style="color:blue">C-p C-b</code> (move up one line and backward one character: "{")
    <code style="color:blue">C-h }</code> (kill fenced region: "{" through "}")
    <code style="color:blue">DEL</code> (delete previous character: " ")
    <code style="color:blue">C-p C-h C-l</code> (move to previous line and kill it)
    </pre>
    Yields:

    <pre>
    $found = FALSE;
    if($r == 0)
    <span style="color:red">(^)</span>else
    </pre>

    <pre>
    <code style="color:blue">C-b</code> (move backward one character to end of previous line)
    <code style="color:blue">SPC C-_ C-y</code> (insert a space and yank -1th entry from kill ring)
    <code style="color:blue">C-u 3 C-a</code> (move to beginning of third line down)
    <code style="color:blue">C-y</code> (yank 0th entry from kill ring)
    <code style="color:blue">C-p C-x u</code> (move to previous line and convert it to upper case)
    <code style="color:blue">C-p</code> (move to beginning of previous line...)
    <code style="color:blue">ESC C-l</code> (and convert first word to lower case)
    </pre>
    Yields:

    <pre>
    $found = FALSE;
    if($r == 0) {
        echo "Complete Address List";
        $found = TRUE;
        }
    else
        echo<span style="color:red">(^)</span> "NO ADDRESSES FOUND";
    </pre>

12. Other things you can do:

    <pre>
    <code style="color:blue">ESC C-\</code> (trim white space from end of all lines in buffer)
    <code style="color:blue">C-_ 4 TAB</code> (set tabbing style to four spaces)
    <code style="color:blue">C-u ESC )</code> (indent all lines in region two tab stops)
    <code style="color:blue">C-u 5 C-x C-e 4</code> (change 4-space tabs to hard tabs in next five lines)
    <code style="color:blue">C-x ( ... C-x ) C-u 10 C-x e</code> (record keystrokes, then play back 10 times)
    <code style="color:blue">C-x v sort RTN</code> (filter buffer through "sort" command and replace with result)
    <code style="color:blue">C-x ` date RTN</code> (execute "date" command and insert its output at point)
    <code style="color:blue">ESC t</code> (truncate buffer -- delete all text from point to end of buffer)
    <code style="color:blue">ESC u</code> (undelete -- restore most recently-deleted text)
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
This distribution of MightEMacs is version 9.1.0.   64-bit binaries are included for GNU/Linux variants (built on a CentOS vers.
6 machine) and macOS vers. 10.10 and later on Intel.  The sources should compile on other platforms as well; however, this has
not been tested and there may be some (hopefully minor) issues which will need to be resolved.  If you are compiling on a
platform other than one of those listed above and encounter any problems, please contact the author with the details.

Note that a portion of a library (ProLib) is included which contains routines that are used by the editor.  The most important
ones are in datum.c.  These functions manage "Datum" objects, which are objects that contain data of various types, including
strings that can be of any length and built on the fly.  It also includes array.c and hash.c, which contain routines for
managing arrays and hash tables, and getswitch.c, which contains a function for parsing and validating command line switches.
Prolib contains these and other functions that can be used in any C program.  The library is independent of MightEMacs, but
included with it for convenience.

Credits
-------
MightEMacs (c) Copyright 2018 Richard W. Marinelli was originally based on the MicroEMACS 3.12 text editor (c) Copyright 1993
Daniel M. Lawrence, et al., but it has been extensively rewritten and enhanced.  Consequently, little or none of the original
code remains.

MightEMacs is written by Rick Marinelli <italian389@yahoo.com>.

MicroEMACS was written by Dan Lawrence with contributions by Dana Hoggatt and C. Smith.

See License.txt for the MightEMacs license.
