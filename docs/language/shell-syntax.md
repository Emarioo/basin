









/*
    This script produces a release package of Ender Compute

    releases
        enderc-0.0.1-windows-x86
            bin/enderc
            public
                index.html
                ...
*/

import "basin" as basin
import "logger" as logger
import "os"

// alias, redefining 'RELEASES' is not allowed
RELEASES <- "releases"

global time: f32
const  pi:   f32 = 3.14

make_package RELEASES

// variable size struct
string :: struct {
    len: u8
    data: char[len]
}



FuncPtr :: fn rel_dir: string -> i32


make_package :: fn rel_dir: string -> i32 {

    local_sum: i32
    for nr in 0..10
        local_sum += nr;
    
    VERSION <- "0.0.1"
    TARGET  <- "windows-x86_64"
    PACKAGE <- f"{rel_dir}/enderc-{VERSION}-{TARGET}"

    SRC <- dirname __file__

    mkdir PACKAGE
    
    logger.print "Compiling" file = logger.stderr
    

    basin.compile f"{SRC}/main.bsn", basin.inherit_dirs __file__, debug = true

    basin.compile f"{SRC}/main.bsn", (basin.inherit_dirs __file__)[0:2], debug = true
    basin.compile f"{SRC}/main.bsn", basin.inherit_dirs(__file__)[0:2], debug = true

    basin.compile f"{SRC}/main.bsn", { dirs := basin.inherit_dirs __file__; dirs }, debug = true

    basin.compile f"{SRC}/main.bsn", { dirs := basin.inherit_dirs __file__; [ dirs[0], dirs[-1] ]}, debug = true

    basin.compile f"{SRC}/main.bsn", [ dirname __file__, (basin.inherit_dirs __file__)[0] ], debug = true

}


// basin.bsn

compile :: fn file: string, import_dirs: string[], debug: bool = false {
    ...
}

length_of_strings :: fn strs: string[] -> i32 { ... }

fn_ptr := tostring

length_of_strings __file__ tostring 234
// length_of_strings(__file__, tostring, 234)

length_of_strings __file__ fn_ptr 234
// length_of_strings(__file__, fn_ptr, 234)

length_of_strings __file__ (tostring) 234
// length_of_strings(__file__, fn_ptr, 234)   the expression '(to_string)' is of type function pointer

length_of_strings __file__ (tostring 234)
// length_of_strings(__file__, tostring(234))

length_of_strings __file__ , tostring 234
// length_of_strings(__file__, tostring(234))

length_of_strings __file__ , tostring, 234
// length_of_strings(__file__, tostring, 234)


length_of_strings __file__ tostring(234)

length_of_strings
    __file__
    tostring(234)
// length_of_strings; __file__; tostring(234)

length_of_strings(
    __file__,
    tostring(234)
)
// length_of_strings(__file__,tostring(234))

length_of_strings (
    __file__,
    tostring(234)
)
// length_of_strings( (__file__,tostring(234)) )

length_of_strings (
    __file__
    tostring(234)
)
// length_of_strings( (__file__(tostring(234)) )

[ length_of_strings __file__, tostring 234 ]
// [ length_of_strings(__file__, tostring(234)) ]

[ length_of_strings __file__ tostring, 234 ]
// [ length_of_strings(__file__, tostring, 234) ]
// [ length_of_strings(__file__, tostring), 234 ]


[ length_of_strings, other_call 23 ]
// [ length_of_strings, other_call(23) ]


mkdir :: fn path: string {
    _mkdir (
        if is_relative path
            f"{getcwd path}/{path}"
        else
            path
    )
    
}

_mkdir :: fn path: string { ... }



import "system" as system

map :: struct {
    
}

system.configure [
    networking.hostName = "mymachine"
    
    users["you"] = [
        packages = [ "vim", "git" ]
    ]
]

sub rsp, SystemConfig.SIZE

// networking.hostName
mov [rsp], "mymachine".len
mov [rsp+8], str_mymachine

// users
mov [rsp], "mymachine".len
mov [rsp+8], str_mymachine

lea rdi, [rsp]
call system__configure
