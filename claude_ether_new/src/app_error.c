#include "app_error.h"

#include <stdbool.h>

#define DEFINE_ERROR_TABLES
#include "thread_registry_errors.h"
#include "thread_status_errors.h"
#include "thread_result_errors.h"
#undef DEFINE_ERROR_TABLES

struct DomainTable {
    enum ErrorDomain domain;
    const ErrorTableEntry* entries;
    size_t count;
};

static struct DomainTable domain_tables[ERROR_DOMAIN_MAX];
static bool tables_initialized = false;

static void init_domain_tables(void) {
    if (!tables_initialized) {
        domain_tables[THREAD_REGISTRY_DOMAIN] = (struct DomainTable){
            .domain = THREAD_REGISTRY_DOMAIN,
            .entries = thread_registry_errors,
            .count = sizeof(thread_registry_errors) / sizeof(thread_registry_errors[0])
        };
        
        domain_tables[THREAD_STATUS_DOMAIN] = (struct DomainTable){
            .domain = THREAD_STATUS_DOMAIN,
            .entries = thread_status_errors,
            .count = sizeof(thread_status_errors) / sizeof(thread_status_errors[0])
        };
        
        domain_tables[THREAD_RESULT_DOMAIN] = (struct DomainTable){
            .domain = THREAD_RESULT_DOMAIN,
            .entries = thread_result_errors,
            .count = sizeof(thread_result_errors) / sizeof(thread_result_errors[0])
        };
        
        tables_initialized = true;
    }
}

const char* app_error_get_message(enum ErrorDomain domain, int code) {
    init_domain_tables();
    
    if (domain >= ERROR_DOMAIN_MAX) {
        return "Invalid error domain";
    }
    
    const struct DomainTable* table = &domain_tables[domain];
    for (size_t j = 0; j < table->count; j++) {
        if (table->entries[j].code == code) {
            return table->entries[j].message;
        }
    }
    
    return "Unknown error";
}
