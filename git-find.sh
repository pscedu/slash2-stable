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

# This must match whatever is in file slash2/mk/pre.mk

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

output=$(git log | grep "^commit" | awk 'NR=='$which'')
output=${output#* }

printf "Total # of commit is $total, you can checkout commit #$commit as follows:\n"
printf "\ngit checkout $output\n"
printf "\nWarning: You can only checkout a commit made directly in the stable tree!\n"
