The driver and tasks.


# Driver



```c

// Parse tasks can be submitted at any time.
// They do not depend on anything.
task = PARSE
task.compilation
task.import
submit_task(task)

// IR tasks requires functions and types to be checked.
// Only the ones referenced by the AST are needed.
// BUT we might as well require all parse tasks
// to be done. MEANING, no other tasks
// can be done.
task = IR
task.compilation
task.ast
task.ast_function
submit_task(task)

// We require that all ir functions have been made.
// Or that all ir functions this function referencesd exists.
// Depending on comp time who knows.
task = MACHINE
task.compilation
task.ir_function
submit_task(task)

// We require that all machine functions have been made.
task = OBJECT
task.compilation
submit_task(task)


void thread_runner() {

    task = pick_fulfilled_task()

    switch task {
        case PARSE:

        case IR:

        case MACHINE:

        case OBJECT:

    }   
}

Task pick_fulfilled_task() {

    // efficiently pick a fulfilled task.

    // Perhaps two queues.

    // A queue with fulfilled tasks.

    // A queue with tasks that need dependency checking.

    // A queue per compilation or no?

}

```