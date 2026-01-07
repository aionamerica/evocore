/**
 * Evocore Parameter Synthesis Implementation
 *
 * Cross-context knowledge transfer and parameter hybridization.
 */

#define _GNU_SOURCE
#include "evocore/synthesis.h"
#include "evocore/log.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*========================================================================
 * Constants
 *========================================================================*/

#define DEFAULT_EXPLORATION 0.1
#define DEFAULT_TREND_STRENGTH 0.5
#define DEFAULT_ENSEMBLE_COUNT 3
#define DEFAULT_ADJUSTMENT 0.5
#define MIN_SIMILARITY 0.3
#define MAX_DISTANCE 1000.0

/*========================================================================
 * Synthesis Operations
 *========================================================================*/

evocore_synthesis_request_t* evocore_synthesis_request_create(
    evocore_synthesis_strategy_t strategy,
    size_t param_count,
    size_t source_count
) {
    if (param_count == 0 || source_count == 0) return NULL;

    evocore_synthesis_request_t *req = calloc(1, sizeof(evocore_synthesis_request_t));
    if (!req) return NULL;

    req->strategy = strategy;
    req->target_param_count = param_count;
    req->source_count = source_count;

    req->sources = calloc(source_count, sizeof(evocore_param_source_t));
    if (!req->sources) {
        free(req);
        return NULL;
    }

    req->exploration_factor = DEFAULT_EXPLORATION;
    req->trend_strength = DEFAULT_TREND_STRENGTH;
    req->ensemble_count = DEFAULT_ENSEMBLE_COUNT;

    req->result = calloc(param_count, sizeof(double));
    if (!req->result) {
        free(req->sources);
        free(req);
        return NULL;
    }

    req->synthesis_confidence = 0.0;

    return req;
}

void evocore_synthesis_request_free(evocore_synthesis_request_t *request) {
    if (!request) return;

    if (request->sources) {
        for (size_t i = 0; i < request->source_count; i++) {
            free(request->sources[i].parameters);
            free(request->sources[i].context_id);
        }
        free(request->sources);
    }

    free(request->result);
    free(request);
}

bool evocore_synthesis_add_source(
    evocore_synthesis_request_t *request,
    size_t index,
    const double *parameters,
    double confidence,
    double fitness,
    const char *context_id
) {
    if (!request || index >= request->source_count) return false;
    if (!parameters || confidence < 0.0 || confidence > 1.0) return false;

    evocore_param_source_t *source = &request->sources[index];

    source->parameters = calloc(request->target_param_count, sizeof(double));
    if (!source->parameters) return false;

    memcpy(source->parameters, parameters,
           request->target_param_count * sizeof(double));

    source->param_count = request->target_param_count;
    source->confidence = confidence;
    source->fitness = fitness;
    source->timestamp = time(NULL);

    if (context_id) {
        source->context_id = strdup(context_id);
    } else {
        source->context_id = NULL;
    }

    return true;
}

bool evocore_synthesis_execute(
    evocore_synthesis_request_t *request,
    double *out_parameters,
    double *out_confidence,
    unsigned int *seed
) {
    if (!request || !out_parameters) return false;
    if (!evocore_synthesis_validate(request)) return false;

    bool success = false;
    double confidence = 0.0;

    switch (request->strategy) {
        case EVOCORE_SYNTHESIS_AVERAGE:
            success = evocore_synthesis_average(request, out_parameters);
            confidence = 0.5;
            break;

        case EVOCORE_SYNTHESIS_WEIGHTED:
            success = evocore_synthesis_weighted(request, out_parameters);
            /* Weighted average confidence */
            confidence = 0.0;
            for (size_t i = 0; i < request->source_count; i++) {
                confidence += request->sources[i].confidence;
            }
            confidence /= (double)request->source_count;
            break;

        case EVOCORE_SYNTHESIS_TREND:
            success = evocore_synthesis_trend(request, out_parameters,
                                               request->trend_strength);
            confidence = 0.6;
            break;

        case EVOCORE_SYNTHESIS_REGIME:
            success = evocore_synthesis_regime(request, out_parameters, "");
            confidence = 0.7;
            break;

        case EVOCORE_SYNTHESIS_ENSEMBLE:
            success = evocore_synthesis_ensemble(request, out_parameters, seed);
            confidence = 0.8;
            break;

        case EVOCORE_SYNTHESIS_NEAREST:
            success = evocore_synthesis_nearest(request, out_parameters, "");
            confidence = 0.5;
            break;
    }

    if (!success) return false;

    /* Add exploration if requested */
    if (request->exploration_factor > 0.0 && seed) {
        for (size_t i = 0; i < request->target_param_count; i++) {
            double random_val = (double)rand_r(seed) / (double)RAND_MAX;
            out_parameters[i] = (1.0 - request->exploration_factor) * out_parameters[i]
                               + request->exploration_factor * random_val;
        }
        confidence *= (1.0 - request->exploration_factor * 0.5);
    }

    if (out_confidence) {
        *out_confidence = confidence;
    }

    return true;
}

