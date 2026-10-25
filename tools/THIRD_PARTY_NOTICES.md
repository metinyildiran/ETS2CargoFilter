# HashFS research reader

`read-hashfs.mjs` implements the short-input CityHash 2011 algorithm described
by the scs_tools implementation and uses its format documentation:

- https://github.com/Wilps93/scs_tools/blob/main/src/cityhash.rs
- https://github.com/Wilps93/scs_tools/blob/main/FORMAT.md

MIT License

Copyright (c) 2026 scs_tools contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

The upstream implementation identifies the original CityHash algorithm as
Copyright 2011 Google. CityHash source and license: https://github.com/google/cityhash.

Game resources extracted into `.work/` remain SCS Software assets and are
not part of this project's authored source or distribution. The official SCS
packer downloaded for local analysis is also excluded from source distribution.
