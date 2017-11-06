#!/bin/bash
#
# 11/12/2015: slash2-merge.sh
#
# Pull commits from each subtree of slash2
#
# It has been tested on git version 1.8.3.1 
#
# 11/06/2017: Due to .gitignore at the top directory, we have
#             to add a file by force (-f). 

function bail-if-fail {
    if [ $? -ne 0 ] 
    then
        exit 1
    fi  
}

pfl=0
sft=0
mfio=0
wokfs=0
slash2=0
zfsfuse=0
distribfuse=0
distribiozone=0
for name in $(git remote)
do
    if [[ "$name" == "pfl" ]]
    then
        pfl=1
    fi
    if [[ "$name" == "sft" ]]
    then
        sft=1
    fi
    if [[ "$name" == "mfio" ]]
    then
        mfio=1
    fi
    if [[ "$name" == "wokfs" ]]
    then
        wokfs=1
    fi
    if [[ "$name" == "slash2" ]]
    then
        slash2=1
    fi
    if [[ "$name" == "zfs-fuse" ]]
    then
        zfsfuse=1
    fi
    if [[ "$name" == "distrib.fuse" ]]
    then
        distribfuse=1
    fi
    if [[ "$name" == "distrib.iozone" ]]
    then
        distribiozone=1
    fi
done

if [ $pfl -eq 0 ]
then
    git remote add pfl https://github.com/pscedu/pfl
    bail-if-fail
fi

if [ $sft -eq 0 ]
then
    git remote add sft https://github.com/pscedu/sft
    bail-if-fail
fi

if [ $mfio -eq 0 ]
then
    git remote add mfio https://github.com/pscedu/mfio
    bail-if-fail
fi

if [ $wokfs -eq 0 ]
then
    git remote add wokfs https://github.com/pscedu/wokfs
    bail-if-fail
fi

if [ $slash2 -eq 0 ]
then
    git remote add slash2 https://github.com/pscedu/slash2
    bail-if-fail
fi

if [ $zfsfuse -eq 0 ]
then
    git remote add zfs-fuse https://github.com/pscedu/zfs-fuse
    bail-if-fail
fi

if [ $distribfuse -eq 0 ]
then
    git remote add distrib.fuse https://github.com/pscedu/distrib.fuse
    bail-if-fail
fi

if [ $distribiozone -eq 0 ]
then
    git remote add distrib.iozone https://github.com/pscedu/distrib.iozone
    bail-if-fail
fi

git pull -X theirs --no-edit pfl master
bail-if-fail

git pull -X subtree=sft -X theirs --no-edit sft master
bail-if-fail

git pull -X subtree=mfio -X theirs --no-edit mfio master
bail-if-fail

git pull -X subtree=wokfs -X theirs --no-edit wokfs master
bail-if-fail

git pull -X subtree=slash2 -X theirs --no-edit slash2 master
bail-if-fail

git pull -X subtree=zfs-fuse -X theirs --no-edit zfs-fuse master
bail-if-fail
 
git pull -X subtree=distrib/fuse -X theirs --no-edit distrib.fuse master
bail-if-fail

git pull -X subtree=distrib/iozone -X theirs --no-edit distrib.iozone master
bail-if-fail

echo
echo "Please do a recursive diff to make sure that two trees are identical (e.g., diff -dru -x .git tree1 tree2)!"
exit 0
