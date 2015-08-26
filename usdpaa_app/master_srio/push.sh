#!/bin/sh

git add $1
git commit -m "$1mm"
git pull --rebase ssh://root@192.168.103.85/home/ctx/fival_51/
git push  -u ssh://root@192.168.103.85/home/ctx/fival_51/  master
