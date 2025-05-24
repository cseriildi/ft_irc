#!/bin/bash

# Usage: ./sync_branches.sh "<commit message>"

COMMIT_MSG="${1:-Sync doxygen and clean branches}"

DOX_BRANCH="server-doxygen"
CLEAN_BRANCH="server"

set -e

REPO_DIR=$(git rev-parse --show-toplevel)
cd "$REPO_DIR"

echo "Committing and pushing changes on $DOX_BRANCH..."
git checkout "$DOX_BRANCH"

if ! git diff --cached --quiet || ! git diff --quiet; then
	git add .
	git commit -m "$COMMIT_MSG"
fi

git push origin "$DOX_BRANCH"

echo "Syncing $CLEAN_BRANCH with $DOX_BRANCH..."
git checkout "$CLEAN_BRANCH"
git pull origin "$CLEAN_BRANCH"
git merge "$DOX_BRANCH" --no-commit

echo "Stripping Doxygen comments..."
find . \( -name '*.cpp' -o -name '*.hpp' \) -print0 | while IFS= read -r -d '' file; do
	sed -i '/^\s*\/\*\*/,/^\s*\*\//d' "$file"    # Remove /** ... */
	sed -i '/^\s*\/\/\//d' "$file"               # Remove /// lines
	sed -i 's/\/\/\/<.*$//' "$file"              # Remove ///< inline
done

if ! git diff --cached --quiet || ! git diff --quiet; then
	git add .
	git commit -m "$COMMIT_MSG"
fi

git push origin "$CLEAN_BRANCH"

echo "Both branches pushed successfully!"
