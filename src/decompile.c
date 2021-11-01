#include "qvmd.h"

int         qvm_decompile(qvm_t *qvm, char *filename);
static void qvm_decompile_header(qvm_t *qvm, file_t *file);
static void qvm_decompile_globals(qvm_t *qvm, file_t *file);
static void qvm_decompile_functions(qvm_t *qvm, file_t *file);
static void qvm_decompile_function_header(file_t *file, qvm_function_t *func);
static void qvm_decompile_function_code(file_t *file, qvm_function_t *func);
static void qvm_decompile_function_locals(file_t *file, qvm_function_t *func);

int qvm_decompile(qvm_t *qvm, char *filename)
{
    file_t  *file;

    printf("Decompilling QVM to %s...", filename);
    fflush(stdout);

    // create the output file
    if (!(file = file_create(filename)))
        return 0;

    // print the header in file
    qvm_decompile_header(qvm, file);

    // print all globals in file
    qvm_decompile_globals(qvm, file);

    // print all functions code in file
    qvm_decompile_functions(qvm, file);

    // free the created file
    file_free(file);

    printf("Success.\n");

    // success
    return 1;
}

static void qvm_decompile_header(qvm_t *qvm, file_t *file)
{
    file_print(file, "/*\n");
    file_print(file, "\tQVM Decompiler " QVMD_VERSION " by zen\n\n");
    file_print(file, "\tName: %s\n", qvm->file->name);
    file_print(file, "\tOpcodes Count: %i\n", qvm->header->instructions_count);
    // TODO: Opblocks Count
    file_print(file, "\tFunctions Count: %i\n", qvm->functions_count);
    file_print(file, "\tSyscalls Count: %i\n", qvm->syscalls_count);
    file_print(file, "\tGlobals Count: %i\n", qvm->globals_count);
    file_print(file, "\tCalls Restored: %.2f\n", qvm->restored_calls_perc);
    file_print(file, "*/\n\n");
}

static void qvm_decompile_globals(qvm_t *qvm, file_t *file)
{
    qvm_variable_t      *var;
    qvm_function_list_t *list;

    // get the variables list
    var = qvm->globals;

    // browse all variables
    while (var) {
        // print variable type
        if (var->size == 4)
            file_print(file, "int\t\t");
        else if (var->size == 2)
            file_print(file, "short\t");
        else
            file_print(file, "char\t");

        // print variable name
        file_print(file, "%s", var->name);

        // print variable size if needed
        if (var->size > 4 || var->size < 1 || var->size == 3)
            file_print(file, "[%u]", var->size);

        // print variable content if needed
        if (var->address < qvm->sections[S_DATA].length) {
            file_print(file, " = ");
            if (var->size == 1)
                file_print(file, "%hhi", *var->content);
            else if (var->size == 2)
                file_print(file, "%hi", *(short *)var->content);
            else if (var->size == 4)
                file_print(file, "%i", *(int *)var->content);
            else {
                file_print(file, "\"");
                for (unsigned int i = 0; i < var->size; i++)
                    file_print(file, "\\x%02hhx", var->content[i]);
                file_print(file, "\"");
            }
        }

        // print a semicolon to end the line
        file_print(file, ";");

        // print the variable 'used by' comments
        list = var->parents;
        if (list)
            file_print(file, " // Used by: ");
        while (list) {
            if (list != var->parents)
                file_print(file, ", ");
            file_print(file, "%s", list->function->name);
            list = list->next;
        }

        // go to the next line
        file_print(file, "\n");

        // go to the next variable
        var = var->next;
    }
    
    // print an end of line after all the variables
    file_print(file, "\n");
}

static void qvm_decompile_functions(qvm_t *qvm, file_t *file)
{
    qvm_function_t  *func;

    // browse all functions
    for (unsigned int i = 0; i < qvm->functions_count; i++) {
        // get the current function
        func = &qvm->functions[i];

        // print the function header
        qvm_decompile_function_header(file, func);

        // print the function code
        qvm_decompile_function_code(file, func);

        // print an end of line after the functions
        file_print(file, "\n");
    }
}

static void qvm_decompile_function_header(file_t *file, qvm_function_t *func)
{
    qvm_function_list_t *list;

    // print header format
    file_print(file, "/*\n");
    file_print(file, "=================\n");

    // print function name
    file_print(file, "%s\n\n", func->name);

    // print function address
    file_print(file, "Address: 0x%x\n", func->address);

    // print function stack size
    file_print(file, "Stack Size: 0x%x\n", func->stack_size);

    // TODO: print function opcodes count

    // TODO: print function opblocks count

    // TODO: print function locals count

    // print function calls
    if (func->calls) {
        list = func->calls;
        file_print(file, "Calls: ");
        while (list) {
            if (list != func->calls)
                file_print(file, ", ");
            file_print(file, "%s", list->function->name);
            list = list->next;
        }
        file_print(file, "\n");
    }

    // print function called by
    if (func->called_by) {
        list = func->called_by;
        file_print(file, "Called by: ");
        while (list) {
            if (list != func->called_by)
                file_print(file, ", ");
            file_print(file, "%s", list->function->name);
            list = list->next;
        }
        file_print(file, "\n");
    }

    // print header format
    file_print(file, "=================\n");
    file_print(file, "*/\n");
}

static void qvm_decompile_function_code(file_t *file, qvm_function_t *func)
{
    qvm_opblock_t   *opb;

    // browse all function opblocks
    for (opb = func->opblock_start; opb != func->opblock_end; opb = opb->next) {
        // if the opblock have opcodes
        if (opb->opcodes_count) {
            // print the tab if needed
            if (opb->info->id != OPB_FUNC_ENTER && opb->info->id != OPB_FUNC_LEAVE && opb->info->id != OPB_FUNC_ARG)
                file_print(file, "\t");
                
            // print the decompiled opblock
            opb_print(file, opb);

            // print the semicolon if needed
            if (opb->info->id != OPB_FUNC_ENTER && opb->info->id != OPB_FUNC_LEAVE && opb->info->id != OPB_FUNC_ARG)
                file_print(file, ";");

            // print an end of line after the opblock code
            file_print(file, "\n");
        }

        // if the opblock is a jumppoint
        if (opb->info->id == OPB_JUMP_POINT) {
            // print the jumppoint code
            opb_print(file, opb);

            // print an end of line after the jumppoint
            file_print(file, "\n");
        }

        // if the opblock is a function enter
        if (opb->info->id == OPB_FUNC_ENTER)
            qvm_decompile_function_locals(file, func);
    }
}

static void qvm_decompile_function_locals(file_t *file, qvm_function_t *func)
{
    qvm_variable_t  *var;

    // print all locals
    var = func->locals;
    while (var && var->address < func->stack_size) {
        if (var->size == 4)
            file_print(file, "\tint\t\t%s;\n", var->name);
        else if (var->size == 2)
            file_print(file, "\tshort\t%s;\n", var->name);
        else if (var->size == 1)
            file_print(file, "\tchar\t%s;\n", var->name);
        else
            file_print(file, "\tchar\t%s[%u];\n", var->name, var->size);
        var = var->next;
    }

    // print an end of line after the variables
    file_print(file, "\n");
}