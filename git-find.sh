#!/bin/bash
#
# 01/28/2016: git-find.sh, locate the git commit given a revision number
#

if [ $# -gt 1 ]
then
    echo "Usage: git-find.sh [revision]"
    exit
fi

if [ $# -eq 1 ]
then
    commit=$1
    if [ $commit -le 0 ]
    then
        echo "Reversion number must be at least 1"
        exit 0
    fi
fi

total=$(git log | grep "^commit" | wc -l)

if [ $# -eq 1 ]
then
    if [ $commit -gt $total ]
    then
        echo "Total number of commit is only $total"
        exit 0
    fi
else
    commit=$total
fi

which=$(($total - $commit + 1))

echo "Total commit is $total, the one you want is as follows:"
echo

output=$(git log | grep "^commit" | awk 'NR=='$which'')
echo "$commit: $output"