/*========================================================================
 * Strategy Implementations
 *========================================================================*/

bool evocore_synthesis_average(
    const evocore_synthesis_request_t *request,
    double *out_parameters
) {
    if (!request || !out_parameters) return false;

    /* Initialize sums */
    for (size_t i = 0; i < request->target_param_count; i++) {
        out_parameters[i] = 0.0;
    }

    /* Sum all sources */
    for (size_t s = 0; s < request->source_count; s++) {
        const evocore_param_source_t *source = &request->sources[s];
        if (!source->parameters) continue;

        for (size_t i = 0; i < request->target_param_count; i++) {
            out_parameters[i] += source->parameters[i];
        }
    }

    /* Average */
    for (size_t i = 0; i < request->target_param_count; i++) {
        out_parameters[i] /= (double)request->source_count;
    }

    return true;
}

bool evocore_synthesis_weighted(
    const evocore_synthesis_request_t *request,
    double *out_parameters
) {
    if (!request || !out_parameters) return false;

    /* Calculate weight sum */
    double weight_sum = 0.0;
    for (size_t s = 0; s < request->source_count; s++) {
        weight_sum += request->sources[s].confidence;
    }

    if (weight_sum < 0.0001) {
        /* All zero confidence, fall back to average */
        return evocore_synthesis_average(request, out_parameters);
    }

    /* Weighted average */
    for (size_t i = 0; i < request->target_param_count; i++) {
        out_parameters[i] = 0.0;
        for (size_t s = 0; s < request->source_count; s++) {
            const evocore_param_source_t *source = &request->sources[s];
            if (!source->parameters) continue;

            double weight = source->confidence / weight_sum;
            out_parameters[i] += weight * source->parameters[i];
        }
    }

    return true;
}

bool evocore_synthesis_trend(
    const evocore_synthesis_request_t *request,
    double *out_parameters,
    double trend_strength
) {
    if (!request || !out_parameters) return false;
    if (request->source_count < 2) return false;

    /* Sort sources by timestamp (oldest first) */
    /* For now, assume sources are roughly in order */

    /* Calculate trend per parameter */
    for (size_t i = 0; i < request->target_param_count; i++) {
        double slope = 0.0;
        double weight_sum = 0.0;

        /* Linear regression for trend */
        double sum_x = 0.0, sum_y = 0.0, sum_xy = 0.0, sum_x2 = 0.0;
        size_t n = 0;

        for (size_t s = 0; s < request->source_count; s++) {
            const evocore_param_source_t *source = &request->sources[s];
            if (!source->parameters) continue;

            double x = (double)s;
            double y = source->parameters[i];
            double w = source->confidence;

            sum_x += w * x;
            sum_y += w * y;
            sum_xy += w * x * y;
            sum_x2 += w * x * x;
            weight_sum += w;
            n++;
        }

        if (n < 2 || weight_sum < 0.0001) {
            out_parameters[i] = request->sources[0].parameters[i];
            continue;
        }

        /* Calculate slope */
        double denom = weight_sum * sum_x2 - sum_x * sum_x;
        if (fabs(denom) > 0.0001) {
            slope = (weight_sum * sum_xy - sum_x * sum_y) / denom;
        }

        /* Project: use latest source + trend */
        double latest = request->sources[request->source_count - 1].parameters[i];
        out_parameters[i] = latest + slope * trend_strength;
    }

    return true;
}

