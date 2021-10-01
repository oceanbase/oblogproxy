#!/bin/bash

set -e

ROOT=$(git rev-parse --show-toplevel)

ln -sf $ROOT/git-hooks/pre-commit $ROOT/.git/hooks/pre-commit
echo "installed git-hooks/pre-commit to .git/hooks/pre-commit"

