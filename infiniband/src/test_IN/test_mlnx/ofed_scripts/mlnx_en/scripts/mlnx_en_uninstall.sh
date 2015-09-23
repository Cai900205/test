# MLNX_EN uninstall script
if ( grep -E "Ubuntu|Debian" /etc/issue > /dev/null 2>&1); then
	rpm -e --force-debian `rpm -qa | grep -E "mstflint|mlnx.en"` > /dev/null 2>&1
	apt-get remove -y `dpkg --list | grep -E "mstflint|mlnx" | awk '{print $2}'` > /dev/null 2>&1
else
	rpm -e `rpm -qa | grep -E "mstflint|mlnx.en"` > /dev/null 2>&1
fi

/bin/rm -f $0

echo "MLNX_EN uninstall done"
