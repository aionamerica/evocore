#define _GNU_SOURCE
#include "evocore/domain.h"
#include "internal.h"
#include "evocore/log.h"
#include <stdio.h>
#include <string.h>

/*========================================================================
 * Domain Registry State
 * ======================================================================*/

typedef struct {
    evocore_domain_t domains[EVOCORE_MAX_DOMAINS];
    int count;
    bool initialized;
} domain_registry_t;

static domain_registry_t g_registry = {0};

/*========================================================================
 * Registry Management
 * ======================================================================*/

evocore_error_t evocore_domain_registry_init(void) {
    if (g_registry.initialized) {
        return EVOCORE_OK;
    }

    memset(&g_registry, 0, sizeof(g_registry));
    g_registry.count = 0;
    g_registry.initialized = true;

    evocore_log_debug("Domain registry initialized");
    return EVOCORE_OK;
}

void evocore_domain_registry_shutdown(void) {
    if (!g_registry.initialized) {
        return;
    }

    /* Free any allocated name strings */
    for (int i = 0; i < g_registry.count; i++) {
        if (g_registry.domains[i].name != NULL) {
            evocore_free((void*)g_registry.domains[i].name);
        }
        if (g_registry.domains[i].version != NULL) {
            evocore_free((void*)g_registry.domains[i].version);
        }
    }

    memset(&g_registry, 0, sizeof(g_registry));
    evocore_log_debug("Domain registry shut down");
}

/*========================================================================
 * Domain Registration
 * ======================================================================*/

evocore_error_t evocore_register_domain(const evocore_domain_t *domain) {
    if (!g_registry.initialized) {
        evocore_log_error("Domain registry not initialized");
        return EVOCORE_ERR_UNKNOWN;
    }

    if (domain == NULL) {
        evocore_log_error("Cannot register NULL domain");
        return EVOCORE_ERR_NULL_PTR;
    }

    if (domain->name == NULL) {
        evocore_log_error("Domain name cannot be NULL");
        return EVOCORE_ERR_INVALID_ARG;
    }

    /* Check for duplicate name */
    for (int i = 0; i < g_registry.count; i++) {
        if (evocore_string_equals(g_registry.domains[i].name, domain->name)) {
            evocore_log_warn("Domain '%s' already registered", domain->name);
            return EVOCORE_ERR_INVALID_ARG;
        }
    }

    /* Check capacity */
    if (g_registry.count >= EVOCORE_MAX_DOMAINS) {
        evocore_log_error("Maximum number of domains reached (%d)", EVOCORE_MAX_DOMAINS);
        return EVOCORE_ERR_POP_FULL;
    }

    /* Copy domain descriptor */
    int idx = g_registry.count;
    g_registry.domains[idx].name = evocore_strdup(domain->name);
    g_registry.domains[idx].version = domain->version ?
        evocore_strdup(domain->version) : evocore_strdup("1.0.0");
    g_registry.domains[idx].genome_size = domain->genome_size;
    g_registry.domains[idx].genome_ops = domain->genome_ops;
    g_registry.domains[idx].fitness = domain->fitness;
    g_registry.domains[idx].user_context = domain->user_context;
    g_registry.domains[idx].serialize_genome = domain->serialize_genome;
    g_registry.domains[idx].deserialize_genome = domain->deserialize_genome;
    g_registry.domains[idx].get_statistics = domain->get_statistics;

    if (g_registry.domains[idx].name == NULL ||
        g_registry.domains[idx].version == NULL) {
        evocore_log_error("Failed to allocate memory for domain name/version");
        /* Cleanup on error */
        evocore_free((void*)g_registry.domains[idx].name);
        evocore_free((void*)g_registry.domains[idx].version);
        return EVOCORE_ERR_OUT_OF_MEMORY;
    }

    g_registry.count++;

    evocore_log_info("Registered domain '%s' version %s (genome size: %zu)",
                   domain->name, g_registry.domains[idx].version,
                   domain->genome_size);

    return EVOCORE_OK;
}

