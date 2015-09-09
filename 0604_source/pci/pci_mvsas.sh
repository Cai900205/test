#! /bin/sh
# We should get two pcie controller for disk.
pcies=`dmesg | grep "mvsas" | grep -c "PCI-E"`
if [ $pcies -ne 2 ]
then
	echo "[PCIE-MVSAS-`date +'%Y-%m-%d %T'`]: We just found $pcies pcie controller for disk."
	exit 1 
fi

echo "[PCIE-MVSAS-`date +'%Y-%m-%d %T'`]: Find the plugged pcie controller for disk success."
exit 0
