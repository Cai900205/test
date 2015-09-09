#!/bin/sh
#
# [$1]: Expected lanes for the link.
# [$2]: Expected generation of the link.
#
errors=0
lanes=`dmesg | grep pci_endp | sed 's/\(.*\)lanes = \([0-9]\),\(.*\)/\2/g'`
if [$lanes -ne $1 ]
then
	echo "[PCIE-LINK-`date +'%Y-%m-%d %T'`]: We don't found $1 lanes: $lanes."
	errors=`expr $errors + 1`
fi

gen=`dmesg | grep pci_endp | sed 's/\(.*\)Gen \([0-9]\),\(.*\)/\2/g'`
if [$gen -lt $2 ]
then
	echo "[PCIE-LINK-`date +'%Y-%m-%d %T'`]: The Gen of the link is invalid."
	errors=`expr $errors + 1`
fi

if [ $errors -eq 0 ]
then
	echo "[PCIE-LINK-`date +'%Y-%m-%d %T'`]: Check the link status success: $lanes lanes, Gen $gen"
	exit 0
else
	echo "[PCIE-LINK-`date +'%Y-%m-%d %T'`]: Check the link status success: $lanes lanes, Gen $gen"
	exit 1
fi




