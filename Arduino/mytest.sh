#!/bin/bash

while IFS= read -r BOARD; do
    if [[ "$BOARD" =~ ^[[:space:]]*$ ]]; then
        continue #skip blank lines
    fi

    if [[ "$BOARD" == \#* || "$BOARD" == " "*#* ]]; then
        continue  # Skip comments and lines starting with space + #
    fi

    echo "$BOARD"
done < board_list.txt