# Contributing to libethercat

Thanks for your interest in contributing! A few quick guidelines to make collaboration smoother.

## Code style
- Follow the existing C style. A `.clang-format` is provided; please run `clang-format -i` on changed files.

## Pull requests
- Fork the repo and open a PR against `master`.
- Keep PRs small and focused.
- Include a description of the problem, the approach, and relevant testing steps.

## Tests
- Add unit tests where feasible. We recommend C testing frameworks such as CMocka or Unity.

## Static analysis
- CI runs `cppcheck`. Please ensure new code does not add new cppcheck warnings.

## Contact
- Open an issue first for larger design changes to discuss before implementing.

Thanks — maintainers
