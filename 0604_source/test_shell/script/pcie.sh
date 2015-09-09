#!/bin/sh
#$1 bridge number

echo -ne "POWERPC-PCIE_TEST-V1.00"
bridge_num=`lspci  | grep "PEX 8624 24-lane, 6-Port PCI Express "|wc -l`
echo "PCIE bridge number:$bridge_num"
if [ $bridge_num -eq $1 ]
then
	echo "[PCIE-LINK-`date +'%Y-%m-%d %T'`]: Check the link status success."
	exit 0
else
	echo "[PCIE-LINK-`date +'%Y-%m-%d %T'`]: Check the link status failed."
	exit 1
fi




