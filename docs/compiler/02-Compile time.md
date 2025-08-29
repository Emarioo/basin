
Decisions around compile time.


## Here
- Emit can't write to stdout because compiler is multithreaded. If it wasn't we could set stdout to a temporary file, write to it in compile time expression, then read from it when we're done. This would mean any function that use std_print could be used to emit code. BUT compiler is multhtreaded and stdout is shared so we can't do this.