bool evocore_synthesis_regime(
    const evocore_synthesis_request_t *request,
    double *out_parameters,
    const char *current_regime
) {
    (void)current_regime;  /* TODO: use regime info */

    if (!request || !out_parameters) return false;

    /* Select source with highest fitness */
    double best_fitness = -INFINITY;
    size_t best_source = 0;

    for (size_t s = 0; s < request->source_count; s++) {
        const evocore_param_source_t *source = &request->sources[s];
        if (source->fitness > best_fitness) {
            best_fitness = source->fitness;
            best_source = s;
        }
    }

    if (!request->sources[best_source].parameters) return false;

    /* Copy best source */
    memcpy(out_parameters, request->sources[best_source].parameters,
           request->target_param_count * sizeof(double));

    return true;
}

bool evocore_synthesis_ensemble(
    const evocore_synthesis_request_t *request,
    double *out_parameters,
    unsigned int *seed
) {
    if (!request || !out_parameters) return false;

    /* Combine multiple strategies */
    double *avg_result = calloc(request->target_param_count, sizeof(double));
    double *weighted_result = calloc(request->target_param_count, sizeof(double));

    if (!avg_result || !weighted_result) {
        free(avg_result);
        free(weighted_result);
        return false;
    }

    evocore_synthesis_average(request, avg_result);
    evocore_synthesis_weighted(request, weighted_result);

    /* Mix results */
    for (size_t i = 0; i < request->target_param_count; i++) {
        out_parameters[i] = 0.5 * avg_result[i] + 0.5 * weighted_result[i];
    }

    free(avg_result);
    free(weighted_result);

    return true;
}

bool evocore_synthesis_nearest(
    const evocore_synthesis_request_t *request,
    double *out_parameters,
    const char *target_context
) {
    (void)target_context;  /* TODO: use context matching */

    if (!request || !out_parameters) return false;
    if (request->source_count == 0) return false;

    /* For now, just return the first source */
    const evocore_param_source_t *source = &request->sources[0];
    if (!source->parameters) return false;

    memcpy(out_parameters, source->parameters,
           request->target_param_count * sizeof(double));

    return true;
}

/*========================================================================
 * Similarity and Distance
 *========================================================================*/

evocore_similarity_matrix_t* evocore_similarity_matrix_create(
    size_t context_count,
    char **context_ids
) {
    if (context_count == 0 || !context_ids) return NULL;

    evocore_similarity_matrix_t *matrix = calloc(1, sizeof(evocore_similarity_matrix_t));
    if (!matrix) return NULL;

    matrix->context_count = context_count;
    matrix->context_ids = context_ids;
    matrix->last_update = time(NULL);

    matrix->similarity_matrix = calloc(context_count, sizeof(double*));
    if (!matrix->similarity_matrix) {
        free(matrix);
        return NULL;
    }

    for (size_t i = 0; i < context_count; i++) {
        matrix->similarity_matrix[i] = calloc(context_count, sizeof(double));
        if (!matrix->similarity_matrix[i]) {
            for (size_t j = 0; j < i; j++) {
                free(matrix->similarity_matrix[j]);
            }
            free(matrix->similarity_matrix);
            free(matrix);
            return NULL;
        }
    }

    /* Initialize diagonal to 1.0 (self-similarity) */
    for (size_t i = 0; i < context_count; i++) {
        matrix->similarity_matrix[i][i] = 1.0;
    }

    return matrix;
}

void evocore_similarity_matrix_free(evocore_similarity_matrix_t *matrix) {
    if (!matrix) return;

    if (matrix->similarity_matrix) {
        for (size_t i = 0; i < matrix->context_count; i++) {
            free(matrix->similarity_matrix[i]);
        }
        free(matrix->similarity_matrix);
    }

    free(matrix);
}

bool evocore_similarity_update(
    evocore_similarity_matrix_t *matrix,
    const char *context_a,
    const char *context_b,
    double similarity
) {
    (void)matrix;
    (void)context_a;
    (void)context_b;
    (void)similarity;
    /* TODO: implement context ID lookup */
    return false;
}

