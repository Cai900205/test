#! /bin/sh 


dmesg | grep "PCI" | grep "link is down"
if [ $? -eq 0 ]
then
   exit 1 
fi    
echo "all pci have linked"
