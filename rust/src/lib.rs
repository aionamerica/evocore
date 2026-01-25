//! FFI bindings for EvoCore meta-evolutionary framework
//!
//! This crate provides Rust bindings to the EvoCore C library, enabling
//! meta-evolutionary optimization for adaptive AI behavior.

use std::ffi::{c_char, CString};
use std::ptr::NonNull;

// Opaque types for EvoCore structs
#[repr(C)]
pub struct evocore_genome_t {
    _private: [u8; 0],
}

#[repr(C)]
pub struct evocore_context_dimension_t {
    pub name: *mut c_char,
    pub value_count: usize,
    pub values: *mut *mut c_char,
}

#[repr(C)]
pub struct evocore_context_system_t {
    _private: [u8; 0],
}

#[repr(C)]
pub struct evocore_weighted_array_t {
    _private: [u8; 0],
}

#[repr(C)]
pub struct evocore_context_stats_t {
    _private: [u8; 0],
}

extern "C" {
    // Context system
    pub fn evocore_context_system_create(
        dimensions: *const evocore_context_dimension_t,
        dimension_count: usize,
        param_count: usize,
    ) -> *mut evocore_context_system_t;

    pub fn evocore_context_system_free(system: *mut evocore_context_system_t);

    pub fn evocore_context_add_dimension(
        system: *mut evocore_context_system_t,
        name: *const c_char,
        values: *const *const c_char,
        value_count: usize,
    ) -> bool;

    pub fn evocore_context_build_key(
        system: *const evocore_context_system_t,
        dimension_values: *const *const c_char,
        out_key: *mut c_char,
        key_size: usize,
    ) -> bool;

    // Learning operations
    pub fn evocore_context_learn(
        system: *mut evocore_context_system_t,
        dimension_values: *const *const c_char,
        parameters: *const f64,
        param_count: usize,
        fitness: f64,
    ) -> bool;

    pub fn evocore_context_learn_key(
        system: *mut evocore_context_system_t,
        context_key: *const c_char,
        parameters: *const f64,
        param_count: usize,
        fitness: f64,
    ) -> bool;

    // Sampling
    pub fn evocore_context_sample(
        system: *const evocore_context_system_t,
        dimension_values: *const *const c_char,
        out_parameters: *mut f64,
        param_count: usize,
        exploration_factor: f64,
        seed: *mut u32,
    ) -> bool;

    pub fn evocore_context_sample_key(
        system: *const evocore_context_system_t,
        context_key: *const c_char,
        out_parameters: *mut f64,
        param_count: usize,
        exploration_factor: f64,
        seed: *mut u32,
    ) -> bool;

    // Statistics
    pub fn evocore_context_get_stats(
        system: *mut evocore_context_system_t,
        dimension_values: *const *const c_char,
        out_stats: *mut *mut evocore_context_stats_t,
    ) -> bool;

    pub fn evocore_context_has_data(
        stats: *const evocore_context_stats_t,
        min_samples: usize,
    ) -> bool;

    // Persistence
    pub fn evocore_context_save_json(
        system: *const evocore_context_system_t,
        filepath: *const c_char,
    ) -> bool;

    pub fn evocore_context_load_json(
        filepath: *const c_char,
        out_system: *mut *mut evocore_context_system_t,
    ) -> bool;

    pub fn evocore_context_save_binary(
        system: *const evocore_context_system_t,
        filepath: *const c_char,
    ) -> bool;

    pub fn evocore_context_load_binary(
        filepath: *const c_char,
        out_system: *mut *mut evocore_context_system_t,
    ) -> bool;

    // Utility
    pub fn evocore_context_count(system: *const evocore_context_system_t) -> usize;
    pub fn evocore_context_get_param_count(system: *const evocore_context_system_t) -> usize;
}

/// Simple Rust wrapper for EvoCore context system
///
/// This provides a simplified interface for the Yue use case.
pub struct EvoCoreContextSystem {
    inner: NonNull<evocore_context_system_t>,
    param_count: usize,
}

