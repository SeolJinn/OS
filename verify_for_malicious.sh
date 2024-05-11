#!/bin/bash

#GOOD UNTIL CHECKING FOR NON-ASCII.
#TEXT EDITORS always trigger false positives. This only seems to work if I do: printf '\xFF' > file.txt directly
#I've tried directly looking if a char is outside [x00 ; x7F]. And 2 other methods but none fixed my issue

# Argument check
if [ $# -ne 1 ]; then
    echo "Usage: $0 <file>"
    exit 1
fi

file="$1"

# Check if file exists
if [ ! -f "$file" ]; then
    echo "File does not exist: $file"
    exit 1
fi

# Check if file has no permissions for owner, group, and others
if [ ! -r "$file" ] && [ ! -w "$file" ] && [ ! -x "$file" ]; then
    echo "File is missing every single permission: $file"
    exit 1
fi

# Perform basic analysis
if grep -qE "corrupted|dangerous|risk|attack|malware|malicious" "$file"; then
    echo "Potentially malicious: $file"
    exit 1
fi

# Convert hexadecimal ASCII range to decimal range
#dec_start=$(printf "%d" 0x00)
#dec_end=$(printf "%d" 0x7F)

# Check if file contains non-ASCII characters or non-printable characters
#if awk -v dec_start="$dec_start" -v dec_end="$dec_end" '{for(i=1;i<=NF;i++) if ($i < dec_start || $i > dec_end) exit 1}' "$file" || grep -Pq "[[:cntrl:]]" "$file"; then
#    echo "File contains non-ASCII characters or non-printable characters: $file"
#    exit 1
#fi

# Count the number of lines, words, and characters in the file
lines=$(wc -l < "$file" | awk '{print $1}')
words=$(wc -w < "$file" | awk '{print $1}')
chars=$(wc -m < "$file" | awk '{print $1}')

# Check if the file meets the additional condition
if [ "$lines" -lt 3 ] && [ "$words" -gt 1000 ] && [ "$chars" -gt 2000 ]; then
    echo "File meets malicious criteria: $file"
    exit 1
fi

# If no malicious content and no non-ASCII characters found and file has at least one permission, exit successfully
exit 0
