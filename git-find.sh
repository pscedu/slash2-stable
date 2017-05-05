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

total=$(git log | grep -c ^commit)

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

# This must match whatever is in file slash2/mk/pre.mk

# I used to use --topo-order with git log below. But it
# does not always work.

output=$(git log | grep "^commit" | awk 'NR=='$which'')
output=${output#* }

if [ $# -eq 1 ]
then
    printf "Total # of commits is $total, you can checkout commit #$commit as follows:\n"
    printf "\ngit checkout $output\n"
    printf "\nWarning: You should only checkout a commit made directly in the stable tree!\n"
else
    printf "The current # of commits in the stable tree sandbox is $total.\n"
fi
