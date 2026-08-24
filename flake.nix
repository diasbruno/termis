{
  description = "Termis compiler development environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  };

  outputs =
    { nixpkgs, ... }:
    let
      systems = [
        "aarch64-darwin"
        "x86_64-darwin"
        "aarch64-linux"
        "x86_64-linux"
      ];

      forAllSystems =
        f:
        nixpkgs.lib.genAttrs systems (
          system:
          f (import nixpkgs {
            inherit system;
          })
        );
    in
    {
      devShells = forAllSystems (
        pkgs:
        let
          llvm = pkgs.llvmPackages_21;
        in
        {
          default = pkgs.mkShell {
            packages = [
              llvm.clang
              llvm.llvm
              llvm.lld
              pkgs.cmake
              pkgs.ninja
              pkgs.pkg-config
            ];

            shellHook = ''
              export CC=clang
              export CXX=clang++
              export LLVM_CONFIG=${llvm.llvm.dev}/bin/llvm-config
              echo "Termis development shell: LLVM $(llvm-config --version)"
            '';
          };
        }
      );
    };
}
