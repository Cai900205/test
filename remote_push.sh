#!/bin/sh

#git add $1
#git commit -m "$1mm"
git pull --rebase origin
git push  -u origin master
