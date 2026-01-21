use std::path::PathBuf;

fn main() {
    // Get the absolute path to the evocore-sys crate directory
    let crate_dir = PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap());
    let evocore_root = crate_dir.join("..");
    let build_path = evocore_root.join("build");
    let lib_path = build_path.join("libevocore.a");

    // Canonicalize to get absolute paths for linking
    let build_path = build_path.canonicalize().unwrap_or_else(|_| {
        panic!(
            "EvoCore build directory not found at {}. \
            Please build EvoCore first:\n  cd {} && make",
            build_path.display(),
            evocore_root.display()
        )
    });

    // Check if library exists
    if !lib_path.exists() {
        panic!(
            "EvoCore library not found at {}. \
            Please build EvoCore first:\n  cd {} && make",
            lib_path.display(),
            evocore_root.display()
        );
    }

    println!("cargo:rustc-link-search={}", build_path.display());
    println!("cargo:rustc-link-lib=static=evocore");

    // Also add include path for any direct C header includes
    let include_path = evocore_root.join("include");
    println!("cargo:include={}", include_path.display());
}
