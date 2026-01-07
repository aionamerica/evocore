#ifndef EVOCORE_DOMAIN_H
#define EVOCORE_DOMAIN_H

#include "evocore/genome.h"
#include "evocore/fitness.h"
#include "evocore/error.h"

/*========================================================================
 * Domain System
 * ======================================================================
 *
 * The domain system allows problem-specific logic to be plugged into
 * the evocore framework. Each domain provides callbacks for genome
 * operations and fitness evaluation.
 *
 * Domains are registered by name and can be retrieved for use in
 * evolutionary runs.
 */

/**
 * Maximum number of domains that can be registered simultaneously
 */
#define EVOCORE_MAX_DOMAINS 16

/**
 * Genome operations vtable
 *
 * Domains provide these functions to tell evocore how to handle
 * their specific genome type.
 */
typedef struct {
    /**
     * Create a random genome
     *
     * @param genome    Genome to initialize (already allocated)
     * @param context   User-provided context
     */
    void (*random_init)(evocore_genome_t *genome, void *context);

    /**
     * Mutate a genome in-place
     *
     * @param genome    Genome to mutate
     * @param rate      Mutation rate (0.0 to 1.0)
     * @param context   User-provided context
     */
    void (*mutate)(evocore_genome_t *genome, double rate, void *context);

    /**
     * Crossover two genomes to produce offspring
     *
     * @param parent1   First parent genome
     * @param parent2   Second parent genome
     * @param child1    First child (already allocated)
     * @param child2    Second child (already allocated)
     * @param context   User-provided context
     */
    void (*crossover)(const evocore_genome_t *parent1,
                      const evocore_genome_t *parent2,
                      evocore_genome_t *child1,
                      evocore_genome_t *child2,
                      void *context);

    /**
     * Calculate genome diversity (0.0 to 1.0)
     *
     * @param a         First genome
     * @param b         Second genome
     * @param context   User-provided context
     * @return Diversity measure (0.0 = identical, 1.0 = completely different)
     */
    double (*diversity)(const evocore_genome_t *a,
                        const evocore_genome_t *b,
                        void *context);
} evocore_genome_ops_t;

/**
 * Domain descriptor
 *
 * Domains register with evocore to provide problem-specific logic.
 */
typedef struct {
    const char *name;              /* Domain name (e.g., "trading", "tsp") */
    const char *version;           /* Version string */

    size_t genome_size;            /* Expected genome size in bytes */

    evocore_genome_ops_t genome_ops; /* Genome operations */
    evocore_fitness_func_t fitness;  /* Fitness evaluation */

    void *user_context;            /* User-provided context pointer */

    /**
     * Optional: Serialize genome to string
     *
     * @param genome    Genome to serialize
     * @param buffer    Output buffer
     * @param size      Buffer size
     * @param context   User-provided context
     * @return Number of bytes written, or negative on error
     */
    int (*serialize_genome)(const evocore_genome_t *genome,
                            char *buffer,
                            size_t size,
                            void *context);

    /**
     * Optional: Deserialize genome from string
     *
     * @param buffer    Input buffer
     * @param genome    Genome to initialize
     * @param context   User-provided context
     * @return EVOCORE_OK on success, error code otherwise
     */
    evocore_error_t (*deserialize_genome)(const char *buffer,
                                        evocore_genome_t *genome,
                                        void *context);

    /**
     * Optional: Get genome statistics
     *
     * @param genome    Genome to analyze
     * @param buffer    Output buffer
     * @param size      Buffer size
     * @param context   User-provided context
     * @return Number of bytes written, or negative on error
     */
    int (*get_statistics)(const evocore_genome_t *genome,
                          char *buffer,
                          size_t size,
                          void *context);
} evocore_domain_t;

/*========================================================================
 * Domain Management API
 * ======================================================================*/

/**
 * Initialize the domain registry
 *
 * Must be called before any domain operations.
 * Safe to call multiple times.
 *
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_domain_registry_init(void);

/**
 * Shutdown the domain registry
 *
 * Cleans up all registered domains.
 */
void evocore_domain_registry_shutdown(void);

/**
 * Register a domain with evocore
 *
 * The domain structure is copied, so the original can be
 * stack-allocated.
 *
 * @param domain    Domain descriptor to register
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_register_domain(const evocore_domain_t *domain);

/**
 * Unregister a domain by name
 *
 * @param name      Domain name to unregister
 * @return EVOCORE_OK on success, EVOCORE_ERR_CONFIG_NOT_FOUND if not found
 */
evocore_error_t evocore_unregister_domain(const char *name);

/**
 * Get domain by name
 *
 * @param name      Domain name to lookup
 * @return Domain pointer, or NULL if not found
 */
const evocore_domain_t* evocore_get_domain(const char *name);

/**
 * Check if a domain is registered
 *
 * @param name      Domain name to check
 * @return true if registered, false otherwise
 */
bool evocore_has_domain(const char *name);

/**
 * Get number of registered domains
 *
 * @return Count of registered domains
 */
int evocore_domain_count(void);

/**
 * Get domain name by index
 *
 * @param index     Domain index (0 to count-1)
 * @return Domain name, or NULL if index out of range
 */
const char* evocore_domain_name(int index);

/*========================================================================
 * Domain Convenience Helpers
 * ======================================================================*/

/**
 * Create a random genome for a domain
 *
 * Convenience function that initializes a genome and calls
 * the domain's random_init callback.
 *
 * @param domain_name   Name of registered domain
 * @param genome        Genome to initialize
 * @return EVOCORE_OK on success, error code otherwise
 */
evocore_error_t evocore_domain_create_genome(const char *domain_name,
                                         evocore_genome_t *genome);

/**
 * Mutate a genome using its domain
 *
 * @param genome    Genome to mutate
 * @param domain    Domain to use for mutation
 * @param rate      Mutation rate
 */
void evocore_domain_mutate_genome(evocore_genome_t *genome,
                                const evocore_domain_t *domain,
                                double rate);

/**
 * Evaluate fitness using domain
 *
 * @param genome    Genome to evaluate
 * @param domain    Domain to use for fitness
 * @return Fitness value (higher is better, may be NaN)
 */
double evocore_domain_evaluate_fitness(const evocore_genome_t *genome,
                                     const evocore_domain_t *domain);

/**
 * Calculate diversity between two genomes
 *
 * @param a         First genome
 * @param b         Second genome
 * @param domain    Domain to use for diversity calculation
 * @return Diversity (0.0 = identical, 1.0 = completely different)
 */
double evocore_domain_diversity(const evocore_genome_t *a,
                              const evocore_genome_t *b,
                              const evocore_domain_t *domain);

#endif /* EVOCORE_DOMAIN_H */
