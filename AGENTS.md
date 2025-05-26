# Guidelines for Codex Agents

Please see the `README.md` and `docs/WHITEPAPER.md` for more information about the project.

## Development

* All code is formatted with `clang-format` using `.clang-format` in the repo root.
* Builds rely on CMake and GoogleTest. Use `scripts/run-tests.sh` to configure,
  build and run the unit tests. Pass `Debug` or `Release` to choose the build
  type (default: `Release`).
* CI runs on GCC 13 and Clang 16 in both Debug and Release modes and also runs
  `clang-tidy` on the Clang builds.
* Please include doxygen comments with code, and keep them updated if code changes merit such a change.
* Write functional unit tests for any new features added, or update them when functionality changes.