impl EvoCoreContextSystem {
    /// Create a new context system
    ///
    /// # Arguments
    /// * `dimension_names` - Names of dimensions (e.g., ["type", "domain", "tools"])
    /// * `dimension_values` - Possible values for each dimension
    /// * `param_count` - Number of parameters to track
    pub fn new(
        dimension_names: &[&str],
        dimension_values: &[Vec<&str>],
        param_count: usize,
    ) -> Result<Self, String> {
        if dimension_names.len() != dimension_values.len() {
            return Err("Dimension names and values must have same length".to_string());
        }

        unsafe {
            // Build dimension structures for the C API
            // Based on the pattern from test_context.c
            let mut dims: Vec<evocore_context_dimension_t> = Vec::with_capacity(dimension_names.len());

            // We need to keep the CString and pointer data alive during the call
            let mut _value_strings: Vec<Vec<CString>> = Vec::with_capacity(dimension_values.len());
            let mut _value_ptrs: Vec<Vec<*mut i8>> = Vec::with_capacity(dimension_values.len());

            for (name, values) in dimension_names.iter().zip(dimension_values.iter()) {
                let c_name = CString::new(*name).unwrap();
                let c_values: Vec<CString> =
                    values.iter().map(|v| CString::new(*v).unwrap()).collect();
                let c_ptrs: Vec<*mut i8> = c_values.iter().map(|s| s.as_ptr() as *mut i8).collect();

                // Create the dimension struct - note we take ownership of the c_name pointer
                let dim = evocore_context_dimension_t {
                    name: c_name.into_raw(),
                    value_count: c_values.len(),
                    values: c_ptrs.as_ptr() as *mut *mut i8,
                };

                _value_ptrs.push(c_ptrs);
                _value_strings.push(c_values);
                dims.push(dim);
            }

            // Create context system with dimensions
            let system = evocore_context_system_create(
                dims.as_ptr(),
                dims.len(),
                param_count,
            );

            if system.is_null() {
                // Clean up allocated name strings
                for dim in dims {
                    let _ = CString::from_raw(dim.name);
                }
                return Err("Failed to create context system".to_string());
            }

            Ok(Self {
                inner: NonNull::new(system).expect("context system was null"),
                param_count,
            })
        }
    }

    /// Learn from experience with parameters
    ///
    /// # Arguments
    /// * `dimension_values` - Values for each dimension
    /// * `parameters` - Parameter values that were used
    /// * `fitness` - Fitness score (higher is better)
    pub fn learn(
        &mut self,
        dimension_values: &[&str],
        parameters: &[f64],
        fitness: f64,
    ) -> Result<(), String> {
        if parameters.len() != self.param_count {
            return Err(format!(
                "Parameter count mismatch: expected {}, got {}",
                self.param_count,
                parameters.len()
            ));
        }

        unsafe {
            let c_strings: Vec<CString> = dimension_values
                .iter()
                .map(|s| CString::new(*s).unwrap())
                .collect();

            let c_ptrs: Vec<*const c_char> = c_strings.iter().map(|s| s.as_ptr()).collect();

            if !evocore_context_learn(
                self.inner.as_ptr(),
                c_ptrs.as_ptr(),
                parameters.as_ptr(),
                self.param_count,
                fitness,
            ) {
                return Err("Failed to learn from context".to_string());
            }

            Ok(())
        }
    }

    /// Sample parameters for a context
    ///
    /// # Arguments
    /// * `dimension_values` - Values for each dimension
    /// * `exploration` - 0.0 = pure exploit, 1.0 = pure explore
    ///
    /// # Returns
    /// Sampled parameter values
    pub fn sample(
        &self,
        dimension_values: &[&str],
        exploration: f64,
    ) -> Result<Vec<f64>, String> {
        unsafe {
            let c_strings: Vec<CString> = dimension_values
                .iter()
                .map(|s| CString::new(*s).unwrap())
                .collect();

            let c_ptrs: Vec<*const c_char> = c_strings.iter().map(|s| s.as_ptr()).collect();

            let mut params = vec![0.0; self.param_count];
            let mut seed = rand::random::<u32>();

            if !evocore_context_sample(
                self.inner.as_ptr(),
                c_ptrs.as_ptr(),
                params.as_mut_ptr(),
                self.param_count,
                exploration,
                &mut seed,
            ) {
                return Err("Failed to sample parameters".to_string());
            }

            Ok(params)
        }
    }

    /// Save context system to file
    pub fn save(&self, filepath: &str) -> Result<(), String> {
        unsafe {
            let c_path = CString::new(filepath).unwrap();

            if !evocore_context_save_json(self.inner.as_ptr(), c_path.as_ptr()) {
                return Err("Failed to save context system".to_string());
            }

            Ok(())
        }
    }

    /// Load context system from file
    pub fn load(filepath: &str) -> Result<Self, String> {
        unsafe {
            let c_path = CString::new(filepath).unwrap();
            let mut system = std::ptr::null_mut();

            if !evocore_context_load_json(c_path.as_ptr(), &mut system) {
                return Err("Failed to load context system".to_string());
            }

            // Get param_count from loaded system instead of hardcoding
            let param_count = evocore_context_get_param_count(system);

            Ok(Self {
                inner: NonNull::new(system).expect("loaded system was null"),
                param_count,
            })
        }
    }

    /// Get number of contexts stored
    pub fn context_count(&self) -> usize {
        unsafe { evocore_context_count(self.inner.as_ptr()) }
    }
}

// SAFETY: The EvoCore context system can be safely sent between threads
// as long as it's not accessed concurrently from multiple threads.
unsafe impl Send for EvoCoreContextSystem {}

impl Drop for EvoCoreContextSystem {
    fn drop(&mut self) {
        unsafe {
            evocore_context_system_free(self.inner.as_ptr());
        }
    }
}

// Re-export rand for convenience
pub use rand;
