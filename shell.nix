{ pkgs ? import <nixpkgs> {} }:

pkgs.mkShell {
  buildInputs = with pkgs; [
    # C build tools
    gcc
    gnumake
    pkg-config

    # C standard library
    glibc

    # Development and debugging tools
    valgrind
    gdb
    clang-tools  # For clang-format, clang-tidy

    # Testing
    check  # C unit testing framework
  ];

  shellHook = ''
    echo ""
    echo "╔════════════════════════════════════════════╗"
    echo "║     Evocore C Library - Dev Environment   ║"
    echo "║     Pure C Context Learning System        ║"
    echo "╚════════════════════════════════════════════╝"
    echo ""
    echo "🔨 Build:"
    echo "   make              # Build static library"
    echo "   make libevocore.so   # Build shared library"
    echo "   make clean        # Clean build"
    echo ""
    echo "🧪 Test:"
    echo "   make test         # Run tests (if available)"
    echo ""
    echo "📦 Output:"
    echo "   build/libevocore.a   # Static library"
    echo "   build/libevocore.so  # Shared library (for Python FFI)"
    echo ""
  '';
}
