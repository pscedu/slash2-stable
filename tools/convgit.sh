#!/bin/sh
# $Id$

base=$(basename $0)
dir=$(dirname $0)
cd $dir
mv $base $base.oldsvn
git clone https://github.com/pscedu/slash2.git
git clone https://github.com/pscedu/pfl.git
