#!/bin/sh
### sarah install script
#
# Author grandhelmsman<desupport@grandhelmsman.com>

bin_file=sarah-booter
conf_file=config.json
service_file=sarah
install_dir=/usr/local/bin
base_dir=/etc/sarah

get_booter_pid() {
    # return `ps -ef|grep sarah-booter|grep -v grep|awk '{print $2}'`
    booter_pid=0
    if [ -f /etc/sarah/sarah.pid ]; then
        booter_pid=`cat /etc/sarah/sarah.pid`
    fi

    if [ $booter_pid = 0 ] || [ $booter_pid -ne 0 ]; then
        booter_pid=`ps -ef|grep sarah-booter|grep -v grep|awk '{print $2}'`
    fi

    return $booter_pid
}

# check and stop the running service first
get_booter_pid
booter_pid=$?
if [ -n "$booter_pid" ] && [ $booter_pid -ne 0 ] ; then
    echo -n "+-Try to stop the running sarah-booter with pid=$booter_pid ... "
    # kill $booter_pid
    # sudo kill -s 15 $booter_pid
    sudo service sarah stop
    echo " --[Ok]"
fi

echo -n "+-Try to copy the $bin_file to $install_dir ... "
sudo /bin/cp -f $bin_file $install_dir
sudo chmod 755 $install_dir/$bin_file
echo " --[Ok]"


if [ ! -d $base_dir ]; then
    echo -n "+-Try to create the base dir $base_dir ... "
    sudo mkdir -p $base_dir
    echo " --[Ok]"
fi

if [ -f $base_dir/$conf_file ]; then
    echo "|--$base_dir/$conf_file already exists !!!"
else
    echo -n "+-Try to copy the $conf_file to the $base_dir ... "
    sudo /bin/cp -f $conf_file $base_dir
    echo " --[Ok]"
fi


echo -n "+-Try to set the service manager ... "
sudo /bin/cp -f $service_file /etc/init.d/
sudo chmod 755 /etc/init.d/$service_file
sudo update-rc.d $service_file defaults
echo " --[Ok]"


echo -n "+-Try to set the auto boot start ... "
if [ ! -f /etc/rc2.d/S01sarah ] ; then
    sudo ln -s /etc/init.d/sarah /etc/rc2.d/S01sarah
fi
echo " --[Ok]"


# start it after install
echo -n "+-Try to start the sarah service ... "
sudo service sarah start
echo " --[Ok]"
