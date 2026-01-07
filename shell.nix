{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # Base build tools
    gcc
    gnumake
    # CUDA toolkit
    cudaPackages.cudat
    cudaPackages.cuda_cccl
    cudaPackages.cudnn
    # Math libraries
    glibc
    # Development tools
    valgrind
    gdb
    # Optional: GPU monitoring
    cudaPackages.nsight_compute
    cudaPackages.nsight_systems
  ];

  # CUDA environment variables
  shellHook = ''
    export CUDA_HOME=${pkgs.cudaPackages.cudat}
    export PATH=$CUDA_HOME/bin:$PATH
    export LD_LIBRARY_PATH=$CUDA_HOME/lib64:$LD_LIBRARY_PATH
    export EVOCORE_HAVE_CUDA=1
    echo "evocore development environment with CUDA loaded"
    echo "CUDA_HOME: $CUDA_HOME"
  '';

  # Hardening disable for CUDA
  hardeningDisable = [ "all" ];
}
