#include "qvmd.h"

static qvm_variable_t   *var_new(void);
qvm_variable_t          *var_get(qvm_t *qvm, qvm_function_t *function, unsigned int address, unsigned int size, qvm_function_t *parent);
qvm_variable_t          *var_find(qvm_variable_t *list, unsigned int address);
static qvm_variable_t   *var_create(qvm_t *qvm, qvm_function_t *function, unsigned int address, unsigned int used_size, qvm_function_t *parent);
void                    var_rename(qvm_variable_t *var, char *name);
qvm_variable_t          *var_cut(qvm_t *qvm, qvm_function_t *function, unsigned int address);

static qvm_variable_t *var_new(void)
{
    qvm_variable_t  *var;

    // allocate a new variable
    if (!(var = malloc(sizeof(*var)))) {
        printf("Error: Couldn't allocate new variable.\n");
        return NULL;
    }

    // initialize the variable
    *var->name = 0;
    var->address = 0;
    var->prob_size[1] = 0;
    var->prob_size[2] = 0;
    var->prob_size[4] = 0;
    var->size = 0;
    var->content = NULL;
    var->next = NULL;
    var->parents = NULL;
    var->variadic = 0;

    // return the variable
    return var;
}

qvm_variable_t *var_get(qvm_t *qvm, qvm_function_t *function, unsigned int address, unsigned int size, qvm_function_t *parent)
{
    qvm_variable_t      *list;
    qvm_variable_t      *var;

    // get the variables list
    list = function ? function->locals : qvm->globals;

    // check if the variable already exist
    if (!(var = var_find(list, address))) {
        // create the variable if needed
        if (!(var = var_create(qvm, function, address, size, parent)))
            return NULL;
    } else {
        // add the variable parent
        if (parent && !func_list_add(&var->parents, parent))
            return NULL;
    }

    // return the variable
    return var;
}

qvm_variable_t *var_find(qvm_variable_t *list, unsigned int address)
{
    // find the variable from address
    for (; list; list = list->next)
        if (list->address == address)
            return list;

    // we didn't find it
    return NULL;
}

static qvm_variable_t *var_find_prev(qvm_variable_t *list, unsigned int address)
{
    qvm_variable_t  *prev = NULL;

    // browse the variables list given
    for (; list; list = list->next) {
        // if there is no previous variables
        if (!prev) {
            // check if the address is before the address wanted
            if (list->address < address)
                prev = list;
            continue;
        }

        // check if the address is before the address wanted and
        if (list->address < address && list->address > prev->address)
            prev = list;
    }

    // return the previous variable if any
    return prev;
}

static qvm_variable_t *var_create(qvm_t *qvm, qvm_function_t *function, unsigned int address, unsigned int used_size, qvm_function_t *parent)
{
    qvm_variable_t      **list;
    qvm_variable_t      *var;
    qvm_variable_t      *prev;

    // create a new variable
    if (!(var = var_new()))
        return NULL;

    // set the variable address
    var->address = address;

    // get the variables list
    list = function ? &function->locals : &qvm->globals;

    // set the variable name and status
    if (function) {
        if (address >= function->stack_size) {
            sprintf(var->name, "arg_%i", (address - function->stack_size - 8) / 4);
            var->status = VS_ARG;
        }
        else {
            sprintf(var->name, "local_%x", address);
            var->status = VS_LOCAL;
        }
    }
    else {
        if (address < qvm->sections[S_DATA].length) {
            sprintf(var->name, "global_%x", address);
            var->status = VS_GLOBAL;
        }
        else if (address < qvm->sections[S_DATA].length + qvm->sections[S_LIT].length) {
            sprintf(var->name, "lit_%x", address);
            var->status = VS_LITERAL;
        }
        else {
            sprintf(var->name, "bss_%x", address);
            var->status = VS_BSS;
        }
    }

    // set the variable content if needed
    if (!function && address < qvm->sections[S_DATA].length + qvm->sections[S_LIT].length)
        var->content = qvm->sections[S_DATA].content + address;

    // set the probable size
    if (used_size == 4 || used_size == 1 || used_size == 2)
        var->prob_size[used_size]++;

    // count the locals and globals
    if (function) {
        function->locals_count++;
        qvm->locals_count++;
    }
    else
        qvm->globals_count++;

    // add the variable in the list
    if (!(prev = var_find_prev(*list, address))) {
        var->next = *list;
        *list = var;
    } else {
        var->next = prev->next;
        prev->next = var;
    }

    // set the variable parent
    if (parent && !func_list_add(&var->parents, parent)) {
        free(var);
        return NULL;
    }

    // return the variable
    return var;
}

void var_rename(qvm_variable_t *var, char *name)
{
    // check the name size overflow
    if (strlen(name) >= sizeof(var->name)) {
        printf("Warning: Couldn't rename variable %s.\n", var->name);
        return;
    }

    // change the variable name
    sprintf(var->name, "%s", name);
}

qvm_variable_t *var_cut(qvm_t *qvm, qvm_function_t *function, unsigned int address)
{
    qvm_variable_t *var;
    qvm_variable_t *new_var;

    // search the variable to cut
    for (var = function ? function->locals : qvm->globals; var; var = var->next) {
        // check if the variable is already cut
        if (var->address == address)
            return var;

        // cut the variable if needed
        if (var->address < address) {
            if (!var->next || var->next->address > address) {
                // create a new variable
                if (!(new_var = var_create(qvm, function, address, 0, NULL)))
                    return NULL;

                // set the new var size
                new_var->size = var->size - (address - var->address);

                // change the variable size
                var->size = address - var->address;

                // return the new variable
                return new_var;
            }
        }
    }

    // failure
    printf("Error: Couldn't find variable %0x to cut it.\n", address);
    return NULL;
}