double evocore_similarity_get(
    const evocore_similarity_matrix_t *matrix,
    const char *context_a,
    const char *context_b
) {
    (void)matrix;
    (void)context_a;
    (void)context_b;
    /* TODO: implement */
    return 0.5;
}

const char* evocore_similarity_find_nearest(
    const evocore_similarity_matrix_t *matrix,
    const char *target_context
) {
    (void)matrix;
    (void)target_context;
    /* TODO: implement */
    return NULL;
}

double evocore_param_distance(
    const double *params1,
    const double *params2,
    size_t count
) {
    if (!params1 || !params2 || count == 0) return 0.0;

    double sum_sq = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = params1[i] - params2[i];
        sum_sq += diff * diff;
    }

    return sqrt(sum_sq);
}

double evocore_param_similarity(
    const double *params1,
    const double *params2,
    size_t count,
    double max_distance
) {
    if (max_distance <= 0.0) max_distance = MAX_DISTANCE;

    double distance = evocore_param_distance(params1, params2, count);

    /* Convert to similarity using exponential decay */
    return exp(-distance / max_distance);
}

/*========================================================================
 * Transfer Learning
 *========================================================================*/

bool evocore_transfer_params(
    const double *source_params,
    const char *source_context,
    const char *target_context,
    const evocore_similarity_matrix_t *similarity_matrix,
    size_t param_count,
    double *out_params,
    double adjustment_factor
) {
    (void)source_context;
    (void)target_context;
    (void)similarity_matrix;

    if (!source_params || !out_params) return false;
    if (param_count == 0) return false;

    /* Simple copy with adjustment */
    for (size_t i = 0; i < param_count; i++) {
        out_params[i] = source_params[i] * (1.0 - adjustment_factor);
    }

    return true;
}

size_t evocore_find_transferable_contexts(
    const char *target_context,
    const evocore_similarity_matrix_t *similarity_matrix,
    double min_similarity,
    const char **out_contexts,
    size_t max_contexts
) {
    (void)target_context;
    (void)similarity_matrix;
    (void)min_similarity;
    (void)out_contexts;
    (void)max_contexts;
    /* TODO: implement */
    return 0;
}

/*========================================================================
 * Utility Functions
 *========================================================================*/

bool evocore_synthesis_validate(const evocore_synthesis_request_t *request) {
    if (!request) return false;
    if (request->target_param_count == 0) return false;
    if (request->source_count == 0) return false;

    /* Check at least one source has data */
    for (size_t i = 0; i < request->source_count; i++) {
        if (request->sources[i].parameters) {
            return true;
        }
    }

    return false;
}

const char* evocore_synthesis_strategy_name(evocore_synthesis_strategy_t strategy) {
    switch (strategy) {
        case EVOCORE_SYNTHESIS_AVERAGE: return "average";
        case EVOCORE_SYNTHESIS_WEIGHTED: return "weighted";
        case EVOCORE_SYNTHESIS_TREND: return "trend";
        case EVOCORE_SYNTHESIS_REGIME: return "regime";
        case EVOCORE_SYNTHESIS_ENSEMBLE: return "ensemble";
        case EVOCORE_SYNTHESIS_NEAREST: return "nearest";
        default: return "unknown";
    }
}

evocore_synthesis_cache_t* evocore_synthesis_cache_create(void) {
    return calloc(1, sizeof(evocore_synthesis_cache_t));
}

void evocore_synthesis_cache_free(evocore_synthesis_cache_t *cache) {
    if (!cache) return;

    if (cache->last_request) {
        evocore_synthesis_request_free(cache->last_request);
    }
    free(cache->cached_result);
    free(cache);
}

void evocore_synthesis_cache_clear(evocore_synthesis_cache_t *cache) {
    if (!cache) return;

    if (cache->last_request) {
        evocore_synthesis_request_free(cache->last_request);
        cache->last_request = NULL;
    }
    free(cache->cached_result);
    cache->cached_result = NULL;
    cache->cache_time = 0;
}
