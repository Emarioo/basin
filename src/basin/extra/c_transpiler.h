/*
    This is the interface to compile C headers to internal Basin Compiler structures.
    
    Either we transpile to basin language code or the AST. AST is more efficient
    but having the language code could be useful. It lets you convert .h to .bsn files and
    then you can do some tweaks on it?
    
    Lets do parsing first.

    Interface is:
      same as basin?
      initial file, it compiles
*/


typedef struct CAST {

} CAST;

CAST* cts__compile(const char* path);

CAST* cts__compile_text(const char* text, int size);