evocore_error_t evocore_unregister_domain(const char *name) {
    if (!g_registry.initialized) {
        return EVOCORE_ERR_UNKNOWN;
    }

    if (name == NULL) {
        return EVOCORE_ERR_NULL_PTR;
    }

    /* Find the domain */
    int idx = -1;
    for (int i = 0; i < g_registry.count; i++) {
        if (evocore_string_equals(g_registry.domains[i].name, name)) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        evocore_log_warn("Domain '%s' not found for unregistration", name);
        return EVOCORE_ERR_CONFIG_NOT_FOUND;
    }

    /* Free name and version */
    evocore_free((void*)g_registry.domains[idx].name);
    evocore_free((void*)g_registry.domains[idx].version);

    /* Shift remaining domains down */
    for (int i = idx; i < g_registry.count - 1; i++) {
        g_registry.domains[i] = g_registry.domains[i + 1];
    }

    g_registry.count--;

    evocore_log_info("Unregistered domain '%s'", name);
    return EVOCORE_OK;
}

const evocore_domain_t* evocore_get_domain(const char *name) {
    if (!g_registry.initialized || name == NULL) {
        return NULL;
    }

    for (int i = 0; i < g_registry.count; i++) {
        if (evocore_string_equals(g_registry.domains[i].name, name)) {
            return &g_registry.domains[i];
        }
    }

    return NULL;
}

bool evocore_has_domain(const char *name) {
    return evocore_get_domain(name) != NULL;
}

int evocore_domain_count(void) {
    return g_registry.count;
}

const char* evocore_domain_name(int index) {
    if (!g_registry.initialized || index < 0 || index >= g_registry.count) {
        return NULL;
    }
    return g_registry.domains[index].name;
}

/*========================================================================
 * Convenience Helpers
 * ======================================================================*/

evocore_error_t evocore_domain_create_genome(const char *domain_name,
                                         evocore_genome_t *genome) {
    const evocore_domain_t *domain = evocore_get_domain(domain_name);
    if (domain == NULL) {
        evocore_log_error("Domain '%s' not found", domain_name);
        return EVOCORE_ERR_CONFIG_NOT_FOUND;
    }

    evocore_error_t err = evocore_genome_init(genome, domain->genome_size);
    if (err != EVOCORE_OK) {
        return err;
    }

    if (domain->genome_ops.random_init != NULL) {
        domain->genome_ops.random_init(genome, domain->user_context);
    }

    return EVOCORE_OK;
}

void evocore_domain_mutate_genome(evocore_genome_t *genome,
                                const evocore_domain_t *domain,
                                double rate) {
    if (domain == NULL || genome == NULL) {
        return;
    }

    if (domain->genome_ops.mutate != NULL) {
        domain->genome_ops.mutate(genome, rate, domain->user_context);
    }
}

double evocore_domain_evaluate_fitness(const evocore_genome_t *genome,
                                     const evocore_domain_t *domain) {
    if (domain == NULL || genome == NULL) {
        return 0.0;
    }

    if (domain->fitness != NULL) {
        return domain->fitness(genome, domain->user_context);
    }

    return 0.0;
}

double evocore_domain_diversity(const evocore_genome_t *a,
                              const evocore_genome_t *b,
                              const evocore_domain_t *domain) {
    if (domain == NULL || a == NULL || b == NULL) {
        return 0.0;
    }

    if (domain->genome_ops.diversity != NULL) {
        return domain->genome_ops.diversity(a, b, domain->user_context);
    }

    /* Default: use Hamming distance */
    size_t distance = 0;
    if (evocore_genome_distance(a, b, &distance) == EVOCORE_OK) {
        size_t min_size = a->size < b->size ? a->size : b->size;
        if (min_size > 0) {
            return (double)distance / (double)min_size;
        }
    }
    return 0.0;
}
