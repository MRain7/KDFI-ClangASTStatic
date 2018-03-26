#!/bin/bash
clang -Xclang -fsyntax-only -Xclang -load -Xclang ./KDFI.so -Xclang -plugin -Xclang KDFI -c test.c
