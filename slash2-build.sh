#!/bin/bash
#
# 11/06/2015: slash2-build.sh
#
# Build a container git repository out of several slash2 related git
# trees.  After this, you can run slash2-merge.sh to keep this tree
# in sync with those individual slash2 related trees.

function bail-if-fail {
    if [ $? -ne 0 ]
    then
        exit 1
    fi
}

target="stable"
if [ $# -eq 1 ]
then
    target=$1
else
    rm -rvf $target
fi

mkdir $target
if [ $? -ne 0 ]
then
    exit 1
fi

cd $target 
git init
touch README.stable
git add README.stable 
git commit -m "Initial Commit"

git remote add pfl https://github.com/pscedu/pfl
git fetch pfl
bail-if-fail
git merge -s ours --no-commit pfl/master
git read-tree --prefix=/ -u pfl/master
git commit -m "Add pfl project at root directory"

git remote add slash2 https://github.com/pscedu/slash2
git fetch slash2 
bail-if-fail
git merge -s ours --no-commit slash2/master
git read-tree --prefix=slash2/ -u slash2/master
git commit -m "Add slash2 project under slash2 directory"

git remote add zfs-fuse https://github.com/pscedu/zfs-fuse
git fetch zfs-fuse
bail-if-fail
git merge -s ours --no-commit zfs-fuse/master
git read-tree --prefix=zfs-fuse/ -u zfs-fuse/master
git commit -m "Add zfs-fuse project under zfs-fuse directory"

git remote add sft https://github.com/pscedu/sft
git fetch sft 
bail-if-fail
git merge -s ours --no-commit sft/master
git read-tree --prefix=sft/ -u sft/master
git commit -m "Add sft project under sft directory"

git remote add mfio https://github.com/pscedu/mfio
git fetch mfio 
bail-if-fail
git merge -s ours --no-commit mfio/master
git read-tree --prefix=mfio/ -u mfio/master
git commit -m "Add mfio project under mfio directory"

git remote add distrib.fuse https://github.com/pscedu/distrib.fuse
git fetch distrib.fuse 
bail-if-fail
git merge -s ours --no-commit distrib.fuse/master
git read-tree --prefix=distrib/fuse -u distrib.fuse/master
git commit -m "Add distrib.fuse project under distrib/fuse directory"

git remote add distrib.iozone https://github.com/pscedu/distrib.iozone
git fetch distrib.iozone
git merge -s ours --no-commit distrib.iozone/master
git read-tree --prefix=distrib/iozone -u distrib.iozone/master
git commit -m "Add distrib.iozone project under distrib/iozone directory"

git remote add wokfs https://github.com/pscedu/wokfs
git fetch wokfs
bail-if-fail
git merge -s ours --no-commit wokfs/master
git read-tree --prefix=wokfs/ -u wokfs/master
git commit -m "Add wokfs project under wokfs directory"

echo "slash2 tree now lives under $(pwd)"
exit 0
