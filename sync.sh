#!/bin/bash

# Usage: ./sync.sh "Commit message"
COMMIT_MSG="${1:-Sync branches}"

DOXYGEN_BRANCH="server-doxygen"
CLEAN_BRANCH="server"

set -e

REPO_DIR=$(git rev-parse --show-toplevel)
cd "$REPO_DIR"

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

if [[ "$CURRENT_BRANCH" != "$DOXYGEN_BRANCH" && "$CURRENT_BRANCH" != "$CLEAN_BRANCH" ]]; then
	echo "❌ You must be on '$DOXYGEN_BRANCH' or '$CLEAN_BRANCH' to sync."
	exit 1
fi

if ! git diff --quiet || ! git diff --cached --quiet; then
	echo "📦 Committing changes on $CURRENT_BRANCH..."
	git add .
	git commit -m "$COMMIT_MSG"
fi

git push origin "$CURRENT_BRANCH"

if [ "$CURRENT_BRANCH" = "$DOXYGEN_BRANCH" ]; then
	FROM="$DOXYGEN_BRANCH"
	TO="$CLEAN_BRANCH"
	STRIP=true
elif [ "$CURRENT_BRANCH" = "$CLEAN_BRANCH" ]; then
	FROM="$CLEAN_BRANCH"
	TO="$DOXYGEN_BRANCH"
	STRIP=false
fi

echo "🔁 Syncing $TO with $FROM..."
git checkout "$TO"
git pull origin "$TO"
git merge "$FROM" --no-commit

if [ "$STRIP" = true ]; then
	echo "🧹 Stripping Doxygen comments..."
	find . \( -name '*.cpp' -o -name '*.hpp' \) -print0 | while IFS= read -r -d '' file; do
		sed -i '/^\s*\/\*\*/,/^\s*\*\//d' "$file"
		sed -i '/^\s*\/\/\//d' "$file"
		sed -i 's/\/\/\/<.*$//' "$file"
	done
fi

git add .
git commit -m "$COMMIT_MSG" || echo "✅ No new changes to commit."

git push origin "$TO"

echo "✅ Both branches are now in sync!"
