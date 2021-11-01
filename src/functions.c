#include "qvmd.h"

static qvm_function_t       *func_new(void);
qvm_function_t              *func_find(qvm_t *qvm, unsigned int address);
qvm_function_t              *func_add_syscall(qvm_t *qvm, unsigned int address);
int                         func_rename(qvm_function_t *func, char *name);
static qvm_function_list_t  *func_list_new(void);
static qvm_function_list_t  *func_list_find(qvm_function_list_t *list, qvm_function_t *func);
qvm_function_list_t         *func_list_add(qvm_function_list_t **list, qvm_function_t *func);

static qvm_function_t *func_new(void)
{
    qvm_function_t  *func;

    // allocate the function
    if (!(func = malloc(sizeof(*func)))) {
        printf("Error: Couldn't allocate new function.\n");
        return NULL;
    }

    // initialize the function
    func->address = 0;
    *func->name = 0;
    func->stack_size = 0;
    func->return_size = 0;
    func->opblock_start = NULL;
    func->opblock_end = NULL;
    func->locals = NULL;
    func->next = NULL;
    func->calls = NULL;
    func->called_by = NULL;

    // return the function
    return func;
}

qvm_function_t *func_find(qvm_t *qvm, unsigned int address)
{
    qvm_function_t  *sysc = qvm->syscalls;

    // search for function
    for (unsigned int i = 0; i < qvm->functions_count; i++) {
        if (qvm->functions[i].address == address)
            return &qvm->functions[i];
    }

    // search for syscall
    while (sysc) {
        if (sysc->address == address)
            return sysc;
        sysc = sysc->next;
    }

    // we didn't find it
    return NULL;
}

qvm_function_t *func_add_syscall(qvm_t *qvm, unsigned int address)
{
    qvm_function_t  *func;

    // check if the function already exist
    if ((func = func_find(qvm, address)))
        return func;

    // create the new function
    if (!(func = func_new()))
        return NULL;

    // set the function name and address
    func->address = address;
    sprintf(func->name, "trap_%x", address);

    // add the syscalls to the list
    func->next = qvm->syscalls;
    qvm->syscalls = func;

    // increase the syscalls count
    qvm->syscalls_count++;

    // success
    return func;
}

int func_rename(qvm_function_t *func, char *name)
{
    // check the name size
    if (strlen(name) >= sizeof(func->name)) {
        printf("Error: Couldn't rename function %s: Name too big.\n", func->name);
        return 0;
    }

    // rename the function
    sprintf(func->name, "%s", name);

    // success
    return 1;
}

static qvm_function_list_t *func_list_new(void)
{
    qvm_function_list_t  *list;

    // allocate the function
    if (!(list = malloc(sizeof(*list)))) {
        printf("Error: Couldn't allocate new function list.\n");
        return NULL;
    }

    // initialize the function list
    list->function = NULL;
    list->next = NULL;

    // return the function
    return list;
}

qvm_function_list_t *func_list_find(qvm_function_list_t *list, qvm_function_t *func)
{
    // search in the list
    while (list) {
        if (list->function == func)
            return list;
        list = list->next;
    }

    // we didn't find it
    return NULL;
}

qvm_function_list_t *func_list_add(qvm_function_list_t **list, qvm_function_t *func)
{
    qvm_function_list_t *fl;

    // find the element in function list
    if ((fl = func_list_find(*list, func)))
        return fl;

    // create the new function list element
    if (!(fl = func_list_new()))
        return NULL;

    // set the function list values
    fl->function = func;
    fl->next = *list;
    *list = fl;

    return fl;
}