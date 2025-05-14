#include "../../include/include.h"
// #include "exec_domain.h"

E_exec_domain E_exec_domain_init(E_exec_domain exec_domain)
{
    LOG_PRINT("start to initiation exec domain......");
    E_exec_domain new_exec_domain_init = (E_exec_domain)checked_malloc(sizeof(*exec_domain));
    LOG_PRINT("end to initiation exec domain......");
    return new_exec_domain_init;
}