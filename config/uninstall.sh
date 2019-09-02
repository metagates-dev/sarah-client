#!/bin/sh
### sarah uninstall script
#
# Author grandhelmsman<desupport@grandhelmsman.com>

bin_file=sarah-booter
service_file=sarah
install_dir=/usr/local/bin


get_booter_pid() {
    return `ps -ef|grep sarah-booter|grep -v grep|awk '{print $2}'`
}

# check and stop the running service first
get_booter_pid
booter_pid=$?
if [ -n "$booter_pid" ] && [ $booter_pid -ne 0 ] ; then
    echo -n "+-Try to stop the running sarah-booter with pid=$booter_pid ... "
    # kill $collector_pid;
    sudo service sarah stop
    echo " --[Ok]"
fi

echo -n "+-Try to remove the $install_dir/$bin_file ... "
sudo rm -f $install_dir/$bin_file
echo " --[Ok]"

echo -n "+-Try to remove the service manager script ... "
sudo rm -f /etc/init.d/$service_file
sudo update-rc.d $service_file remove
echo " --[Ok]"

echo -n "+-Try to remove the boot auto start script ... "
sudo rm -f /etc/rc2.d/S01sarah
echo " --[Ok]"
